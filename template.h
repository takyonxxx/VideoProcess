//AVFrame* convertedAudioFrame = av_frame_alloc();
//if (!convertedAudioFrame) {
//    fprintf(stderr, "Error allocating converted audio AVFrame.\n");
//    return;
//}
//    swrAudioContext = swr_alloc();
//    if (!swrAudioContext) {
//        fprintf(stderr, "Error allocating SwrContext.\n");
//        return false;
//    }
//    av_opt_set_chlayout(swrAudioContext, "in_channel_layout", &audioCodecContext->ch_layout, 0);
//    av_opt_set_chlayout(swrAudioContext, "out_channel_layout", &audioCodecContext->ch_layout, 0);
//    av_opt_set_int(swrAudioContext, "in_sample_rate", audioCodecContext->sample_rate, 0);
//    av_opt_set_int(swrAudioContext, "out_sample_rate", audioCodecContext->sample_rate, 0);
//    av_opt_set_sample_fmt(swrAudioContext, "in_sample_fmt", audioCodecContext->sample_fmt, 0);
//    av_opt_set_sample_fmt(swrAudioContext, "out_sample_fmt", audioCodecContext->sample_fmt, 0);

//    if (swr_init(swrAudioContext) < 0) {
//        fprintf(stderr, "Error initializing SwrContext.\n");
//        return false;
//    }
//                convertedFrame->ch_layout = audioCodecContext->ch_layout;
//                convertedFrame->format = audioCodecContext->sample_fmt;
//                convertedFrame->sample_rate = audioCodecContext->sample_rate;
//                convertedFrame->nb_samples = audio_frame->nb_samples;

//                if (av_frame_get_buffer(convertedFrame, 0) < 0) {
//                    fprintf(stderr, "Error allocating converted frame buffer.\n");
//                    av_frame_free(&convertedFrame);
//                    break;
//                }

//                swr_convert_frame(swrAudioContext, convertedFrame, audio_frame);
//                if(m_ioAudioDevice)
//                    m_ioAudioDevice->write(reinterpret_cast<char*>(convertedFrame->data[0]), convertedFrame->linesize[0]);

//                av_frame_free(&convertedFrame);

//    //    Open the output file
//    auto file_mp3 = QString("%1/output.mp3").arg(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
//    FILE* outputMp3File = fopen(file_mp3.toStdString().c_str(), "wb");
//    if (!outputMp3File) {
//        return;
//    }


//    fwrite(audio_frame->data[0], 1, audio_frame->linesize[0], outputMp3File);
