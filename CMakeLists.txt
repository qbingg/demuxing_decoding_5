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

if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    message(STATUS ">>> Building on Windows host")
    if(MSVC)
        message(STATUS ">>> The compiler is MSVC")
    elseif(MINGW)
        message(STATUS ">>> The compiler is GNU ABI on Windows (MinGW)")
    endif()
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    message(STATUS ">>> Building on Linux host")
endif()
