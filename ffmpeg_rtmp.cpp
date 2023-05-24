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

    const char* ffmpegVersion = av_version_info();
    std::cout << "FFmpeg version: " << ffmpegVersion << std::endl;

    // QBuffer buffer;
    QAudioFormat format;
    format.setSampleFormat(QAudioFormat::Int16);
    format.setSampleRate(44100);
    format.setChannelCount(2);

    audio_buffer.open(QIODevice::ReadWrite);
    audio_buffer.setData(QByteArray(), 0);

    // get default output device
    QAudioDevice audioDevice(QMediaDevices::defaultAudioOutput());
    qDebug() << audioDevice.description();

    audioSink = new QAudioSink(format, this);
    connect(audioSink, SIGNAL(stateChanged(QAudio::State)), this, SLOT(handleStateChanged(QAudio::State)));
    audioSink->start(&audio_buffer);

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
                     && !interface.humanReadableName().contains("VM") && !interface.hardwareAddress().startsWith("00:") && interface.hardwareAddress() != "")
                {
                    in_filename  = "rtmp://" + entry.ip().toString() + ":8889/live";
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
    qDebug() << in_filename;

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

void ffmpeg_rtmp::handleStateChanged(QAudio::State newState)
{
    switch (newState) {
    case QAudio::IdleState:
        // Finished playing (no more data)
        audioSink->stop();
        delete audioSink;
        break;

    case QAudio::StoppedState:
        // Stopped for other reasons
        if (audioSink->error() != QAudio::NoError) {
            // Error handling
        }
        break;

    default:
        // ... other cases as appropriate
        break;
    }
}

void ffmpeg_rtmp::run()
{
    m_stop = false;
    int frameCount = 0;
    int videoWidth = 0;
    int videoHeight = 0;

    emit sendInfo("Trying to start Rtmp stream server.");

    if (!prepare_ffmpeg())
    {
        emit sendConnectionStatus(false);
        return;
    }

    emit sendConnectionStatus(true);

    // Read packets from the input stream and write to the output file
    AVPacket* packet = av_packet_alloc();

    // Print the video codec
    if (video_idx != -1) {
        AVCodecParameters* codecVideoParams = inputContext->streams[video_idx]->codecpar;
        AVCodecID codecVideoId = codecVideoParams->codec_id;
        auto codecVideoName = avcodec_get_name(codecVideoId);
        videoWidth = codecVideoParams->width;
        videoHeight = codecVideoParams->height;

        AVCodecParameters* codecAudioParams = inputContext->streams[audio_idx]->codecpar;
        AVCodecID codecAudioId = codecAudioParams->codec_id;
        auto codecAudioName = avcodec_get_name(codecAudioId);

        video_info = "Video Codec: " + QString(codecVideoName) + " Audio Codec: " + QString(codecAudioName);
        emit sendInfo(video_info);

        video_info = "Video width: " + QString::number(videoWidth) + " Video height: " + QString::number(videoHeight);
        emit sendInfo(video_info);

        // Get pixel format name
        const char* pixelFormatName = av_get_pix_fmt_name(static_cast<AVPixelFormat>(codecVideoParams->format));
        if (!pixelFormatName) {
            video_info = "Unknown pixel format";
        } else {
            QString pixelFormat = QString::fromUtf8(pixelFormatName);
            video_info = "Video Pixel Format: " + pixelFormat;
        }
        emit sendInfo(video_info);

    } else {
        video_info = "Video stream not found ";
    }

    while (av_read_frame(inputContext, packet) == 0)
    {
        if(m_stop)
        {
            emit sendConnectionStatus(false);
            break;
        }

        if (packet->stream_index == audio_idx)
        {
            QByteArray audioData(reinterpret_cast<const char*>(packet->data), packet->size);
            audio_buffer.write(audioData);
        }

        // for preview
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

                SwsContext* swsContext = sws_getContext(frame->width, frame->height, ctx_codec->pix_fmt,
                                                        frame->width, frame->height, AV_PIX_FMT_RGB32,
                                                        SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (!swsContext) {
                    std::cout << "Failed to create SwsContext" << std::endl;
                    break;
                }

                // Initialize the SwsContext
                sws_init_context(swsContext, nullptr, nullptr);

                uint8_t* destData[1] = { nullptr };
                int destLinesize[1] = { 0 };

                QImage image(frame->width, frame->height, QImage::Format_RGB32);

                destData[0] = image.bits();
                destLinesize[0] = image.bytesPerLine();

                sws_scale(swsContext, frame->data, frame->linesize, 0, frame->height, destData, destLinesize);

                // Cleanup
                sws_freeContext(swsContext);
                emit sendFrame(image);
                frameCount++;
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

    audioSink->stop();
    delete audioSink;

    // Write the output file trailer
    av_write_trailer(outputContext);

    // Close input and output contexts
    avformat_close_input(&inputContext);
    if (outputContext && !(outputContext->oformat->flags & AVFMT_NOFILE))
        avio_close(outputContext->pb);
    avformat_free_context(outputContext);
}
