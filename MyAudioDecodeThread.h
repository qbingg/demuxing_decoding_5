#ifndef MYAUDIODECODETHREAD_H
#define MYAUDIODECODETHREAD_H

#include <QDebug>
#include <QThread>
#include <SDL3/SDL.h>

struct AVCodecContext;
struct AVPacket;
struct AVFrame;
struct FFmpegPlayerCtx;

class MyAudioDecodeThread : public QThread
{
    Q_OBJECT
public:
    explicit MyAudioDecodeThread(QObject *parent = nullptr);
    ~MyAudioDecodeThread();
    void setPlayerCtx(FFmpegPlayerCtx *ctx);
    void stopThread();

    /**
     * 解码音视频数据包，将AVPacket解析为AVFrame
     * @param dec 解码器上下文，怎么来的请看open_codec_context()
     * @param pkt 输入包
     * @param frame 一个包有多个帧，并不是最终输出帧，只是作为载体，全局变量不用重复new AVFrame
     * @return 解码结果状态码
     */
    int decode_packet(AVCodecContext *dec, const AVPacket *pkt ,AVFrame *frame);

    void getAudioData(unsigned char *stream, int len);

    double getBytesToSamples() const;

signals:
    void sendMessage();
    void sendpcmPeakBar(double,int16_t,int16_t);

private:
    FFmpegPlayerCtx *is = nullptr;

    std::atomic<bool> m_stop = 0;

    int m_frameIndex = 0;
    int16_t m_maxVal = std::numeric_limits<int16_t>::min();//-32768 获取 qint16 类型能表示的最小值。
    int16_t m_minVal = std::numeric_limits<int16_t>::max();// 32767 获取 qint16 类型能表示的最大值。

    //第一次降采样函数
    int pcmS16PeakBarDownSampling(int16_t *src,const int srcLen);

protected:
    void run()override;
};

#endif // MYAUDIODECODETHREAD_H
