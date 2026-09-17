#pragma once

#include "newconvert9918/core/ConversionJobController.hpp"
#include "newconvert9918/core/ImageTransform.hpp"
#include "newconvert9918/formats/Export.hpp"
#include "newconvert9918/imageio/ImageLoader.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <memory>
#include <optional>
#include <vector>

class ImageInputController final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString sourcePreview READ sourcePreview NOTIFY sourceChanged)
    Q_PROPERTY(QString convertedPreview READ convertedPreview NOTIFY conversionChanged)
    Q_PROPERTY(QString palettePreview READ palettePreview NOTIFY conversionChanged)
    Q_PROPERTY(QVariantList paletteColors READ paletteColors NOTIFY conversionChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY sourceChanged)
    Q_PROPERTY(QString sourceDetails READ sourceDetails NOTIFY sourceChanged)
    Q_PROPERTY(QString conversionDetails READ conversionDetails NOTIFY conversionChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY statusChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString outputSummary READ outputSummary NOTIFY exportChanged)
    Q_PROPERTY(QString overwriteMessage READ overwriteMessage NOTIFY exportChanged)
    Q_PROPERTY(bool hasImage READ hasImage NOTIFY sourceChanged)
    Q_PROPERTY(bool hasConversion READ hasConversion NOTIFY conversionChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY conversionChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY settingsChanged)
    Q_PROPERTY(bool scanlinePaletteAvailable READ scanlinePaletteAvailable NOTIFY conversionChanged)

    Q_PROPERTY(int conversionMode READ conversionMode WRITE setConversionMode NOTIFY settingsChanged)
    Q_PROPERTY(int ditherMode READ ditherMode WRITE setDitherMode NOTIFY settingsChanged)
    Q_PROPERTY(int scalingFilter READ scalingFilter WRITE setScalingFilter NOTIFY settingsChanged)
    Q_PROPERTY(int fillMode READ fillMode WRITE setFillMode NOTIFY settingsChanged)
    Q_PROPERTY(int exportFormat READ exportFormat WRITE setExportFormat NOTIFY exportChanged)
    Q_PROPERTY(bool perceptualColorMatching READ perceptualColorMatching WRITE
                   setPerceptualColorMatching NOTIFY settingsChanged)
    Q_PROPERTY(bool stretchHistogram READ stretchHistogram WRITE setStretchHistogram NOTIFY
                   settingsChanged)
    Q_PROPERTY(double maximumColorShift READ maximumColorShift WRITE setMaximumColorShift NOTIFY
                   settingsChanged)
    Q_PROPERTY(double gamma READ gamma WRITE setGamma NOTIFY settingsChanged)
    Q_PROPERTY(double lumaEmphasis READ lumaEmphasis WRITE setLumaEmphasis NOTIFY settingsChanged)
    Q_PROPERTY(int maximumMulticolorDifference READ maximumMulticolorDifference WRITE
                   setMaximumMulticolorDifference NOTIFY settingsChanged)
    Q_PROPERTY(int orderedBrightness READ orderedBrightness WRITE setOrderedBrightness NOTIFY
                   settingsChanged)
    Q_PROPERTY(int horizontalOffset READ horizontalOffset WRITE setHorizontalOffset NOTIFY
                   settingsChanged)
    Q_PROPERTY(int verticalOffset READ verticalOffset WRITE setVerticalOffset NOTIFY
                   settingsChanged)

