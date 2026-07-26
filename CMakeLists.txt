cmake_minimum_required(VERSION 3.19)
project(demuxing_decoding_5 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(demuxing_decoding_5
    WIN32 MACOSX_BUNDLE
    main.cpp
    MainWindow.cpp
    MainWindow.h
    MainWindow.ui
)

# MinGW/gcc：把当前源码目录（MyGLWidget.h 所在目录）加入头文件搜索路径
target_include_directories(demuxing_decoding_5 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(demuxing_decoding_5
    PRIVATE
        Qt::Core
        Qt::Widgets
)

include(GNUInstallDirs)

install(TARGETS demuxing_decoding_5
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET demuxing_decoding_5
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

set(MY_TARGET_NAME demuxing_decoding_5)
message(STATUS ">>> Linking FFmpeg dev lib")
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    message(STATUS ">>> Building on Windows host")
    if(MSVC OR MINGW)
        message(STATUS ">>> The compiler is MSVC OR MINGW")
        # 1. 指定 FFmpeg 根目录
        set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/ffmpeg-n8.0.1-34-gbfa334de42-win64-gpl-shared-8.0")
        message(STATUS ">>> FFMPEG_DIR = ${FFMPEG_DIR}")
        # 2. 添加 FFmpeg 头文件路径
        target_include_directories(${MY_TARGET_NAME}
            PRIVATE
                ${FFMPEG_DIR}/include
        )
        # 3. 添加 FFmpeg 库文件路径
        target_link_directories(${MY_TARGET_NAME}
            PRIVATE
                ${FFMPEG_DIR}/lib
        )
        # 5. 链接 FFmpeg 库（按需添加）
        target_link_libraries(${MY_TARGET_NAME}
            PRIVATE
                avformat
                avcodec
                avutil
                avfilter
                swscale
                swresample
                ws2_32  # Windows 系统库
        )
        # # 6. 自动复制 DLL 到输出目录（可选）
        # # 添加到“高级系统设置”可注释掉：D:\Codes\cppCode\FFmpegCode\ffmpeg-master-latest-win64-gpl-shared\bin
        # # 但是要注意只能有一个ffmpeg路径，否则cmd输入ffmpeg就会显示拒绝访问
        # add_custom_command(TARGET ${MY_TARGET_NAME} POST_BUILD
        #         COMMAND ${CMAKE_COMMAND} -E copy_directory
        #         "${FFMPEG_DIR}/bin/"
        #         $<TARGET_FILE_DIR:${MY_TARGET_NAME}>
        # )
    # elseif(MINGW)
    #     message(STATUS ">>> The compiler is GNU ABI on Windows (MinGW)")
    endif()
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    message(STATUS ">>> Building on Linux host")
    # 1. 指定 FFmpeg 根目录
    set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/ffmpeg-n8.0.1-34-gbfa334de42-linux64-gpl-shared-8.0")
    message(STATUS ">>> FFMPEG_DIR = ${FFMPEG_DIR}")
    # 2. 添加 FFmpeg 头文件路径
    target_include_directories(${MY_TARGET_NAME}
        PRIVATE
            ${FFMPEG_DIR}/include
    )
    # 3. 添加 FFmpeg 库文件路径
    target_link_directories(${MY_TARGET_NAME}
        PRIVATE
            ${FFMPEG_DIR}/lib
    )
    # 5. 链接 FFmpeg 库（按需添加）
    target_link_libraries(${MY_TARGET_NAME}
        PRIVATE
            avformat
            avcodec
            avutil
            avfilter
            swscale
            swresample
    )
endif()

set(MY_TARGET_NAME demuxing_decoding_5)
message(STATUS ">>> Linking SDL3 dev lib >>> https://wiki.libsdl.org/SDL3/README-cmake")
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    message(STATUS ">>> Building on Windows host")
    if(MSVC OR MINGW)
        message(STATUS ">>> The compiler is MSVC OR MINGW")
        set(SDL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SDL3-devel-3.4.2-VC/SDL3-3.4.2")
        message(STATUS ">>> SDL_DIR = ${SDL_DIR}")
        target_include_directories(${MY_TARGET_NAME}
            PRIVATE
                ${SDL_DIR}/include
        )
        target_link_directories(${MY_TARGET_NAME}
            PRIVATE
                ${SDL_DIR}/lib/x64
        )
        # 4. 添加编译定义，避免 SDL 重定义 main，告诉 SDL：不要替我处理 main 入口，我自己管理程序入口。
        target_compile_definitions(${MY_TARGET_NAME}
            PRIVATE
                SDL_MAIN_HANDLED
        )
        target_link_libraries(${MY_TARGET_NAME}
            PRIVATE
                SDL3 # 只链接 SDL3，不链接 SDL3main
        )
        # 6. 自动复制 DLL 到输出目录（可选）
        # 添加到“高级系统设置”可注释掉：XXX\third_party\SDL3-devel-3.4.2-VC\SDL3-3.4.2\lib\x64
        add_custom_command(TARGET ${MY_TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${SDL_DIR}/lib/x64"
                $<TARGET_FILE_DIR:${MY_TARGET_NAME}>
        )
    # elseif(MINGW)
    #     message(STATUS ">>> The compiler is GNU ABI on Windows (MinGW)")
    endif()
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    message(STATUS ">>> Building on Linux host")

endif()
