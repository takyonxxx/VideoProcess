#include "camera.h"
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
#include "ui_camera_mobile.h"
#else
#include "ui_camera.h"
#endif
#include "videosettings.h"
#include "imagesettings.h"
#include "metadatadialog.h"

#include <QMediaRecorder>
#include <QVideoWidget>
#include <QCameraDevice>
#include <QMediaMetaData>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QAudioInput>

#include <QMessageBox>
#include <QPalette>
#include <QImage>

#include <QtWidgets>
#include <QMediaDevices>
#include <QMediaFormat>

Camera::Camera()
    : ui(new Ui::Camera)
{
    ui->setupUi(this);

    ui->pushStream->setStyleSheet("font-size: 12pt; font-weight: bold; color: white;background-color:#154360; padding: 3px; spacing: 3px;");
    ui->pushExit->setStyleSheet("font-size: 12pt; font-weight: bold; color: white;background-color:#154360; padding: 3px; spacing: 3px;");
    ui->textTerminal->setStyleSheet("font: 10pt; color: #00cccc; background-color: #001a1a;");

    m_audioInput.reset(new QAudioInput);
    m_captureSession.setAudioInput(m_audioInput.get());

    m_ffmpeg_rtmp = new ffmpeg_rtmp();
    if(m_ffmpeg_rtmp)
    {
        connect(m_ffmpeg_rtmp,&ffmpeg_rtmp::sendInfo,this, &Camera::setInfo);
        connect(m_ffmpeg_rtmp,&ffmpeg_rtmp::sendConnectionStatus,this, &Camera::setConnectionStatus);
        connect(m_ffmpeg_rtmp,&ffmpeg_rtmp::sendFrame,this, &Camera::setFrame);
    }

    //Camera devices:

    videoDevicesGroup = new QActionGroup(this);
    videoDevicesGroup->setExclusive(true);
    updateCameras();
    connect(&m_devices, &QMediaDevices::videoInputsChanged, this, &Camera::updateCameras);

    connect(videoDevicesGroup, &QActionGroup::triggered, this, &Camera::updateCameraDevice);
    connect(ui->captureWidget, &QTabWidget::currentChanged, this, &Camera::updateCaptureMode);

    setCamera(QMediaDevices::defaultVideoInput());

    //        QString streamUrl  = "https://demo.unified-streaming.com/k8s/features/stable/video/tears-of-steel/tears-of-steel.ism/.m3u8";
    //        player = new QMediaPlayer;
    //        player->setVideoOutput(ui->viewfinder);
    //        player->setSource(QUrl(streamUrl));
    //        connect(player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString& errorString)
    //        {
    //            qDebug() << "Error:" << errorString;
    //        });
    //        player->play();
}

void Camera::setCamera(const QCameraDevice &cameraDevice)
{
    m_camera.reset(new QCamera(cameraDevice));
    m_captureSession.setCamera(m_camera.data());

    connect(m_camera.data(), &QCamera::activeChanged, this, &Camera::updateCameraActive);
    connect(m_camera.data(), &QCamera::errorOccurred, this, &Camera::displayCameraError);

    if (!m_mediaRecorder) {
        m_mediaRecorder.reset(new QMediaRecorder);
        m_captureSession.setRecorder(m_mediaRecorder.data());
        connect(m_mediaRecorder.data(), &QMediaRecorder::recorderStateChanged, this, &Camera::updateRecorderState);
    }

    m_imageCapture = new QImageCapture;
    m_captureSession.setImageCapture(m_imageCapture);

    connect(m_mediaRecorder.data(), &QMediaRecorder::durationChanged, this, &Camera::updateRecordTime);
    connect(m_mediaRecorder.data(), &QMediaRecorder::errorChanged, this, &Camera::displayRecorderError);

    m_captureSession.setVideoOutput(ui->viewfinder);

    updateCameraActive(m_camera->isActive());
    updateRecorderState(m_mediaRecorder->recorderState());

    connect(m_imageCapture, &QImageCapture::readyForCaptureChanged, this, &Camera::readyForCapture);
    connect(m_imageCapture, &QImageCapture::imageCaptured, this, &Camera::processCapturedImage);
    connect(m_imageCapture, &QImageCapture::imageSaved, this, &Camera::imageSaved);
    connect(m_imageCapture, &QImageCapture::errorOccurred, this, &Camera::displayCaptureError);
    readyForCapture(m_imageCapture->isReadyForCapture());

    updateCaptureMode();

    if (m_camera->cameraFormat().isNull()) {
        auto formats = cameraDevice.videoFormats();
        if (!formats.isEmpty()) {
            // Choose a decent camera format: Maximum resolution at at least 30 FPS
            // we use 29 FPS to compare against as some cameras report 29.97 FPS...
            QCameraFormat bestFormat;
            for (const auto &fmt : formats) {
                if (bestFormat.maxFrameRate() < 29 && fmt.maxFrameRate() > bestFormat.maxFrameRate())
                    bestFormat = fmt;
                else if (bestFormat.maxFrameRate() == fmt.maxFrameRate() &&
                         bestFormat.resolution().width()*bestFormat.resolution().height() <
                         fmt.resolution().width()*fmt.resolution().height())
                    bestFormat = fmt;
            }

            m_camera->setCameraFormat(bestFormat);
            m_mediaRecorder->setVideoFrameRate(bestFormat.maxFrameRate());
        }
    }

    m_camera->start();
}

