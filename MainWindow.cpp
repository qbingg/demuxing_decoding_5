#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "log.h"
#include "my_ffmpeg_headers.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setAcceptDrops(true);// 开启对整个窗口的拖放操作的支持

    ui->btnPause->setCheckable(true);

    initDurPcmBarChart();
}

MainWindow::~MainWindow()
{
    delete ui;
}

int MainWindow::initPlayer()
{
    // 检查文件是否存在
    QFileInfo fileInfo(windowTitle());
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "warning", "请拖入有效的文件");
        return -1;
    }

    /* 为新的视频文件初始化播放器结构体 */
    playerCtx = new FFmpegPlayerCtx;
    // 获取输入文件信息
    playerCtx->iFile = fileInfo;
    // 初始化解封装线程
    m_demuxThread = new MyDemuxThread;
    m_demuxThread->setPlayerCtx(playerCtx);
    if (m_demuxThread->initDemuxThread() != 0) {
        qDebug() << "DemuxThread init Failed.";
        return -1;
    }

    return 0;
}

void MainWindow::cleanPlayer()
{
    if (m_demuxThread) {
        // m_demuxThread->requestInterruption();
        m_demuxThread->stopThread();
        m_demuxThread->wait();
        m_demuxThread->finiDemuxThread();
        delete m_demuxThread;
        m_demuxThread = nullptr;
    }
    qDebug() << "已清空解封装线程";
    if (playerCtx) {
        delete playerCtx;
        playerCtx = nullptr;
    }
    qDebug() << "已清空playerCtx";
}

void MainWindow::initDurPcmBarChart()
{
    m_durWaveSeries = new QLineSeries();
    m_durWaveSeries->setName("分块峰值降采样法");
    m_durWaveSeries->setPen(QPen(QColor(0, 180, 255), 1)); // 浅蓝色线条
    m_durAxisX = new QValueAxis();
    m_durAxisX->setTitleText("时间 (s)");
    // m_durAxisX->setRange(0, (playerCtx->audio_stream->duration * av_q2d(playerCtx->audio_stream->time_base))); // 时长 0~duration(音频流)，注意要考虑时间基
    m_durAxisY = new QValueAxis();
    m_durAxisY->setTitleText("采样值");
    m_durAxisY->setRange(-32768, 32767); // 16位有符号整数范围

    m_durChart = new QChart();
    m_durChart->addSeries(m_durWaveSeries);
    m_durChart->addAxis(m_durAxisX, Qt::AlignBottom);
    m_durChart->addAxis(m_durAxisY, Qt::AlignLeft);

    //波形数据使用这两个坐标轴映射
    m_durWaveSeries->attachAxis(m_durAxisX);
    m_durWaveSeries->attachAxis(m_durAxisY);

    // 显示到UI的QChartView控件（对象名：chartView）
    ui->durPcmChartView->setChart(m_durChart);
    ui->durPcmChartView->setRenderHint(QPainter::Antialiasing); // 抗锯齿

    /*为pcm图表显示进行布局优化*/
    m_durChart->setTitle("");//去掉标题
    m_durChart->legend()->hide();//隐藏图表用于解释颜色和系列名称的图例框
    m_durChart->layout()->setContentsMargins(0, 0, 0, 0);//去掉外层layout的margin间隔
    m_durChart->setMargins(QMargins(0, 0, 0, 0));//去掉chart内层的margin间隔
    m_durChart->setBackgroundRoundness(0);//去掉圆角（Qt文档：此属性表示图表背景四角处圆角的直径。）
    m_durChart->setAnimationOptions(QChart::NoAnimation); // 静态图关闭动画
    // 去掉坐标轴标题
    m_durAxisX->setTitleVisible(false);
    m_durAxisY->setTitleVisible(false);
    // 去掉坐标轴刻度
    // m_durAxisX->setLabelsVisible(false);
    m_durAxisY->setLabelsVisible(false);
    // // 去掉坐标轴网格
    // m_durAxisX->setGridLineVisible(false);
    // m_durAxisY->setGridLineVisible(false);
}

void MainWindow::resetDurPcmBarChart()
{
    /* 清除chart旧视频的idrSeries
     * 不建议：
     * m_durChart->removeSeries(m_durWaveSeries);//releases the ownership
     * m_durChart ->removeAllSeries();//Qt文档：Removes and deletes
     * m_durChart->addSeries(m_durWaveSeries);
     * 因为：Qt文档：A newly added series is not attached to any axes by default
     *      （默认情况下，新添加的系列不会附加到任何轴上）
     * 需要再次：
     * m_durWaveSeries->attachAxis(m_durAxisX);
     * m_durWaveSeries->attachAxis(m_durAxisY);
     */
    const auto seriesList = m_durChart->series();
    for (QAbstractSeries *series : seriesList) {
        if (series != m_durWaveSeries) {
            m_durChart->removeSeries(series);
            delete series;
        }
    }

    //清屏（注意通过replace更新，并不存储数据）
    m_durWaveSeries->clear();
    //新视频的总时长
    m_durAxisX->setRange(0, (playerCtx->audio_stream->duration * av_q2d(playerCtx->audio_stream->time_base))); // 时长 0~duration(音频流)，注意要考虑时间基
    //清空进度条list（存储的旧视频数据）
    m_durBarPoints.clear();
}

void MainWindow::finiDurPcmBarChart()
{
    //do nothing

    /* 1. m_durChart已经通过ui->durPcmChartView->setChart(m_durChart);接管
     * 2. m_durWaveSeries已经通过m_durChart->addSeries(m_durWaveSeries);接管
     * 3. m_durAxisX已经通过m_durChart->addAxis(m_durAxisX, Qt::AlignBottom);接管
     * 4. m_durAxisY已经通过m_durChart->addAxis(m_durAxisY, Qt::AlignLeft);接管
     * 5. m_durBarPoints不是指针对象，不需要管
     */
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction(); // 接受默认的拖放行为
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();

    // 确保文件数量仅为一个
    if (urls.size() != 1) {
        if (urls.size() > 1) {
            QMessageBox::warning(this, "warning", "请拖入单个文件");
        }
        return;
    }

    const QUrl& url = urls.first();
    QString filePath = url.toLocalFile();

    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "warning", "请拖入有效的文件");
        return;
    }

    //获取文件信息，并显示到标题栏上
    setWindowTitle(fileInfo.absoluteFilePath());
}

void MainWindow::on_btnPlay_clicked()
{
    // 四步：delete -> new -> connect -> start
    cleanPlayer();

    if (initPlayer() < 0) {
        qDebug() << "initPlayer Failed.";
        return;
    }
    resetDurPcmBarChart();

    connect(m_demuxThread,&MyDemuxThread::sendVideoPktIDR,this,[=](double ptsSec){
        QLineSeries *idr = new QLineSeries();
        idr->setPen(QPen(Qt::black, 1));
        ui->durPcmChartView->chart()->addSeries(idr);
        idr->attachAxis(m_durAxisX);
        idr->attachAxis(m_durAxisY);
        idr->append(ptsSec, -32768);
        idr->append(ptsSec, 32767);
        qCDebug(demux)<<"receive VideoPktIDR: "<<ptsSec;
    },Qt::QueuedConnection);

    m_demuxThread->start();
}
