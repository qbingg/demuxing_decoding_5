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
