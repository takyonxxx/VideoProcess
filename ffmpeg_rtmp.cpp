#include "ffmpeg_rtmp.h"

ffmpeg_rtmp::ffmpeg_rtmp(QObject *parent)
    : QThread{parent}
{
    in_filename  = "rtmp://0.0.0.0:8889/live/app";
    //    av_register_all();
    //    avcodec_register_all();
    avformat_network_init();
}

void ffmpeg_rtmp::stop()
{
    avformat_close_input(&ifmt_ctx);
    sendInfo("Stream thread stopped.");
}

void ffmpeg_rtmp::run()
{
    sendInfo("Trying to create rtmp stream server.");

    AVDictionary *format_opts = NULL;
    av_dict_set(&format_opts, "timeout", "3", 0);

    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, NULL ,&format_opts)) < 0)
    {
        char buff[256];
        av_strerror(ret, buff, 256);
        sendInfo(buff);
        sendInfo("Cannot open input file.");
        return;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        sendInfo( "Cannot find stream information.");
        return;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream;
        AVCodecContext *codec_ctx;
        stream = ifmt_ctx->streams[i];
        codec_ctx = stream->codec;
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
                || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            /* Open decoder */
            ret = avcodec_open2(codec_ctx,
                                avcodec_find_decoder(codec_ctx->codec_id), NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return;
            }
        }
    }
    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    sendInfo("Rtmp stream server created.");
}
