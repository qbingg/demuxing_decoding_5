#include "MainWindow.h"

#include <QApplication>

/* 目标：跨平台播放器Demo
 * 1. 写出Win/Linux通用的CMakeLists.txt
 * 2. 通用的third_party第三方库引入文件夹
 * 3. 通用的third_party测试视频，便于调试
 * 4. 在Win/Linux均可以运行
 * 5. 学习跨平台宏
 * 6. 基于demuxing_decoding_4的研究成果，写一个更好的空白播放器模板
 */

//TEST
extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>  // （视频）必须包含这个头文件
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}
#include <SDL3/SDL.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
