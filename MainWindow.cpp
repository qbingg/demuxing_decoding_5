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

    /* 因为ui->durPcmChartView不会因为重播而重置生命周期，所以不能写在btnPlay里，
     * 不然每重播一次就会多connect一次，而旧的又不会disconnect也会生效。
     * 所以改为在初始化chart后，仅连接一次即可。 */
    connect(ui->durPcmChartView, &MyDurChartView::sendMouseSeek, this, [=](double sec) {
        if (!playerCtx)
            return;
        seekAbsolute(sec);
    });
}

MainWindow::~MainWindow()
{
    delete ui;

    finiDurPcmBarChart();
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
    if (m_demuxThread->initDemuxThread() < 0) {
        qDebug() << "DemuxThread init Failed.";
        return -1;
    }
    m_audioDecodeThread = new MyAudioDecodeThread;
    m_audioDecodeThread->setPlayerCtx(playerCtx);
    m_videoDecodeThread = new MyVideoDecodeThread;
    m_videoDecodeThread->setPlayerCtx(playerCtx);

    return 0;
}

void MainWindow::cleanPlayer()
{
    if(m_videoDecodeThread){
        // m_videoDecodeThread->requestInterruption();
        m_videoDecodeThread->stopThread();
        m_videoDecodeThread->wait();
        delete m_videoDecodeThread;
        m_videoDecodeThread = nullptr;
    }
    qDebug()<<"Cleanup of videoDecodeThread finished.";
    if(m_audioDecodeThread){
        // m_audioDecodeThread->requestInterruption();
        m_audioDecodeThread->stopThread();
        m_audioDecodeThread->wait();
        delete m_audioDecodeThread;
        m_audioDecodeThread = nullptr;
    }
    qDebug()<<"Cleanup of audioDecodeThread finished.";
    if (m_demuxThread) {
        // m_demuxThread->requestInterruption();
        m_demuxThread->stopThread();
        m_demuxThread->wait();
        m_demuxThread->finiDemuxThread();
        delete m_demuxThread;
        m_demuxThread = nullptr;
    }
    qDebug() << "Cleanup of demuxThread finished.";
    if (playerCtx) {
        delete playerCtx;
        playerCtx = nullptr;
    }
    qDebug() << "Cleanup of playerCtx finished.";
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
    ui->durPcmChartView->initVerticalSeries(m_durChart,m_durAxisX,m_durAxisY);
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

    m_durAudioClockSeries = new QLineSeries();
    m_durAudioClockSeries->setPen(QPen(QColor(255, 180, 0), 3));
    m_durChart->addSeries(m_durAudioClockSeries);
    m_durAudioClockSeries->attachAxis(m_durAxisX);
    m_durAudioClockSeries->attachAxis(m_durAxisY);

    m_durVideoClockSeries = new QLineSeries();
    m_durVideoClockSeries->setPen(QPen(QColor(180, 0, 255), 3));
    m_durChart->addSeries(m_durVideoClockSeries);
    m_durVideoClockSeries->attachAxis(m_durAxisX);
    m_durVideoClockSeries->attachAxis(m_durAxisY);
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
     * 也不建议：
     * if (series != m_durWaveSeries) {...}
     * 每次新加m_series都需要在这if加上判断防止误删，我已经遭遇2次崩溃，调试发现因为如此。
     */
    const auto seriesList = m_durChart->series();
    for (QAbstractSeries *series : seriesList) {
        if(m_durIdrSeriesList.contains(series)){
            m_durChart->removeSeries(series);
            delete series;
        }
    }
    //清空记录idr的list
    m_durIdrSeriesList.clear();

    //清屏（注意通过replace更新，并不存储数据）
    m_durWaveSeries->clear();
    //新视频的总时长
    m_durAxisX->setRange(0, (playerCtx->audio_stream->duration * av_q2d(playerCtx->audio_stream->time_base))); // 时长 0~duration(音频流)，注意要考虑时间基
    //清空进度条list（存储的旧视频数据）
    m_durBarPoints.clear();

    /*重置Timer
     * 方法1. delete旧视频的Timer
     * 方法2. disconnect比较麻烦不直观
     */
    if(m_durTimer){
        m_durTimer->stop();
        delete m_durTimer;
        m_durTimer = nullptr;
    }
    m_durTimer = new QTimer();
}

