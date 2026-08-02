#ifndef MYVIDEODECODETHREAD_H
#define MYVIDEODECODETHREAD_H

#include <QImage>
#include <QFileInfo>
#include <QThread>

struct AVCodecContext;
struct AVPacket;
struct AVFrame;
struct FFmpegPlayerCtx;

class MyVideoDecodeThread : public QThread
{
    Q_OBJECT
public:
    explicit MyVideoDecodeThread(QObject *parent = nullptr);
    ~MyVideoDecodeThread();

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

signals:
    void sendRgb24Frame(QImage);
    void sendYuv420pFrame(const QByteArray &yPlane,
                          const QByteArray &uPlane,
                          const QByteArray &vPlane,
                          int width,
                          int height);

private:
    FFmpegPlayerCtx *is = nullptr;

    std::atomic<bool> m_stop = 0;

protected:
    void run()override;
};


#endif // MYVIDEODECODETHREAD_H
