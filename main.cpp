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
#if defined(__linux__)
    //Ubuntu26.04,Qt6.5.3缺失窗口阴影和圆角，切换回X11/XWayland
    //https://www.mail-archive.com/ubuntu-bugs%40lists.ubuntu.com/msg6288618.html
    qputenv("QT_QPA_PLATFORM", "xcb");
#endif

    //TEST
    SDL_Init(SDL_INIT_AUDIO);

    QLoggingCategory::setFilterRules(
        "*.debug=true\n"
        "log1.debug=true\n"
        );

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    // 启动时，默认将“测试视频文件路径”添加到标题栏
    // 文件路径依据${CMAKE_CURRENT_SOURCE_DIR}，宏直接展开为字符串字面量
    const char* testVideoPath = TEST_VIDEOS_ABS_PATH;
    QString path = QString::fromUtf8(testVideoPath);
    QFileInfo testFile(path);
    if(testFile.exists())
        w.setWindowTitle(testFile.absoluteFilePath());

    return a.exec();
}
