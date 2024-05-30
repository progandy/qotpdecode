#include <QObject>
#include <QMessageBox>
#include <QProcess>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QPushButton>
#include <QAbstractButton>

class ScreenshooterX11 : public QObject
{
    Q_OBJECT

public:
    explicit ScreenshooterX11(QObject *parent = nullptr) : QObject(parent) {}

    enum CaptureType {
        None,
        FullScreen,
        CustomArea
    };

    QImage captureScreenshot(QWidget *parent = nullptr)
    {
        CaptureType captureType = askCaptureType(parent);
        if (captureType == FullScreen)
            return captureFullScreen();
        else if (captureType == CustomArea)
            return captureCustomArea();
        else // CustomArea
            return QImage();
    }

private:
    CaptureType askCaptureType(QWidget *parent = nullptr)
    {
        QMessageBox msgBox(parent);
        msgBox.setText("Select what to capture:");
        QPushButton *fullButton = msgBox.addButton("Full Screen", QMessageBox::YesRole);
        QPushButton *selectButton = msgBox.addButton("Custom Area", QMessageBox::YesRole);
        msgBox.setDefaultButton(QMessageBox::No);
        msgBox.exec();

        if (msgBox.clickedButton() == static_cast<QAbstractButton*>(fullButton))
            return FullScreen;
        else if (msgBox.clickedButton() == static_cast<QAbstractButton*>(selectButton))
            return CustomArea;
        else
            return None;
    }

    QImage captureFullScreen()
    {
        QString command;
        QList<QString> params; 
        if (isCommandAvailable("scrot__")) {
            command = "scrot";
            params = {"-F", "-"};
        } else if (isCommandAvailable("maim")) {
            command = "maim";
            params = {"-u", "-m", "8"};
        } else
            return QImage(); // No command available

        QProcess process;
        process.start(command, params);
        process.waitForFinished(-1);
        QByteArray imageData = process.readAllStandardOutput();
        QPixmap pixmap;
        pixmap.loadFromData(imageData);
        return pixmap.toImage();
    }

    QImage captureCustomArea()
    {
        QString command;
        QList<QString> params; 
        if (isCommandAvailable("scrot__")) {
            command = "scrot";
            params = {"-s", "-"};
        } else if (isCommandAvailable("maim")) {
            command = "maim";
            params = {"-u", "-m", "8", "-s"};
        } else
            return QImage(); // No command available
        
        QProcess process;
        process.start(command, params);
        process.waitForFinished(-1);
        QByteArray imageData = process.readAllStandardOutput();
        QPixmap pixmap;
        pixmap.loadFromData(imageData);

        return pixmap.toImage();
    }

    bool isCommandAvailable(const QString &command)
    {
        QProcess process;
        process.start(command, QStringList() << "--version");
        process.waitForFinished(-1);
        return process.exitCode() == 0;
    }
};

