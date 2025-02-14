#ifndef RTMP_H
#define RTMP_H

#include <QCamera>
#include <QImageCapture>
#include <QMediaRecorder>
#include <QScopedPointer>
#include <QMediaMetaData>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QAudioInput>
#include <QMediaPlayer>
#include <QMainWindow>
#include <QMediaFormat>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QTimer>
#include <fftw3.h>
#include "ffmpeg_rtmp.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Camera; }
class QActionGroup;
QT_END_NAMESPACE

#define DEFAULT_SAMPLE_RATE		44100
#define DEFAULT_FFT_SIZE        4096
#define RESET_FFT_FACTOR        -72.0f
#define REAL 0
#define IMAG 1

class MetaDataDialog;

class Rtmp : public QMainWindow
{
    Q_OBJECT

public:
    Rtmp();

public slots:
    void saveMetaData();

private slots:
    void setCamera(const QCameraDevice &cameraDevice);

    void startCamera();
    void stopCamera();

    void record();
    void pause();
    void stop();
    void setMuted(bool);

    void takeImage();
    void displayCaptureError(int, QImageCapture::Error, const QString &errorString);

    void configureCaptureSettings();
    void configureVideoSettings();
    void configureImageSettings();

    void displayRecorderError();
    void displayCameraError();

    void updateCameraDevice(QAction *action);

    void updateCameraActive(bool active);
    void updateCaptureMode();
    void updateRecorderState(QMediaRecorder::RecorderState state);
    void setExposureCompensation(int index);

    void updateRecordTime();

    void processCapturedImage(int requestId, const QImage &img);

    void displayViewfinder();
    void displayCapturedImage();

    void readyForCapture(bool ready);
    void imageSaved(int id, const QString &fileName);

    void updateCameras();

    void showMetaDataDialog();

    void setInfo(QString);
    void setUrl(QString);
    void setConnectionStatus(bool);
    void setVideoFrame(QImage);
    void setAudioFrame(const char*, int);

    void on_pushStream_clicked();
    void on_pushExit_clicked();
    void outputDeviceChanged(int index);

    void initSpectrumGraph();
    void runFFTW(float *buffer, int fftsize);
    void onSpectrumProcessed(int fftSize);

signals:
    void spectValueChanged(int fftSize);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent* event) override;
    void handleResizeEvent(QResizeEvent* event);

private:
    Ui::Camera *ui;

    ffmpeg_rtmp* m_ffmpeg_rtmp = nullptr;
    QActionGroup *videoDevicesGroup  = nullptr;
    QMediaDevices m_devices;
    QMediaCaptureSession m_captureSession;
    QScopedPointer<QCamera> m_camera;
    QScopedPointer<QAudioInput> m_audioInput;
    QImageCapture *m_imageCapture;
    QScopedPointer<QMediaRecorder> m_mediaRecorder;

    QGraphicsScene *scene;
    QGraphicsView *view;

    bool m_isCapturingImage = false;
    bool m_applicationExiting = false;
    bool m_doImageCapture = true;

    unsigned int sampleCount = 0;
    float   *d_realFftData;
    float   *d_iirFftData;
    float signalInput[DEFAULT_FFT_SIZE];
    float d_fftAvg;

    MetaDataDialog *m_metaDataDialog = nullptr;
};

#endif
