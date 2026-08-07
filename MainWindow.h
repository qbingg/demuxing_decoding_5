#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QChart>
#include <QChartView>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QGraphicsLayout>
#include <QLineSeries>
#include <QMainWindow>
#include <QMessageBox>
#include <QMimeData>
#include <QTimer>
#include <QValueAxis>
#include "MyAudioDecodeThread.h"
#include "MyDemuxThread.h"
#include "MyVideoDecodeThread.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct FFmpegPlayerCtx;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    int initPlayer();
    void cleanPlayer();

    // void initDurPcmBarChart();
    // void resetDurPcmBarChart();
    // void finiDurPcmBarChart();

    void seekRelative(double offsetSec);
    void seekAbsolute(double targetSec);

protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
private slots:
    void on_btnPlay_clicked();

    void on_btnPause_clicked(bool checked);

    void on_btnRewind_clicked();

    void on_btnForward_clicked();

private:
    Ui::MainWindow *ui;

    /* 播放器会话id
     * 1. 每次播放视频都是一次新的会话，id唯一。
     * 2. 用于GUI线程，与跨线程通信相关的槽函数
     * 3. GUI消息队列的槽函数需判断会话有效性，才能执行。
     *      因为重播时，旧视频的对象已经被销毁，但是排队中的槽函数仍会执行，
     *      新视频就会显示旧视频数据，所以必须干预处理。*/
    uint64_t m_playSessionId = 0;

    FFmpegPlayerCtx *playerCtx = nullptr;

    MyDemuxThread *m_demuxThread = nullptr;

    MyAudioDecodeThread *m_audioDecodeThread = nullptr;

    MyVideoDecodeThread *m_videoDecodeThread = nullptr;

    //进度条音频波形图
    QChart *m_durChart = nullptr;
    QLineSeries *m_durWaveSeries = nullptr;
    QValueAxis *m_durAxisX = nullptr;
    QValueAxis *m_durAxisY = nullptr;
    QList<QPointF> m_durBarPoints;//sdl每次取水的min/max点：1024->2（降采样）
    QTimer *m_durTimer = nullptr;
    QList<QLineSeries*> m_durIdrSeriesList;
    QLineSeries *m_durAudioClockSeries = nullptr;
    QLineSeries *m_durVideoClockSeries = nullptr;

};
#endif // MAINWINDOW_H
