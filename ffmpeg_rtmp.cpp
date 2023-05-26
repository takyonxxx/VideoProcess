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

void printAudioFrameInfo(const AVCodecContext* codecContext, const AVFrame* frame)
{
    // See the following to know what data type (unsigned char, short, float, etc) to use to access the audio data:
    // http://ffmpeg.org/doxygen/trunk/samplefmt_8h.html#af9a51ca15301871723577c730b5865c5
    std::cout << "Audio frame info:\n"
              << "  Sample count: " << frame->nb_samples << '\n'
              << "  Channel count: " << codecContext->ch_layout.nb_channels << '\n'
              << "  Format: " << av_get_sample_fmt_name(codecContext->sample_fmt) << '\n'
              << "  Bytes per sample: " << av_get_bytes_per_sample(codecContext->sample_fmt) << '\n'
              << "  Is planar? " << av_sample_fmt_is_planar(codecContext->sample_fmt) << '\n';
}


ffmpeg_rtmp::ffmpeg_rtmp(QObject *parent)
    : QThread{parent}
{
    // Enable FFmpeg logging
    av_log_set_level(AV_LOG_ERROR);
    //    av_log_set_callback(ffmpegLogCallback);
    //tTM/2!**

    const char* ffmpegVersion = av_version_info();
    std::cout << "FFmpeg version: " << ffmpegVersion << std::endl;

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
                    qDebug() << in_filename;
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

    auto video_codec = avcodec_find_decoder(vid_stream->codecpar->codec_id);
    if (!video_codec) {
        qDebug() << "error video avcodec_find_decoder";
        return false;
    }
    videoCodecContext = avcodec_alloc_context3(video_codec);

    if(avcodec_parameters_to_context(videoCodecContext, vid_stream->codecpar)<0)
        std::cout << 512;

    if (avcodec_open2(videoCodecContext, video_codec, nullptr)<0) {
        std::cout << 5;
        return false;
    }

    auto audio_codec = avcodec_find_decoder(aud_stream->codecpar->codec_id);
    if (!audio_codec) {
        qDebug() << "error audio avcodec_find_decoder";
        return false;
    }
    audioCodecContext = avcodec_alloc_context3(audio_codec);
    /* put sample parameters */

    if(avcodec_parameters_to_context(audioCodecContext, aud_stream->codecpar)<0)
        std::cout << 512;

    if (avcodec_open2(audioCodecContext, audio_codec, nullptr)<0) {
        std::cout << 5;
        return false;
    }

    video_frame = av_frame_alloc();
    if (!video_frame) {
        qDebug() << "error video av_frame_alloc";
        avcodec_free_context(&videoCodecContext);
        return false;
    }

    audio_frame = av_frame_alloc();
    if (!audio_frame) {
        qDebug() << "error audio av_frame_alloc";
        avcodec_free_context(&audioCodecContext);
        return false;
    }

    return true;
}

int ffmpeg_rtmp::start_audio_device()
{
    QAudioFormat format;
    format.setChannelCount(audioCodecContext->ch_layout.nb_channels);
    format.setSampleRate(audioCodecContext->sample_rate);
    format.setSampleFormat(QAudioFormat::Int16);
    format.setChannelConfig(QAudioFormat::ChannelConfigStereo);

    qDebug() << format.sampleRate() << format.channelCount() << format.sampleFormat();

    QAudioDevice deviceInfo(QMediaDevices::defaultAudioOutput());
    if (!deviceInfo.isFormatSupported(format)) {
        qWarning() << "Raw audio format not supported by backend, cannot play audio.";
        return false;
    }

    m_audioOutput.reset(new QAudioSink(deviceInfo, format));

    m_ioAudioDevice = m_audioOutput->start();

    qreal initialVolume = QAudio::convertVolume(m_audioOutput->volume(),
                                                QAudio::LinearVolumeScale,
                                                QAudio::LogarithmicVolumeScale);


    info = "Audio Device: " + deviceInfo.description() + " Volume: " + QString::number(initialVolume) + " Ch: " + QString::number(format.channelCount());
    qDebug() << info;
    emit sendInfo(info);

    return true;
}

