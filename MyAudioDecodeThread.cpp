#include "MyAudioDecodeThread.h"

#include "log.h"
#include "my_ffmpeg_headers.h"

/* this function will be called (usually in a background thread) when the audio stream is consuming data. */
/* 当音频流消耗数据时，该函数会被调用（通常运行在后台线程中） */
static void SDLCALL FeedTheAudioStreamMore(void *userdata, SDL_AudioStream *astream, int additional_amount, int total_amount)
{
    /* total_amount is how much data the audio stream is eating right now, additional_amount is how much more it needs
       than what it currently has queued (which might be zero!). You can supply any amount of data here; it will take what
       it needs and use the extra later. If you don't give it enough, it will take everything and then feed silence to the
       hardware for the rest. Ideally, though, we always give it what it needs and no extra, so we aren't buffering more
       than necessary. */
    /*
       total_amount：音频流本轮总共要消耗的数据量
       additional_amount：除了当前已排队的数据（可能为0）之外，还需要补充的数据量

       你可以在这里传入任意体量的数据：流会先取走当下需要的部分，多余的数据会缓存起来留待后续使用。
       如果提供的数据不足，流会先把所有数据用完，剩余的缺口会向音频硬件输出静音来填充。
       不过理想状态下，我们只提供刚好满足需求的数据、不多给，避免产生不必要的缓冲冗余。*/
    MyAudioDecodeThread *adt = reinterpret_cast<MyAudioDecodeThread *>(userdata);
    int len = additional_amount;
    additional_amount /= (adt->getBytesToSamples());  /* convert from bytes to samples *//* 将单位从字节转换为采样点数 */

    if (additional_amount > 0){
        QByteArray samples;
        samples.reserve(len);
        adt->getAudioData((unsigned char *)samples.data(),len);
        SDL_PutAudioStreamData(astream, samples.data(), len);
    }

    // //示例数据格式：audio in as mono, float32 data at 8000Hz.
    // additional_amount /= sizeof (float);  /* convert from bytes to samples *//* 将单位从字节转换为采样点数 */
    // while (additional_amount > 0) {
    //     float samples[128];  /* this will feed 128 samples each iteration until we have enough. *//* 每次循环生成128个采样点，循环直到补足所需数据量 */
    //     const int total = SDL_min(additional_amount, SDL_arraysize(samples));
    //     int i;
    //
    //     /* generate a 440Hz pure tone */
    //     /* 生成440Hz的正弦纯音 */
    //     for (i = 0; i < total; i++) {
    //         const int freq = 440;
    //         const float phase = current_sine_sample * freq / 8000.0f;
    //         samples[i] = SDL_sinf(phase * 2 * SDL_PI_F);
    //         current_sine_sample++;
    //     }
    //
    //     /* wrapping around to avoid floating-point errors */
    //     /* 对采样计数取模回绕，避免数值持续增大导致浮点计算出现误差 */
    //     current_sine_sample %= 8000;
    //
    //     /* feed the new data to the stream. It will queue at the end, and trickle out as the hardware needs more data. */
    //     /* 将新生成的数据送入音频流。数据会在流的末尾排队，随着硬件的需求逐步输出 */
    //     SDL_PutAudioStreamData(astream, samples, total * sizeof (float));
    //     additional_amount -= total;  /* subtract what we've just fed the stream. *//* 减去本次已填充的采样数 */
    // }
}

MyAudioDecodeThread::MyAudioDecodeThread(QObject *parent)
    : QThread(parent)
{}

MyAudioDecodeThread::~MyAudioDecodeThread(){}

