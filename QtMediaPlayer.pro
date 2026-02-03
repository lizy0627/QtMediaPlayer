QT       += core gui multimedia multimediawidgets network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# 编译器警告
DEFINES += QT_DEPRECATED_WARNINGS

# 包含路径
INCLUDEPATH += $$PWD

# 源文件
SOURCES += \
    main.cpp \
    widget.cpp

# 头文件
HEADERS += \
    audioplayer.h \
    lyricdownloader.h \
    lyricparser.h \
    lyricwidget.h \
    menu.h \
    onlinemusicsearch.h \
    playhistory.h \
    spectrumwidget.h \
    videoplayer.h \
    widget.h

# UI 文件
FORMS += \
    widget.ui

# Windows 特定配置
win32 {
    LIBS += -ldwmapi
}

# 默认部署规则
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# 资源文件
RESOURCES +=
