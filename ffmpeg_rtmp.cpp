#include "ffmpeg_rtmp.h"
#include <QStandardPaths>

#define STR(x) #x
#define XSTR(x) STR(x)

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
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(57, 0, 0)
    std::cout << "Audio frame info:\n"
              << "  Sample count: " << frame->nb_samples << '\n'
              << "  Channel count: " << codecContext->channels << '\n'
              << "  Format: " << av_get_sample_fmt_name(codecContext->sample_fmt) << '\n'
              << "  Bytes per sample: " << av_get_bytes_per_sample(codecContext->sample_fmt) << '\n'
              << "  Is planar? " << av_sample_fmt_is_planar(codecContext->sample_fmt) << '\n';
#else
    std::cout << "Audio frame info:\n"
              << "  Sample count: " << frame->nb_samples << '\n'
              << "  Channel count: " << codecContext->ch_layout.nb_channels << '\n'
              << "  Format: " << av_get_sample_fmt_name(codecContext->sample_fmt) << '\n'
              << "  Bytes per sample: " << av_get_bytes_per_sample(codecContext->sample_fmt) << '\n'
              << "  Is planar? " << av_sample_fmt_is_planar(codecContext->sample_fmt) << '\n';
#endif
}


ffmpeg_rtmp::ffmpeg_rtmp(QObject *parent)
    : QThread{parent}
{
    // Enable FFmpeg logging
    av_log_set_level(AV_LOG_ERROR);
    //    av_log_set_callback(ffmpegLogCallback);
    //tTM/2!**

    const char* ffmpegVersion = av_version_info();
    std::string avutil_verison = XSTR(LIBAVUTIL_VERSION);
    std::cout << "FFmpeg version: " << ffmpegVersion << " avutil_verison : " << avutil_verison << std::endl;

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
    QAudioDevice deviceInfo(QMediaDevices::defaultAudioOutput());
    QAudioFormat format = deviceInfo.preferredFormat();

    if (!deviceInfo.isFormatSupported(format)) {
        qWarning() << "Raw audio format not supported by backend, cannot play audio.";
        return false;
    }
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(57, 0, 0)
    format.setChannelCount(audioCodecContext->channels);
#else
    format.setChannelCount(audioCodecContext->ch_layout.nb_channels);
#endif
    format.setSampleRate(audioCodecContext->sample_rate);
    format.setSampleFormat(QAudioFormat::Int16);
    format.setChannelConfig(QAudioFormat::ChannelConfigStereo);

    qDebug() << format.sampleRate() << format.channelCount() << format.sampleFormat();

    m_audioSinkOutput.reset(new QAudioSink(deviceInfo, format));
    int bufferSize = 4096; // Set your desired buffer size in bytes

    m_audioSinkOutput->setBufferSize(bufferSize);

    m_ioAudioDevice = m_audioSinkOutput->start();

    qreal initialVolume = QAudio::convertVolume(m_audioSinkOutput->volume(),
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
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(57, 0, 0)
    info = "Audio Format: " + QString(av_get_sample_fmt_name(audioCodecContext->sample_fmt)) + " Channels: " + QString::number(codecAudioParams->channels);
#else
    info = "Audio Format: " + QString(av_get_sample_fmt_name(audioCodecContext->sample_fmt)) + " Channels: " + QString::number(codecAudioParams->ch_layout.nb_channels);
#endif
    emit sendInfo(info);

    return true;
}

int ffmpeg_rtmp::init_swr_context(AVSampleFormat out_format)
{

    swrAudioContext = swr_alloc();
    if (!swrAudioContext) {
        fprintf(stderr, "Error allocating SwrContext.\n");
        return false;
    }

#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(57, 0, 0)
    av_opt_set_channel_layout(swrAudioContext, "in_channel_layout", audioCodecContext->channel_layout, 0);
    av_opt_set_channel_layout(swrAudioContext, "out_channel_layout", audioCodecContext->channel_layout, 0);
#else
    av_opt_set_chlayout(swrAudioContext, "in_channel_layout", &audioCodecContext->ch_layout, 0);
    av_opt_set_chlayout(swrAudioContext, "out_channel_layout", &audioCodecContext->ch_layout, 0);
#endif
    av_opt_set_int(swrAudioContext, "in_sample_rate", audioCodecContext->sample_rate, 0);
    av_opt_set_int(swrAudioContext, "out_sample_rate", audioCodecContext->sample_rate, 0);
    av_opt_set_sample_fmt(swrAudioContext, "in_sample_fmt", audioCodecContext->sample_fmt, 0);
    av_opt_set_sample_fmt(swrAudioContext, "out_sample_fmt", out_format, 0);

    if (swr_init(swrAudioContext) < 0) {
        fprintf(stderr, "Error initializing SwrContext.\n");
        return false;
    }

    return true;
}

AVFrame* ffmpeg_rtmp::convert_audio_frame(AVSampleFormat out_format)
{
    AVFrame* convertedAudioFrame = av_frame_alloc();
    if (!convertedAudioFrame) {
        fprintf(stderr, "Error allocating converted audio AVFrame.\n");
        return NULL;
    }

#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(57, 0, 0)
    convertedAudioFrame->channel_layout = audioCodecContext->channel_layout;
#else
    convertedAudioFrame->ch_layout = audioCodecContext->ch_layout;
#endif
    convertedAudioFrame->format = out_format;
    convertedAudioFrame->sample_rate = audioCodecContext->sample_rate;
    convertedAudioFrame->nb_samples = audio_frame->nb_samples;

    if (av_frame_get_buffer(convertedAudioFrame, 0) < 0) {
        fprintf(stderr, "Error allocating converted frame buffer.\n");
        av_frame_free(&convertedAudioFrame);
        return NULL;
    }
    swr_convert_frame(swrAudioContext, convertedAudioFrame, audio_frame);
    return convertedAudioFrame;
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

        AVStream* inputStream = inputContext->streams[packet->stream_index];
        AVStream* outputStream = outputContext->streams[packet->stream_index];

        if (packet->stream_index >= 0 && (unsigned int)packet->stream_index < inputContext->nb_streams)
        {

            if (packet->stream_index == audio_idx)
            {
                int ret = avcodec_send_packet(audioCodecContext, packet);
                if (ret < 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    std::cout << "audio avcodec_send_packet: " << ret << std::endl;
                    break;
                }

                while (ret >= 0) {
                    ret = avcodec_receive_frame(audioCodecContext, audio_frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    }

                    if (m_ioAudioDevice)
                    {
                        if (av_sample_fmt_is_planar(audioCodecContext->sample_fmt) == 1)
                        {
                            // Calculate the total number of samples in the frame
                            int numSamples = audio_frame->nb_samples;
                            int channels = audioCodecContext->channels;

                            // Allocate memory for the PCM 16-bit frame
                            int16_t* pcm16Frame = new int16_t[numSamples * channels];
                            auto f_contstant = 32767.0f;

                            // Convert planar float frame to PCM 16-bit frame
                            for (int i = 0; i < numSamples; ++i)
                            {
                                const float* const* planarFloatData = reinterpret_cast<const float* const*>(audio_frame->extended_data);

                                for (int channel = 0; channel < channels; ++channel)
                                {
                                    // Scale the float sample to the range of int16_t (-32768 to 32767)
                                    float scaledSample = planarFloatData[channel][i] * f_contstant;

                                    // Clamp the sample value to the valid range of int16_t
                                    scaledSample = std::clamp<float>(scaledSample, -1 * f_contstant, f_contstant);

                                    // Convert to int16_t with rounding
                                    pcm16Frame[i * channels + channel] = static_cast<int16_t>(scaledSample + 0.5f);
                                }
                            }

                            int bytesToWrite = numSamples * channels * sizeof(int16_t);

                            // Write the PCM 16-bit frame to m_ioAudioDevice
                            const char* pcm16FramePtr = reinterpret_cast<const char*>(pcm16Frame);
                            qint64 totalBytesWritten = 0;

                            while (totalBytesWritten < bytesToWrite) {
                                qint64 bytesWritten = m_ioAudioDevice->write(pcm16FramePtr + totalBytesWritten, bytesToWrite - totalBytesWritten);
                                if (bytesWritten == -1) {
                                    // Handle the error case
                                    break;
                                }
                                totalBytesWritten += bytesWritten;
                            }

                            // Clean up the allocated memory
                            delete[] pcm16Frame;
                        }
                        else
                        {
                            m_ioAudioDevice->write(reinterpret_cast<char*>(audio_frame->data[0]), audio_frame->linesize[0]);
                        }
                    }

                    av_frame_unref(audio_frame);
                }
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

            // Rescale packet timestamps
            packet->pts = av_rescale_q(packet->pts, inputStream->time_base, outputStream->time_base);
            packet->dts = av_rescale_q(packet->dts, inputStream->time_base, outputStream->time_base);
            packet->duration = av_rescale_q(packet->duration, inputStream->time_base, outputStream->time_base);
            packet->pos = -1;

            int ret = av_interleaved_write_frame(outputContext, packet);
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN)) {
                    // Handle EAGAIN error
                } else if (ret == AVERROR_EOF) {
                    // Handle EOF error
                } else {
                    // Handle other errors
                    char error_buffer[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(ret, error_buffer, sizeof(error_buffer));
                    qDebug() << "Error writing frame: " << error_buffer;
                }
            }
        }

        av_packet_unref(packet);
    }

    emit sendConnectionStatus(false);
    m_audioSinkOutput->stop();

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
