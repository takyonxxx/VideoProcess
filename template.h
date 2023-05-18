#ifndef TEMPLATE_H
#define TEMPLATE_H
//        while (ret >= 0) {
//            ret = avcodec_receive_frame(codecContext, frame);
//            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
//                break;
//            } else if (ret < 0) {
//                qWarning() << "Error during video decoding.";
//                break;
//            }

//            if (frame->format != AV_PIX_FMT_RGB24) {
//                // Convert the video frame to RGB format
//                sws_scale(swsContext, frame->data, frame->linesize, 0, frame->height,
//                          frameRGB->data, frameRGB->linesize);
//            } else {
//                // Use the existing RGB frame
//                frameRGB = frame;
//            }

//            // Create a QImage from the RGB frame
//            QImage image(frameRGB->data[0], frameRGB->width, frameRGB->height, QImage::Format_RGB888);

//            // Set the image as the media source for QMediaPlayer
//            QVideoFrame videoFrame(image);
//            mediaPlayer->setVideoOutput(videoFrame.handle());

//            // Play the media
//            mediaPlayer->play();

//            // Delay the playback based on the frame rate (fps)
//            int delay = 1000 / fps;
//            QThread::msleep(delay);

//            av_frame_unref(frame);

//            qDebug() << frame->format << frame->width << frame->height;
//        }

#endif // TEMPLATE_H
