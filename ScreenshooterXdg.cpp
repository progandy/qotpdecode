#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QImage>
#include <QDebug>
#include <QUrl>
#include <QRandomGenerator>
#include "ScreenshooterXdg.h"

        QString ScreenshooterXdg::generateHandleToken()
        {
            // Generate a unique handle token (you can adjust this based on your requirements)
            QString tokenPrefix = "qotpdecoder_";
            QString randomNumber = QString::number(QRandomGenerator().generate());
            return tokenPrefix + randomNumber;
        }

    void ScreenshooterXdg::takeScreenshot()
    {
        QDBusInterface portalInterface("org.freedesktop.portal.Desktop",
                                    "/org/freedesktop/portal/desktop",
                                    "org.freedesktop.portal.Screenshot");

        if (!portalInterface.isValid()) {
            qWarning() << "Failed to connect to the Screenshot portal interface";
            return;
        }

        // Parameters for the Screenshot method call
        QString parentWindow = QStringLiteral("");
        QVariantMap options;
        options.insert("handle_token", generateHandleToken()); // Generate a unique token for the handle
        options.insert("interactive", true);

        QDBusMessage call = portalInterface.call("Screenshot", parentWindow, options);

        if (call.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "Failed to take screenshot:" << call.errorMessage();
            return;
        }

        // Retrieve the handle
        QString handle = call.arguments().at(0).toString();

        // Subscribe to the Response signal
        QDBusConnection::sessionBus().connect("org.freedesktop.portal.Desktop", handle,
                                            "org.freedesktop.portal.Request", "Response",
                                            this, SLOT(handleScreenshotResponse(uint,QVariantMap)));

        qDebug() << "Screenshot requested, waiting for response...";
    }
    
        void ScreenshooterXdg::handleScreenshotResponse(uint response, const QVariantMap &results) {
            if (response == 0) {
                QString uri = results.value("uri").toString();
                qDebug() << "Screenshot captured. URI:" << uri;

                // Load the screenshot into a QImage (you may need to adjust this based on the URI format)
                QImage screenshot(QUrl(uri).toLocalFile());
                if (screenshot.isNull()) {
                    qWarning() << "Failed to load screenshot into QImage";
                    return;
                }

                // Emit the screenshot captured signal
                emit screenshotCaptured(screenshot);
            } else {
                qWarning() << "Failed to capture screenshot. Response code:" << response;
            }
        }

