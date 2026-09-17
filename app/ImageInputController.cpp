#include "ImageInputController.hpp"

#include <QBuffer>
#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>

ImageInputController::ImageInputController(QObject* parent)
    : QObject(parent)
{
}

void ImageInputController::accept(newconvert9918::imageio::ImageLoadResult result,
                                  QString sourceName)
{
    if (!result) {
        errorMessage_ = result.error;
        emit sourceChanged();
        return;
    }

    QImage preview = newconvert9918::imageio::toQImage(*result.image);
    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);
    if (!preview.save(&buffer, "PNG")) {
        errorMessage_ = tr("The decoded image could not be prepared for display.");
        emit sourceChanged();
        return;
    }

    image_ = std::move(result.image);
    sourcePreview_ = QStringLiteral("data:image/png;base64,")
        + QString::fromLatin1(encoded.toBase64());
    sourceName_ = std::move(sourceName);
    sourceDetails_ = tr("%1 — %2×%3 — %4%5")
        .arg(result.metadata.formatName)
        .arg(image_->width())
        .arg(image_->height())
        .arg(result.metadata.colorSpace)
        .arg(result.metadata.animated
                 ? tr(" — first of %1 frames").arg(result.metadata.frameCount)
                 : QString{});
    errorMessage_.clear();
    emit sourceChanged();
}

void ImageInputController::openUrl(const QUrl& url)
{
    if (!url.isLocalFile()) {
        errorMessage_ = tr("Only local image files can be opened.");
        emit sourceChanged();
        return;
    }
    const QString path = url.toLocalFile();
    accept(newconvert9918::imageio::loadImageFile(path), QFileInfo(path).fileName());
}

void ImageInputController::pasteClipboard()
{
    const QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr || clipboard->mimeData() == nullptr
        || !clipboard->mimeData()->hasImage()) {
        errorMessage_ = tr("The clipboard does not contain an image.");
        emit sourceChanged();
        return;
    }
    accept(newconvert9918::imageio::loadClipboardImage(clipboard->image()),
           tr("Clipboard image"));
}