void MainWindow::finiDurPcmBarChart()
{
    /* 1. m_durChart已经通过ui->durPcmChartView->setChart(m_durChart);接管
     * 2. m_durWaveSeries已经通过m_durChart->addSeries(m_durWaveSeries);接管
     * 3. m_durAxisX已经通过m_durChart->addAxis(m_durAxisX, Qt::AlignBottom);接管
     * 4. m_durAxisY已经通过m_durChart->addAxis(m_durAxisY, Qt::AlignLeft);接管
     * 5. m_durBarPoints不是指针对象，不需要管
     */

    // 如果是new QTimer(this);有父对象，则不需要此步
    if(m_durTimer){
        m_durTimer->stop();
        delete m_durTimer;
        m_durTimer = nullptr;
    }
}

void MainWindow::seekRelative(double offsetSec)
{
    double targetSec = playerCtx->audio_clock + offsetSec;
    //边界检查 0 <= targetSec <= durationSec
    double durationSec = (playerCtx->audio_stream->duration * av_q2d(playerCtx->audio_stream->time_base)); // 时长 0~duration(音频流)，注意要考虑时间基
    targetSec = qBound(0.0, targetSec, durationSec);

    qDebug() << "seekRelative to:" << targetSec << "audio_clock:" << playerCtx->audio_clock;

    myffut::stream_seek(playerCtx,targetSec);
}