int ffmpeg_rtmp::set_parameters()
{
    int videoWidth = 0;
    int videoHeight = 0;

    AVCodecParameters* codecVideoParams = inputContext->streams[video_idx]->codecpar;
    if (!codecVideoParams)
        return false;

    AVCodecID codecVideoId = codecVideoParams->codec_id;
    auto codecVideoName = avcodec_get_name(codecVideoId);
    videoWidth = codecVideoParams->width;
    videoHeight = codecVideoParams->height;

    AVCodecParameters* codecAudioParams = inputContext->streams[audio_idx]->codecpar;
    if (!codecAudioParams)
        return false;

    AVCodecID codecAudioId = codecAudioParams->codec_id;
    auto codecAudioName = avcodec_get_name(codecAudioId);

    info = "Video Codec: " + QString(codecVideoName);
    emit sendInfo(info);

    info = "Video width: " + QString::number(videoWidth) + " Video height: " + QString::number(videoHeight);
    emit sendInfo(info);

    // Get pixel format name
    const char* pixelFormatName = av_get_pix_fmt_name(static_cast<AVPixelFormat>(codecVideoParams->format));
    if (!pixelFormatName) {
        info = "Unknown pixel format";
    } else {
        QString pixelFormat = QString::fromUtf8(pixelFormatName);
        info = "Video Pixel Format: " + pixelFormat;
    }
    emit sendInfo(info);

    info = "Audio Codec: " + QString(codecAudioName) + " sr: " + QString::number(codecAudioParams->sample_rate);
    emit sendInfo(info);
    info = "Audio Format: " + QString(av_get_sample_fmt_name(audioCodecContext->sample_fmt)) + " Channels: " + QString::number(codecAudioParams->ch_layout.nb_channels);
    emit sendInfo(info);

    m_ioAudioDevice = m_audioOutput->start();

    return true;
}

