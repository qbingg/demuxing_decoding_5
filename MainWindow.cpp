#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "log.h"
#include "my_ffmpeg_headers.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btnPlay_clicked()
{
    if(m_demuxThread){
        // m_demuxThread->requestInterruption();
        m_demuxThread->stopThread();
        m_demuxThread->wait();
        m_demuxThread->finiDemuxThread();
        delete m_demuxThread;
        m_demuxThread = nullptr;
    }
    qDebug()<<"已清空解封装线程";
    if(playerCtx){
        delete playerCtx;
        playerCtx = nullptr;
    }
    qDebug()<<"已清空playerCtx";

    // 检查文件是否存在
    QFileInfo fileInfo(windowTitle());
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "warning", "请拖入有效的文件");
        return;
    }

    /* 为新的视频文件初始化播放器结构体 */
    playerCtx = new FFmpegPlayerCtx;
    // 获取输入文件信息
    playerCtx->iFile = fileInfo;
    // 初始化解封装线程
    m_demuxThread = new MyDemuxThread;
    m_demuxThread->setPlayerCtx(playerCtx);
    if (m_demuxThread->initDemuxThread() != 0) {
        qDebug()<< "DemuxThread init Failed.";
        return;
    }

    m_demuxThread->start();
}
