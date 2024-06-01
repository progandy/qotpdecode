#ifndef WEBCAMQRCODEWIDGET_H
#define WEBCAMQRCODEWIDGET_H

#include <QWidget>
#include <QCamera>
#include <QComboBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QVideoFrame>
#include <QList>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QCameraViewfinder>
#include <QVideoProbe>
#include <QCameraInfo>
#define CAM_INFO QCameraInfo
#else
#include <QVideoWidget>
#include <QMediaCaptureSession>
#include <QCameraDevice>
#define CAM_INFO QCameraDevice
#endif

#include "ZXingQt/ZXingQtReader.h"

Q_DECLARE_METATYPE(CAM_INFO);

class WebcamQRCodeWidget : public QWidget {
    Q_OBJECT

public:
    explicit WebcamQRCodeWidget(QWidget *parent = nullptr);
    ~WebcamQRCodeWidget();

signals:
    void qrCodeDetected(const QList<ZXingQt::Result> &data);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onCameraSelected(int index);
    void processFrame(const QVideoFrame &frame);

private:
    void setupUI();
    void populateCameraList();
    void startCamera(const CAM_INFO &cameraInfo);
    void setCameraResolution(QCamera *camera);

    QComboBox *cameraComboBox;
    QCamera *camera;
    
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCameraViewfinder *viewfinder;
    QVideoProbe *capture;
#else
    QVideoWidget *viewfinder;
    QMediaCaptureSession *capture;
#endif
    bool processingInProgress;
    QByteArray lastProcessedCodes;

    struct CameraInfoEx {
        CAM_INFO cameraInfo;
        QSize maxResolution;
        bool supportsMJPEG;
    };

    QList<CameraInfoEx> sortCameras(const QList<CAM_INFO> &cameras);
    CameraInfoEx getCameraInfoEx(const CAM_INFO &cameraInfo);
};

#endif // WEBCAMQRCODEWIDGET_H
