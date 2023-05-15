#include "ffmpeg_rtmp.h"

ffmpeg_rtmp::ffmpeg_rtmp(QObject *parent)
    : QThread{parent}
{
    in_filename  = "rtmp://192.168.1.6:8889/live/app";
    av_register_all();
    avcodec_register_all();
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
    av_dict_set(&format_opts, "timeout", "10", 0);

    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, NULL ,&format_opts)) < 0)
    {
        char buff[256];
        av_strerror(ret, buff, 256);
        sendInfo(buff);
        sendInfo("Could not create rtmp stream server.");
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        sendInfo( "Failed to retrieve input stream information");
    }

    sendInfo("Rtmp stream server created.");
}