void ffmpeg_rtmp::start_streamer()
{
    if (!prepare_ffmpeg())
    {
        emit sendConnectionStatus(false);
        return;
    }

    if (!start_audio_device())
    {
        emit sendConnectionStatus(false);
        return;
    }

    // Print the video codec
    if (video_idx != -1 && audio_idx != -1) {
        if (!set_parameters())
        {
            emit sendConnectionStatus(false);
            return;
        }

    } else {
        info = "Video or Audio stream not found ";
        emit sendInfo(info);
        return;
    }

    emit sendConnectionStatus(true);

    AVFrame* convertedAudioFrame = av_frame_alloc();
    if (!convertedAudioFrame) {
        fprintf(stderr, "Error allocating converted audio AVFrame.\n");
        return;
    }

    SwrContext* swrAudioContext = swr_alloc();
    if (!swrAudioContext) {
        fprintf(stderr, "Error allocating SwrContext.\n");
        return;
    }

    av_opt_set_chlayout(swrAudioContext, "in_channel_layout", &audioCodecContext->ch_layout, 0);
    av_opt_set_chlayout(swrAudioContext, "out_channel_layout", &audioCodecContext->ch_layout, 0);
    av_opt_set_int(swrAudioContext, "in_sample_rate", audioCodecContext->sample_rate, 0);
    av_opt_set_int(swrAudioContext, "out_sample_rate", audioCodecContext->sample_rate, 0);
    av_opt_set_sample_fmt(swrAudioContext, "in_sample_fmt", audioCodecContext->sample_fmt, 0);
    av_opt_set_sample_fmt(swrAudioContext, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    if (swr_init(swrAudioContext) < 0) {
        fprintf(stderr, "Error initializing SwrContext.\n");
        return;
    }

    // Read packets from the input stream and write to the output file
    AVPacket* packet = av_packet_alloc();

    while (!m_stop)
    {
        int ret = av_read_frame(inputContext, packet);
        if(ret < 0)
        {
            std::cout << "there is no packet!" << ret << std::endl;
            break;
        }

        if (packet->stream_index == audio_idx)
        {
            // QByteArray audioData(reinterpret_cast<const char*>(packet->data), packet->size);
            // m_ioAudioDevice->write(audioData);
            int ret = avcodec_send_packet(audioCodecContext, packet);
            if (ret < 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                std::cout << "audio avcodec_send_packet: " << ret << std::endl;
                break;
            }
            while (ret  >= 0) {
                ret = avcodec_receive_frame(audioCodecContext, audio_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    //std::cout << "audio avcodec_receive_frame: " << ret << std::endl;
                    break;
                }

                convertedAudioFrame->ch_layout = audioCodecContext->ch_layout;
                convertedAudioFrame->format = AV_SAMPLE_FMT_S16;
                convertedAudioFrame->sample_rate = audioCodecContext->sample_rate;
                convertedAudioFrame->nb_samples = audio_frame->nb_samples;

                if (av_frame_get_buffer(convertedAudioFrame, 0) < 0) {
                    fprintf(stderr, "Error allocating converted frame buffer.\n");
                    av_frame_free(&convertedAudioFrame);
                    break;
                }
                swr_convert_frame(swrAudioContext, convertedAudioFrame, audio_frame);

//              printAudioFrameInfo(audioCodecContext, convertedAudioFrame);
                if(m_ioAudioDevice)
                    m_ioAudioDevice->write(reinterpret_cast<char*>(convertedAudioFrame->data[0] ), convertedAudioFrame->linesize[0]);

            }
            av_frame_unref(audio_frame);
        }
        // for preview
        else if (packet->stream_index == video_idx)
        {
            int ret = avcodec_send_packet(videoCodecContext, packet);
            if (ret < 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                std::cout << "video avcodec_send_packet: " << ret << std::endl;
                break;
            }
            while (ret  >= 0) {
                ret = avcodec_receive_frame(videoCodecContext, video_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    //std::cout << "video avcodec_receive_frame: " << ret << std::endl;
                    break;
                }

                SwsContext* swsContext = sws_getContext(video_frame->width, video_frame->height, videoCodecContext->pix_fmt,
                                                        video_frame->width, video_frame->height, AV_PIX_FMT_RGB32,
                                                        SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (!swsContext) {
                    std::cout << "Failed to create SwsContext" << std::endl;
                    break;
                }

                // Initialize the SwsContext
                auto ret = sws_init_context(swsContext, nullptr, nullptr);
                if (ret < 0) {
                    std::cout << "Failed to init SwsContext" << std::endl;
                    break;
                }

                uint8_t* destData[1] = { nullptr };
                int destLinesize[1] = { 0 };

                QImage image(video_frame->width, video_frame->height, QImage::Format_RGB32);

                destData[0] = image.bits();
                destLinesize[0] = image.bytesPerLine();

                sws_scale(swsContext, video_frame->data, video_frame->linesize, 0, video_frame->height, destData, destLinesize);
                emit sendFrame(image);

                // Cleanup
                sws_freeContext(swsContext);
                av_frame_unref(video_frame);
            }
        }

        AVStream* inputStream = inputContext->streams[packet->stream_index];
        AVStream* outputStream = outputContext->streams[packet->stream_index];

        if (packet->stream_index >= 0 && (unsigned int)packet->stream_index < inputContext->nb_streams)
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

    emit sendConnectionStatus(false);
    m_audioOutput->stop();

    // Write the output file trailer
    av_write_trailer(outputContext);

    // Close input and output contexts
    avformat_close_input(&inputContext);
    if (outputContext && !(outputContext->oformat->flags & AVFMT_NOFILE))
        avio_close(outputContext->pb);
    avformat_free_context(outputContext);
}

void ffmpeg_rtmp::run()
{
    m_stop = false;

    emit sendInfo("Trying to start Rtmp stream server.");
    start_streamer();
}
