#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QImage>
#include <QDebug>

class ScreenshooterXdg : public QObject
{
    Q_OBJECT
    public:
        ScreenshooterXdg(QObject *parent = nullptr) : QObject(parent) {}

        QString generateHandleToken();
        void takeScreenshot();
    
    signals:
        void screenshotCaptured(const QImage &screenshot);

    private slots:
        void handleScreenshotResponse(uint response, const QVariantMap &results);
};
