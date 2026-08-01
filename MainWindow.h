#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFileInfo>
#include <QMainWindow>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QChart>
#include <QLineSeries>
#include <QValueAxis>
#include <QChartView>
#include <QGraphicsLayout>
#include <QTimer>
#include "MyDemuxThread.h"

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

private:
    Ui::MainWindow *ui;

    FFmpegPlayerCtx *playerCtx = nullptr;

    MyDemuxThread *m_demuxThread = nullptr;


    //进度条音频波形图
    QChart *m_durChart = nullptr;
    QLineSeries *m_durWaveSeries = nullptr;
    QValueAxis *m_durAxisX = nullptr;
    QValueAxis *m_durAxisY = nullptr;
    QList<QPointF> m_durBarPoints;//sdl每次取水的min/max点：1024->2（降采样）

};
#endif // MAINWINDOW_H
