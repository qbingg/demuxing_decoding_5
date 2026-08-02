#ifndef MY_FFMPEG_HEADERS_H
#define MY_FFMPEG_HEADERS_H

/* my_ffmpeg_headers 头文件作用：
 * 1. 包含ffmpeg的头文件
 * 2. FFmpegPlayerCtx从MainWindow.h迁移到这里
 * 3. 声明我自己封装的ffmpeg函数
 *
 * 规定：
 * 1. #include "my_ffmpeg_headers.h"只会在.cpp出现，不允许.h出现
 * 2. 在.h中使用前向声明，如struct FFmpegPlayerCtx; struct AVFrame;
 */

/** 1. 包含ffmpeg的头文件 */
extern "C"{
// demux_decode.c的头文件
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
// 用于yuv转rgb
#include <libswscale/swscale.h>
// 用于音频重采样
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

/** 2. FFmpegPlayerCtx */
#include <QFileInfo>
#include <SDL3/SDL_audio.h>
#include "MyAudioBufQueue.h"
#include "MyPacketQueue.h"

/* 解封装线程宏 */
// 根据ffplay.c的q->size += pkt1.pkt->size + sizeof(pkt1);
// 可知，单位是Byte，所以这里的16估计和16位深bit，没什么关系。
#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
// 5 * 256 KByte
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)
// 192000 Byte（48000采样率*2声道*2位深，即1秒的数据）
#define MAX_AUDIO_FRAME_SIZE 192000
#define MAX_AUDIO_BUF_Q_SIZE ((MAX_AUDIO_FRAME_SIZE * 3) / 2)

struct FFmpegPlayerCtx {
    /* 解封装 */
    QFileInfo iFile;
    AVFormatContext *fmt_ctx = NULL;

    int video_stream_idx = -1;
    AVCodecContext *video_dec_ctx = NULL;
    AVStream *video_stream = NULL;
    MyPacketQueue videoq;

    int audio_stream_idx = -1;
    AVCodecContext *audio_dec_ctx = NULL;
    AVStream *audio_stream = NULL;
    MyPacketQueue audioq;

    /* 音频解码 */
    MyAudioBufQueue audio_buf_q;
    // 约定：SDL照着这些格式初始化，ffmpeg经过sws转为这些格式
    int audio_tgt_freq = 48000;
    AVSampleFormat audio_tgt_fmt = AV_SAMPLE_FMT_S16;
    SDL_AudioFormat audio_tgt_sdl_fmt = SDL_AUDIO_S16;//AUDIO_S16SYS;
    int audio_tgt_channels = 2;

    //这就和 ffmpeg-simple-player 的核心思想一致：用已入队音频末尾时间，减去队列里还没播放的音频时长。
    std::atomic<double> audio_enqueue_tail_clock = 0;//解码frame.pts + frame的PCM播放持续时间
    std::atomic<double> audio_clock = 0;

    /* 暂停功能：只需要暂停消费端，生产端不需要控制
     * 1、暂停音频播放设备：SDL_PauseAudio(0);//跟OpenGL一样是状态机，可全局调用
     * 2、暂停视频解码：if(is->pause) {msleep(10);continue;}
     */
    std::atomic<bool> pause = false;

    /* SDL3音频播放设备流 */
    SDL_AudioStream *sdl_audio_stream = NULL;

    /* 跳转seek功能
     * 精度为I帧AVPacket级别 */
    // seek flags and pos for seek
    std::atomic<bool> seek_req; //请求标志
    int seek_flags;             //跳转标志：前进、后退
    int64_t seek_pos; //跳转的时间(微秒)，注意：seek_pos = sec * AV_TIME_BASE 是先把秒统一成 FFmpeg 的通用微秒时间戳；av_rescale_q 再把这个通用时间戳换算成具体音频/视频流自己的时间基。
    // flush flag for seek//清的是 FFmpeg 解码器内部缓存。（旧的 P/B 帧依赖的前后帧等）
    std::atomic<bool> flush_actx = false;
    std::atomic<bool> flush_vctx = false;
};

