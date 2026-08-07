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

    ui->playbackTimelineView->initChart();
    /* 因为ui->durPcmChartView不会因为重播而重置生命周期，所以不能写在btnPlay里，
     * 不然每重播一次就会多connect一次，而旧的又不会disconnect也会生效。
     * 所以改为在初始化chart后，仅连接一次即可。 */
    connect(ui->playbackTimelineView, &MyPlaybackTimelineView::sendMouseSeek, this, [=](double sec) {
        if (!playerCtx)
            return;
        seekAbsolute(sec);
    });
}

MainWindow::~MainWindow()
{
    delete ui;

    if(m_playbackTimer){
        m_playbackTimer->stop();
        delete m_playbackTimer;
        m_playbackTimer = nullptr;
    }
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
    /*重置Timer
     * 方法1. delete旧视频的Timer
     * 方法2. disconnect比较麻烦不直观
     */
    if(m_playbackTimer){
        m_playbackTimer->stop();
        delete m_playbackTimer;
        m_playbackTimer = nullptr;
    }

    if (initPlayer() < 0) {
        qDebug() << "initPlayer Failed.";
        return;
    }
    // 重置MyPlaybackTimelineView
    double durationSec = playerCtx->audio_stream->duration * av_q2d(playerCtx->audio_stream->time_base); // 时长 0~duration(音频流)，注意要考虑时间基
    ui->playbackTimelineView->resetChart(durationSec);
    ui->playbackTimelineView->resetTotalBarsOfFirstDsp(durationSec,
                                                       playerCtx->audio_tgt_freq,
                                                       playerCtx->firstDspIntervalFrames);
    m_playbackTimer = new QTimer();

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
        qCDebug(demux)<<"receive VideoPktIDR: "<<ptsSec;
        ui->playbackTimelineView->receiveVideoPktIDR(ptsSec);
    },Qt::QueuedConnection);
    connect(m_audioDecodeThread,&MyAudioDecodeThread::sendpcmPeakBar,this,[=](double timeSec,int16_t max,int16_t min){
        if(m_playSessionId != playSessionId){
            qDebug()<<"receive pcmPeakBar: playSessionId已改变，不往新视频插入旧数据";
            return;
        }
        qCDebug(dsp1) << "receive pcmPeakBar audio time(sec):" << QString::number(timeSec, 'd', 3)
                      << "\t 1stDpsIntervalMs: "
                      << playerCtx->theFirstDownSamplingIntervalMilliseconds;
        ui->playbackTimelineView->receiveFirstDspBar(timeSec,max,min);
    },Qt::QueuedConnection);
    connect(m_playbackTimer,&QTimer::timeout,this,[=]{
        ui->playbackTimelineView->updateChartView(playerCtx->audio_clock,playerCtx->video_clock);
    });
    m_playbackTimer->start(100);
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