void MainWindow::seekAbsolute(double targetSec)
{
    //边界检查 0 <= targetSec <= durationSec
    double durationSec = (playerCtx->audio_stream->duration * av_q2d(playerCtx->audio_stream->time_base)); // 时长 0~duration(音频流)，注意要考虑时间基
    targetSec = qBound(0.0, targetSec, durationSec);

    qDebug() << "seekAbsolute to:" << targetSec << "audio_clock:" << playerCtx->audio_clock;

    myffut::stream_seek(playerCtx,targetSec);
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
    //播放新视频，更新会话id
    m_playSessionId++;
    const uint64_t playSessionId = m_playSessionId;

    // 四步：delete -> new -> connect -> start
    cleanPlayer();

    if (initPlayer() < 0) {
        qDebug() << "initPlayer Failed.";
        return;
    }
    resetDurPcmBarChart();

    connect(m_demuxThread,&MyDemuxThread::sendVideoPktIDR,this,[=](double ptsSec){
        /* 在极端情况下，是可触发的，所以还是加判断吧。
         * 18:44:45: Starting D:\Codes\cppCode\FFmpegCode\demuxing_decoding_5\build\Desktop_Qt_6_5_3_MSVC2019_64bit-Debug\demuxing_decoding_5.exe...
         * Decoding audio from file ' QFileInfo(D:\Codes\cppCode\FFmpegCode\third_party\test_videos\352x288_25fps.mp4)
         * receive VideoPktIDR: playSessionId已改变，不往新视频插入旧数据
         * receive pcmPeakBar: playSessionId已改变，不往新视频插入旧数据 */
        if(m_playSessionId != playSessionId){
            qDebug()<<"receive VideoPktIDR: playSessionId已改变，不往新视频插入旧数据";
            return;
        }
        QLineSeries *idr = new QLineSeries();
        idr->setPen(QPen(Qt::black, 1));
        ui->durPcmChartView->chart()->addSeries(idr);
        idr->attachAxis(m_durAxisX);
        idr->attachAxis(m_durAxisY);
        idr->append(ptsSec, -32768);
        idr->append(ptsSec, 32767);
        qCDebug(demux)<<"receive VideoPktIDR: "<<ptsSec;
        // 记录idr以便reset时清除chart旧视频的idrSeries
        m_durIdrSeriesList.append(idr);
    },Qt::QueuedConnection);
    connect(m_audioDecodeThread,&MyAudioDecodeThread::sendpcmPeakBar,this,[=](double time,int16_t max,int16_t min){
        if(m_playSessionId != playSessionId){
            qDebug()<<"receive pcmPeakBar: playSessionId已改变，不往新视频插入旧数据";
            return;
        }
        m_durBarPoints.append(QPointF(time,max));
        m_durBarPoints.append(QPointF(time,min));
    },Qt::QueuedConnection);
    // connect(m_durTimer,&QTimer::timeout,this,[=]{
    //     // m_durWaveSeries->replace(m_durPoints);

    //     //第一次采样：totalBars -> totalCbBars
    //     //第二次采样：totalCbBars -> pixelBars

    //     //目标柱状图数量
    //     const int pixelBars = ui->durPcmChartView->width();
    //     //总时长
    //     const double duration = playerCtx->audio_stream->duration * av_q2d(playerCtx->audio_stream->time_base);
    //     //采样率（不是解码后的，而是swr后给sdl播放的）
    //     const double sampleRate = playerCtx->audio_tgt_freq;//采样率（每秒采样次数）44100.0;
    //     //总采样数
    //     const double samples = duration * sampleRate;
    //     //sdl callback总次数（取水次数）
    //     const double totalCbBars = samples / 1024.0;
    //     //第二次采样间隔
    //     const int dspBarsInterval = totalCbBars / pixelBars;

    //     QList<QPointF> pList;
    //     // blockDownSampling(m_durPoints,pList,pixelBars);
    //     // intervalDownSampling(m_durPoints,pList,dspBarsInterval);
    //     myffut::durBarChartDownSampling(m_durBarPoints,totalCbBars,pList,ui->durPcmChartView->width());

    //     m_durWaveSeries->replace(pList);

    //     QList<QPointF> pAudioClockList;
    //     pAudioClockList.append(QPointF(playerCtx->audio_clock,0));
    //     pAudioClockList.append(QPointF(playerCtx->audio_clock,-32768));
    //     m_durAudioClockSeries->replace(pAudioClockList);

    //     QList<QPointF> pVideoClockList;
    //     pVideoClockList.append(QPointF(playerCtx->video_clock,0));
    //     pVideoClockList.append(QPointF(playerCtx->video_clock,32767));
    //     m_durVideoClockSeries->replace(pVideoClockList);

    // });
    connect(m_durTimer,&QTimer::timeout,this,[=]{

        //目标柱状图数量
        const int totalBarsOfSecondDsp = ui->durPcmChartView->width();
        //总时长
        const uint64_t duration = playerCtx->audio_stream->duration * av_q2d(playerCtx->audio_stream->time_base);
        //采样率（不是解码后的，而是swr后给sdl播放的）
        const double sampleRate = playerCtx->audio_tgt_freq;//采样率（每秒采样次数）48000.0;
        //总采样数（考虑声道，如2sample=1frame）
        const uint64_t totalFrames = duration * sampleRate;
        //第一次降采样后，总采样数
        uint64_t totalBarsOfFirstDsp = totalFrames / playerCtx->firstDspIntervalFrames;
        //第二次采样间隔
        const int secondDspIntervalBars = totalBarsOfFirstDsp / totalBarsOfSecondDsp;

        QList<QPointF> pList;
        // blockDownSampling(m_durPoints,pList,pixelBars);
        // intervalDownSampling(m_durPoints,pList,dspBarsInterval);
        myffut::durBarChartDownSampling(m_durBarPoints,totalBarsOfFirstDsp,pList,ui->durPcmChartView->width());

        m_durWaveSeries->replace(pList);

        QList<QPointF> pAudioClockList;
        pAudioClockList.append(QPointF(playerCtx->audio_clock,0));
        pAudioClockList.append(QPointF(playerCtx->audio_clock,-32768));
        m_durAudioClockSeries->replace(pAudioClockList);

        QList<QPointF> pVideoClockList;
        pVideoClockList.append(QPointF(playerCtx->video_clock,0));
        pVideoClockList.append(QPointF(playerCtx->video_clock,32767));
        m_durVideoClockSeries->replace(pVideoClockList);

    });
    m_durTimer->start(100);
    connect(m_videoDecodeThread,
            &MyVideoDecodeThread::sendYuv420pFrame,
            ui->widget,
            &MyYUV420POpenGLWidget::setYuv420pFrame,
            Qt::QueuedConnection);

    m_demuxThread->start();
    m_audioDecodeThread->start();
    m_videoDecodeThread->start();
}

void MainWindow::on_btnPause_clicked(bool checked)
{
    if (!playerCtx && !playerCtx->sdl_audio_stream) {
        ui->btnPause->setChecked(false);
        return;
    }

    if(checked){
        ui->btnPause->setText("继续");

        playerCtx->pause = true;
        // SDL_PauseAudio(1);
        SDL_PauseAudioStreamDevice(playerCtx->sdl_audio_stream);


    }else{
        ui->btnPause->setText("暂停");

        playerCtx->pause = false;
        // SDL_PauseAudio(0);
        SDL_ResumeAudioStreamDevice(playerCtx->sdl_audio_stream);
    }

    qDebug() << "playerCtx->pause: " << playerCtx->pause;
}

void MainWindow::on_btnRewind_clicked()
{
    if (!playerCtx)
        return;

    seekRelative(-10);
}

void MainWindow::on_btnForward_clicked()
{
    if (!playerCtx)
        return;

    seekRelative(+10);
}