public:
    explicit ImageInputController(QObject* parent = nullptr);
    ~ImageInputController() override;

    [[nodiscard]] QString sourcePreview() const { return sourcePreview_; }
    [[nodiscard]] QString convertedPreview() const { return convertedPreview_; }
    [[nodiscard]] QString palettePreview() const { return palettePreview_; }
    [[nodiscard]] QVariantList paletteColors() const { return paletteColors_; }
    [[nodiscard]] QString sourceName() const { return sourceName_; }
    [[nodiscard]] QString sourceDetails() const { return sourceDetails_; }
    [[nodiscard]] QString conversionDetails() const { return conversionDetails_; }
    [[nodiscard]] QString errorMessage() const { return errorMessage_; }
    [[nodiscard]] QString statusMessage() const { return statusMessage_; }
    [[nodiscard]] QString outputSummary() const { return outputSummary_; }
    [[nodiscard]] QString overwriteMessage() const { return overwriteMessage_; }
    [[nodiscard]] bool hasImage() const { return static_cast<bool>(image_); }
    [[nodiscard]] bool hasConversion() const;
    [[nodiscard]] bool busy() const { return busy_; }
    [[nodiscard]] bool canUndo() const { return !undoStack_.empty(); }
    [[nodiscard]] bool scanlinePaletteAvailable() const { return !palettePreview_.isEmpty(); }

    [[nodiscard]] int conversionMode() const;
    [[nodiscard]] int ditherMode() const;
    [[nodiscard]] int scalingFilter() const { return static_cast<int>(scalingFilter_); }
    [[nodiscard]] int fillMode() const { return static_cast<int>(fillMode_); }
    [[nodiscard]] int exportFormat() const { return exportFormat_; }
    [[nodiscard]] bool perceptualColorMatching() const
    {
        return settings_.perceptualColorMatching;
    }
    [[nodiscard]] bool stretchHistogram() const { return settings_.stretchHistogram; }
    [[nodiscard]] double maximumColorShift() const
    {
        return settings_.maximumColorShiftPercent;
    }
    [[nodiscard]] double gamma() const { return settings_.gamma; }
    [[nodiscard]] double lumaEmphasis() const { return settings_.lumaEmphasis; }
    [[nodiscard]] int maximumMulticolorDifference() const
    {
        return settings_.maximumMulticolorDifferencePercent;
    }
    [[nodiscard]] int orderedBrightness() const { return settings_.orderedDitherBrightness; }
    [[nodiscard]] int horizontalOffset() const { return horizontalOffset_; }
    [[nodiscard]] int verticalOffset() const { return verticalOffset_; }

    Q_INVOKABLE void openUrl(const QUrl& url);
    Q_INVOKABLE void pasteClipboard();
    Q_INVOKABLE void applyPreset(int presetIndex);
    Q_INVOKABLE void undoSettings();
    Q_INVOKABLE void resetSettings();
    Q_INVOKABLE void exportToDirectory(const QUrl& directory);
    Q_INVOKABLE void confirmOverwrite();
    Q_INVOKABLE void cancelOverwrite();

    void setConversionMode(int value);
    void setDitherMode(int value);
    void setScalingFilter(int value);
    void setFillMode(int value);
    void setExportFormat(int value);
    void setPerceptualColorMatching(bool value);
    void setStretchHistogram(bool value);
    void setMaximumColorShift(double value);
    void setGamma(double value);
    void setLumaEmphasis(double value);
    void setMaximumMulticolorDifference(int value);
    void setOrderedBrightness(int value);
    void setHorizontalOffset(int value);
    void setVerticalOffset(int value);

signals:
    void sourceChanged();
    void conversionChanged();
    void settingsChanged();
    void statusChanged();
    void exportChanged();

private:
    struct SettingsSnapshot {
        newconvert9918::core::ConversionSettings settings;
        newconvert9918::core::ScalingFilter scalingFilter;
        newconvert9918::core::ImageFillMode fillMode;
        int horizontalOffset{};
        int verticalOffset{};
    };

    void accept(newconvert9918::imageio::ImageLoadResult result, QString sourceName);
    void scheduleConversion();
    void startConversion();
    void publishConversion(const newconvert9918::core::ConversionRequest& request,
                           newconvert9918::core::ConversionResult result);
    void updatePaletteInspection();
    void updateExportSummary();
    void recordUndo();
    void applySnapshot(const SettingsSnapshot& snapshot);
    [[nodiscard]] SettingsSnapshot snapshot() const;
    void loadSettings();
    void saveSettings() const;
    void settingsWereChanged();
    [[nodiscard]] newconvert9918::formats::GeneratedFileManifest exportManifest() const;
    [[nodiscard]] QString exportBaseName() const;

    std::shared_ptr<newconvert9918::core::RgbImage> image_;
    std::optional<newconvert9918::core::ConversionResult> result_;
    newconvert9918::core::ConversionSettings settings_;
    newconvert9918::core::ScalingFilter scalingFilter_{
        newconvert9918::core::ScalingFilter::Bilinear};
    newconvert9918::core::ImageFillMode fillMode_{newconvert9918::core::ImageFillMode::Fit};
    int horizontalOffset_{};
    int verticalOffset_{};
    int exportFormat_{};
    bool busy_{};
    bool applyingSnapshot_{};

    newconvert9918::core::ConversionJobController jobs_;
    QTimer debounceTimer_;
    QTimer undoCoalesceTimer_;
    std::vector<SettingsSnapshot> undoStack_;

    QString sourcePreview_;
    QString convertedPreview_;
    QString palettePreview_;
    QVariantList paletteColors_;
    QString sourceName_;
    QString sourceDetails_;
    QString conversionDetails_;
    QString errorMessage_;
    QString statusMessage_;
    QString outputSummary_;
    QString overwriteMessage_;
    QString pendingExportDirectory_;
    std::optional<newconvert9918::formats::GeneratedFileManifest> pendingManifest_;
};
