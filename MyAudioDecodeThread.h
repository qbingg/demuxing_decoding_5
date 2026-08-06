#ifndef MYAUDIODECODETHREAD_H
#define MYAUDIODECODETHREAD_H

#include <atomic>
#include <limits>
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

    /* 第一次降采样是在SDL回调线程进行的，在音频解码线程直接修改存在并发风险
     * 声明为原子变量也不能完全解决问题，
     * 因为index、max、min三个值，全部为原子变量，并不能完全同步，要知道SDL回调可是约10ms一次的超高频率
     * 更稳妥的做法就是音频解码线程不修改，只发起一个bool标志，让三个值的读写操作都在SDL回调线程里 */
    // load()读，store(false)写，exchange(false)“读旧值”和“写新值”
    std::atomic<bool> m_flushFirstDspFrameIndex = false;
    // SDL thread use only.
    int m_sampleIndex = 0;// frameIndex = sampleIndex / channels;
    int16_t m_maxVal = std::numeric_limits<int16_t>::min();//-32768 获取 qint16 类型能表示的最小值。
    int16_t m_minVal = std::numeric_limits<int16_t>::max();// 32767 获取 qint16 类型能表示的最大值。

    //第一次降采样函数
    int pcmS16PeakBarDownSampling(int16_t *src,const int srcLen);

protected:
    void run()override;
};

#endif // MYAUDIODECODETHREAD_H
