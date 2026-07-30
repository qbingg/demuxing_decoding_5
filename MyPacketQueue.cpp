#include "MyPacketQueue.h"

#include "my_ffmpeg_headers.h"

MyPacketQueue::MyPacketQueue(QObject *parent)
    : QObject{parent}
{}

int MyPacketQueue::enqueue(AVPacket *pkt)
{
    AVPacket *pkt1;

    pkt1 = av_packet_alloc();
    if (!pkt1) {
        av_packet_unref(pkt);
        return -1;
    }
    av_packet_move_ref(pkt1, pkt);

    {
        const QMutexLocker locker(&mutex); //加锁，锁不上就在这里阻塞着
        // 入队
        queue.enqueue(pkt1);
        size += pkt1->size; // 注意不是队列元素个数size = queue.size();

        cond.wakeOne(); // 队列不为空，可以唤醒一个因为cond.wait(&mutex, 500);阻塞的消费者了。
    }
    return 0;
}

int MyPacketQueue::dequeue(AVPacket *pkt, std::atomic<bool> &quit)
{
    int ret = 0;
    {
        const QMutexLocker locker(&mutex); //加锁，锁不上就在这里阻塞着

        while (true) {
            if (!queue.isEmpty()) {
                // 取队首packet
                AVPacket *pkt1 = queue.dequeue();
                size -= pkt1->size; // 注意不是队列元素个数size = queue.size();
                av_packet_move_ref(pkt, pkt1);
                av_packet_free(&pkt1);

                ret = 1;
                break;
            } else {
                // 带500ms超时等待（完全对应SDL_CondWaitTimeout）
                // wait返回false=超时，true=被唤醒
                cond.wait(&mutex, 500);
            }

            // 退出标记
            if (quit) {
                ret = -1;
                break;
            }
        }
    }
    return ret;
}

void MyPacketQueue::packetFlush()
{
    const QMutexLocker locker(&mutex); //加锁，锁不上就在这里阻塞着

    // 释放所有packet内存（和原来一致）
    for (auto &pkt : queue) {
        av_packet_unref(pkt);
    }
    queue.clear();
    size = 0;
}

int MyPacketQueue::getSize() const
{
    return size;
}