/** 3. 声明我自己封装的ffmpeg函数 */
#include <QDebug>
#include <QImage>
// 命名空间：my ffmpeg utility
namespace myffut {

/**
 * @brief 将 YUV 帧转换为 RGB QImage。
 * @param src 输入的 AVFrame（YUV420P），必须非空且数据有效。
 * @param dst 输出的 QImage 引用，函数内会重新分配并填充 RGB 数据。
 * @return 0 成功；-1 失败
 */
inline int yuv_to_rgb(AVFrame *src, QImage &dst)
{
    qDebug() << "准备执行AVFrame转QImage，Video dimensions:"
             << "width =" << src->width
             << "height =" << src->height
             << "crop_top =" << src->crop_top
             << "crop_bottom =" << src->crop_bottom
             << "crop_left =" << src->crop_left
             << "crop_right =" << src->crop_right;

    if (src->width == 0)
    {
        qDebug() << "输入的src为空";
        return -1;
    }

    // cv::cvtColor(src,dst,cv::COLOR_YUV2RGB_I420);

    SwsContext* sws_ctx = nullptr;

    sws_ctx = sws_getContext(
        src->width, src->height, AV_PIX_FMT_YUV420P,  // 输入：YUV420P 平面格式
        src->width, src->height, AV_PIX_FMT_RGB24,    // 输出：RGB24 打包格式，与QImage格式完全匹配
        SWS_BILINEAR, nullptr, nullptr, nullptr
        );

    // 直接创建目标 QImage，由它自行管理内存
    QImage qImg(src->width, src->height, QImage::Format_RGB888);
    // 配置输出缓冲区：直接使用 QImage 内部的像素内存
    uint8_t* dst_data[1] = { qImg.bits() };//返回指向第一个像素数据的指针。
    int dst_linesize[1] = { static_cast<int>(qImg.bytesPerLine()) };//等于linesize，Returns the number of bytes per image scanline.

    // 执行格式转换，结果直接写入 QImage 内存
    sws_scale(sws_ctx,
              src->data, src->linesize,  // 输入 YUV 三平面数据与对应步长
              0, src->height,                 // 转换全部高度的行
              dst_data, dst_linesize         // 输出到 QImage 缓冲区
              );

    dst = qImg.copy();

    if (sws_ctx) {
        sws_freeContext(sws_ctx);
    }

    return 0;
}

inline int swr_cvt_pcm(AVFrame *src,
                       QByteArray &dst,
                       const int dst_channels = 2,
                       const int dst_freq_sample_rate = 48000,
                       const enum AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_S16)
{
    /* 第1步、init变量 swr_ctx */
    SwrContext *swr_ctx = nullptr;
    swr_ctx = swr_alloc();//ffmpeg文档：与libavcodec和libavformat不同，此结构是不透明的。这意味着，如果您想设置选项，必须使用AVOptions API，而不能直接为该结构的成员设置值。
    //输入音频格式，直接用音频解码上下文的参数即可
    // av_opt_set_chlayout(swr_ctx, "in_chlayout", &is->audio_dec_ctx->ch_layout, 0);
    // av_opt_set_int(swr_ctx, "in_sample_rate",       is->audio_dec_ctx->sample_rate, 0);
    // av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", is->audio_dec_ctx->sample_fmt, 0);
    //不使用AVCodecContext而是使用AVFrame的参数，这样就不需要输入音频解码上下文
    av_opt_set_chlayout(swr_ctx, "in_chlayout", &src->ch_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate",       src->sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", static_cast<AVSampleFormat>(src->format), 0);//需要强转：dectx分为sample_fmt/pix_fmt(enum)区分音频视频，而AVFrame不区分:format(int)

    AVChannelLayout outLayout;
    // use stereo
    av_channel_layout_default(&outLayout, dst_channels);
    //输入音频格式，建议在FFmpegPlayerCtx声明输出格式，而不是使用魔法数字
    av_opt_set_chlayout(swr_ctx, "out_chlayout", &outLayout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate",       dst_freq_sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);
    swr_init(swr_ctx);

    /* 第2步、转换音频格式为目标格式 */
    // 确定dst的每个采样点的位深
    int dst_bytes_per_sample = -1;
    switch (dst_sample_fmt) {
    case AV_SAMPLE_FMT_U8:{
        dst_bytes_per_sample = 1;// 8bit
        break;
    }
    case AV_SAMPLE_FMT_S16:{
        dst_bytes_per_sample = 2;// 16bit
        break;
    }
    case AV_SAMPLE_FMT_S32:{
        dst_bytes_per_sample = 4;// 32bit
        break;
    }
    default:
        break;
    }
    if(dst_bytes_per_sample == -1){
        qDebug()<<"swr_cvt_pcm()错误：未知dst_sample_fmt";
        return -1;
    }

    //swr_convert()文档：如果输入的数据量超过输出空间，则输入数据将被缓冲。
    //                  您可以通过使用swr_get_out_samples()函数来获取给定输入样本数所需输出样本数的上限，从而避免这种缓冲。
    int upper_bound_samples_per_channel = swr_get_out_samples(swr_ctx, src->nb_samples);
    uint8_t *out[4] = {0};
    int upper_bound_len = upper_bound_samples_per_channel * dst_bytes_per_sample * dst_channels;
    out[0] = (uint8_t*)av_malloc(upper_bound_len);
    // number of samples output per channel
    int samples = swr_convert(swr_ctx,
                              out,
                              upper_bound_samples_per_channel,//每个通道可用的输出采样点 amount of space available for output in samples per channel
                              (const uint8_t**)src->data,
                              src->nb_samples
                              );
    if (samples > 0) {
        // memcpy(is->audio_buf, out[0], samples * 2 * 2);
        // 入队，数据拷贝到audio_buf
        // is->audio_buf_q.enqueue((const char*)out[0],
        //                         samples * dst_bytes * dst_channels);
        int len = samples * dst_bytes_per_sample * dst_channels;
        dst.append((const char*)out[0], len);
    }
    /* 第3步、释放局部资源 */
    av_free(out[0]);
    /* 第4步、释放全局资源 */
    swr_free(&swr_ctx);
    swr_ctx = nullptr;

    return 0;
}

inline int frame_to_yuv420planes(AVFrame *src,
                                 QByteArray &dst_yPlane,
                                 QByteArray &dst_uPlane,
                                 QByteArray &dst_vPlane)
{
    const int width = src->width;
    const int height = src->height;
    const int chromaWidth = (width + 1) / 2;
    const int chromaHeight = (height + 1) / 2;

    if(width <= 0 || height <= 0){
        qDebug()<<"frame_to_yuv420planes：frame的宽高无效，"
                 <<"width:"<<width
                 <<"height"<<height;
        return -1;
    }

    //注意memcpy前，一定要给空的QByteArray分配空间
    dst_yPlane.resize(width * height);
    dst_uPlane.resize(chromaWidth * chromaHeight);
    dst_vPlane.resize(chromaWidth * chromaHeight);

    //三平面的for循环参考自：手册19：MyMux.cpp（OpenCV图片合成视频）
    //拷贝对象反转，y、u、v指针不是opencv一样连续的

    // y
    char *y_ptr = dst_yPlane.data();
    for (int y = 0; y < height; ++y) {
        memcpy(y_ptr + y * width,
               src->data[0] + y * src->linesize[0],
               width);
    }
    // u
    char* u_ptr = dst_uPlane.data();
    for (int u = 0; u < chromaHeight; ++u) {
        memcpy(u_ptr + u * chromaWidth,
               src->data[1] + u * src->linesize[1],
               chromaWidth);
    }
    // v
    char* v_ptr = dst_vPlane.data();
    for (int v = 0; v < chromaHeight; ++v) {
        memcpy(v_ptr + v * chromaWidth,
               src->data[2] + v * src->linesize[2],
               chromaWidth);
    }

    return 0;
}

//第一次降采样函数
inline int pcmS16PeakBarDownSampling(int16_t *src,const int srcLen,int16_t &dstMax,int16_t &dstMin)
{
    //求采样点集的最大最小值，无论是LRLRLR,LLLRRR,LLLLLL

    if (!src || srcLen <= 0)
        return -1;

    int16_t maxVal = std::numeric_limits<int16_t>::min();//-32768 获取 qint16 类型能表示的最小值。
    int16_t minVal = std::numeric_limits<int16_t>::max();// 32767 获取 qint16 类型能表示的最大值。

    for (int i = 0; i < srcLen; ++i) {
        maxVal = qMax(maxVal, src[i]);
        minVal = qMin(minVal, src[i]);
    }

    dstMax = maxVal;
    dstMin = minVal;

    return 0;
}

//第二次降采样函数：分块峰值降采样
inline int blockDownSampling(const QList<QPointF> &srcPointList,
                             QList<QPointF> &dstPointList,
                             const int dstBars)
{
    //分块峰值降采样
    //目标柱状图数量 dstBars
    //目标输出点的数量（一个bar对应2个点min/max）
    // const int dstPoints = dstBars * 2;

    dstPointList.clear();

    const int totalBars = srcPointList.size() / 2;
    if (totalBars <= 0 || dstBars <= 0)
        return -1;

    //src太少，直接返回即可
    if (totalBars <= dstBars) {
        dstPointList = srcPointList;
        return 0;
    }

    const int blocks = dstBars; //块数 ==  柱状图数
    //分块
    for (int block = 0; block < blocks; ++block) {
        int startBar = block * totalBars / blocks;
        int endBar = (block + 1) * totalBars / blocks;

        if (startBar >= endBar)
            continue;

        qreal maxVal = std::numeric_limits<qreal>::lowest();
        qreal minVal = std::numeric_limits<qreal>::max();
        for (int bar = startBar; bar < endBar; ++bar) {
            const QPointF &p0 = srcPointList[bar * 2];
            const QPointF &p1 = srcPointList[bar * 2 + 1];
            maxVal = qMax(maxVal, qMax(p0.y(), p1.y()));
            minVal = qMin(minVal, qMin(p0.y(), p1.y()));
        }
        //时间取块的中间值
        qreal x = (srcPointList[startBar * 2].x() + srcPointList[(endBar - 1) * 2 + 1].x()) / 2.0;

        dstPointList.append(QPointF(x, maxVal));
        dstPointList.append(QPointF(x, minVal));
    }
    return 0;
}

//第二次降采样函数：等间隔峰值降采样
inline int intervalDownSampling(const QList<QPointF> &srcPointList,
                                QList<QPointF> &dstPointList,
                                const int dstBarInterval)
{
    //等间隔峰值降采样
    //从srcBar里按BarInterval的大小，分成若干块，在从块里峰值降采样得到dstBar
    //目标输出点的数量（一个bar对应2个点min/max）

    dstPointList.clear();

    const int totalBars = srcPointList.size() / 2;
    if (totalBars <= 0 || dstBarInterval <= 0)
        return -1;

    //间隔为1，直接返回即可
    if (dstBarInterval == 1) {
        dstPointList = srcPointList;
        return 0;
    }

    // const int blocks = (totalBars + dstBarInterval - 1) / dstBarInterval;//如果想向上取整，不能简单blocks整除 +1。
    //不按 bar 分块，而是按照dstBarInterval固定间隔一刀一刀地切
    for (int startBar = 0; startBar < totalBars; startBar += dstBarInterval) {
        // int startBar = block * totalBars / blocks;
        // int endBar = (block + 1) * totalBars / blocks;
        int endBar = qMin(startBar + dstBarInterval, totalBars);

        qreal maxVal = std::numeric_limits<qreal>::lowest();
        qreal minVal = std::numeric_limits<qreal>::max();
        for (int bar = startBar; bar < endBar; ++bar) {
            const QPointF &p0 = srcPointList[bar * 2];
            const QPointF &p1 = srcPointList[bar * 2 + 1];
            maxVal = qMax(maxVal, qMax(p0.y(), p1.y()));
            minVal = qMin(minVal, qMin(p0.y(), p1.y()));
        }
        //时间取块的中间值
        qreal x = (srcPointList[startBar * 2].x() + srcPointList[(endBar - 1) * 2 + 1].x()) / 2.0;
        dstPointList.append(QPointF(x, maxVal));
        dstPointList.append(QPointF(x, minVal));
    }
    return 0;
}

//便利函数：音频波形图依据chart的width降采样
inline int durBarChartDownSampling(const QList<QPointF> &srcPointList,
                            const int totalCbBars,
                            QList<QPointF> &dstPointList,
                            const int width)
{
    //降采样：totalCbBars -> pixelBars
    //目标柱状图数量
    const int pixelBars = width;

    const int barInterval = totalCbBars / pixelBars;
    if (barInterval == 0) {
        dstPointList = srcPointList;
        qDebug() << "src.size太小了，除数为0，无需降采样";
        return 0;
    }

    return intervalDownSampling(srcPointList, dstPointList, barInterval);
}

inline void stream_seek(FFmpegPlayerCtx *is, double targetSec, int rel = -1)
{
    //把秒统一成 FFmpeg 的通用微秒时间戳
    int64_t pos = targetSec *  AV_TIME_BASE;
    /*默认 rel = -1 是因为：
     * is->seek_flags = rel < 0 ? AVSEEK_FLAG_BACKWARD : 0;
     * 这行是在决定：seek 到目标时间附近时，允许 FFmpeg 选目标时间之前还是之后的关键点。
     * 粗略 seek：可以根据方向决定 flag
     * 精确 seek：统一使用 AVSEEK_FLAG_BACKWARD ，后续再解码到目标帧（因为“精确”需要从目标前面的关键帧开始追帧。）
     *因为精确 seek 的经典流程是：
     * 1、seek 到目标时间之前的关键帧
     *     -> flush decoder
     *     -> 从关键帧开始解码
     *     -> 丢弃 target 前的帧
     *     -> 显示 target 附近/之后的第一帧
     * 2、如果只是普通播放器“粗 seek”，它可能这样设计：
     *     向前 +10s：不带 BACKWARD，尽快跳到后面的关键帧
     *     向后 -10s：带 BACKWARD，保证不要跳到目标之后
     *     这样响应会快一点，但不精确。*/
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_flags = rel < 0 ? AVSEEK_FLAG_BACKWARD : 0;
        is->seek_req = true;
    }
}

}




#endif // MY_FFMPEG_HEADERS_H
