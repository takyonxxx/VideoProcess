#ifndef FFMPEG_RTMP_H
#define FFMPEG_RTMP_H

#include <QDebug>
#include <QThread>

#include <stdio.h>

#define __STDC_CONSTANT_MACROS
#define AV_CODEC_FLAG_GLOBAL_HEADER (1 << 22)
#define CODEC_FLAG_GLOBAL_HEADER AV_CODEC_FLAG_GLOBAL_HEADER
#define AVFMT_RAWPICTURE 0x0020
#include <stdio.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include <libavcodec/avcodec.h>
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
#ifdef __cplusplus
};
#endif
#endif

//'1': Use H.264 Bitstream Filter
#define USE_H264BSF 0

class ffmpeg_rtmp : public QThread
{
    Q_OBJECT
public:
    explicit ffmpeg_rtmp(QObject *parent = nullptr);
    void stop();
private:
    AVOutputFormat *ofmt = NULL;
    //Input AVFormatContext and Output AVFormatContext
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    const char *in_filename, *out_filename;
    int ret, i;
    int videoindex=-1;
    int frame_index=0;

protected:
    void run();

signals:
    void sendInfo(QString);

};

#endif // FFMPEG_RTMP_H
