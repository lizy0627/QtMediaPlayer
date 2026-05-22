lessThan(QT_MAJOR_VERSION, 6) {
    error("QtMediaPlayer requires Qt 6.x. Please run qmake from a Qt 6 kit.")
}

greaterThan(QT_MAJOR_VERSION, 6) {
    error("QtMediaPlayer is currently Qt 6.x-only. Please run qmake from a Qt 6 kit.")
}

QT += core gui widgets multimedia multimediawidgets network sql concurrent

CONFIG += c++17

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += \
    $$PWD \
    $$PWD/danmaku \
    $$PWD/network

BUILD_ROOT = $$OUT_PWD
CONFIG(debug, debug|release) {
    BUILD_CONFIG = debug
} else {
    BUILD_CONFIG = release
}

DESTDIR = $$BUILD_ROOT/bin/$$BUILD_CONFIG
OBJECTS_DIR = $$BUILD_ROOT/obj/$$BUILD_CONFIG
MOC_DIR = $$BUILD_ROOT/moc/$$BUILD_CONFIG
RCC_DIR = $$BUILD_ROOT/rcc/$$BUILD_CONFIG
UI_DIR = $$BUILD_ROOT/ui

SOURCES += \
    aichatcontroller.cpp \
    aichatview.cpp \
    aichatwidget.cpp \
    aichatpanel.cpp \
    appbootstrapper.cpp \
    network/aichatservice.cpp \
    network/bilibiliplaybackresolver.cpp \
    network/bilibilisearchservice.cpp \
    authdialogcontroller.cpp \
    authservice.cpp \
    audiodialogservice.cpp \
    audiocontrolbar.cpp \
    audioplaybackcontroller.cpp \
    audioplayercontroller.cpp \
    audioplayer.cpp \
    audioplayerwidget.cpp \
    audiostyle.cpp \
    captureservice.cpp \
    framecaptureservice.cpp \
    databaseconfigloader.cpp \
    localplaybackdiagnostics.cpp \
    videocapture.cpp \
    videocapturecoordinator.cpp \
    databasecontext.cpp \
    videodanmakucoordinator.cpp \
    videoencoder.cpp \
    danmaku/danmakucontroller.cpp \
    danmaku/danmakuinputbar.cpp \
    danmaku/danmakuitem.cpp \
    danmaku/danmakuoverlay.cpp \
    danmaku/danmakupanel.cpp \
    danmaku/danmakurepository.cpp \
    dbmanager.cpp \
    logindialog.cpp \
    lyricparser.cpp \
    lyricwidget.cpp \
    lyricpanel.cpp \
    mediahistory.cpp \
    mediafileprobe.cpp \
    mainwindowcontroller.cpp \
    migrationrunner.cpp \
    network/lyricdownloadservice.cpp \
    lyricservice.cpp \
    main.cpp \
    mediaprobeservice.cpp \
    mediaplaybackrouter.cpp \
    menu.cpp \
    network/networkclient.cpp \
    onlinevideocoordinator.cpp \
    onlinevideosearch.cpp \
    mydanmakudialog.cpp \
    onlinemusicsearch.cpp \
    network/onlinemusicservice.cpp \
    network/onlinevideoservice.cpp \
    playlistmodel.cpp \
    playlistpanel.cpp \
    searchcache.cpp \
    spectrumpanel.cpp \
    usersession.cpp \
    userrepository.cpp \
    uitheme.cpp \
    unifiedhistorydialog.cpp \
    videocontrolbar.cpp \
    videohistorycoordinator.cpp \
    videoplaybackcontroller.cpp \
    videoplayercontroller.cpp \
    videoplayer.cpp \
    widget.cpp

HEADERS += \
    aichatcontroller.h \
    aichatview.h \
    aichatwidget.h \
    aichatpanel.h \
    appbootstrapper.h \
    appstartupstate.h \
    network/aichatservice.h \
    network/bilibiliplaybackresolver.h \
    network/bilibilisearchservice.h \
    authdialogcontroller.h \
    authservice.h \
    audiodialogservice.h \
    audiocontrolbar.h \
    audioplaybackcontroller.h \
    audioplayercontroller.h \
    audioplayer.h \
    audioplayerwidget.h \
    audiostyle.h \
    audiotrack.h \
    captureservice.h \
    framecaptureservice.h \
    databaseconfigloader.h \
    videocapturecoordinator.h \
    databasecontext.h \
    videodanmakucoordinator.h \
    danmaku/danmakucontroller.h \
    danmaku/danmakuinputbar.h \
    danmaku/danmakuitem.h \
    danmaku/danmakuoverlay.h \
    danmaku/danmakupanel.h \
    danmaku/danmakurepository.h \
    dbmanager.h \
    logindialog.h \
    lyricdownloader.h \
    localplaybackdiagnostics.h \
    network/lyricdownloadservice.h \
    lyricpanel.h \
    lyricparser.h \
    lyricservice.h \
    lyricwidget.h \
    mediaprobeservice.h \
    mediafileprobe.h \
    mediaplaybackrouter.h \
    mediahistory.h \
    mainwindowcontroller.h \
    menu.h \
    migrationrunner.h \
    network/networkclient.h \
    onlinevideocoordinator.h \
    mydanmakudialog.h \
    onlinemusicsearch.h \
    network/onlinemusicservice.h \
    network/onlinevideotypes.h \
    network/onlinevideoservice.h \
    onlinevideosearch.h \
    playlistmodel.h \
    playlistpanel.h \
    searchcache.h \
    spectrumpanel.h \
    spectrumwidget.h \
    usersession.h \
    userrepository.h \
    uitheme.h \
    unifiedhistorydialog.h \
    videocontrolbar.h \
    videoplayer.h \
    videohistorycoordinator.h \
    videoplaybackcontroller.h \
    videoplayercontroller.h \
    videocapture.h \
    videoencoder.h \
    widget.h

FORMS += \
    widget.ui

RESOURCES += \
    resources/resources.qrc

win32 {
    LIBS += -ldwmapi
}

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
