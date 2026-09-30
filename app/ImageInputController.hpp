#pragma once

#include "retrovdp/core/ConversionJobController.hpp"
#include "retrovdp/core/ImageTransform.hpp"
#include "retrovdp/core/TargetProfile.hpp"
#include "retrovdp/formats/Export.hpp"
#include "retrovdp/imageio/ImageLoader.hpp"

#include <QObject>
#include <QColor>
#include <QJsonObject>
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
    Q_PROPERTY(bool canReload READ canReload NOTIFY sourceChanged)
    Q_PROPERTY(bool hasConversion READ hasConversion NOTIFY conversionChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY conversionChanged)
    Q_PROPERTY(bool conversionPending READ conversionPending NOTIFY conversionChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY settingsChanged)
    Q_PROPERTY(bool canUndoDrawing READ canUndoDrawing NOTIFY drawingHistoryChanged)
    Q_PROPERTY(bool canRedoDrawing READ canRedoDrawing NOTIFY drawingHistoryChanged)
    Q_PROPERTY(bool canUndoScreenImage READ canUndoScreenImage NOTIFY screenImageHistoryChanged)
    Q_PROPERTY(bool canRedoScreenImage READ canRedoScreenImage NOTIFY screenImageHistoryChanged)
    Q_PROPERTY(bool screenImageEdited READ screenImageEdited NOTIFY conversionChanged)
    Q_PROPERTY(bool hasScreenImageSelection READ hasScreenImageSelection NOTIFY
                   screenImageSelectionChanged)
    Q_PROPERTY(int screenImageSelectionX READ screenImageSelectionX NOTIFY
                   screenImageSelectionChanged)
    Q_PROPERTY(int screenImageSelectionY READ screenImageSelectionY NOTIFY
                   screenImageSelectionChanged)
    Q_PROPERTY(int screenImageSelectionWidth READ screenImageSelectionWidth NOTIFY
                   screenImageSelectionChanged)
    Q_PROPERTY(int screenImageSelectionHeight READ screenImageSelectionHeight NOTIFY
                   screenImageSelectionChanged)
    Q_PROPERTY(bool screenImageFloating READ screenImageFloating NOTIFY screenImageFloatingChanged)
    Q_PROPERTY(bool screenImageFloatingMove READ screenImageFloatingMove NOTIFY
                   screenImageFloatingChanged)
    Q_PROPERTY(int screenImageFloatingWidth READ screenImageFloatingWidth NOTIFY
                   screenImageFloatingChanged)
    Q_PROPERTY(int screenImageFloatingHeight READ screenImageFloatingHeight NOTIFY
                   screenImageFloatingChanged)
    Q_PROPERTY(QString screenImageFloatingPreview READ screenImageFloatingPreview NOTIFY
                   screenImageFloatingChanged)
    Q_PROPERTY(bool screenImageFloatingTopLeft READ screenImageFloatingTopLeft NOTIFY
                   screenImageFloatingChanged)
    Q_PROPERTY(QVariantList screenImageFonts READ screenImageFonts NOTIFY screenImageFontsChanged)
    Q_PROPERTY(QString tiArtistLibraryPath READ tiArtistLibraryPath CONSTANT)
    Q_PROPERTY(QString tiArtistFontsPath READ tiArtistFontsPath CONSTANT)
    Q_PROPERTY(QString screenImageClipArtSourcePreview READ screenImageClipArtSourcePreview NOTIFY
                   screenImageClipArtChanged)
    Q_PROPERTY(QString screenImageClipArtSourceName READ screenImageClipArtSourceName NOTIFY
                   screenImageClipArtChanged)
    Q_PROPERTY(int screenImageClipArtSourceWidth READ screenImageClipArtSourceWidth NOTIFY
                   screenImageClipArtChanged)
    Q_PROPERTY(int screenImageClipArtSourceHeight READ screenImageClipArtSourceHeight NOTIFY
                   screenImageClipArtChanged)
    Q_PROPERTY(bool scanlinePaletteAvailable READ scanlinePaletteAvailable NOTIFY conversionChanged)
    Q_PROPERTY(bool autoUpdate READ autoUpdate WRITE setAutoUpdate NOTIFY settingsChanged)
    Q_PROPERTY(bool livePreview READ livePreview WRITE setLivePreview NOTIFY settingsChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY
                   settingsChanged)
    Q_PROPERTY(QColor foregroundColor READ foregroundColor WRITE setForegroundColor NOTIFY
                   settingsChanged)
    Q_PROPERTY(QColor screenImageBackgroundColor READ screenImageBackgroundColor WRITE
                   setScreenImageBackgroundColor NOTIFY screenImageColorsChanged)
    Q_PROPERTY(QColor screenImageForegroundColor READ screenImageForegroundColor WRITE
                   setScreenImageForegroundColor NOTIFY screenImageColorsChanged)
    Q_PROPERTY(QVariantList backgroundPaletteColors READ backgroundPaletteColors NOTIFY
                   settingsChanged)
    Q_PROPERTY(QVariantList workingPaletteColors READ workingPaletteColors NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList sourceSpectrum16Colors READ sourceSpectrum16Colors NOTIFY
                   sourceColorsChanged)
    Q_PROPERTY(QVariantList sourceSpectrum32Colors READ sourceSpectrum32Colors NOTIFY
                   sourceColorsChanged)
    Q_PROPERTY(QVariantList sourceSpectrum64Colors READ sourceSpectrum64Colors NOTIFY
                   sourceColorsChanged)
    Q_PROPERTY(QVariantList sourceSwatchColors READ sourceSwatchColors NOTIFY sourceColorsChanged)
    Q_PROPERTY(QVariantList sourceUsedColors READ sourceUsedColors NOTIFY sourceColorsChanged)

    Q_PROPERTY(int targetProfile READ targetProfile WRITE setTargetProfile NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList targetProfileNames READ targetProfileNames CONSTANT)
    Q_PROPERTY(QVariantList availableConversionModeNames READ availableConversionModeNames NOTIFY
                   settingsChanged)
    Q_PROPERTY(QVariantList availableConversionModeValues READ availableConversionModeValues NOTIFY
                   settingsChanged)
    Q_PROPERTY(int conversionMode READ conversionMode WRITE setConversionMode NOTIFY settingsChanged)
    Q_PROPERTY(int ditherMode READ ditherMode WRITE setDitherMode NOTIFY settingsChanged)
    Q_PROPERTY(int scalingFilter READ scalingFilter WRITE setScalingFilter NOTIFY settingsChanged)
    Q_PROPERTY(int fillMode READ fillMode WRITE setFillMode NOTIFY settingsChanged)
    Q_PROPERTY(int exportFormat READ exportFormat WRITE setExportFormat NOTIFY exportChanged)
    Q_PROPERTY(bool perceptualColorMatching READ perceptualColorMatching WRITE
                   setPerceptualColorMatching NOTIFY settingsChanged)
    Q_PROPERTY(bool stretchHistogram READ stretchHistogram WRITE setStretchHistogram NOTIFY
                   settingsChanged)
    Q_PROPERTY(int perceptualRedWeight READ perceptualRedWeight WRITE setPerceptualRedWeight NOTIFY
                   settingsChanged)
    Q_PROPERTY(int perceptualGreenWeight READ perceptualGreenWeight WRITE setPerceptualGreenWeight
                   NOTIFY settingsChanged)
    Q_PROPERTY(int perceptualBlueWeight READ perceptualBlueWeight WRITE setPerceptualBlueWeight NOTIFY
                   settingsChanged)
    Q_PROPERTY(double maximumColorShift READ maximumColorShift WRITE setMaximumColorShift NOTIFY
                   settingsChanged)
    Q_PROPERTY(double gamma READ gamma WRITE setGamma NOTIFY settingsChanged)
    Q_PROPERTY(double lumaEmphasis READ lumaEmphasis WRITE setLumaEmphasis NOTIFY settingsChanged)
    Q_PROPERTY(int maximumMulticolorDifference READ maximumMulticolorDifference WRITE
                   setMaximumMulticolorDifference NOTIFY settingsChanged)
    Q_PROPERTY(int orderedBrightness READ orderedBrightness WRITE setOrderedBrightness NOTIFY
                   settingsChanged)
    Q_PROPERTY(int errorAccumulationMode READ errorAccumulationMode WRITE
                   setErrorAccumulationMode NOTIFY settingsChanged)
    Q_PROPERTY(int orderedDitherMapSize READ orderedDitherMapSize WRITE
                   setOrderedDitherMapSize NOTIFY settingsChanged)
    Q_PROPERTY(int errorDownLeft READ errorDownLeft WRITE setErrorDownLeft NOTIFY settingsChanged)
    Q_PROPERTY(int errorDown READ errorDown WRITE setErrorDown NOTIFY settingsChanged)
    Q_PROPERTY(int errorDownRight READ errorDownRight WRITE setErrorDownRight NOTIFY settingsChanged)
    Q_PROPERTY(int errorRight READ errorRight WRITE setErrorRight NOTIFY settingsChanged)
    Q_PROPERTY(int errorFarRight READ errorFarRight WRITE setErrorFarRight NOTIFY settingsChanged)
    Q_PROPERTY(int errorDownTwo READ errorDownTwo WRITE setErrorDownTwo NOTIFY settingsChanged)
    Q_PROPERTY(int paletteSelectionMode READ paletteSelectionMode WRITE setPaletteSelectionMode
                   NOTIFY settingsChanged)
    Q_PROPERTY(int scanlineStaticColorCount READ scanlineStaticColorCount WRITE
                   setScanlineStaticColorCount NOTIFY settingsChanged)
    Q_PROPERTY(bool scanlineRegion1 READ scanlineRegion1 WRITE setScanlineRegion1 NOTIFY
                   settingsChanged)
    Q_PROPERTY(bool scanlineRegion2 READ scanlineRegion2 WRITE setScanlineRegion2 NOTIFY
                   settingsChanged)
    Q_PROPERTY(bool scanlineRegion3 READ scanlineRegion3 WRITE setScanlineRegion3 NOTIFY
                   settingsChanged)
    Q_PROPERTY(bool powerPaintFraming READ powerPaintFraming WRITE setPowerPaintFraming NOTIFY
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
    [[nodiscard]] QString sourcePath() const { return sourcePath_; }
    [[nodiscard]] QString sourceDetails() const { return sourceDetails_; }
    [[nodiscard]] QString conversionDetails() const { return conversionDetails_; }
    [[nodiscard]] QString errorMessage() const { return errorMessage_; }
    [[nodiscard]] QString statusMessage() const { return statusMessage_; }
    [[nodiscard]] QString outputSummary() const { return outputSummary_; }
    [[nodiscard]] QString overwriteMessage() const { return overwriteMessage_; }
    [[nodiscard]] bool hasImage() const { return static_cast<bool>(image_); }
    [[nodiscard]] bool canReload() const { return !sourcePath_.isEmpty(); }
    [[nodiscard]] bool hasConversion() const;
    [[nodiscard]] bool busy() const { return busy_; }
    [[nodiscard]] bool conversionPending() const { return conversionPending_; }
    [[nodiscard]] bool canUndo() const { return !undoStack_.empty(); }
    [[nodiscard]] bool canUndoDrawing() const { return !drawingUndoStack_.empty(); }
    [[nodiscard]] bool canRedoDrawing() const { return !drawingRedoStack_.empty(); }
    [[nodiscard]] bool canUndoScreenImage() const { return !screenImageUndoStack_.empty(); }
    [[nodiscard]] bool canRedoScreenImage() const { return !screenImageRedoStack_.empty(); }
    [[nodiscard]] bool screenImageEdited() const { return screenImageEdited_; }
    [[nodiscard]] bool hasScreenImageSelection() const
    {
        return screenImageSelectionWidth_ > 0 && screenImageSelectionHeight_ > 0;
    }
    [[nodiscard]] int screenImageSelectionX() const { return screenImageSelectionX_; }
    [[nodiscard]] int screenImageSelectionY() const { return screenImageSelectionY_; }
    [[nodiscard]] int screenImageSelectionWidth() const { return screenImageSelectionWidth_; }
    [[nodiscard]] int screenImageSelectionHeight() const { return screenImageSelectionHeight_; }
    [[nodiscard]] bool screenImageFloating() const
    {
        return static_cast<bool>(screenImageFloatingImage_);
    }
    [[nodiscard]] bool screenImageFloatingMove() const { return screenImageFloatingMove_; }
    [[nodiscard]] int screenImageFloatingWidth() const
    {
        return screenImageFloatingImage_
            ? static_cast<int>(screenImageFloatingImage_->width()) : 0;
    }
    [[nodiscard]] int screenImageFloatingHeight() const
    {
        return screenImageFloatingImage_
            ? static_cast<int>(screenImageFloatingImage_->height()) : 0;
    }
    [[nodiscard]] QString screenImageFloatingPreview() const
    {
        return screenImageFloatingPreview_;
    }
    [[nodiscard]] bool screenImageFloatingTopLeft() const
    {
        return screenImageFloatingTopLeft_;
    }
    [[nodiscard]] QVariantList screenImageFonts() const { return screenImageFonts_; }
    [[nodiscard]] QString tiArtistLibraryPath() const { return tiArtistLibraryPath_; }
    [[nodiscard]] QString tiArtistFontsPath() const { return tiArtistFontsPath_; }
    [[nodiscard]] QString screenImageClipArtSourcePreview() const
    {
        return screenImageClipArtSourcePreview_;
    }
    [[nodiscard]] QString screenImageClipArtSourceName() const
    {
        return screenImageClipArtSourceName_;
    }
    [[nodiscard]] int screenImageClipArtSourceWidth() const
    {
        return screenImageClipArtSourceImage_
            ? static_cast<int>(screenImageClipArtSourceImage_->width()) : 0;
    }
    [[nodiscard]] int screenImageClipArtSourceHeight() const
    {
        return screenImageClipArtSourceImage_
            ? static_cast<int>(screenImageClipArtSourceImage_->height()) : 0;
    }
    [[nodiscard]] bool scanlinePaletteAvailable() const { return !palettePreview_.isEmpty(); }
    [[nodiscard]] bool autoUpdate() const { return autoUpdate_; }
    [[nodiscard]] bool livePreview() const { return livePreview_; }
    [[nodiscard]] QColor backgroundColor() const;
    [[nodiscard]] QColor foregroundColor() const;
    [[nodiscard]] QColor screenImageBackgroundColor() const;
    [[nodiscard]] QColor screenImageForegroundColor() const;
    [[nodiscard]] QVariantList backgroundPaletteColors() const;
    [[nodiscard]] QVariantList workingPaletteColors() const;
    [[nodiscard]] QVariantList sourceSpectrum16Colors() const { return sourceSpectrum16Colors_; }
    [[nodiscard]] QVariantList sourceSpectrum32Colors() const { return sourceSpectrum32Colors_; }
    [[nodiscard]] QVariantList sourceSpectrum64Colors() const { return sourceSpectrum64Colors_; }
    [[nodiscard]] QVariantList sourceSwatchColors() const { return sourceSwatchColors_; }
    [[nodiscard]] QVariantList sourceUsedColors() const { return sourceUsedColors_; }

    [[nodiscard]] int targetProfile() const;
    [[nodiscard]] QVariantList targetProfileNames() const;
    [[nodiscard]] QVariantList availableConversionModeNames() const;
    [[nodiscard]] QVariantList availableConversionModeValues() const;
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
    [[nodiscard]] int perceptualRedWeight() const;
    [[nodiscard]] int perceptualGreenWeight() const;
    [[nodiscard]] int perceptualBlueWeight() const;
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
    [[nodiscard]] int errorAccumulationMode() const
    {
        return static_cast<int>(settings_.errorAccumulation);
    }
    [[nodiscard]] int orderedDitherMapSize() const
    {
        return static_cast<int>(settings_.orderedDitherMapSize);
    }
    [[nodiscard]] int errorDownLeft() const { return settings_.errorDistribution.downLeft; }
    [[nodiscard]] int errorDown() const { return settings_.errorDistribution.down; }
    [[nodiscard]] int errorDownRight() const { return settings_.errorDistribution.downRight; }
    [[nodiscard]] int errorRight() const { return settings_.errorDistribution.right; }
    [[nodiscard]] int errorFarRight() const { return settings_.errorDistribution.farRight; }
    [[nodiscard]] int errorDownTwo() const { return settings_.errorDistribution.downTwo; }
    [[nodiscard]] int paletteSelectionMode() const
    {
        return static_cast<int>(settings_.paletteSelection);
    }
    [[nodiscard]] int scanlineStaticColorCount() const
    {
        return settings_.scanlineStaticColorCount;
    }
    [[nodiscard]] bool scanlineRegion1() const { return settings_.scanlineRegion1; }
    [[nodiscard]] bool scanlineRegion2() const { return settings_.scanlineRegion2; }
    [[nodiscard]] bool scanlineRegion3() const { return settings_.scanlineRegion3; }
    [[nodiscard]] bool powerPaintFraming() const { return powerPaintFraming_; }
    [[nodiscard]] int horizontalOffset() const { return horizontalOffset_; }
    [[nodiscard]] int verticalOffset() const { return verticalOffset_; }

    Q_INVOKABLE void openUrl(const QUrl& url);
    Q_INVOKABLE void newScreenImage();
    Q_INVOKABLE void reloadSource();
    Q_INVOKABLE void pasteClipboard();
    Q_INVOKABLE void applyPreset(int presetIndex);
    Q_INVOKABLE void undoSettings();
    Q_INVOKABLE void resetSettings();
    Q_INVOKABLE void exportToDirectory(const QUrl& directory);
    Q_INVOKABLE void confirmOverwrite();
    Q_INVOKABLE void cancelOverwrite();
    Q_INVOKABLE void updateConversion();
    Q_INVOKABLE void nudgeSource(int horizontal, int vertical);
    Q_INVOKABLE void centerSource();
    Q_INVOKABLE void pickBackgroundColor(double normalizedX, double normalizedY);
    Q_INVOKABLE void pickColor(double normalizedX, double normalizedY, bool foreground);
    Q_INVOKABLE void swapSourceColors(double normalizedX, double normalizedY);
    Q_INVOKABLE void beginSourceStroke(double normalizedX,
                                       double normalizedY,
                                       int diameter,
                                       bool eraser,
                                       bool hardEdges = false,
                                       bool squareBrush = false);
    Q_INVOKABLE void continueSourceStroke(double normalizedX, double normalizedY);
    Q_INVOKABLE void continueSourceRay(double normalizedX, double normalizedY);
    Q_INVOKABLE void endSourceStroke();
    Q_INVOKABLE void drawSourceShape(double fromNormalizedX,
                                     double fromNormalizedY,
                                     double toNormalizedX,
                                     double toNormalizedY,
                                     int diameter,
                                     bool ellipse,
                                     bool hardEdges = false,
                                     bool fillBackground = false,
                                     bool squareBrush = false);
    Q_INVOKABLE void drawSourceLine(double fromNormalizedX,
                                    double fromNormalizedY,
                                    double toNormalizedX,
                                    double toNormalizedY,
                                    int diameter,
                                    bool hardEdges = false,
                                    bool squareBrush = false);
    Q_INVOKABLE void undoDrawing();
    Q_INVOKABLE void redoDrawing();
    Q_INVOKABLE void pickScreenImageColor(double normalizedX,
                                          double normalizedY,
                                          bool foreground);
    Q_INVOKABLE void swapScreenImageColors(double normalizedX,
                                           double normalizedY);
    Q_INVOKABLE void beginScreenImageStroke(double normalizedX,
                                            double normalizedY,
                                            int diameter,
                                            bool eraser,
                                            bool hardEdges = false,
                                            bool squareBrush = false);
    Q_INVOKABLE void continueScreenImageStroke(double normalizedX,
                                               double normalizedY);
    Q_INVOKABLE void continueScreenImageRay(double normalizedX,
                                            double normalizedY);
    Q_INVOKABLE void endScreenImageStroke();
    Q_INVOKABLE void drawScreenImageShape(double fromNormalizedX,
                                          double fromNormalizedY,
                                          double toNormalizedX,
                                          double toNormalizedY,
                                          int diameter,
                                          bool ellipse,
                                          bool hardEdges = false,
                                          bool fillBackground = false,
                                          bool squareBrush = false);
    Q_INVOKABLE void drawScreenImageLine(double fromNormalizedX,
                                         double fromNormalizedY,
                                         double toNormalizedX,
                                         double toNormalizedY,
                                         int diameter,
                                         bool hardEdges = false,
                                         bool squareBrush = false);
    Q_INVOKABLE void nudgeScreenImage(int horizontal, int vertical);
    Q_INVOKABLE void mirrorScreenImage();
    Q_INVOKABLE void flipScreenImage();
    Q_INVOKABLE void invertScreenImage();
    Q_INVOKABLE void removeScreenImageColor();
    Q_INVOKABLE void clearScreenImage();
    Q_INVOKABLE void setScreenImageSelection(double fromNormalizedX,
                                              double fromNormalizedY,
                                              double toNormalizedX,
                                              double toNormalizedY);
    Q_INVOKABLE void clearScreenImageSelection();
    Q_INVOKABLE void beginMoveScreenImageSelection();
    Q_INVOKABLE void placeScreenImageFloating(double normalizedCenterX,
                                              double normalizedCenterY);
    Q_INVOKABLE void cancelScreenImageFloating();
    Q_INVOKABLE void reloadScreenImageFonts();
    Q_INVOKABLE void openTiArtistFontsFolder();
    Q_INVOKABLE void prepareScreenImageText(const QString& text,
                                            const QString& fontKey,
                                            int pixelSize);
    Q_INVOKABLE void loadScreenImageClipArt(const QUrl& url);
    Q_INVOKABLE QString screenImageClipArtPreview(int width,
                                                  int height,
                                                  int colorMode,
                                                  bool transparentBackground,
                                                  bool useSelectedColors) const;
    Q_INVOKABLE void prepareScreenImageClipArt(int width,
                                               int height,
                                               int colorMode,
                                               bool transparentBackground,
                                               bool useSelectedColors);
    Q_INVOKABLE void copyScreenImage();
    Q_INVOKABLE void pasteScreenImage();
    Q_INVOKABLE void undoScreenImage();
    Q_INVOKABLE void redoScreenImage();
    Q_INVOKABLE void applyScreenImageEdits();
    Q_INVOKABLE void restorePerceptualWeights();
    Q_INVOKABLE void setWorkingPaletteColor(int index, const QColor& color);
    Q_INVOKABLE void resetWorkingPalette();

    [[nodiscard]] QJsonObject recipeSettings() const;
    bool applyRecipeSettings(const QJsonObject& object, QString* error = nullptr);

    void setTargetProfile(int value);
    void setConversionMode(int value);
    void setDitherMode(int value);
    void setScalingFilter(int value);
    void setFillMode(int value);
    void setExportFormat(int value);
    void setPerceptualColorMatching(bool value);
    void setStretchHistogram(bool value);
    void setPerceptualRedWeight(int value);
    void setPerceptualGreenWeight(int value);
    void setPerceptualBlueWeight(int value);
    void setMaximumColorShift(double value);
    void setGamma(double value);
    void setLumaEmphasis(double value);
    void setMaximumMulticolorDifference(int value);
    void setOrderedBrightness(int value);
    void setErrorAccumulationMode(int value);
    void setOrderedDitherMapSize(int value);
    void setErrorDownLeft(int value);
    void setErrorDown(int value);
    void setErrorDownRight(int value);
    void setErrorRight(int value);
    void setErrorFarRight(int value);
    void setErrorDownTwo(int value);
    void setPaletteSelectionMode(int value);
    void setScanlineStaticColorCount(int value);
    void setScanlineRegion1(bool value);
    void setScanlineRegion2(bool value);
    void setScanlineRegion3(bool value);
    void setPowerPaintFraming(bool value);
    void setHorizontalOffset(int value);
    void setVerticalOffset(int value);
    void setAutoUpdate(bool value);
    void setLivePreview(bool value);
    void setBackgroundColor(const QColor& value);
    void setForegroundColor(const QColor& value);
    void setScreenImageBackgroundColor(const QColor& value);
    void setScreenImageForegroundColor(const QColor& value);

signals:
    void sourceChanged();
    void sourceColorsChanged();
    void conversionChanged();
    void settingsChanged();
    void drawingHistoryChanged();
    void screenImageHistoryChanged();
    void screenImageColorsChanged();
    void screenImageSelectionChanged();
    void screenImageFloatingChanged();
    void screenImageFontsChanged();
    void screenImageClipArtChanged();
    void statusChanged();
    void exportChanged();

private:
    struct SettingsSnapshot {
        retrovdp::core::ConversionSettings settings;
        retrovdp::core::ScalingFilter scalingFilter;
        retrovdp::core::ImageFillMode fillMode;
        int horizontalOffset{};
        int verticalOffset{};
        retrovdp::core::RgbColor foregroundColor{};
        retrovdp::core::RgbColor backgroundColor{};
        std::vector<retrovdp::core::RgbColor> workingPalette;
        bool powerPaintFraming{};
    };

    void accept(retrovdp::imageio::ImageLoadResult result, QString sourceName);
    void refreshSourcePreview();
    void scheduleConversion();
    void startConversion();
    void publishConversion(const retrovdp::core::ConversionRequest& request,
                           retrovdp::core::ConversionResult result);
    void updatePaletteInspection();
    void refreshSourceColorChoices();
    void drawSourceStrokeSegment(double fromNormalizedX,
                                 double fromNormalizedY,
                                 double toNormalizedX,
                                 double toNormalizedY);
    void paintDrawingPixel(int x,
                           int y,
                           retrovdp::core::RgbColor color,
                           double coverage);
    void fillSourceShape(double fromNormalizedX,
                         double fromNormalizedY,
                         double toNormalizedX,
                         double toNormalizedY,
                         bool ellipse,
                         double inset);
    void ensureDrawingLayer();
    void compositeDrawingLayer(retrovdp::core::RgbImage& canvas) const;
    void beginDrawingTransaction();
    void commitDrawingTransaction(bool changed);
    void clearDrawingHistory();
    void refreshAfterDrawingHistoryChange();
    void beginScreenImageTransaction();
    void commitScreenImageTransaction(bool changed);
    void clearScreenImageHistory();
    void resetScreenImageSelectionState();
    [[nodiscard]] std::shared_ptr<retrovdp::core::RgbImage>
    copyScreenImageRegion(int x, int y, int width, int height) const;
    void beginScreenImageFloating(
        std::shared_ptr<retrovdp::core::RgbImage> image,
        bool movingSelection,
        bool topLeftAnchor = false);
    [[nodiscard]] std::shared_ptr<retrovdp::core::RgbImage>
    renderScreenImageText(const QString& text,
                          const QString& fontKey,
                          int pixelSize,
                          QString* error = nullptr) const;
    [[nodiscard]] std::shared_ptr<retrovdp::core::RgbImage>
    renderScreenImageClipArt(int width,
                             int height,
                             int colorMode,
                             bool transparentBackground,
                             bool useSelectedColors,
                             QString* error = nullptr) const;
    void refreshScreenImage();
    void paintScreenImagePixel(int x,
                               int y,
                               retrovdp::core::RgbColor color,
                               double coverage);
    void fillScreenImageShape(double fromNormalizedX,
                              double fromNormalizedY,
                              double toNormalizedX,
                              double toNormalizedY,
                              bool ellipse,
                              double inset);
    void drawScreenImageStrokeSegment(double fromNormalizedX,
                                      double fromNormalizedY,
                                      double toNormalizedX,
                                      double toNormalizedY);
    void updateExportSummary();
    void recordUndo();
    void applySnapshot(const SettingsSnapshot& snapshot);
    [[nodiscard]] SettingsSnapshot snapshot() const;
    void loadSettings();
    void saveSettings() const;
    void settingsWereChanged(bool sourceTransformChanged = false);
    void setErrorDistributionWeight(int index, int value);
    [[nodiscard]] retrovdp::formats::GeneratedFileManifest exportManifest() const;
    [[nodiscard]] QString exportBaseName() const;

    std::shared_ptr<retrovdp::core::RgbImage> image_;
    std::optional<retrovdp::core::RgbImage> framedSource_;
    std::shared_ptr<retrovdp::core::RgbImage> drawingLayer_;
    std::optional<retrovdp::core::ConversionResult> result_;
    mutable std::optional<retrovdp::core::ConversionResult> screenImageExportResult_;
    retrovdp::core::ConversionSettings settings_;
    retrovdp::core::ScalingFilter scalingFilter_{
        retrovdp::core::ScalingFilter::Bilinear};
    retrovdp::core::ImageFillMode fillMode_{retrovdp::core::ImageFillMode::Fit};
    int horizontalOffset_{};
    int verticalOffset_{};
    retrovdp::core::RgbColor foregroundColor_{};
    retrovdp::core::RgbColor backgroundColor_{};
    retrovdp::core::RgbColor screenImageForegroundColor_{255, 255, 255};
    retrovdp::core::RgbColor screenImageBackgroundColor_{};
    std::vector<retrovdp::core::RgbColor> workingPalette_;
    bool powerPaintFraming_{};
    int exportFormat_{};
    bool busy_{};
    bool conversionPending_{};
    bool autoUpdate_{true};
    bool livePreview_{};
    bool screenImageEdited_{};
    bool applyingSnapshot_{};
    bool sourceStrokeActive_{};
    bool sourceStrokeTouched_{};
    double sourceStrokeX_{};
    double sourceStrokeY_{};
    int sourceStrokeDiameter_{1};
    bool sourceStrokeEraser_{};
    bool sourceStrokeHardEdges_{};
    bool sourceStrokeSquareBrush_{};
    std::shared_ptr<retrovdp::core::RgbImage> drawingBeforeImage_;
    std::vector<std::shared_ptr<retrovdp::core::RgbImage>> drawingUndoStack_;
    std::vector<std::shared_ptr<retrovdp::core::RgbImage>> drawingRedoStack_;
    std::shared_ptr<retrovdp::core::RgbImage> screenImage_;
    std::shared_ptr<retrovdp::core::RgbImage> screenImageBefore_;
    std::vector<std::shared_ptr<retrovdp::core::RgbImage>> screenImageUndoStack_;
    std::vector<std::shared_ptr<retrovdp::core::RgbImage>> screenImageRedoStack_;
    bool screenImageStrokeActive_{};
    bool screenImageStrokeTouched_{};
    double screenImageStrokeX_{};
    double screenImageStrokeY_{};
    int screenImageStrokeDiameter_{1};
    bool screenImageStrokeEraser_{};
    bool screenImageStrokeHardEdges_{};
    bool screenImageStrokeSquareBrush_{};
    int screenImageSelectionX_{};
    int screenImageSelectionY_{};
    int screenImageSelectionWidth_{};
    int screenImageSelectionHeight_{};
    std::shared_ptr<retrovdp::core::RgbImage> screenImageFloatingImage_;
    QString screenImageFloatingPreview_;
    bool screenImageFloatingMove_{};
    bool screenImageFloatingTopLeft_{};
    int screenImageFloatingSourceX_{};
    int screenImageFloatingSourceY_{};
    QVariantList screenImageFonts_;
    QString tiArtistLibraryPath_;
    QString tiArtistFontsPath_;
    std::shared_ptr<retrovdp::core::RgbImage> screenImageClipArtSourceImage_;
    QString screenImageClipArtSourcePreview_;
    QString screenImageClipArtSourceName_;

    retrovdp::core::ConversionJobController jobs_;
    QTimer debounceTimer_;
    QTimer undoCoalesceTimer_;
    std::vector<SettingsSnapshot> undoStack_;

    QString sourcePreview_;
    QString convertedPreview_;
    QString palettePreview_;
    QVariantList paletteColors_;
    QString sourceName_;
    QString sourcePath_;
    QString sourceDetails_;
    QVariantList sourceSpectrum16Colors_;
    QVariantList sourceSpectrum32Colors_;
    QVariantList sourceSpectrum64Colors_;
    QVariantList sourceSwatchColors_;
    QVariantList sourceUsedColors_;
    QString conversionDetails_;
    QString errorMessage_;
    QString statusMessage_;
    QString outputSummary_;
    QString overwriteMessage_;
    QString pendingExportDirectory_;
    std::optional<retrovdp::formats::GeneratedFileManifest> pendingManifest_;
};
