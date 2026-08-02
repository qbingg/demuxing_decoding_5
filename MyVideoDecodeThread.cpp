#include "MyVideoDecodeThread.h"

#include "log.h"
#include "my_ffmpeg_headers.h"

MyVideoDecodeThread::MyVideoDecodeThread(QObject *parent)
    : QThread(parent)
{}

MyVideoDecodeThread::~MyVideoDecodeThread() {}

void MyVideoDecodeThread::setPlayerCtx(FFmpegPlayerCtx *ctx)
{
    is = ctx;
}

void MyVideoDecodeThread::stopThread()
{
    m_stop = 1;
}

int MyVideoDecodeThread::decode_packet(AVCodecContext *dec, const AVPacket *pkt, AVFrame *frame)
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

        QByteArray yPlane, uPlane, vPlane;
        myffut::frame_to_yuv420planes(frame, yPlane, uPlane, vPlane);
        emit sendYuv420pFrame(yPlane, uPlane, vPlane, frame->width, frame->height);

        // QImage rgb;
        // myffut::yuv_to_rgb(frame,rgb);
        {
            // pts概念：呈现时间戳Presentation timestamp
            // 时间换算：duration = pts * 时间基time_base

            // 解码后的frame->time_base是0、video_dec_ctx->time_base也是0
            // 【手册24】时间基用video_stream->time_base

            double video_clock = frame->pts * av_q2d(is->video_stream->time_base);
            double audio_clock = is->audio_clock;

            QString diff = QString::number((video_clock-audio_clock),'d',15);

            qCDebug(vdec) << "frame pts:\t" << frame->pts << "\t"
                          << "video_clock:\t" << video_clock << "\t"
                          << "audio_clock:\t" << audio_clock << "       \t"
                          << "diff：" << diff;

            // 每帧持续时间ms
            AVRational frame_dur = av_inv_q(is->video_stream->avg_frame_rate);//平均帧率的倒数，也就是每帧持续时间(s)
            double sleep_dur = av_q2d(frame_dur) * 1000;//(ms)

            // 如果“视频时钟快”于“音频时钟”
            if(video_clock > audio_clock)
                msleep(sleep_dur);
        }

        av_frame_unref(frame);
    }

    return ret;
}

void MyVideoDecodeThread::run()
{
    int ret = 0;

    if(!is){
        qDebug() << "解码线程的is为空";
        return;
    }
    if (is->video_stream)
        qDebug() << "Decoding video from file '" << is->iFile;

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

    while (true) {

        if(m_stop)
            break;

        if (is->pause) {
            msleep(10);
            continue;
        }

        // //seek后，刷新dec_ctx解码上下文
        // if (is->flush_vctx) {
        //     qCDebug(logSeek) << "视频解码线程：seek后，刷新dec_ctx解码上下文";
        //     avcodec_flush_buffers(is->video_dec_ctx);
        //     is->flush_vctx = false;
        //     continue;
        // }

        //尝试从队列获取一个包（阻塞）
        if(is->videoq.dequeue(pkt,m_stop) < 0){
            qDebug() << "解码线程：获取包失败。";
            break;
        }

        ret = decode_packet(is->video_dec_ctx, pkt ,frame);
        av_packet_unref(pkt);
        if (ret < 0)
            break;
        qCDebug(vdec)<<"解码线程：完成消费：pkt_size :"<<is->videoq.getSize();
    }

end:
    av_frame_free(&frame);
    av_packet_free(&pkt);
}
