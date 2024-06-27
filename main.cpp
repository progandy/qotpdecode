/**
 *
 * qotpdecode - Decode QR Codes and URLs containing OTPAUTH information
 *
 * Copyright 2024 progandy
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <QApplication>
#include <QClipboard>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QMimeData>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTextEdit>
#include <QToolTip>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <Qt>

#include "ZXingQt/ZXingQtReader.h"
#include "ScreenshooterXdg.h"
#include "ScreenshooterX11.h"

#ifdef WITH_CAMERA
#include "WebcamQRCodeWidget.h"
#endif

using namespace ZXingQt;

class KeyValueItem : public QWidget {
  Q_OBJECT
public:
  KeyValueItem(const QString &key, const QString &value,
               QWidget *parent = nullptr)
      : QWidget(parent) {
    QHBoxLayout *layout = new QHBoxLayout(this);

    QLabel *keyLabel = new QLabel(key + ":", this);
    QLineEdit *valueLineEdit = new QLineEdit(value, this);
    valueLineEdit->setReadOnly(true);

    QPushButton *copyButton = new QPushButton(this);
    copyButton->setIcon(QIcon::fromTheme("edit-copy"));
    copyButton->setIconSize(QSize(16, 16));
    copyButton->setToolTip("Copy value to clipboard");

    connect(copyButton, &QPushButton::clicked,
            [=]() { QApplication::clipboard()->setText(value); });

    layout->addWidget(keyLabel);
    layout->addWidget(valueLineEdit);
    layout->addWidget(copyButton);
  }
};

class ImageDisplayWidget : public QWidget {
  Q_OBJECT
public:
  ImageDisplayWidget(QWidget *parent = nullptr) : QWidget(parent) {
    QHBoxLayout *layout = new QHBoxLayout(this);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    layout->addLayout(leftLayout);

    imageLabel = new QLabel(this);
    imageLabel->setFixedSize(256, 256);
    imageLabel->setText("Drop image here");
    imageLabel->setAlignment(Qt::AlignCenter);
    leftLayout->addWidget(imageLabel);
    
#ifdef WITH_CAMERA
    camera = new WebcamQRCodeWidget();
    camera->setVisible(false);
    leftLayout->addWidget(camera);
    QObject::connect(camera, &WebcamQRCodeWidget::qrCodeDetected, this, &ImageDisplayWidget::qrCodeDetected);
#endif

    QPushButton *openButton = new QPushButton("Open Image", this);
    leftLayout->addWidget(openButton);
    connect(openButton, &QPushButton::clicked, this,
            &ImageDisplayWidget::openImage);

    QPushButton *pasteButton = new QPushButton("Paste", this);
    leftLayout->addWidget(pasteButton);
    connect(pasteButton, &QPushButton::clicked, this,
            &ImageDisplayWidget::pasteImage);
    
    
    QPushButton *screenshotButton = new QPushButton("Screenshot", this);
    leftLayout->addWidget(screenshotButton);
    connect(screenshotButton, &QPushButton::clicked, this,
            &ImageDisplayWidget::makeScreenshot);

#ifdef WITH_CAMERA    
    QPushButton *cameraButton = new QPushButton("Camera", this);
    leftLayout->addWidget(cameraButton);
    connect(cameraButton, &QPushButton::clicked, this,
            &ImageDisplayWidget::startCamera);
#endif
    
    QVBoxLayout *rightLayout = new QVBoxLayout;
    layout->addLayout(rightLayout);
    

    resultTextEdit = new QTextEdit(this);
    resultTextEdit->setReadOnly(true);
    resultTextEdit->setPlaceholderText(
        "Decode a QR Code or otpauth:// URI containing OTP setup information");
    rightLayout->addWidget(resultTextEdit);

    otpauthLineEdit = new QLineEdit(this);
    otpauthLineEdit->setReadOnly(true);
    otpauthLineEdit->setVisible(false);
    rightLayout->addWidget(otpauthLineEdit);

    paramListWidget = new QListWidget(this);
    paramListWidget->setVisible(false);
    rightLayout->addWidget(paramListWidget);

    setAcceptDrops(true);
    
    // Connect to the screenshotCaptured signal
    QObject::connect(&screenshooterXdg, &ScreenshooterXdg::screenshotCaptured, this, &ImageDisplayWidget::capturedImage);
    
  }

protected:
  void dragEnterEvent(QDragEnterEvent *event) override {
    if (event->mimeData()->hasImage() || event->mimeData()->hasUrls() ||
        event->mimeData()->hasText()) {
      event->acceptProposedAction();
    }
  }

  void dropEvent(QDropEvent *event) override {
    QImage image;
    if (extractImageFromMimeData(event->mimeData(), image)) {
      displayImageFromImage(image);
      decodeBarcodes(image);
    } else if (event->mimeData()->hasText()) {
      QString droppedText = event->mimeData()->text();
      if (isOtpAuthUrl(droppedText)) {
        displayImageFromThemeIcon("text-x-generic");
        displayOtpAuthUrl(droppedText);
      } else {
        QString dataUrl = findDataUrl(droppedText);
        if (!dataUrl.isEmpty()) {
          displayImageFromDataUrl(dataUrl);
          decodeBarcodes(this->pixmap.toImage());
        }
      }
    }
    event->acceptProposedAction();
  }

private slots:
  void openImage() {
    QString filePath =
        QFileDialog::getOpenFileName(this, "Open Image", QString(),
                                     "Image Files (*.png *.jpg *.jpeg *.bmp)");
    if (!filePath.isEmpty()) {
      displayImageFromFile(filePath);
      decodeBarcodes(QImageReader(filePath).read());
    }
  }
  
  void makeScreenshot() {
    if (QGuiApplication::platformName() == "xcb") {
      QImage screenshot = ScreenshooterX11().captureScreenshot(this);
      displayImageFromImage(screenshot);
      decodeBarcodes(screenshot);
    } else {
      screenshooterXdg.takeScreenshot();
    }
  }
  
  void capturedImage(const QImage &screenshot) {
    displayImageFromImage(screenshot);
    decodeBarcodes(screenshot);
  }

  void pasteImage() {
    const QClipboard *clipboard = QApplication::clipboard();
    QImage image;
    if (extractImageFromMimeData(clipboard->mimeData(), image)) {
      displayImageFromImage(image);
      decodeBarcodes(image);
    } else {
      // Check if pasted text contains a data URL
      QString pastedText = clipboard->text();
      if (!pastedText.isEmpty()) {
        if (isOtpAuthUrl(pastedText)) {
          displayImageFromThemeIcon("text-x-generic");
          displayOtpAuthUrl(pastedText);
        } else {
          QString dataUrl = findDataUrl(pastedText);
          if (!dataUrl.isEmpty()) {
            displayImageFromDataUrl(dataUrl);
            decodeBarcodes(this->pixmap.toImage());
          }
        }
      }
    }
  }
  
  void startCamera() {
    imageLabel->setVisible(false);
    if (camera) {
      camera->setVisible(true);
    }
  }
  
  void qrCodeDetected(const QList<Result> &barcodes) {
    if (barcodes.size() == 1 && isOtpAuthUrl(barcodes[0].text())) {
      displayOtpAuthUrl(barcodes[0].text());
    } else {
      displayTextResult(barcodes);
    }
  }


private:
  bool extractImageFromMimeData(const QMimeData *mimeData, QImage &image) {
    if (mimeData->hasImage()) {
      image = qvariant_cast<QImage>(mimeData->imageData());
      return true;
    } else if (mimeData->hasUrls()) {
      QList<QUrl> urlList = mimeData->urls();
      foreach (const QUrl &url, urlList) {
        QString filePath = url.toLocalFile();
        if (!filePath.isEmpty()) {
          QPixmap pixmap(filePath);
          image = pixmap.toImage();
          return true;
        }
      }
    }
    return false;
  }

  QString findDataUrl(const QString &text) {
    QRegularExpression regex("data:image/[a-zA-Z]+;base64,[\\w\\d+/=\\s]+");
    QRegularExpressionMatch match = regex.match(text);
    if (match.hasMatch()) {
      return match.captured(0);
    }
    return QString();
  }

  void displayImageFromFile(const QString &filePath) {
    QPixmap pixmap(filePath);
    displayImageFromPixmap(pixmap);
  }

  void displayImageFromImage(const QImage &image) {
    QPixmap pixmap = QPixmap::fromImage(image);
    displayImageFromPixmap(pixmap);
  }

  void displayImageFromThemeIcon(const QString &name) {
    QPixmap pixmap = QIcon::fromTheme(name).pixmap(imageLabel->size());
    displayImageFromPixmap(pixmap);
  }

  void displayImageFromPixmap(const QPixmap &pixmap) {
    this->pixmap = pixmap;
    imageLabel->setPixmap(pixmap.scaled(imageLabel->size(), Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation));
    chooseImage();
  }

  void displayImageFromDataUrl(const QString &dataUrl) {
    QPixmap pixmap;
    pixmap.loadFromData(QByteArray::fromBase64(dataUrl.split(",")[1].toUtf8()),
                        "PNG");
    displayImageFromPixmap(pixmap);
  }

  void decodeBarcodes(const QImage &image) {
    auto options = ReaderOptions()
                       .setFormats(ZXing::BarcodeFormat::QRCode)
                       .setTryInvert(true)
                       .setTextMode(ZXing::TextMode::HRI)
                       .setMaxNumberOfSymbols(10);
    auto barcodes = ReadBarcodes(image, options);
    if (barcodes.size() == 1 && isOtpAuthUrl(barcodes[0].text())) {
      displayOtpAuthUrl(barcodes[0].text());
    } else {
      displayTextResult(barcodes);
    }
  }

  bool isOtpAuthUrl(const QString &text) {
    return text.startsWith("otpauth://");
  }

  void displayOtpAuthUrl(const QString &otpauthUrl) {
    QUrl url(otpauthUrl);

    // Extract type and label from the URL
    QString type = url.host();
    QString label = url.path().mid(1); // Remove leading '/'

    resultTextEdit->setVisible(false);
    otpauthLineEdit->setText(otpauthUrl);
    otpauthLineEdit->setVisible(true);
    paramListWidget->setVisible(true);

    QMap<QString, QString> paramMap;
    QUrlQuery query(url.query());
    for (const auto &item : query.queryItems()) {
      paramMap[item.first] = item.second;
    }

    QStringList allParams = {"issuer",  "secret", "algorithm", "digits",
                             "counter", "period", "image"};
    paramListWidget->clear();

    auto addParamItem = [this](const QString &key, const QString &value) {
      KeyValueItem *itemWidget = new KeyValueItem(key, value);
      QListWidgetItem *listItem = new QListWidgetItem(paramListWidget);
      listItem->setSizeHint(itemWidget->sizeHint());
      paramListWidget->addItem(listItem);
      paramListWidget->setItemWidget(listItem, itemWidget);
      listItem->setToolTip(tooltipForParameter(key));
    };

    addParamItem("type", type);
    addParamItem("label", label);

    for (const QString &param : allParams) {
      if (param == "counter" && type != "hotp" && !paramMap.contains(param)) {
        continue;
      } else if (param == "period" && type != "totp" &&
                 !paramMap.contains(param)) {
        continue;
      }
      QString value = paramMap.contains(param)
                          ? paramMap[param]
                          : defaultValueForParameter(param);
      addParamItem(param, value);
    }

    for (const auto &item : query.queryItems()) {
      QString param = item.first;
      if (!allParams.contains(param)) {
        addParamItem(param, item.second);
      }
    }
  }

  QString defaultValueForParameter(const QString &param) {
    if (param == "issuer" || param == "secret" || param == "image") {
      return "(empty)";
    } else if (param == "algorithm") {
      return "SHA1";
    } else if (param == "digits") {
      return "6";
    } else if (param == "period") {
      return "30";
    } else if (param == "counter") {
      return "0";
    }
    return QString();
  }
  QString tooltipForParameter(const QString &param) {
    if (param == "issuer") {
      return "The name of the provider. Default value: (empty)";
    } else if (param == "secret") {
      return "The shared secret key. Default value: (empty)";
    } else if (param == "algorithm") {
      return "The algorithm used for generating the one-time password. Default "
             "value: SHA1";
    } else if (param == "digits") {
      return "The number of digits in the one-time password. Default value: 6";
    } else if (param == "period") {
      return "The period of time in seconds for which the one-time password "
             "will be valid. Default value: 30";
    } else if (param == "counter") {
      return "The initial counter value for the HOTP algorithm. Default value: "
             "0";
    } else if (param == "image") {
      return "The URL of an image to be displayed as part of the account "
             "information. Default value: (empty)";
    }
    return QString();
  }

  void displayTextResult(const QList<Result> &barcodes) {
    QString resultText;
    for (const auto &result : barcodes) {
      resultText += result.text() + "\n";
    }
    otpauthLineEdit->setVisible(false);
    paramListWidget->setVisible(false);
    resultTextEdit->setText(resultText.trimmed());
    resultTextEdit->setVisible(true);
  }
    
  void chooseImage() {
    if (camera) {
      camera->setVisible(false);
    }
    imageLabel->setVisible(true);
  }

  QLabel *imageLabel;
  WebcamQRCodeWidget *camera;
  QLineEdit *otpauthLineEdit;
  QListWidget *paramListWidget;
  QTextEdit *resultTextEdit;
  QPixmap pixmap;
  ScreenshooterXdg screenshooterXdg;

};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  QMainWindow mainWindow;
  mainWindow.setAttribute(Qt::WA_X11NetWmWindowTypeDialog);
  mainWindow.resize(680, 450);
  ImageDisplayWidget *imageDisplayWidget = new ImageDisplayWidget(&mainWindow);
  mainWindow.setCentralWidget(imageDisplayWidget);
  mainWindow.setWindowTitle("OTPAuth Decoder");
  mainWindow.show();

  return app.exec();
}

#include "main.moc"
