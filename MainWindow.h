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

    void initDurPcmBarChart();
    void resetDurPcmBarChart();
    void finiDurPcmBarChart();

protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
private slots:
    void on_btnPlay_clicked();

    void on_btnPause_clicked(bool checked);

private:
    Ui::MainWindow *ui;

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

};
#endif // MAINWINDOW_H
