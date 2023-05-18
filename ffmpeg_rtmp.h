#ifndef FFMPEG_RTMP_H
#define FFMPEG_RTMP_H

#include <QDebug>
#include <QThread>

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
#ifdef __cplusplus
};
#endif
#endif

class AVFrameWrapper : public QObject
{
    Q_OBJECT

public:
    AVFrameWrapper(AVFrame frame) : QObject(), framePtr(frame) {}

    AVFrame getFrame() const { return framePtr; }

private:
    AVFrame framePtr;
};


class ffmpeg_rtmp : public QThread
{
    Q_OBJECT
public:
    explicit ffmpeg_rtmp(QObject *parent = nullptr);
    void stop();
private:
    int prepare_ffmpeg();

    bool m_stop {false};

    //Input AVFormatContext and Output AVFormatContext
    AVFormatContext* inputContext{nullptr};
    AVFormatContext* outputContext{nullptr};
    AVCodecContext *ctx_codec{nullptr};
    AVFrame *frame{nullptr};
    AVFrame *frameRGB{nullptr};
    AVStream *inputStream{nullptr};
    AVStream *outputStream{nullptr};
    AVStream *vid_stream{nullptr};
    int video_idx;

    const char *in_filename, *out_filename;
protected:
    void run();

signals:
    void sendInfo(QString);
    void sendConnectionStatus(bool);
    void sendFrame(AVFrame);

};

#endif // FFMPEG_RTMP_H
