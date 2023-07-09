TEMPLATE = app
TARGET = video_process_ai

QT += multimedia multimediawidgets widgets network

HEADERS = \
    Plotter.h \
    camera.h \
    ffmpeg_rtmp.h \
    imagesettings.h \
    videosettings.h \
    metadatadialog.h

SOURCES = \
    Plotter.cpp \
    main.cpp \
    ffmpeg_rtmp.cpp \
    camera.cpp \
    imagesettings.cpp \
    videosettings.cpp \
    metadatadialog.cpp

FORMS += \
    imagesettings.ui

android|ios {
    FORMS += \
        camera_mobile.ui \
        videosettings_mobile.ui
} else {
    FORMS += \
        camera.ui \
        videosettings.ui
}

win32 {
  message("Win32 enabled")
  DEFINES += WIN32_LEAN_AND_MEAN
  RC_ICONS += $$PWD\images\app.ico
  INCLUDEPATH += $$PWD\lib\ffmpeg
  INCLUDEPATH += $$PWD\lib\fftw
  LIBS += -L$$PWD\lib\libav -llibavformat -llibavcodec -llibavutil -llibavfilter -llibswscale -lswresample
  LIBS += -L$$PWD\lib\fftw -llibfftw3-3 -llibfftw3f-3 -llibfftw3l-3
}

unix:!macx {
    message("linux enabled")
    INCLUDEPATH += /usr/include/x86_64-linux-gnu/libavcodec
    INCLUDEPATH += /usr/include/x86_64-linux-gnu/libavformat
    INCLUDEPATH += /usr/include/x86_64-linux-gnu/libavfilter
    LIBS += -L/usr/include/x86_64-linux-gnu/ -lavformat -lavcodec -lavutil -lavfilter -lswscale -lswresample
}

unix:macx {
    message("macx enabled")
    INCLUDEPATH += /opt/homebrew/Cellar/ffmpeg/6.0/include
    INCLUDEPATH += /opt/homebrew/Cellar/fftw/3.3.10_1/include
    LIBS += -L/opt/homebrew/Cellar/ffmpeg/6.0/lib -lavformat -lavcodec -lavutil -lavfilter -lswscale -lswresample
    LIBS += -L/opt/homebrew/Cellar/fftw/3.3.10_1/lib -lfftw3
}

RESOURCES += camera.qrc

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

include(./shared/shared.pri)

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
OTHER_FILES += android/AndroidManifest.xml

# ffmpeg -re -i test.flv  -vcodec libx264 -preset fast -crf 30 -acodec aac -ab 128k -ar 44100 -strict experimental -f flv rtmp://192.168.1.9:8889/live
# ffmpeg -re -i test.flv  -vcodec libx264 -preset fast -crf 30 -acodec aac -ab 128k -ar 44100 -strict experimental -f flv rtmp://172.26.241.24:8889/live
