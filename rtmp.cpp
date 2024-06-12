#include "rtmp.h"
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

static fftw_plan  planFft;
static fftw_complex* in= (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*DEFAULT_FFT_SIZE);
static fftw_complex* out= (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*DEFAULT_FFT_SIZE);

Rtmp::Rtmp()
    : ui(new Ui::Camera)
{
    ui->setupUi(this);

    ui->pushStream->setStyleSheet("font-size: 12pt; font-weight: bold; color: white;background-color:#154360; padding: 3px; spacing: 3px;");
    ui->pushExit->setStyleSheet("font-size: 12pt; font-weight: bold; color: white;background-color:#154360; padding: 3px; spacing: 3px;");
    ui->labelRtmpUrl->setStyleSheet("font-size: 14pt; font-weight: bold; color: #ECF0F1;background-color: #2E4053;   padding: 6px; spacing: 6px;");
    ui->textTerminal->setStyleSheet("font: 10pt; color: #00cccc; background-color: #001a1a;");
    //    ui->audioOutputDeviceBox->setStyleSheet("font-size: 10pt; font-weight: bold; color: white;background-color:orange; padding: 6px; spacing: 6px;");
    connect(ui->audioOutputDeviceBox, QOverload<int>::of(&QComboBox::activated), this, &Rtmp::outputDeviceChanged);
    QObject::connect(this, SIGNAL(spectValueChanged(int)),this, SLOT(onSpectrumProcessed(int)));

    ui->graphicsView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    for (auto &device: QMediaDevices::audioOutputs()) {
        auto name = device.description();
        ui->audioOutputDeviceBox->addItem(name, QVariant::fromValue(device));
    }

    m_audioInput.reset(new QAudioInput);
    m_captureSession.setAudioInput(m_audioInput.get());

    m_ffmpeg_rtmp = new ffmpeg_rtmp();
    if(m_ffmpeg_rtmp)
    {
        connect(m_ffmpeg_rtmp,&ffmpeg_rtmp::sendUrl,this, &Rtmp::setUrl);
        connect(m_ffmpeg_rtmp,&ffmpeg_rtmp::sendInfo,this, &Rtmp::setInfo);
        connect(m_ffmpeg_rtmp,&ffmpeg_rtmp::sendConnectionStatus,this, &Rtmp::setConnectionStatus);
        connect(m_ffmpeg_rtmp,&ffmpeg_rtmp::sendVideoFrame,this, &Rtmp::setVideoFrame);
        connect(m_ffmpeg_rtmp,&ffmpeg_rtmp::sendAudioFrame,this, &Rtmp::setAudioFrame);
        m_ffmpeg_rtmp->setUrl();
    }

    //Camera devices:

    videoDevicesGroup = new QActionGroup(this);
    videoDevicesGroup->setExclusive(true);
    updateCameras();
    connect(&m_devices, &QMediaDevices::videoInputsChanged, this, &Rtmp::updateCameras);

    connect(videoDevicesGroup, &QActionGroup::triggered, this, &Rtmp::updateCameraDevice);
    connect(ui->captureWidget, &QTabWidget::currentChanged, this, &Rtmp::updateCaptureMode);

    scene = new QGraphicsScene;
    QBrush brush(Qt::black); // Set the desired background color
    scene->setBackgroundBrush(brush);
    view = ui->graphicsView;
    view->setScene(scene);
    setCamera(QMediaDevices::defaultVideoInput());
    initSpectrumGraph();
}


void Rtmp::resizeEvent(QResizeEvent *event)
{
    int width = event->size().width();
    int height = width * 9 / 16; // Calculate height based on 16:9 aspect ratio

//    ui->graphicsView->setFixedSize(2 * width, height);
}

void Rtmp::onSpectrumProcessed(int fftSize)
{
    ui->Plotter->setNewFttData(d_iirFftData, d_realFftData, fftSize/2);
}

void Rtmp::initSpectrumGraph()
{
    auto fftSize = DEFAULT_FFT_SIZE;
    auto sampleRate = DEFAULT_SAMPLE_RATE;

    d_realFftData = new float[fftSize];
    d_iirFftData = new float[fftSize]();

    for (int i = 0; i < fftSize; i++)
        d_iirFftData[i] = RESET_FFT_FACTOR;  // dBFS

    for (int i = 0; i < fftSize; i++)
        d_realFftData[i] = RESET_FFT_FACTOR;

    d_fftAvg = 1.0 - 1.0e-2 * ((float)75);

    ui->Plotter->setTooltipsEnabled(true);
    ui->Plotter->setSampleRate(sampleRate);
    ui->Plotter->setSpanFreq((quint32)sampleRate);
    ui->Plotter->setCenterFreq(sampleRate/2);
    ui->Plotter->setFftCenterFreq(0);
    ui->Plotter->setFftRate(sampleRate/fftSize);
    ui->Plotter->setFftRange(-140.0f, 20.0f);

    ui->Plotter->setFreqUnits(1000);
    ui->Plotter->setPercent2DScreen(75);
    ui->Plotter->setFreqDigits(1);
    ui->Plotter->setFilterBoxEnabled(true);
    ui->Plotter->setCenterLineEnabled(true);
    ui->Plotter->setBookmarksEnabled(true);
    ui->Plotter->setVdivDelta(50);
    ui->Plotter->setHdivDelta(50);
    ui->Plotter->setFftPlotColor(Qt::green);
    ui->Plotter->setFftFill(true);

}