void Camera::keyPressEvent(QKeyEvent * event)
{
    if (event->isAutoRepeat())
        return;

    switch (event->key()) {
    case Qt::Key_CameraFocus:
        displayViewfinder();
        event->accept();
        break;
    case Qt::Key_Camera:
        if (m_doImageCapture) {
            takeImage();
        } else {
            if (m_mediaRecorder->recorderState() == QMediaRecorder::RecordingState)
                stop();
            else
                record();
        }
        event->accept();
        break;
    default:
        QMainWindow::keyPressEvent(event);
    }
}

void Camera::keyReleaseEvent(QKeyEvent *event)
{
    QMainWindow::keyReleaseEvent(event);
}

void Camera::updateRecordTime()
{
    QString str = QString("Recorded %1 sec").arg(m_mediaRecorder->duration()/1000);
    ui->statusbar->showMessage(str);
}

void Camera::processCapturedImage(int requestId, const QImage& img)
{
    Q_UNUSED(requestId);
    QImage scaledImage = img.scaled(ui->viewfinder->size(),
                                    Qt::KeepAspectRatio,
                                    Qt::SmoothTransformation);

    ui->lastImagePreviewLabel->setPixmap(QPixmap::fromImage(scaledImage));

    // Display captured image for 4 seconds.
    displayCapturedImage();
    QTimer::singleShot(4000, this, &Camera::displayViewfinder);
}

void Camera::configureCaptureSettings()
{
    if (m_doImageCapture)
        configureImageSettings();
    else
        configureVideoSettings();
}

