TEMPLATE = app
TARGET = video_process_ai

QT += multimedia multimediawidgets

HEADERS = \
    camera.h \
    ffmpeg_rtmp.h \
    imagesettings.h \
    videosettings.h \
    metadatadialog.h

SOURCES = \
    ffmpeg_rtmp.cpp \
    main.cpp \
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

win32
{
  message("Win32 enabled")
  DEFINES += WIN32_LEAN_AND_MEAN
  RC_ICONS += $$PWD\images\app.ico
  INCLUDEPATH += $$PWD\lib\ffmpeg
  LIBS += -L$$PWD\lib\libav -llibavformat -llibavcodec -llibavutil -llibavfilter -llibswscale -lswresample
}

RESOURCES += camera.qrc

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

QT += widgets
include(./shared/shared.pri)

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
OTHER_FILES += android/AndroidManifest.xml
