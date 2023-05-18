//#ifndef TEMPLATE_H
//#define TEMPLATE_H
//SwsContext *swsContext = sws_alloc_context();
//int width = 1280;
//int height = 720;

//AVFrame* frameRGB;    // Output frame in RGB format

//frameRGB = av_frame_alloc();
//frameRGB->format = AV_PIX_FMT_BGR24;
//frameRGB->width = width;
//frameRGB->height = height;
//av_frame_get_buffer(frameRGB, 0);

//swsContext = sws_getContext(
//            width, height, AVPixelFormat::AV_PIX_FMT_YUV420P, width, height,
//            AVPixelFormat::AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL, NULL);


//    auto v_format = QVideoFrameFormat(image.size(), QVideoFrameFormat::pixelFormatFromImageFormat(image.format()));
//    QVideoFrame v_frame(v_format);
//    v_frame.map(QVideoFrame::WriteOnly);

//    // Copy pixel data from QImage to QVideoFrame
//    const uchar* srcBits = image.constBits();
//    uchar* destBits = v_frame.bits(0);
//    int bytesPerLine = v_frame.bytesPerLine(0);
//    for (int y = 0; y < image.height(); ++y) {
//        memcpy(destBits, srcBits, bytesPerLine);
//        srcBits += image.bytesPerLine();
//        destBits += bytesPerLine;
//    }

//        QString streamUrl  = "https://demo.unified-streaming.com/k8s/features/stable/video/tears-of-steel/tears-of-steel.ism/.m3u8";
//        player = new QMediaPlayer;
//        player->setVideoOutput(ui->viewfinder);
//        player->setSource(QUrl(streamUrl));
//        connect(player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString& errorString)
//        {
//            qDebug() << "Error:" << errorString;
//        });
//        player->play();
//#endif // TEMPLATE_H