int MyAudioDecodeThread::decode_packet(AVCodecContext *dec, const AVPacket *pkt, AVFrame *frame)
{
    int ret = 0;

    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0) {
        qDebug() << "Error submitting a packet for decoding (";// << av_err2str(ret) << ")";
        return ret;
    }

    // get all the available frames from the decoder
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec, frame);
        if (ret < 0) {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                return 0;

            qDebug() << "Error during decoding (";// << av_err2str(ret) << ")";
            return ret;
        }

        /**
         *  将数据转化为目标格式
         */
        QByteArray dst;
        myffut::swr_cvt_pcm(frame,dst);
        is->audio_buf_q.enqueue(dst.data(),dst.size());

        {
            //计算该帧frame的PCM播放持续时间
            uint64_t bytes = dst.size();
            // 换算成采样点sample，公式：Byte = ( sample * 采样点的位深 ) * 声道数
            double channels = static_cast<double>(is->audio_tgt_channels);
            double bytes_per_sample = av_get_bytes_per_sample(is->audio_tgt_fmt);
            uint64_t samples = (bytes / channels) / bytes_per_sample;
            // 换算成时间s，公式：s = 采样点 / 每秒采样次数sample_rate
            double sample_rate = is->audio_tgt_freq;
            double duration = samples / sample_rate;

            double pts = frame->pts * av_q2d(is->audio_stream->time_base);//frame->time_base = 0,！！！24、时间基time_base用AVStream
            double frame_duration = duration;
            is->audio_enqueue_tail_clock = pts + frame_duration;

            qCDebug(adec) << "audio_buf队列数量：" << is->audio_buf_q.getSize() << "pts: " << pts
                          << "frame_duration: " << frame_duration
                          << "audio_enqueue_tail_clock: " << is->audio_enqueue_tail_clock;
        }

        av_frame_unref(frame);
    }

    return ret;
}

void MyAudioDecodeThread::getAudioData(unsigned char *stream, int len)
{
    qDebug()<<"getAudioData:"<<len;

    // decoder is not ready or in pause state, output silence
    if (!is->audio_dec_ctx) {
        memset(stream, 0, len);
        return;
    }

    {
        // 队内剩余Byte
        int bytes = is->audio_buf_q.getSize();
        // 换算成采样点sample，公式：Byte = ( sample * 采样点的位深 ) * 声道数
        double channels = static_cast<double>(is->audio_tgt_channels);
        double bytes_per_sample = av_get_bytes_per_sample(is->audio_tgt_fmt);
        uint64_t samples = (bytes / channels) / bytes_per_sample;
        // 换算成时间s，公式：s = 采样点 / 每秒采样次数sample_rate
        double sample_rate = is->audio_tgt_freq;
        double duration = samples / sample_rate;

        // pts - 队内剩余Byte的时间 = 音频时钟
        is->audio_clock = is->audio_enqueue_tail_clock - duration;
        qCDebug(adec) << "ffmpeg-simple-player audio_clock:" << is->audio_clock;
    }

    int ret = is->audio_buf_q.dequeue(stream, len, m_stop,is->pause);
    if (ret == -1) {
        memset(stream, 0, len);
        return;
    }

    // {
    //     double bytes_per_sample = av_get_bytes_per_sample(is->audio_tgt_fmt);
    //
    //     int16_t max, min;
    //     myffut::pcmS16PeakBarDownSampling(reinterpret_cast<int16_t *>(stream),
    //                                       len / bytes_per_sample,
    //                                       max,
    //                                       min);
    //     emit sendpcmPeakBar(is->audio_clock, max, min);
    // }
    {
        double bytes_per_sample = av_get_bytes_per_sample(is->audio_tgt_fmt);

        pcmS16PeakBarDownSampling(reinterpret_cast<int16_t *>(stream), len / bytes_per_sample);
    }
}

double MyAudioDecodeThread::getBytesToSamples() const
{
    return av_get_bytes_per_sample(is->audio_tgt_fmt);
}

int MyAudioDecodeThread::pcmS16PeakBarDownSampling(int16_t *src, const int srcLen)
{
    //求采样点集的最大最小值，无论是LRLRLR,LLLRRR,LLLLLL

    if (!src || srcLen <= 0)
        return -1;

    // int16_t maxVal = std::numeric_limits<int16_t>::min();//-32768 获取 qint16 类型能表示的最小值。
    // int16_t minVal = std::numeric_limits<int16_t>::max();// 32767 获取 qint16 类型能表示的最大值。

    for (int i = 0; i < srcLen; ++i) {
        m_maxVal = qMax(m_maxVal, src[i]);
        m_minVal = qMin(m_minVal, src[i]);
        m_sampleIndex++;

        // frameIndex = sampleIndex / channels;
        if ((m_sampleIndex / is->audio_tgt_channels) >= is->firstDspIntervalFrames) {
            emit sendpcmPeakBar(is->audio_clock, m_maxVal, m_minVal);
            // 重置索引
            m_sampleIndex = 0;
            m_maxVal = std::numeric_limits<int16_t>::min(); //-32768 获取 qint16 类型能表示的最小值。
            m_minVal = std::numeric_limits<int16_t>::max(); // 32767 获取 qint16 类型能表示的最大值。
        }
    }
    return 0;
}

