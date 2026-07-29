#ifndef MY_FFMPEG_HEADERS_H
#define MY_FFMPEG_HEADERS_H

/* my_ffmpeg_headers 头文件作用：
 * 1. 包含ffmpeg的头文件
 * 2. FFmpegPlayerCtx从MainWindow.h迁移到这里
 * 3. 声明我自己封装的ffmpeg函数
 *
 * 规定：
 * 1. #include "my_ffmpeg_headers"只会在.cpp出现，不允许.h出现
 * 2. 在.h中使用前向声明，如struct FFmpegPlayerCtx; struct AVFrame;
 */

extern "C"{
// demux_decode.c的头文件
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
// 用于yuv转rgb
#include <libswscale/swscale.h>
// 用于音频重采样
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}


#endif // MY_FFMPEG_HEADERS_H
