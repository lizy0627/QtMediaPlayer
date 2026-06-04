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

contains(CONFIG, use_ffmpeg) {
    isEmpty(FFMPEG_ROOT) {
        FFMPEG_ROOT = $$(FFMPEG_ROOT)
    }

    isEmpty(FFMPEG_ROOT) {
        exists($$PWD/third_party/ffmpeg/include/libavformat/avformat.h) {
            FFMPEG_ROOT = $$PWD/third_party/ffmpeg
        } else:exists($$PWD/ffmpeg/include/libavformat/avformat.h) {
            FFMPEG_ROOT = $$PWD/ffmpeg
        }
    }

    isEmpty(FFMPEG_ROOT) {
        error("FFmpeg support requested. Set FFMPEG_ROOT or place the SDK under third_party/ffmpeg.")
    }

    isEmpty(FFMPEG_INCLUDE_DIR) {
        FFMPEG_INCLUDE_DIR = $$FFMPEG_ROOT/include
    }
    isEmpty(FFMPEG_LIB_DIR) {
        FFMPEG_LIB_DIR = $$FFMPEG_ROOT/lib
    }
    isEmpty(FFMPEG_BIN_DIR) {
        FFMPEG_BIN_DIR = $$FFMPEG_ROOT/bin
    }

    !exists($$FFMPEG_INCLUDE_DIR/libavformat/avformat.h) {
        error("FFmpeg headers not found: $$FFMPEG_INCLUDE_DIR")
    }
    !exists($$FFMPEG_LIB_DIR) {
        error("FFmpeg library directory not found: $$FFMPEG_LIB_DIR")
    }

    DEFINES += USE_FFMPEG
    INCLUDEPATH += \
        $$PWD/ffmpeg \
        $$FFMPEG_INCLUDE_DIR

    win32-msvc* {
        FFMPEG_IMPORT_LIBS = \
            avformat.lib \
            avcodec.lib \
            avutil.lib \
            swscale.lib \
            swresample.lib

        !exists($$FFMPEG_LIB_DIR/avformat.lib):exists($$FFMPEG_LIB_DIR/libavformat.lib) {
            FFMPEG_IMPORT_LIBS = \
                libavformat.lib \
                libavcodec.lib \
                libavutil.lib \
                libswscale.lib \
                libswresample.lib
        }

        for(ffmpegLib, FFMPEG_IMPORT_LIBS) {
            LIBS += $$quote($$shell_path($$FFMPEG_LIB_DIR/$$ffmpegLib))
        }
    } else:win32-g++ {
        FFMPEG_IMPORT_LIBS = \
            libavformat.dll.a \
            libavcodec.dll.a \
            libavutil.dll.a \
            libswscale.dll.a \
            libswresample.dll.a

        for(ffmpegLib, FFMPEG_IMPORT_LIBS) {
            LIBS += $$quote($$FFMPEG_LIB_DIR/$$ffmpegLib)
        }
    } else {
        LIBS += -L$$shell_path($$FFMPEG_LIB_DIR) \
            -lavformat \
            -lavcodec \
            -lavutil \
            -lswscale \
            -lswresample
    }

    win32 {
        exists($$FFMPEG_BIN_DIR) {
            message("FFmpeg runtime DLL directory: $$FFMPEG_BIN_DIR")
        } else {
            warning("FFmpeg bin directory not found. Runtime DLLs must be copied beside the executable or added to PATH.")
        }
    }

    SOURCES += \
        ffmpeg/ffmpegplaybackbackend.cpp \
        ffmpeg/ffmpegprobe.cpp
    HEADERS += \
        ffmpeg/ffmpegmediainfo.h \
        ffmpeg/ffmpegplaybackbackend.h \
        ffmpeg/ffmpegprobe.h
}

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

contains(CONFIG, use_ffmpeg):win32:exists($$FFMPEG_BIN_DIR/*.dll) {
    QMAKE_POST_LINK += $$quote($$QMAKE_COPY $$shell_path($$FFMPEG_BIN_DIR/*.dll) $$shell_path($$DESTDIR)) $$escape_expand(\\n\\t)
}

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
    ffmpeg/ffmpegframeextractor.cpp \
    ffmpeg/ffmpegvideowidget.cpp \
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
    mediainfodialog.cpp \
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
    playback/qtmediaplaybackbackend.cpp \
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
    videoqueuedialog.cpp \
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
    ffmpeg/ffmpegframeextractor.h \
    ffmpeg/ffmpegvideowidget.h \
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
    mediainfodialog.h \
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
    playback/iplaybackbackend.h \
    playback/qtmediaplaybackbackend.h \
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
    videoqueuedialog.h \
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
