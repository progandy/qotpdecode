#include "WebcamQRCodeWidget.h"
#include <QVideoFrame>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QCameraInfo>
#else
#include <QMediaDevices>
#endif
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <QtConcurrent>

using namespace ZXingQt;

WebcamQRCodeWidget::WebcamQRCodeWidget(QWidget *parent)
    : QWidget(parent), camera(nullptr), processingInProgress(false) {
    setupUI();
    populateCameraList();
}

WebcamQRCodeWidget::~WebcamQRCodeWidget() {
    if (camera) {
        camera->stop();
        delete camera;
    }
}

void WebcamQRCodeWidget::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    viewfinder = new QCameraViewfinder();
    capture = new QVideoProbe(this);
#else
    viewfinder = new QVideoWidget();
    viewfinder->setFixedSize(256,256);
    capture = new QMediaCaptureSession();
#endif
    layout->addWidget(viewfinder);

    cameraComboBox = new QComboBox(this);
    layout->addWidget(cameraComboBox);

    connect(cameraComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &WebcamQRCodeWidget::onCameraSelected);
}

void WebcamQRCodeWidget::populateCameraList() {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QList<CAM_INFO> cameras = QCameraInfo::availableCameras();
#else
    QList<CAM_INFO> cameras = QMediaDevices::videoInputs();
#endif
    QList<CameraInfoEx> sortedCameras = sortCameras(cameras);

    for (const CameraInfoEx &cameraInfoEx : sortedCameras) {
        cameraComboBox->addItem(cameraInfoEx.cameraInfo.description(), QVariant::fromValue(cameraInfoEx.cameraInfo));
    }
    if (!sortedCameras.isEmpty()) {
        cameraComboBox->setCurrentIndex(0);
        onCameraSelected(0);
    }
}

QList<WebcamQRCodeWidget::CameraInfoEx> WebcamQRCodeWidget::sortCameras(const QList<CAM_INFO> &cameras) {
    QList<CameraInfoEx> cameraInfos;
    for (const CAM_INFO &cameraInfo : cameras) {
        cameraInfos.append(getCameraInfoEx(cameraInfo));
    }

    std::sort(cameraInfos.begin(), cameraInfos.end(), [](const CameraInfoEx &a, const CameraInfoEx &b) {
        if (a.maxResolution != b.maxResolution) {
            return a.maxResolution.width() * a.maxResolution.height() > b.maxResolution.width() * b.maxResolution.height();
        }
        return a.supportsMJPEG && !b.supportsMJPEG;
    });

    return cameraInfos;
}

WebcamQRCodeWidget::CameraInfoEx WebcamQRCodeWidget::getCameraInfoEx(const CAM_INFO &cameraInfo) {
    CameraInfoEx cameraInfoEx = {cameraInfo, QSize(0, 0), false};

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCamera camera(cameraInfo);
    QList<QSize> resolutions = camera.supportedViewfinderResolutions();
    for (const QSize &resolution : resolutions) {
        if (resolution.width() * resolution.height() > cameraInfoEx.maxResolution.width() * cameraInfoEx.maxResolution.height()) {
            cameraInfoEx.maxResolution = resolution;
        }
    }

    QList<QVideoFrame::PixelFormat> formats = camera.supportedViewfinderPixelFormats();
    for (const QVideoFrame::PixelFormat &format : formats) {
        if (format == QVideoFrame::Format_Jpeg) {
            cameraInfoEx.supportsMJPEG = true;
        }
    }
#else
    for (const QCameraFormat &format : cameraInfo.videoFormats()) {
        QSize resolution = format.resolution();
        if (resolution.width() * resolution.height() > cameraInfoEx.maxResolution.width() * cameraInfoEx.maxResolution.height()) {
            cameraInfoEx.maxResolution = resolution;
        }
        if (format.pixelFormat() == QVideoFrameFormat::Format_Jpeg) {
            cameraInfoEx.supportsMJPEG = true;
        }
    }
#endif
    return cameraInfoEx;
}

void WebcamQRCodeWidget::onCameraSelected(int index) {
    CAM_INFO cameraInfo = cameraComboBox->itemData(index).value<CAM_INFO>();
    startCamera(cameraInfo);
}

void WebcamQRCodeWidget::startCamera(const CAM_INFO &cameraInfo) {
    if (!this->isVisible()) {
        return;
    }
    if (camera) {
        camera->stop();
        delete camera;
        camera = nullptr;
    }
    camera = new QCamera(cameraInfo, this);

    setCameraResolution(camera);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    capture->setSource(camera);
    camera->setViewfinder(viewfinder);
    connect(capture, &QVideoProbe::videoFrameProbed, this, &WebcamQRCodeWidget::processFrame);
#else
    capture->setCamera(camera);
    capture->setVideoOutput(viewfinder);
    connect(viewfinder->videoSink(), &QVideoSink::videoFrameChanged, this, &WebcamQRCodeWidget::processFrame);
#endif

    
    camera->start();
}

void WebcamQRCodeWidget::setCameraResolution(QCamera *camera) {
    return;
    /*
    QList<QSize> resolutions = camera->supportedViewfinderResolutions();
    QSize maxResolution(0, 0);

    for (const QSize &resolution : resolutions) {
        if (resolution.width() * resolution.height() > maxResolution.width() * maxResolution.height()) {
            maxResolution = resolution;
        }
    }

    QCameraViewfinderSettings settings;
    settings.setResolution(maxResolution);
    settings.setPixelFormat(QVideoFrame::Format_Jpeg); // Prefer MJPEG if available
    camera->setViewfinderSettings(settings);
    */
}

void WebcamQRCodeWidget::processFrame(const QVideoFrame &frame) {
    // only start qr detection when idle and the frame is valid.
    if (processingInProgress || !frame.isValid()) {
        return;
    }

    // Run qr detection in background to keep framerate up.
    processingInProgress = true;
    static_cast<void>(QtConcurrent::run([this, frame]() {
        QList<Result> results = ReadBarcodes(frame);
        if (!results.empty()) {
            emit qrCodeDetected(results);
            /*
            QByteArray newData;
            for (auto result: results) {
                newData.append("||");
                if (result.isValid()) {
                    newData.append(result.bytes());
                } else {
                    newData.append("##");
                }
            }
            
            if (lastProcessedCodes.size() != newData.size() || newData.compare(lastProcessedCodes) != 0) {
                lastProcessedCodes = newData;
                emit qrCodeDetected(results);
            }
            */
        }
        processingInProgress = false;
    }));
}


void WebcamQRCodeWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    int index = cameraComboBox->currentIndex();
    if (index >= 0) {
        CAM_INFO cameraInfo = cameraComboBox->itemData(index).value<CAM_INFO>();
        startCamera(cameraInfo);
    }
}

void WebcamQRCodeWidget::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    if (camera) {
        camera->stop();
        delete camera;
        camera = NULL;
    }
}