void MyAudioDecodeThread::setPlayerCtx(FFmpegPlayerCtx *ctx)
{
    is = ctx;
}

void MyAudioDecodeThread::stopThread()
{
    m_stop = 1;
}

void MyAudioDecodeThread::run()
{
    int ret = 0;

    if(!is){
        qDebug() << "音频解码线程的is为空";
        return;
    }
    if (is->video_stream)
        qDebug() << "Decoding audio from file '" << is->iFile;

    AVFrame *frame = NULL;
    AVPacket *pkt = NULL;
    frame = av_frame_alloc();
    if (!frame) {
        qDebug() << "Could not allocate frame";
        ret = AVERROR(ENOMEM);
        goto end;
    }
    pkt = av_packet_alloc();
    if (!pkt) {
        qDebug() << "Could not allocate packet";
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* SDL初始化*/
    SDL_AudioSpec spec;
    //SDL initialize
    if (!SDL_Init(SDL_INIT_AUDIO))    // 支持AUDIO
    {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        return;
    }
    // 音频参数设置SDL_AudioSpec
    // spec.freq = is->audio_dec_ctx->sample_rate;// 采样频率48000
    // spec.format = AUDIO_F32SYS; // 采样点格式 AUDIO_S16SYS
    // spec.channels = 1; //is->audio_dec_ctx->ch_layout.nb_channels;// 2通道
    // spec.silence = 0;
    // spec.samples = 1024;// 23.2ms -> 46.4ms 每次读取的采样数量，多久产生一次回调和 samples
    // spec.callback = FN_Audio_Cb; // 回调函数
    // spec.userdata = this;
    spec.freq = is->audio_tgt_freq;
    spec.format = is->audio_tgt_sdl_fmt;
    spec.channels = is->audio_tgt_channels;
    // spec.silence = 0;
    // spec.samples = 1024;// 23.2ms -> 46.4ms 每次读取的采样数量，多久产生一次回调和 samples
    // spec.callback = FN_Audio_Cb; // 回调函数
    // spec.userdata = this;

    //打开音频设备
    is->sdl_audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, FeedTheAudioStreamMore, this);//NULL);
    if (!is->sdl_audio_stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
        goto _FAIL;
    }
    /* SDL_OpenAudioDeviceStream starts the device paused. You have to tell it to start! */
    SDL_ResumeAudioStreamDevice(is->sdl_audio_stream);

    while (true) {

        if(m_stop)
            break;

        //seek后，刷新dec_ctx解码上下文，音频解码线程还得单独清理PCM buf队列
        if (is->flush_actx) {
            is->flush_actx = false;
            qCDebug(adec) << "音频解码线程：seek后，刷新dec_ctx解码上下文";
            avcodec_flush_buffers(is->audio_dec_ctx);
            //清理PCM buf队列，不然1~2秒后才会播放seek的音频
            is->audio_buf_q.bufFlush();
            //seek后重置 m_sampleIndex/m_maxVal/m_minVal，重新累计第一次降采样
            m_sampleIndex = 0; // 重置索引
            m_maxVal = std::numeric_limits<int16_t>::min(); //-32768 获取 qint16 类型能表示的最小值。
            m_minVal = std::numeric_limits<int16_t>::max(); // 32767 获取 qint16 类型能表示的最大值。

            continue;
        }

        // 检查audio_buf队列的数量
        if(is->audio_buf_q.getSize() > MAX_AUDIO_BUF_Q_SIZE){
            msleep(10);// SDL_Delay(10);
            continue;
        }

        //尝试从队列获取一个包（阻塞）
        if(is->audioq.dequeue(pkt,m_stop) < 0){
            qDebug() << "解码线程：获取包失败。";
            break;
        }

        ret = decode_packet(is->audio_dec_ctx, pkt ,frame);
        av_packet_unref(pkt);
        if (ret < 0)
            break;
        qCDebug(adec)<<"解码线程：完成消费：pkt_size :"<<is->videoq.getSize();
        // qCDebug(adec)<<"SDL_GetAudioStreamQueued():"<<SDL_GetAudioStreamQueued(stream);
    }
_FAIL:
    //release some resources
    // 关闭音频设备
    // SDL_CloseAudio();
    //quit SDL
    SDL_Quit();
end:
    av_frame_free(&frame);
    av_packet_free(&pkt);
}
