#include "ffmpeg_rtmp.h"
#include <QStandardPaths>

void ffmpegLogCallback(void *avcl, int level, const char *fmt, va_list vl)
{
    if (level <= av_log_get_level()) // Only log messages with a level equal to or higher than the current FFmpeg log level
    {
        char message[1024];
        vsnprintf(message, sizeof(message), fmt, vl);
        qDebug() << message;
    }
}

ffmpeg_rtmp::ffmpeg_rtmp(QObject *parent)
    : QThread{parent}
{
    // Enable FFmpeg logging
    //    av_log_set_level(AV_LOG_DEBUG);
    //    av_log_set_callback(ffmpegLogCallback);
    //tTM/2!**

    out_filename = QString("%1/output.mp4").arg(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    avformat_network_init();
}

void ffmpeg_rtmp::stop()
{
    m_stop = true;
}

void ffmpeg_rtmp::setUrl()
{
    bool found = false;
    foreach(QNetworkInterface interface, QNetworkInterface::allInterfaces())
    {
        if (interface.flags().testFlag(QNetworkInterface::IsUp) && !interface.flags().testFlag(QNetworkInterface::IsLoopBack))
            foreach (QNetworkAddressEntry entry, interface.addressEntries())
            {
                //qDebug() << entry.ip().toString() + " " + interface.hardwareAddress()  + " " + interface.humanReadableName();
                if ( !found && interface.hardwareAddress() != "00:00:00:00:00:00" && entry.ip().toString().contains(".")
                     && !interface.humanReadableName().contains("VM") && !interface.hardwareAddress().startsWith("00:"))
                {
                    in_filename  = "rtmp://" + entry.ip().toString() + ":8889/live/app";
                    emit sendUrl(in_filename);
                    found = true;
                }
            }
    }
}


int ffmpeg_rtmp::prepare_ffmpeg()
{
    // Open the RTMP stream
    AVDictionary *format_opts = NULL;
    av_dict_set(&format_opts, "timeout", "10", 0);

    if (avformat_open_input(&inputContext, in_filename.toStdString().c_str() , nullptr, &format_opts) != 0) {
        // Error handling
        qDebug() << "error avformat_open_input";
        return false;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(inputContext, nullptr) < 0) {
        // Error handling
        qDebug() << "error avformat_find_stream_info";
        return false;
    }

    // Create the output file context
    if (avformat_alloc_output_context2(&outputContext, nullptr, nullptr, out_filename.toStdString().c_str()) < 0) {
        // Error handling
        qDebug() << "error avformat_alloc_output_context2";
        return false;
    }

    // Iterate through input streams and copy all streams to output
    for (unsigned int i = 0; i < inputContext->nb_streams; ++i) {
        inputStream = inputContext->streams[i];
        outputStream = avformat_new_stream(outputContext, nullptr);
        if (inputContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            vid_stream = inputContext->streams[i];
            video_idx = i;
            qDebug() << "video_idx : " << video_idx;
        }
        else if (inputContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            aud_stream = inputContext->streams[i];;
            audio_idx = i;
            qDebug() << "audio_idx : " << audio_idx;
        }
        if (!outputStream) {
            // Error handling
            return false;
        }
        avcodec_parameters_copy(outputStream->codecpar, inputStream->codecpar);
    }

    // Open the output file for writing
    if (avio_open(&outputContext->pb, out_filename.toStdString().c_str(), AVIO_FLAG_WRITE) < 0) {
        // Error handling
        qDebug() << "error avio_open";
        return false ;
    }

    // Write the output file header
    if (avformat_write_header(outputContext, nullptr) < 0) {
        // Error handling
        qDebug() << "error avformat_write_header";
        return false;
    }

    auto codec = avcodec_find_decoder(vid_stream->codecpar->codec_id);
    if (!codec) {
        qDebug() << "error avcodec_find_decoder";
        return false;
    }
    ctx_codec = avcodec_alloc_context3(codec);

    if(avcodec_parameters_to_context(ctx_codec, vid_stream->codecpar)<0)
        std::cout << 512;

    if (avcodec_open2(ctx_codec, codec, nullptr)<0) {
        std::cout << 5;
        return false;
    }

    frame = av_frame_alloc();
    if (!frame) {
        qDebug() << "error av_frame_alloc";
        avcodec_free_context(&ctx_codec);
        return false;
    }

    return true;
}

void ffmpeg_rtmp::run()
{
    m_stop = false;

    sendInfo("Trying to start Rtmp stream server.");

    if (!prepare_ffmpeg())
    {
        sendConnectionStatus(false);
        return;
    }

    sendConnectionStatus(true);

    // Read packets from the input stream and write to the output file
    AVPacket* packet = av_packet_alloc();
    //    AVStream* videoStream = outputContext->streams[video_idx]; // Video stream
    //    AVStream* audioStream = outputContext->streams[audio_idx]; // Audio stream

    //    int64_t videoPts = 0; // Current video presentation timestamp
    //    int64_t audioPts = 0; // Current audio presentation timestamp

    while (av_read_frame(inputContext, packet) == 0)
    {
        if(m_stop)
        {
            sendConnectionStatus(false);
            break;
        }

        // for preview get only video
        if (packet->stream_index == video_idx)
        {
            int ret = avcodec_send_packet(ctx_codec, packet);
            if (ret < 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                std::cout << "avcodec_send_packet: " << ret << std::endl;
                break;
            }
            while (ret  >= 0) {
                ret = avcodec_receive_frame(ctx_codec, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    //std::cout << "avcodec_receive_frame: " << ret << std::endl;
                    continue;
                }
                emit sendFrame(*frame);
            }
        }

        AVStream* inputStream = inputContext->streams[packet->stream_index];
        AVStream* outputStream = outputContext->streams[packet->stream_index];

        if (packet->stream_index >= 0 && packet->stream_index < inputContext->nb_streams)
        {
            // Rescale packet timestamps
            packet->pts = av_rescale_q(packet->pts, inputStream->time_base, outputStream->time_base);
            packet->dts = av_rescale_q(packet->dts, inputStream->time_base, outputStream->time_base);
            packet->duration = av_rescale_q(packet->duration, inputStream->time_base, outputStream->time_base);
            packet->pos = -1;
            av_interleaved_write_frame(outputContext, packet);
        }

        av_packet_unref(packet);
    }

    // Write the output file trailer
    av_write_trailer(outputContext);

    // Close input and output contexts
    avformat_close_input(&inputContext);
    if (outputContext && !(outputContext->oformat->flags & AVFMT_NOFILE))
        avio_close(outputContext->pb);
    avformat_free_context(outputContext);
}
