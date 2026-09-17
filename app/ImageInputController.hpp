#pragma once

#include "newconvert9918/imageio/ImageLoader.hpp"

#include <QObject>
#include <QString>
#include <QUrl>

class ImageInputController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString sourcePreview READ sourcePreview NOTIFY sourceChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY sourceChanged)
    Q_PROPERTY(QString sourceDetails READ sourceDetails NOTIFY sourceChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY sourceChanged)
    Q_PROPERTY(bool hasImage READ hasImage NOTIFY sourceChanged)

public:
    explicit ImageInputController(QObject* parent = nullptr);

    [[nodiscard]] QString sourcePreview() const { return sourcePreview_; }
    [[nodiscard]] QString sourceName() const { return sourceName_; }
    [[nodiscard]] QString sourceDetails() const { return sourceDetails_; }
    [[nodiscard]] QString errorMessage() const { return errorMessage_; }
    [[nodiscard]] bool hasImage() const { return image_.has_value(); }

    Q_INVOKABLE void openUrl(const QUrl& url);
    Q_INVOKABLE void pasteClipboard();

signals:
    void sourceChanged();

private:
    void accept(newconvert9918::imageio::ImageLoadResult result, QString sourceName);

    std::optional<newconvert9918::core::RgbImage> image_;
    QString sourcePreview_;
    QString sourceName_;
    QString sourceDetails_;
    QString errorMessage_;
};
