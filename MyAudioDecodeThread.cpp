#include "MyAudioDecodeThread.h"

#include "log.h"
#include "my_ffmpeg_headers.h"

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

    {
        double bytes_per_sample = av_get_bytes_per_sample(is->audio_tgt_fmt);

        int16_t max, min;
        myffut::pcmS16PeakBarDownSampling(reinterpret_cast<int16_t *>(stream),
                                          len / bytes_per_sample,
                                          max,
                                          min);
        emit sendpcmPeakBar(is->audio_clock, max, min);
    }
}
