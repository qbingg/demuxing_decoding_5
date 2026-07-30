#ifndef MYPACKETQUEUE_H
#define MYPACKETQUEUE_H

#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QWaitCondition>

struct AVPacket;

class MyPacketQueue : public QObject
{
    Q_OBJECT
public:
    explicit MyPacketQueue(QObject *parent = nullptr);

    //入队，参考自ffplay.c的int packet_queue_put(PacketQueue *q, AVPacket *pkt)
    int enqueue(AVPacket *pkt);
    //出队 packet_queue_get()
    int dequeue(AVPacket *pkt, std::atomic<bool> &quit);
    //清空队列
    void packetFlush();
    int getSize() const;

signals:

private:
    QQueue<AVPacket*> queue;
    /* 参考自ffpaly.c的struct PacketQueue { int size; }
     * size 是总数据量（字节数），不是包个数queue.size()。
     *      用总数据量可以精确控制缓冲区的内存上限，避免大包撑爆内存或小包堆积过多。
     *      这也解释了为什么每次取出包后要 size -= packet.size，而不是简单的 size--。
     *      这种设计在多线程音视频缓冲中是标准做法，将条件变量与字节级流量控制结合，既保证了并发效率，又保证了内存安全。
     */
    std::atomic<int> size = 0;

    QMutex mutex; // protects the buffer and the counter
    // 不建议参照QWaitCondition的Wait Conditions Example生产者消费者示例
    // 而使用两个QWaitCondition，因为生产者消费者相互阻塞和唤醒得有个前提：知道size的最大值。
    // 再者，队列就是队列，将生产者消费者的判断逻辑写在队列，不太好。
    // QWaitCondition queueNotEmpty;
    // QWaitCondition queueNotFull;
    QWaitCondition cond;
};


#endif // MYPACKETQUEUE_H