void Rtmp::setVideoFrame(QImage image)
{
    scene->clear();
    QGraphicsPixmapItem *pixmapItem = scene->addPixmap(QPixmap::fromImage(image));
    view->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    view->update();
}

void Rtmp::setAudioFrame(const char * payloadbuf, int payloadlen)
{

    for (int i = 0; i < payloadlen; i++)
    {
        if(sampleCount < DEFAULT_FFT_SIZE)
        {
            signalInput[sampleCount] = payloadbuf[i];
            sampleCount ++;
        }
        else
        {
            runFFTW(signalInput,DEFAULT_FFT_SIZE);
            sampleCount = 0;
        }
    }
}

void Rtmp::runFFTW(float *buffer, int fftsize)
{
    if(fftsize > 0)
    {
        float pwr, lpwr;
        float pwr_scale = 1.0 / ((float)fftsize * (float)fftsize);
        int i;

        planFft = fftw_plan_dft_1d(fftsize, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

        for (i=0; i < fftsize; i++)
        {
            in[i][0] = (float)buffer[i] / 127.5f - 1.f;
            in[i][1] = (float)-buffer[i+1] / 127.5f - 1.f;
        }

        fftw_execute(planFft);

        for (i = 0; i < fftsize; ++i)
        {
            pwr  = sqrt(out[i][REAL] * out[i][REAL] + out[i][IMAG] * out[i][IMAG]);
            lpwr = 15.f * log10f(pwr_scale * pwr);

            if(d_realFftData[i] < lpwr)
                d_realFftData[i] = lpwr;
            else d_realFftData[i] -= (d_realFftData[i] - lpwr) / 5.f;
                d_iirFftData[i] += d_fftAvg * (d_realFftData[i] - d_iirFftData[i]);
        }
        emit spectValueChanged(fftsize);
    }
}

void Rtmp::outputDeviceChanged(int index)
{
    QAudioDevice ouputDevice = ui->audioOutputDeviceBox->itemData(index).value<QAudioDevice>();
    m_ffmpeg_rtmp->set_audio_device(ouputDevice);
}


void Rtmp::setCamera(const QCameraDevice &cameraDevice)
{
    m_camera.reset(new QCamera(cameraDevice));
    m_captureSession.setCamera(m_camera.data());

    connect(m_camera.data(), &QCamera::activeChanged, this, &Rtmp::updateCameraActive);
    connect(m_camera.data(), &QCamera::errorOccurred, this, &Rtmp::displayCameraError);

    if (!m_mediaRecorder) {
        m_mediaRecorder.reset(new QMediaRecorder);
        m_captureSession.setRecorder(m_mediaRecorder.data());
        connect(m_mediaRecorder.data(), &QMediaRecorder::recorderStateChanged, this, &Rtmp::updateRecorderState);
    }

    m_imageCapture = new QImageCapture;
    m_captureSession.setImageCapture(m_imageCapture);

    connect(m_mediaRecorder.data(), &QMediaRecorder::durationChanged, this, &Rtmp::updateRecordTime);
    connect(m_mediaRecorder.data(), &QMediaRecorder::errorChanged, this, &Rtmp::displayRecorderError);

    m_captureSession.setVideoOutput(ui->viewfinder);

    updateCameraActive(m_camera->isActive());
    updateRecorderState(m_mediaRecorder->recorderState());

    connect(m_imageCapture, &QImageCapture::readyForCaptureChanged, this, &Rtmp::readyForCapture);
    connect(m_imageCapture, &QImageCapture::imageCaptured, this, &Rtmp::processCapturedImage);
    connect(m_imageCapture, &QImageCapture::imageSaved, this, &Rtmp::imageSaved);
    connect(m_imageCapture, &QImageCapture::errorOccurred, this, &Rtmp::displayCaptureError);
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

void Rtmp::keyPressEvent(QKeyEvent * event)
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

void Rtmp::keyReleaseEvent(QKeyEvent *event)
{
    QMainWindow::keyReleaseEvent(event);
}

void Rtmp::updateRecordTime()
{
    QString str = QString("Recorded %1 sec").arg(m_mediaRecorder->duration()/1000);
    //    ui->statusbar->showMessage(str);
}

void Rtmp::processCapturedImage(int requestId, const QImage& img)
{
    Q_UNUSED(requestId);
    QImage scaledImage = img.scaled(ui->viewfinder->size(),
                                    Qt::KeepAspectRatio,
                                    Qt::SmoothTransformation);

    ui->lastImagePreviewLabel->setPixmap(QPixmap::fromImage(scaledImage));

    // Display captured image for 4 seconds.
    displayCapturedImage();
    QTimer::singleShot(4000, this, &Rtmp::displayViewfinder);

}

void Rtmp::configureCaptureSettings()
{
    if (m_doImageCapture)
        configureImageSettings();
    else
        configureVideoSettings();
}

void Rtmp::configureVideoSettings()
{
    VideoSettings settingsDialog(m_mediaRecorder.data());
    settingsDialog.setWindowFlags(settingsDialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    if (settingsDialog.exec())
        settingsDialog.applySettings();
}

void Rtmp::configureImageSettings()
{
    ImageSettings settingsDialog(m_imageCapture);
    settingsDialog.setWindowFlags(settingsDialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    if (settingsDialog.exec()) {
        settingsDialog.applyImageSettings();
    }
}

void Rtmp::record()
{
    m_mediaRecorder->record();
    updateRecordTime();
}

void Rtmp::pause()
{
    m_mediaRecorder->pause();
}

void Rtmp::stop()
{
    m_mediaRecorder->stop();
}

void Rtmp::setMuted(bool muted)
{
    m_captureSession.audioInput()->setMuted(muted);
}

void Rtmp::takeImage()
{
    m_isCapturingImage = true;
    m_imageCapture->captureToFile();
}

void Rtmp::displayCaptureError(int id, const QImageCapture::Error error, const QString &errorString)
{
    Q_UNUSED(id);
    Q_UNUSED(error);
    QMessageBox::warning(this, tr("Image Capture Error"), errorString);
    m_isCapturingImage = false;
}

void Rtmp::startCamera()
{
    m_camera->start();
}

void Rtmp::stopCamera()
{
    m_camera->stop();
}

void Rtmp::updateCaptureMode()
{
    int tabIndex = ui->captureWidget->currentIndex();
    m_doImageCapture = (tabIndex == 0);
}

void Rtmp::updateCameraActive(bool active)
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

void Rtmp::updateRecorderState(QMediaRecorder::RecorderState state)
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

void Rtmp::setExposureCompensation(int index)
{
    m_camera->setExposureCompensation(index*0.5);
}

void Rtmp::displayRecorderError()
{
    if (m_mediaRecorder->error() != QMediaRecorder::NoError)
        QMessageBox::warning(this, tr("Capture Error"), m_mediaRecorder->errorString());
}

void Rtmp::displayCameraError()
{
    if (m_camera->error() != QCamera::NoError)
        QMessageBox::warning(this, tr("Camera Error"), m_camera->errorString());
}

void Rtmp::updateCameraDevice(QAction *action)
{
    setCamera(qvariant_cast<QCameraDevice>(action->data()));
}

void Rtmp::displayViewfinder()
{
    ui->stackedWidget->setCurrentIndex(0);
}

void Rtmp::displayCapturedImage()
{
    ui->stackedWidget->setCurrentIndex(1);
}

void Rtmp::readyForCapture(bool ready)
{

}

void Rtmp::imageSaved(int id, const QString &fileName)
{
    Q_UNUSED(id);
    //    ui->statusbar->showMessage(tr("Captured \"%1\"").arg(QDir::toNativeSeparators(fileName)));

    m_isCapturingImage = false;
    if (m_applicationExiting)
        close();
}

void Rtmp::closeEvent(QCloseEvent *event)
{
    if (m_isCapturingImage) {
        setEnabled(false);
        m_applicationExiting = true;
        event->ignore();
    } else {
        event->accept();
    }
}

void Rtmp::updateCameras()
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

void Rtmp::showMetaDataDialog()
{
    if (!m_metaDataDialog)
        m_metaDataDialog = new MetaDataDialog(this);
    m_metaDataDialog->setAttribute(Qt::WA_DeleteOnClose, false);
    if (m_metaDataDialog->exec() == QDialog::Accepted)
        saveMetaData();
}

void Rtmp::saveMetaData()
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

void Rtmp::on_pushStream_clicked()
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

void Rtmp::setInfo(QString message)
{
    ui->textTerminal->append(message);
}

void Rtmp::setUrl(QString url)
{
    ui->labelRtmpUrl->setText(url);
}

void Rtmp::setConnectionStatus(bool status)
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

void Rtmp::on_pushExit_clicked()
{
    QApplication::quit();
}