void Camera::configureVideoSettings()
{
    VideoSettings settingsDialog(m_mediaRecorder.data());
    settingsDialog.setWindowFlags(settingsDialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    if (settingsDialog.exec())
        settingsDialog.applySettings();
}

void Camera::configureImageSettings()
{
    ImageSettings settingsDialog(m_imageCapture);
    settingsDialog.setWindowFlags(settingsDialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    if (settingsDialog.exec()) {
        settingsDialog.applyImageSettings();
    }
}

void Camera::record()
{
    m_mediaRecorder->record();
    updateRecordTime();
}

void Camera::pause()
{
    m_mediaRecorder->pause();
}

void Camera::stop()
{
    m_mediaRecorder->stop();
}

void Camera::setMuted(bool muted)
{
    m_captureSession.audioInput()->setMuted(muted);
}

void Camera::takeImage()
{
    m_isCapturingImage = true;
    m_imageCapture->captureToFile();
}

void Camera::displayCaptureError(int id, const QImageCapture::Error error, const QString &errorString)
{
    Q_UNUSED(id);
    Q_UNUSED(error);
    QMessageBox::warning(this, tr("Image Capture Error"), errorString);
    m_isCapturingImage = false;
}

void Camera::startCamera()
{
    m_camera->start();
}

void Camera::stopCamera()
{
    m_camera->stop();
}

void Camera::updateCaptureMode()
{
    int tabIndex = ui->captureWidget->currentIndex();
    m_doImageCapture = (tabIndex == 0);
}

void Camera::updateCameraActive(bool active)
{
    //    if (active) {
    //        ui->actionStartCamera->setEnabled(false);
    //        ui->actionStopCamera->setEnabled(true);
    //        ui->captureWidget->setEnabled(true);
    //        ui->actionSettings->setEnabled(true);
    //    } else {
    //        ui->actionStartCamera->setEnabled(true);
    //        ui->actionStopCamera->setEnabled(false);
    //        ui->captureWidget->setEnabled(false);
    //        ui->actionSettings->setEnabled(false);
    //    }
}

void Camera::updateRecorderState(QMediaRecorder::RecorderState state)
{
    switch (state) {
    case QMediaRecorder::StoppedState:
        break;
    case QMediaRecorder::PausedState:
        break;
    case QMediaRecorder::RecordingState:
        break;
    }
}

void Camera::setExposureCompensation(int index)
{
    m_camera->setExposureCompensation(index*0.5);
}

void Camera::displayRecorderError()
{
    if (m_mediaRecorder->error() != QMediaRecorder::NoError)
        QMessageBox::warning(this, tr("Capture Error"), m_mediaRecorder->errorString());
}

void Camera::displayCameraError()
{
    if (m_camera->error() != QCamera::NoError)
        QMessageBox::warning(this, tr("Camera Error"), m_camera->errorString());
}

void Camera::updateCameraDevice(QAction *action)
{
    setCamera(qvariant_cast<QCameraDevice>(action->data()));
}

void Camera::displayViewfinder()
{
    ui->stackedWidget->setCurrentIndex(0);
}

void Camera::displayCapturedImage()
{
    ui->stackedWidget->setCurrentIndex(1);
}

void Camera::readyForCapture(bool ready)
{

}

void Camera::imageSaved(int id, const QString &fileName)
{
    Q_UNUSED(id);
    ui->statusbar->showMessage(tr("Captured \"%1\"").arg(QDir::toNativeSeparators(fileName)));

    m_isCapturingImage = false;
    if (m_applicationExiting)
        close();
}

void Camera::closeEvent(QCloseEvent *event)
{
    if (m_isCapturingImage) {
        setEnabled(false);
        m_applicationExiting = true;
        event->ignore();
    } else {
        event->accept();
    }
}

void Camera::updateCameras()
{
    ui->menuDevices->clear();
    const QList<QCameraDevice> availableCameras = QMediaDevices::videoInputs();
    for (const QCameraDevice &cameraDevice : availableCameras) {
        QAction *videoDeviceAction = new QAction(cameraDevice.description(), videoDevicesGroup);
        videoDeviceAction->setCheckable(true);
        videoDeviceAction->setData(QVariant::fromValue(cameraDevice));
        if (cameraDevice == QMediaDevices::defaultVideoInput())
            videoDeviceAction->setChecked(true);

        ui->menuDevices->addAction(videoDeviceAction);
    }
}

void Camera::showMetaDataDialog()
{
    if (!m_metaDataDialog)
        m_metaDataDialog = new MetaDataDialog(this);
    m_metaDataDialog->setAttribute(Qt::WA_DeleteOnClose, false);
    if (m_metaDataDialog->exec() == QDialog::Accepted)
        saveMetaData();
}

void Camera::saveMetaData()
{
    QMediaMetaData data;
    for (int i = 0; i < QMediaMetaData::NumMetaData; i++) {
        QString val = m_metaDataDialog->m_metaDataFields[i]->text();
        if (!val.isEmpty()) {
            auto key = static_cast<QMediaMetaData::Key>(i);
            if (i == QMediaMetaData::CoverArtImage) {
                QImage coverArt(val);
                data.insert(key, coverArt);
            }
            else if (i == QMediaMetaData::ThumbnailImage) {
                QImage thumbnail(val);
                data.insert(key, thumbnail);
            }
            else if (i == QMediaMetaData::Date) {
                QDateTime date = QDateTime::fromString(val);
                data.insert(key, date);
            }
            else {
                data.insert(key, val);
            }
        }
    }
    m_mediaRecorder->setMetaData(data);
}

void Camera::on_pushStream_clicked()
{
    if(ui->pushStream->text() == "Start")
    {
        m_ffmpeg_rtmp->start();
        ui->pushStream->setText("Stop");
    }
    else
    {
        m_ffmpeg_rtmp->stop();
        ui->pushStream->setText("Start");
    }
}

void Camera::setInfo(QString message)
{
    ui->textTerminal->append(message);
}

void Camera::setConnectionStatus(bool status)
{
    if(status)
    {
        ui->pushStream->setText("Stop");
        setInfo("Rtmp stream started.");
    }
    else
    {
        ui->pushStream->setText("Start");
        setInfo("Rtmp stream stopped.");
    }
}

void Camera::setFrame(AVFrame frame)
{/*


    // Create a QImage from the RGB frame
    QImage image(frameRGB->data[0], frameRGB->width, frameRGB->height, QImage::Format_RGB888);

    // Set the image as the media source for QMediaPlayer
    QVideoFrame videoFrame(image);
    mediaPlayer->setVideoOutput(videoFrame.handle());

    // Play the media
    mediaPlayer->play();

    // Delay the playback based on the frame rate (fps)
    int delay = 1000 / fps;
    QThread::msleep(delay);

    av_frame_unref(frame);*/
    qDebug() << frame.width << frame.height;
}

void Camera::on_pushExit_clicked()
{
    QApplication::quit();
}

