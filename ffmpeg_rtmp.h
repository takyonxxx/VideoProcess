#ifndef FFMPEG_RTMP_H
#define FFMPEG_RTMP_H

#include <QDebug>
#include <QThread>
#include <QNetworkInterface>
#include <QImage>
#include <QMediaDevices>
#include <QAudioSink>
#include <QScopedPointer>

#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>

#ifdef _WIN32
//Windows
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavformat/avio.h>
#include <libavutil/log.h>
#include <libavformat/version.h>
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavformat/avio.h>
#include <libavutil/log.h>
#include <libavformat/version.h>
#ifdef __cplusplus
};
#endif
#endif


class ffmpeg_rtmp : public QThread
{
    Q_OBJECT
public:
    explicit ffmpeg_rtmp(QObject *parent = nullptr);
    void stop();
    void setUrl();
private:
    int prepare_ffmpeg();

    bool m_stop {false};

    //Input AVFormatContext and Output AVFormatContext
    AVFormatContext* inputContext{nullptr};
    AVFormatContext* outputContext{nullptr};
    AVCodecContext *videoCodecContext{nullptr};
    AVCodecContext *audioCodecContext{nullptr};
    AVFrame *video_frame{nullptr};
    AVFrame *audio_frame{nullptr};
    AVFrame *frameRGB{nullptr};
    AVStream *inputStream{nullptr};
    AVStream *outputStream{nullptr};
    AVStream *vid_stream{nullptr};
    AVStream *aud_stream{nullptr};
    int video_idx = -1;
    int audio_idx = -1;
    QString in_filename, out_filename;
    QString video_info;

    QMediaDevices *m_devices{nullptr};
    QIODevice *m_ioAudioDevice{nullptr};
    QScopedPointer<QAudioSink> m_audioOutput;

protected:
    void run();

signals:
    void sendInfo(QString);
    void sendUrl(QString);
    void sendConnectionStatus(bool);
    void sendFrame(QImage);

};

#endif // FFMPEG_RTMP_H
