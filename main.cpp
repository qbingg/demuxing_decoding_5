#include "MainWindow.h"

#include <QApplication>
#include "log.h"

/* 目标：跨平台播放器Demo
 * 1. 写出Win/Linux通用的CMakeLists.txt
 * 2. 通用的third_party第三方库引入文件夹
 * 3. 通用的third_party测试视频，便于调试
 * 4. 在Win/Linux均可以运行
 * 5. 学习跨平台宏
 * 6. 基于demuxing_decoding_4的研究成果，写一个更好的空白播放器模板
 * 7. 统一ffmpeg头文件包含
 * 8. qDebug分类，低频通用打印和高频单独打印
 * 9. 函数出口风格规定：单一出口风格，多出口风格，一个函数内部只能选用一种范式，不允许混搭。
 *      （简单地以用到 ret 为依据划分，涉及循环内判断，涉及FFmpeg/SDL等C库需要批量统一释放资源等情况，使用单一出口风格。）
 * 10. SDL3音频使用callback模式，未来项目在考虑pull模式（我需要循序渐进而不是一步到位）
 * 11. 无限循环业务顺序（如果项存在）：quit(stop)、pause、业务
 * 12. 函数return和if风格统一，规定：使用int init(),if (init() < 0){非负即成功}，不看结果的用void
 *      （ffmpeg-simple-player和SDL2使用的是int init(),if (init() != 0){非零即失败}）
 *      （Qt和SDL3使用的是bool init(),f (!init()){true即成功}）
 *      （我不想在风格上纠结太久，由于是播放器项目，那就风格参考ffplay.c吧，当然未来可考虑现代C++方案）
 * 【弃用】13. QChart每次重播都new新的chart，与FFmpegPlayerCtx心智模型一致：init、clean
 *      （业界更倾向于复用为m_chart，未来可考虑只清空lineseries数据方案：init、reset，fini）
 * 14. QChart复用为m_chart，使用每次重播只清空lineseries数据方案：init、reset，fini
 *      （Qt手册56：难以简化为init、clean，难以获取到上一次chart指针）
 */

int main(int argc, char *argv[])
{
#if defined(__linux__)
    //Ubuntu26.04,Qt6.5.3缺失窗口阴影和圆角，切换回X11/XWayland
    //https://www.mail-archive.com/ubuntu-bugs%40lists.ubuntu.com/msg6288618.html
    qputenv("QT_QPA_PLATFORM", "xcb");
#endif

    QLoggingCategory::setFilterRules(
        /// "*.debug=false\n"       /* 保留，注意：只能false关掉qDebug()，true会打印一堆Qt隐藏debug信息 */
        "log1.debug=true\n"         /* 低频通用打印 */
        "demux.debug=false\n"        /* 高频单独打印 */
        "adec.debug=false\n"         /* 高频单独打印 */
        "vdec.debug=false\n"         /* 高频单独打印 */
        "chart.debug=false\n"         /* 高频单独打印 */
        "dsp1.debug=false\n"         /* 高频单独打印 */
        "dsp2.debug=true\n"         /* 高频单独打印 */
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
