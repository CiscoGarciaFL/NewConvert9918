#include "EditorProjectController.hpp"
#include "ImageInputController.hpp"

#include <QCoreApplication>
#include <QColor>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QSet>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>
#include <QVariant>

#include <atomic>
#include <functional>
#include <iostream>

namespace {

std::atomic<int> qmlBindingErrors{};
QtMessageHandler previousMessageHandler{};

void captureQmlMessages(QtMsgType type,
                        const QMessageLogContext& context,
                        const QString& message)
{
    if ((type == QtWarningMsg || type == QtCriticalMsg)
        && (message.contains(QStringLiteral("TypeError"))
            || message.contains(QStringLiteral("ReferenceError"))
            || message.contains(QStringLiteral("of null")))) {
        ++qmlBindingErrors;
    }
    if (previousMessageHandler != nullptr) {
        previousMessageHandler(type, context, message);
    } else if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        std::cerr << message.toStdString() << '\n';
    }
}

struct TestContext {
    int failures{};

    void expect(bool condition, const char* message)
    {
        if (!condition) {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }
};

bool waitFor(const std::function<bool()>& predicate, int timeoutMilliseconds = 15'000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(10);
    }
    return predicate();
}

QString goldenSource(QStringView relative)
{
    return QDir(QStringLiteral(NEWCONVERT9918_GOLDEN_DIR)).filePath(relative.toString());
}

void testLiveWorkflow(TestContext& test, ImageInputController& controller)
{
    controller.setPerceptualRedWeight(40);
    controller.setPerceptualGreenWeight(40);
    controller.setPerceptualBlueWeight(20);
    controller.restorePerceptualWeights();
    test.expect(controller.perceptualRedWeight() == 30
                    && controller.perceptualGreenWeight() == 52
                    && controller.perceptualBlueWeight() == 18,
                "perceptual weights should restore to the audited 30/52/18 defaults");
    controller.setDitherMode(1);
    test.expect(controller.errorDownLeft() == 3 && controller.errorDown() == 5
                    && controller.errorDownRight() == 1 && controller.errorRight() == 7
                    && controller.errorFarRight() == 0 && controller.errorDownTwo() == 0,
                "selecting a named dither preset should populate all six error weights");
    // Keep this end-to-end UI test fast in unoptimized developer builds. The
    // exhaustive non-zero shift search is covered independently by core tests.
    controller.applyPreset(1); // Pixel art: no dithering or color-shift search.
    controller.openUrl(QUrl::fromLocalFile(goldenSource(u"source/tiny-rgba.png")));
    test.expect(controller.hasImage(), "controller should load a source image");
    const QVariantList sourceColors = controller.sourceUsedColors();
    QSet<QRgb> uniqueSourceColors;
    bool validSourceColors = !sourceColors.isEmpty();
    const auto sourceColorLess = [](const QColor& left, const QColor& right) {
        const bool leftNeutral = left.hsvSaturation() <= 8;
        const bool rightNeutral = right.hsvSaturation() <= 8;
        if (leftNeutral != rightNeutral) return leftNeutral;
        if (leftNeutral) {
            if (left.value() != right.value()) return left.value() < right.value();
            return left.rgb() < right.rgb();
        }
        if (left.hsvHue() != right.hsvHue()) return left.hsvHue() < right.hsvHue();
        if (left.value() != right.value()) return left.value() < right.value();
        if (left.hsvSaturation() != right.hsvSaturation()) {
            return left.hsvSaturation() > right.hsvSaturation();
        }
        return left.rgb() < right.rgb();
    };
    bool sourceColorsSorted = true;
    QColor previousSourceColor;
    for (const QVariant& value : sourceColors) {
        const QColor color = value.value<QColor>();
        validSourceColors &= color.isValid();
        if (previousSourceColor.isValid()
            && sourceColorLess(color, previousSourceColor)) {
            sourceColorsSorted = false;
        }
        previousSourceColor = color;
        uniqueSourceColors.insert(color.rgb());
    }
    test.expect(validSourceColors
                    && uniqueSourceColors.size() == sourceColors.size()
                    && sourceColorsSorted,
                "source-image color choices should be unique and ordered by color");
    test.expect(waitFor([&] { return controller.hasConversion() && !controller.busy(); }),
                "loaded image should produce a debounced background preview");
    test.expect(controller.canReload(),
                "a file-backed source should enable Reload");
    const QString loadedSourceName = controller.sourceName();
    controller.reloadSource();
    test.expect(controller.hasImage() && controller.sourceName() == loadedSourceName
                    && waitFor([&] {
                           return controller.hasConversion() && !controller.busy();
                       }),
                "Reload should reopen the current source file");
    {
        ImageInputController drawingController;
        drawingController.setAutoUpdate(false);
        drawingController.openUrl(
            QUrl::fromLocalFile(goldenSource(u"source/tiny-rgba.png")));
        const QColor pencilColor(17, 34, 51);
        const QColor eraserColor(68, 85, 102);
        drawingController.setForegroundColor(pencilColor);
        drawingController.setBackgroundColor(eraserColor);
        const QString sourceBeforeDrawing = drawingController.sourcePreview();
        drawingController.beginSourceStroke(0.25, 0.5, 3, false);
        drawingController.continueSourceStroke(0.75, 0.5);
        test.expect(drawingController.sourcePreview() == sourceBeforeDrawing,
                    "active pencil stroke should defer source-preview regeneration");
        drawingController.endSourceStroke();
        const QString sourceAfterPencil = drawingController.sourcePreview();
        test.expect(sourceAfterPencil != sourceBeforeDrawing
                        && drawingController.sourceUsedColors().contains(
                            QVariant::fromValue(pencilColor))
                        && drawingController.canUndoDrawing()
                        && !drawingController.canRedoDrawing(),
                    "pencil stroke should paint the original source with the foreground color");
        drawingController.beginSourceStroke(0.5, 0.5, 3, true);
        drawingController.endSourceStroke();
        const QString sourceAfterEraser = drawingController.sourcePreview();
        test.expect(sourceAfterEraser != sourceAfterPencil
                        && drawingController.sourceUsedColors().contains(
                            QVariant::fromValue(eraserColor)),
                    "eraser stroke should paint the original source with the background color");
        drawingController.undoDrawing();
        test.expect(drawingController.sourcePreview() == sourceAfterPencil
                        && drawingController.canUndoDrawing()
                        && drawingController.canRedoDrawing(),
                    "drawing undo should restore the image before the most recent stroke");
        drawingController.undoDrawing();
        test.expect(drawingController.sourcePreview() == sourceBeforeDrawing
                        && !drawingController.canUndoDrawing()
                        && drawingController.canRedoDrawing(),
                    "each drawing stroke should be a separate undo transaction");
        drawingController.redoDrawing();
        drawingController.redoDrawing();
        test.expect(drawingController.sourcePreview() == sourceAfterEraser
                        && drawingController.canUndoDrawing()
                        && !drawingController.canRedoDrawing(),
                    "drawing redo should replay undone pencil and eraser strokes");
        drawingController.undoDrawing();
        drawingController.beginSourceStroke(0.5, 0.25, 2, false);
        drawingController.endSourceStroke();
        test.expect(!drawingController.canRedoDrawing(),
                    "a new drawing change should discard the redo branch");
        const QString sourceBeforeRectangle = drawingController.sourcePreview();
        drawingController.setForegroundColor(QColor(190, 40, 210));
        drawingController.drawSourceShape(0.1, 0.15, 0.9, 0.8, 2, false);
        const QString sourceAfterRectangle = drawingController.sourcePreview();
        test.expect(sourceAfterRectangle != sourceBeforeRectangle
                        && drawingController.canUndoDrawing(),
                    "rectangle tool should commit one drawing transaction");
        drawingController.undoDrawing();
        test.expect(drawingController.sourcePreview() == sourceBeforeRectangle,
                    "rectangle drawing should be undoable as one change");
        drawingController.redoDrawing();
        test.expect(drawingController.sourcePreview() == sourceAfterRectangle,
                    "rectangle drawing should be redoable as one change");
        drawingController.setForegroundColor(QColor(30, 210, 120));
        const QColor shapeFillColor(15, 225, 95);
        drawingController.setBackgroundColor(shapeFillColor);
        drawingController.drawSourceShape(0.2, 0.2, 0.8, 0.75, 2, true,
                                          false, true);
        test.expect(drawingController.sourcePreview() != sourceAfterRectangle
                        && drawingController.sourceUsedColors().contains(
                            QVariant::fromValue(shapeFillColor)),
                    "ellipse tool should support a background-color fill");
        drawingController.drawSourceShape(0.25, 0.25, 0.75, 0.7, 3, false,
                                          true, false);
        test.expect(drawingController.canUndoDrawing(),
                    "hard-edge shapes should participate in drawing history");
        drawingController.reloadSource();
        test.expect(!drawingController.canUndoDrawing()
                        && !drawingController.canRedoDrawing(),
                    "loading a source should begin a fresh drawing history");
    }
    {
        const QString drawingPath = goldenSource(u"source/transparency-rgba.png");
        const QColor paintColor(251, 252, 253);

        ImageInputController softController;
        softController.setAutoUpdate(false);
        softController.openUrl(QUrl::fromLocalFile(drawingPath));
        softController.setForegroundColor(paintColor);
        const QImage sourceBefore = QImage::fromData(QByteArray::fromBase64(
            softController.sourcePreview().section(QLatin1Char(','), 1).toLatin1()), "PNG");
        softController.beginSourceStroke(0.5, 0.5, 8, false, false);
        softController.endSourceStroke();
        const QImage softImage = QImage::fromData(QByteArray::fromBase64(
            softController.sourcePreview().section(QLatin1Char(','), 1).toLatin1()), "PNG");

        ImageInputController hardController;
        hardController.setAutoUpdate(false);
        hardController.openUrl(QUrl::fromLocalFile(drawingPath));
        hardController.setForegroundColor(paintColor);
        hardController.beginSourceStroke(0.5, 0.5, 8, false, true);
        hardController.endSourceStroke();
        const QImage hardImage = QImage::fromData(QByteArray::fromBase64(
            hardController.sourcePreview().section(QLatin1Char(','), 1).toLatin1()), "PNG");

        bool softHasBlend = false;
        bool hardChanged = false;
        bool hardOnlyExact = true;
        for (int y = 0; y < sourceBefore.height(); ++y) {
            for (int x = 0; x < sourceBefore.width(); ++x) {
                const QRgb original = sourceBefore.pixel(x, y);
                const QRgb soft = softImage.pixel(x, y);
                const QRgb hard = hardImage.pixel(x, y);
                softHasBlend |= soft != original && soft != paintColor.rgba();
                if (hard != original) {
                    hardChanged = true;
                    hardOnlyExact &= hard == paintColor.rgba();
                }
            }
        }
        test.expect(!sourceBefore.isNull() && sourceBefore.size() == softImage.size()
                        && sourceBefore.size() == hardImage.size()
                        && softHasBlend && hardChanged && hardOnlyExact,
                    "hard-edge brush should remain an exact overwrite after source framing");

        const QColor fillColor(19, 213, 147);
        hardController.setForegroundColor(QColor(245, 31, 73));
        hardController.setBackgroundColor(fillColor);
        hardController.drawSourceShape(0.2, 0.2, 0.8, 0.8, 6,
                                       false, false, true);
        const QImage filledImage = QImage::fromData(QByteArray::fromBase64(
            hardController.sourcePreview().section(QLatin1Char(','), 1).toLatin1()), "PNG");
        bool fillInteriorIsExact = !filledImage.isNull();
        for (int y = 76; y < 116; ++y) {
            for (int x = 100; x < 156; ++x) {
                fillInteriorIsExact &= filledImage.pixelColor(x, y).rgb()
                    == fillColor.rgb();
            }
        }
        test.expect(fillInteriorIsExact,
                    "shape fill should be fully opaque with no source pixels leaking through");
        hardController.applyPreset(1);
        const QImage reframedFilledImage = QImage::fromData(QByteArray::fromBase64(
            hardController.sourcePreview().section(QLatin1Char(','), 1).toLatin1()), "PNG");
        bool fillSurvivesReframing = !reframedFilledImage.isNull();
        for (int y = 76; y < 116; ++y) {
            for (int x = 100; x < 156; ++x) {
                fillSurvivesReframing &= reframedFilledImage.pixelColor(x, y).rgb()
                    == fillColor.rgb();
            }
        }
        test.expect(fillSurvivesReframing,
                    "prepared-canvas drawing should remain exact when framing changes");
        hardController.updateConversion();
        test.expect(waitFor([&] {
                        return hardController.hasConversion() && !hardController.busy();
                    }),
                    "conversion should accept the already-prepared drawing canvas");
    }
    test.expect(controller.convertedPreview().startsWith(QStringLiteral("data:image/png;base64,")),
                "converted preview should be exposed as displayable PNG data");
    const auto previewPayload = controller.convertedPreview().section(QLatin1Char(','), 1);
    const QImage decodedPreview = QImage::fromData(
        QByteArray::fromBase64(previewPayload.toLatin1()), "PNG");
    test.expect(decodedPreview.size() == QSize(256, 192),
                "converted preview PNG should retain the target dimensions");
    test.expect(controller.conversionDetails().contains(QStringLiteral("256×192")),
                "conversion details should state target dimensions");
    test.expect(!controller.outputSummary().isEmpty(),
                "successful conversion should provide a generated-file summary");

    const auto sourcePayload = controller.sourcePreview().section(QLatin1Char(','), 1);
    const QImage framedSource = QImage::fromData(
        QByteArray::fromBase64(sourcePayload.toLatin1()), "PNG");
    test.expect(framedSource.size() == QSize(256, 192),
                "source preview should show the exact framed conversion input");
    controller.setAutoUpdate(false);
    const QString previousConversion = controller.convertedPreview();
    const QString previousSource = controller.sourcePreview();
    controller.nudgeSource(1, 0);
    const auto paletteColors = controller.backgroundPaletteColors();
    controller.setBackgroundColor(QColor(paletteColors.at(3).toString()));
    test.expect(controller.conversionPending() && !controller.busy()
                    && controller.convertedPreview() == previousConversion
                    && controller.sourcePreview() != previousSource,
                "manual mode should reframe immediately without launching conversion");
    controller.pickBackgroundColor(0.5, 0.5);
    test.expect(paletteColors.contains(controller.backgroundColor().name().toUpper()),
                "eyedropper selection should be constrained to the conversion palette");
    controller.setBackgroundColor(QColor(paletteColors.at(1).toString()));
    controller.setPowerPaintFraming(true);
    const auto powerPaintPayload = controller.sourcePreview().section(QLatin1Char(','), 1);
    const QImage powerPaintSource = QImage::fromData(
        QByteArray::fromBase64(powerPaintPayload.toLatin1()), "PNG");
    test.expect(powerPaintSource.size() == QSize(256, 192)
                    && powerPaintSource.pixelColor(255, 191) == QColor(Qt::black),
                "PowerPaint framing should retain a 256x192 preview with a black padded border");
    controller.setPowerPaintFraming(false);
    controller.centerSource();
    controller.updateConversion();
    test.expect(waitFor([&] {
                    return controller.hasConversion() && !controller.busy()
                        && !controller.conversionPending();
                }),
                "manual Update should run the pending conversion");
    controller.setAutoUpdate(true);

    controller.setConversionMode(3);
    controller.setConversionMode(4);
    controller.setConversionMode(0);
    test.expect(waitFor([&] { return controller.hasConversion() && !controller.busy(); }),
                "rapid changes should settle on one current conversion");
    test.expect(controller.conversionMode() == 0
                    && controller.conversionDetails().contains(QStringLiteral("Bitmap 9918A")),
                "stale preview jobs should not replace the newest requested mode");

    controller.setConversionMode(8);
    const bool scanlineReady = waitFor(
        [&] { return controller.hasConversion() && !controller.busy(); }, 60'000);
    if (!scanlineReady) {
        std::cerr << "Scanline status: " << controller.statusMessage().toStdString()
                  << "; error: " << controller.errorMessage().toStdString()
                  << "; busy=" << controller.busy()
                  << "; pending=" << controller.conversionPending()
                  << "; auto=" << controller.autoUpdate()
                  << "; mode=" << controller.conversionMode() << '\n';
    }
    test.expect(scanlineReady,
                "scanline-palette mode should produce a background preview");
    test.expect(controller.scanlinePaletteAvailable()
                    && controller.palettePreview().startsWith(QStringLiteral("data:image/png;base64,")),
                "scanline-palette mode should expose its optional palette visualization");
    controller.setConversionMode(0);
    test.expect(waitFor([&] { return controller.hasConversion() && !controller.busy(); }),
                "controller should return to the requested export mode");

    const int previousErrorAccumulation = controller.errorAccumulationMode();
    const int changedErrorAccumulation = previousErrorAccumulation == 0 ? 1 : 0;
    const int previousOrderedMapSize = controller.orderedDitherMapSize();
    const int changedOrderedMapSize = previousOrderedMapSize == 2 ? 4 : 2;
    const int previousDitherMode = controller.ditherMode();
    const int previousErrorRight = controller.errorRight();
    const int changedErrorRight = previousErrorRight == 16 ? 15 : previousErrorRight + 1;
    const int previousPaletteSelection = controller.paletteSelectionMode();
    const int previousStaticColorCount = controller.scanlineStaticColorCount();
    const QColor previousWorkingColor(controller.workingPaletteColors().at(0).toString());
    controller.setGamma(1.4);
    controller.setErrorAccumulationMode(changedErrorAccumulation);
    controller.setOrderedDitherMapSize(changedOrderedMapSize);
    controller.setErrorRight(changedErrorRight);
    controller.setPerceptualRedWeight(31);
    controller.setPaletteSelectionMode(previousPaletteSelection == 0 ? 1 : 0);
    controller.setScanlineStaticColorCount(2);
    controller.setWorkingPaletteColor(0, QColor(QStringLiteral("#123456")));
    controller.setLivePreview(true);
    test.expect(controller.canUndo(), "changing a setting should enable undo");
    QSettings persisted;
    const double persistedGamma =
        persisted.value(QStringLiteral("conversion/gamma"), -1.0).toDouble();
    const int persistedErrorAccumulation =
        persisted.value(QStringLiteral("conversion/errorAccumulation"), -1).toInt();
    const int persistedOrderedMapSize =
        persisted.value(QStringLiteral("conversion/orderedDitherMapSize"), -1).toInt();
    const int persistedErrorRight =
        persisted.value(QStringLiteral("conversion/errorRight"), -1).toInt();
    const double persistedPerceptualRed =
        persisted.value(QStringLiteral("conversion/perceptualRedWeight"), -1.0).toDouble();
    const int persistedPaletteSelection =
        persisted.value(QStringLiteral("conversion/paletteSelection"), -1).toInt();
    const int persistedStaticColorCount =
        persisted.value(QStringLiteral("conversion/scanlineStaticColorCount"), -1).toInt();
    const QStringList persistedWorkingPalette =
        persisted.value(QStringLiteral("conversion/workingPalette")).toStringList();
    const bool persistedLivePreview =
        persisted.value(QStringLiteral("conversion/livePreview"), false).toBool();
    if (!qFuzzyCompare(persistedGamma, 1.4)) {
        std::cerr << "Persisted gamma was " << persistedGamma
                  << "; settings status " << persisted.status() << '\n';
    }
    test.expect(qFuzzyCompare(persistedGamma, 1.4)
                    && persistedErrorAccumulation == changedErrorAccumulation
                    && persistedOrderedMapSize == changedOrderedMapSize
                    && persistedErrorRight == changedErrorRight
                    && qFuzzyCompare(persistedPerceptualRed, 0.31)
                    && persistedPaletteSelection == controller.paletteSelectionMode()
                    && persistedStaticColorCount == 2
                    && persistedWorkingPalette.size() == 15
                    && persistedWorkingPalette.front() == QStringLiteral("#123456").toUpper()
                    && persistedLivePreview && controller.livePreview()
                    && controller.ditherMode() == 7,
                "advanced parity and live-preview settings should persist through QSettings");
    controller.undoSettings();
    test.expect(!qFuzzyCompare(controller.gamma(), 1.4)
                    && controller.errorAccumulationMode() == previousErrorAccumulation
                    && controller.orderedDitherMapSize() == previousOrderedMapSize
                    && controller.ditherMode() == previousDitherMode
                    && controller.errorRight() == previousErrorRight
                    && controller.paletteSelectionMode() == previousPaletteSelection
                    && controller.scanlineStaticColorCount() == previousStaticColorCount
                    && QColor(controller.workingPaletteColors().at(0).toString())
                        == previousWorkingColor,
                "undo should restore the prior advanced dithering settings");
    controller.setGamma(1.2);
    test.expect(controller.canUndo(),
                "a new settings change immediately after undo should create a new undo step");
    controller.undoSettings();
    test.expect(!qFuzzyCompare(controller.gamma(), 1.2),
                "the new post-undo settings step should be reversible");
    controller.setConversionMode(0);
    controller.applyPreset(1);
    test.expect(waitFor([&] { return controller.hasConversion() && !controller.busy(); }),
                "preview should settle after undo and preset changes");
}

void testExportWorkflow(TestContext& test, ImageInputController& controller)
{
    controller.setExportFormat(2); // RAW tables
    QTemporaryDir directory(QDir::current().filePath(QStringLiteral("interface-export-XXXXXX")));
    test.expect(directory.isValid(), "interface export test directory should be available");
    controller.exportToDirectory(QUrl::fromLocalFile(directory.path()));
    test.expect(QFileInfo::exists(directory.filePath(QStringLiteral("TINY-RGBA.TIAP")))
                    && QFileInfo::exists(directory.filePath(QStringLiteral("TINY-RGBA.TIAC"))),
                "interface export should write the complete generated manifest");
    controller.exportToDirectory(QUrl::fromLocalFile(directory.path()));
    test.expect(!controller.overwriteMessage().isEmpty(),
                "existing files should produce a clear overwrite prompt state");
    controller.cancelOverwrite();
    test.expect(controller.overwriteMessage().isEmpty(),
                "cancelled overwrite should clear the prompt without changing files");
}

void testEditorProjectRecipe(TestContext& test)
{
    ImageInputController recipeImage;
    recipeImage.setAutoUpdate(false);
    recipeImage.openUrl(QUrl::fromLocalFile(goldenSource(u"source/tiny-rgba.png")));
    recipeImage.setGamma(1.75);
    recipeImage.setForegroundColor(QColor(QStringLiteral("#123456")));

    EditorProjectController project(&recipeImage);
    project.setWorkspaceMode(2);
    project.setPreviewTarget(2);
    project.setEditScope(1);
    project.addSpriteSet();
    project.setActiveSprite(3);
    project.setSpritePlacementMode(true);
    project.setPlacementWidth(320);
    project.setPlacementHeight(200);
    project.moveSprite(3, 91, 47);
    project.setSpritePlacementMode(false);
    project.setEditScope(0);
    project.setActiveSpriteSize(16);
    project.setSpriteDrawingColorIndex(12);
    project.paintSpritePixel(1, 3, 16, 0, 0, true);
    project.setEditScope(1);
    project.setActiveSpriteSize(16);
    project.setActiveSpriteColorDepth(3);
    project.setSpriteDrawingColorIndex(5);
    project.paintSpritePixel(1, 3, 16, 0, 1, true);
    project.setSpritePlacementMode(true);
    project.setActiveCharacterSet(2);
    project.setActiveCharacterPattern(255);
    project.setCharacterForegroundColorIndex(2);
    project.setCharacterBackgroundColorIndex(3);
    project.paintCharacterPixel(2, 255, 0, 0, true);
    project.paintCharacterPixel(2, 255, 0, 7, true);
    project.paintCharacterPixel(2, 255, 1, 4, true);
    project.paintCharacterPixel(2, 255, 1, 4, false);
    const QVariantList editedRows = project.characterPatternRows(2, 255);
    test.expect(project.characterPaletteColors().size() == 16
                    && editedRows.size() == 8
                    && editedRows.at(0).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0x81
                    && editedRows.at(0).toMap().value(QStringLiteral("color")).toInt()
                        == 0x23
                    && editedRows.at(1).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0,
                "character pattern model should expose 8 bitmap/color rows and support pencil and eraser edits");

    EditorProjectController historyProject(&recipeImage);
    historyProject.beginCharacterEdit(0, 0);
    historyProject.paintCharacterPixel(0, 0, 0, 0, true);
    historyProject.paintCharacterPixel(0, 0, 1, 2, true);
    historyProject.endCharacterEdit();
    auto historyRows = historyProject.characterPatternRows(0, 0);
    test.expect(historyProject.canUndoCharacter()
                    && historyRows.at(0).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0x80
                    && historyRows.at(1).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0x20,
                "one character drawing stroke should group all painted pixels into one history entry");
    historyProject.undoCharacterEdit();
    historyRows = historyProject.characterPatternRows(0, 0);
    test.expect(historyProject.canRedoCharacter()
                    && historyRows.at(0).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0
                    && historyRows.at(1).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0,
                "character undo should revert a complete drawing stroke");
    historyProject.redoCharacterEdit();

    historyProject.mirrorActiveCharacterPattern();
    historyRows = historyProject.characterPatternRows(0, 0);
    test.expect(historyRows.at(0).toMap().value(QStringLiteral("pattern")).toInt()
                    == 0x01
                    && historyRows.at(1).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0x04,
                "horizontal mirror should reverse every pattern row");
    historyProject.undoCharacterEdit();
    historyProject.flipActiveCharacterPattern();
    historyRows = historyProject.characterPatternRows(0, 0);
    test.expect(historyRows.at(6).toMap().value(QStringLiteral("pattern")).toInt()
                    == 0x20
                    && historyRows.at(7).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0x80,
                "vertical flip should reverse pattern and row-color order");
    historyProject.undoCharacterEdit();
    historyProject.rotateActiveCharacterPattern();
    historyRows = historyProject.characterPatternRows(0, 0);
    test.expect(historyRows.at(0).toMap().value(QStringLiteral("pattern")).toInt()
                    == 0x01
                    && historyRows.at(2).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0x02,
                "clockwise rotation should rotate the 8 by 8 bitmap");
    historyProject.undoCharacterEdit();
    historyProject.blankActiveCharacterPattern();
    historyRows = historyProject.characterPatternRows(0, 0);
    test.expect(historyRows.at(0).toMap().value(QStringLiteral("pattern")).toInt()
                    == 0
                    && historyRows.at(1).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0,
                "blank should clear the bitmap while remaining undoable");
    historyProject.undoCharacterEdit();

    historyProject.setCharacterPanActive(true);
    historyProject.nudgeCharacterPan(1, 1);
    historyRows = historyProject.characterPatternRows(0, 0);
    test.expect(historyProject.characterPanActive()
                    && historyProject.characterPanX() == 1
                    && historyProject.characterPanY() == 1
                    && historyRows.at(1).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0x40
                    && historyRows.at(2).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0x10,
                "24 by 24 panning should expose a clipped live 8 by 8 center viewport");
    historyProject.centerCharacterPan();
    historyRows = historyProject.characterPatternRows(0, 0);
    test.expect(historyProject.characterPanX() == 0
                    && historyProject.characterPanY() == 0
                    && historyRows.at(0).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0x80,
                "the panning center control should restore the original live position");
    historyProject.nudgeCharacterPan(1, 1);
    historyProject.setCharacterPanActive(false);
    historyRows = historyProject.characterPatternRows(0, 0);
    test.expect(!historyProject.characterPanActive()
                    && historyProject.characterPanX() == 0
                    && historyProject.characterPanY() == 0
                    && historyRows.at(1).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0x40,
                "finishing Pan should commit the final viewport and reset its virtual origin");
    historyProject.undoCharacterEdit();
    historyRows = historyProject.characterPatternRows(0, 0);
    const bool panUndoRestoredOriginal =
        historyRows.at(0).toMap().value(QStringLiteral("pattern")).toInt() == 0x80
        && historyRows.at(1).toMap().value(QStringLiteral("pattern")).toInt() == 0x20;
    historyProject.undoCharacterEdit();
    historyRows = historyProject.characterPatternRows(0, 0);
    test.expect(panUndoRestoredOriginal
                    && historyRows.at(0).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0
                    && historyRows.at(1).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0,
                "all Pan movement should create one undo step between original and final positions");

    QClipboard* clipboard = QGuiApplication::clipboard();
    const QString previousClipboardText = clipboard != nullptr ? clipboard->text() : QString{};
    historyProject.redoCharacterEdit();
    historyProject.redoCharacterEdit();
    const bool copiedPattern = clipboard != nullptr
        && historyProject.copyActiveCharacterPattern();
    const QJsonDocument clipboardDocument = clipboard != nullptr
        ? QJsonDocument::fromJson(clipboard->text().toUtf8()) : QJsonDocument{};
    const QJsonObject clipboardObject = clipboardDocument.object();
    test.expect(copiedPattern && historyProject.canPasteCharacterPattern()
                    && clipboardObject.value(QStringLiteral("format")).toString()
                        == QStringLiteral("newconvert9918.character-pattern")
                    && clipboardObject.value(QStringLiteral("version")).toInt() == 1
                    && clipboardObject.value(QStringLiteral("bitmap")).toArray().size()
                        == 8
                    && clipboardObject.value(QStringLiteral("colors")).toArray().size()
                        == 8,
                "Copy should publish readable, versioned character-pattern JSON to the system clipboard");
    historyProject.setActiveCharacterPattern(1);
    const bool pastedPattern = historyProject.pasteActiveCharacterPattern();
    historyRows = historyProject.characterPatternRows(0, 1);
    test.expect(pastedPattern
                    && historyRows.at(1).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0x40
                    && historyRows.at(2).toMap().value(QStringLiteral("pattern")).toInt()
                        == 0x10,
                "Paste should recreate bitmap and row-color data in another active pattern");
    historyProject.undoCharacterEdit();
    historyRows = historyProject.characterPatternRows(0, 1);
    test.expect(historyRows.at(1).toMap().value(QStringLiteral("pattern")).toInt()
                    == 0
                    && historyProject.canRedoCharacter(),
                "pasting a clipboard pattern should create one undoable character edit");
    if (clipboard != nullptr) {
        clipboard->setText(QStringLiteral("not pattern json"));
        test.expect(!historyProject.canPasteCharacterPattern(),
                    "Paste should reject unrelated clipboard text");
        clipboard->setText(previousClipboardText);
    }

    EditorProjectController spriteProject(&recipeImage);
    spriteProject.setWorkspaceMode(2);
    spriteProject.setActiveSpriteSize(8);
    spriteProject.beginSpriteEdit(0, 0, 8);
    spriteProject.paintSpritePixel(0, 0, 8, 0, 0, true);
    spriteProject.paintSpritePixel(0, 0, 8, 1, 2, true);
    spriteProject.endSpriteEdit();
    QVariantList spritePixels = spriteProject.spritePatternPixels(0, 0, 8);
    test.expect(spriteProject.canUndoSprite() && spritePixels.size() == 64
                    && spritePixels.at(0).toInt() == 1
                    && spritePixels.at(10).toInt() == 1,
                "sprite pencil strokes should edit the active size bank and group into one undo entry");
    spriteProject.undoSpriteEdit();
    spritePixels = spriteProject.spritePatternPixels(0, 0, 8);
    test.expect(spriteProject.canRedoSprite() && spritePixels.at(0).toInt() == 0
                    && spritePixels.at(10).toInt() == 0,
                "sprite undo should revert an entire drawing stroke");
    spriteProject.redoSpriteEdit();
    spriteProject.setEditScope(1);
    spriteProject.setActiveSpriteSize(16);
    spriteProject.setActiveSpriteColorDepth(3);
    spriteProject.setSpriteDrawingColorIndex(6);
    spriteProject.paintSpritePixel(0, 0, 16, 0, 0, true);
    const QVariantList enhanced16 = spriteProject.spritePatternPixels(0, 0, 16);
    spriteProject.setEditScope(0);
    const QVariantList baseline16 = spriteProject.spritePatternPixels(0, 0, 16);
    test.expect(enhanced16.size() == 256 && enhanced16.at(0).toInt() == 6
                    && baseline16.at(0).toInt() == 0
                    && spriteProject.spriteGlobalSize() == 8,
                "F18A sprite pixels, per-sprite size, and color depth should remain non-destructive overrides of the 9918A baseline");
    spriteProject.setEditScope(1);
    const bool copiedSprite = clipboard != nullptr
        && spriteProject.copyActiveSpritePattern();
    const QJsonObject spriteClipboardObject = clipboard != nullptr
        ? QJsonDocument::fromJson(clipboard->text().toUtf8()).object()
        : QJsonObject{};
    spriteProject.setActiveSprite(1);
    spriteProject.setActiveSpriteSize(16);
    spriteProject.setActiveSpriteColorDepth(3);
    const bool pastedSprite = copiedSprite
        && spriteProject.pasteActiveSpritePattern();
    const QVariantList pastedSpritePixels =
        spriteProject.spritePatternPixels(0, 1, 16);
    test.expect(copiedSprite && pastedSprite
                    && spriteClipboardObject.value(QStringLiteral("format")).toString()
                        == QStringLiteral("newconvert9918.sprite-pattern")
                    && spriteClipboardObject.value(QStringLiteral("size")).toInt() == 16
                    && spriteClipboardObject.value(QStringLiteral("colorDepth")).toInt() == 3
                    && pastedSpritePixels.at(0).toInt() == 6,
                "sprite Copy/Paste should exchange readable, versioned JSON while validating size and color depth");
    if (clipboard != nullptr) clipboard->setText(previousClipboardText);
    spriteProject.moveSprite(0, 37, 21);
    spriteProject.moveSprite(1, 37, 21);
    const QVariantList overlapping = spriteProject.activeSpritePlacements();
    test.expect(overlapping.at(0).toMap().value(QStringLiteral("x")).toInt() == 37
                    && overlapping.at(1).toMap().value(QStringLiteral("x")).toInt() == 37
                    && overlapping.at(0).toMap().value(QStringLiteral("y")).toInt() == 21
                    && overlapping.at(1).toMap().value(QStringLiteral("y")).toInt() == 21,
                "sprite placement should retain one-pixel coordinates and permit overlap");

    QVariantList editorSlots = project.characterEditorSlots();
    test.expect(editorSlots.size() == 1 && project.activeCharacterEditor() == 0
                    && editorSlots.at(0).toMap().value(QStringLiteral("loaded")).toBool()
                    && editorSlots.at(0).toMap().value(QStringLiteral("patternIndex")).toInt()
                        == 255,
                "character trays should begin with one loaded active editor");
    project.addCharacterEditor();
    project.setCharacterTilingMode(true);
    project.moveCharacterTile(1, 17, 13);
    editorSlots = project.characterEditorSlots();
    test.expect(editorSlots.size() == 2 && project.activeCharacterEditor() == 1
                    && !editorSlots.at(1).toMap().value(QStringLiteral("loaded")).toBool()
                    && editorSlots.at(1).toMap().value(QStringLiteral("tileX")).toInt()
                        == 16
                    && editorSlots.at(1).toMap().value(QStringLiteral("tileY")).toInt()
                        == 16,
                "adding a character editor should create and select an empty slot");
    project.setActiveCharacterPattern(42);
    editorSlots = project.characterEditorSlots();
    test.expect(editorSlots.at(1).toMap().value(QStringLiteral("loaded")).toBool()
                    && editorSlots.at(1).toMap().value(QStringLiteral("setIndex")).toInt()
                        == 2
                    && editorSlots.at(1).toMap().value(QStringLiteral("patternIndex")).toInt()
                        == 42,
                "choosing a pattern should load it into the active empty editor");
    project.setActiveCharacterEditor(0);
    project.moveCharacterEditor(1, 0);
    editorSlots = project.characterEditorSlots();
    test.expect(project.activeCharacterEditor() == 1
                    && editorSlots.at(0).toMap().value(QStringLiteral("patternIndex")).toInt()
                        == 42
                    && editorSlots.at(1).toMap().value(QStringLiteral("patternIndex")).toInt()
                        == 255,
                "reordering should retain the same active editor and its pattern");
    project.moveCharacterEditor(0, 1);
    project.addCharacterEditor();
    project.setActiveCharacterPattern(42);
    editorSlots = project.characterEditorSlots();
    test.expect(editorSlots.size() == 3
                    && editorSlots.at(1).toMap().value(QStringLiteral("patternIndex")).toInt()
                        == 42
                    && editorSlots.at(2).toMap().value(QStringLiteral("patternIndex")).toInt()
                        == 42
                    && editorSlots.at(1).toMap().value(QStringLiteral("tileX")).toInt()
                        != editorSlots.at(2).toMap().value(QStringLiteral("tileX")).toInt(),
                "duplicate pattern tiles should share pattern identity but retain independent placement");
    project.removeActiveCharacterEditor();
    project.setActiveCharacterEditor(0);
    test.expect(project.characterEditorSlots().size() == 2
                    && project.activeCharacterPattern() == 255,
                "removing should preserve at least one editor and select a valid survivor");

    QTemporaryDir directory(
        QDir::current().filePath(QStringLiteral("interface-recipe-XXXXXX")));
    const QString recipePath = directory.filePath(QStringLiteral("roundtrip.nc9918.json"));
    const bool recipeSaved = directory.isValid()
        && project.saveRecipe(QUrl::fromLocalFile(recipePath));
    if (!recipeSaved) {
        std::cerr << "Recipe save failed: "
                  << project.errorMessage().toStdString() << '\n';
    }
    test.expect(recipeSaved && QFileInfo::exists(recipePath),
                "editor project should save a versioned recipe");

    project.setWorkspaceMode(0);
    project.setF18aEnabled(false);
    project.setActiveSpriteSet(0);
    project.setActiveSprite(0);
    project.setSpritePlacementMode(false);
    project.setPlacementWidth(256);
    project.setCharacterForegroundColorIndex(15);
    project.setCharacterBackgroundColorIndex(1);
    project.setCharacterTilingMode(false);
    project.setActiveCharacterEditor(1);
    project.moveCharacterTile(1, 80, 80);
    project.removeActiveCharacterEditor();
    project.paintCharacterPixel(2, 255, 0, 0, false);
    recipeImage.setGamma(1.0);
    recipeImage.setForegroundColor(Qt::white);

    const bool loaded = project.loadRecipe(QUrl::fromLocalFile(recipePath));
    if (!loaded) {
        std::cerr << "Recipe load failed: "
                  << project.errorMessage().toStdString() << '\n';
    }
    const QVariantList placements = project.activeSpritePlacements();
    const QVariantMap movedSprite = placements.size() > 3
        ? placements.at(3).toMap() : QVariantMap{};
    const QVariantList restoredRows = project.characterPatternRows(2, 255);
    const QVariantList restoredEditors = project.characterEditorSlots();
    const QVariantList restoredSpritePixels = project.spritePatternPixels(1, 3, 16);
    test.expect(loaded && project.workspaceMode() == 2 && project.f18aEnabled()
                    && project.previewTarget() == 2 && project.editScope() == 1
                    && project.spriteSetNames().size() == 2
                    && project.activeSpriteSet() == 1 && project.activeSprite() == 3
                    && project.spritePlacementMode() && project.placementWidth() == 320
                    && project.placementHeight() == 200
                    && movedSprite.value(QStringLiteral("x")).toInt() == 91
                    && movedSprite.value(QStringLiteral("y")).toInt() == 47
                    && movedSprite.value(QStringLiteral("size")).toInt() == 16
                    && movedSprite.value(QStringLiteral("colorDepth")).toInt() == 3
                    && project.activeSpriteSize() == 16
                    && project.spriteGlobalSize() == 16
                    && restoredSpritePixels.size() == 256
                    && restoredSpritePixels.at(0).toInt() == 1
                    && restoredSpritePixels.at(1).toInt() == 5
                    && project.activeCharacterSet() == 2
                    && project.activeCharacterPattern() == 255
                    && project.activeCharacterEditor() == 0
                    && project.characterTilingMode()
                    && restoredEditors.size() == 2
                    && restoredEditors.at(1).toMap()
                           .value(QStringLiteral("patternIndex")).toInt() == 42
                    && restoredEditors.at(1).toMap()
                           .value(QStringLiteral("tileX")).toInt() == 16
                    && restoredEditors.at(1).toMap()
                           .value(QStringLiteral("tileY")).toInt() == 16
                    && project.characterForegroundColorIndex() == 2
                    && project.characterBackgroundColorIndex() == 3
                    && restoredRows.size() == 8
                    && restoredRows.at(0).toMap()
                           .value(QStringLiteral("pattern")).toInt() == 0x81
                    && restoredRows.at(0).toMap()
                           .value(QStringLiteral("color")).toInt() == 0x23
                    && qAbs(recipeImage.gamma() - 1.75) < 0.001
                    && recipeImage.foregroundColor() == QColor(QStringLiteral("#123456"))
                    && recipeImage.sourceName() == QStringLiteral("tiny-rgba.png"),
                "recipe load should restore conversion, profile, set, placement, and source state");
}

void testResponsiveQml(TestContext& test, ImageInputController& controller)
{
    const QDir qmlDirectory(QStringLiteral(NEWCONVERT9918_QML_DIR));
    const QDir iconDirectory(qmlDirectory.filePath(QStringLiteral("../assets/icons")));
    for (const int size : {16, 24, 32, 48, 64, 128, 256, 512, 1024}) {
        const QImage icon(iconDirectory.filePath(
            QStringLiteral("NewConvert9918-%1.png").arg(size)));
        test.expect(!icon.isNull()
                        && icon.size() == QSize(size, size)
                        && icon.pixelColor(0, 0).alpha() == 0,
                    "generated application PNG should have its declared dimensions and a transparent background");
    }
    test.expect(QFileInfo::exists(
                    iconDirectory.filePath(QStringLiteral("NewConvert9918.ico")))
                    && QFileInfo::exists(
                        iconDirectory.filePath(QStringLiteral("NewConvert9918.icns"))),
                "native Windows and macOS application icons should be generated");
    const QIcon applicationIcon(
        iconDirectory.filePath(QStringLiteral("NewConvert9918-256.png")));
    test.expect(!applicationIcon.isNull(),
                "application icon PNG should load as a native Qt icon");
    const QImage watermarkImage(
        iconDirectory.filePath(QStringLiteral("NewConvert9918-watermark-512.png")));
    test.expect(!watermarkImage.isNull()
                    && watermarkImage.size() == QSize(512, 512)
                    && watermarkImage.pixelColor(0, 0).alpha() == 0,
                "preview watermark should have a transparent background");
    QGuiApplication::setWindowIcon(applicationIcon);

    EditorProjectController editorProject(&controller);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("imageInput"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("editorProject"), &editorProject);
    const QString mainQml = QDir(QStringLiteral(NEWCONVERT9918_QML_DIR))
                                .filePath(QStringLiteral("Main.qml"));
    engine.load(QUrl::fromLocalFile(mainQml));
    test.expect(engine.rootObjects().size() == 1,
                "Phase 6 QML should load as one application window");
    if (engine.rootObjects().isEmpty()) return;

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().front());
    test.expect(window != nullptr, "Phase 6 root should be a QQuickWindow");
    if (window == nullptr) return;

    const auto visiblePane = [window](const QString& objectName) -> QObject* {
        for (QObject* pane : window->findChildren<QObject*>(objectName)) {
            if (pane->property("visible").toBool()) return pane;
        }
        return nullptr;
    };

    test.expect(!window->icon().isNull(),
                "application window should load the New Convert 9918 icon");
    const auto hasConfiguredWatermark = [&](const QString& paneName) {
        QObject* pane = visiblePane(paneName);
        if (pane == nullptr) return false;
        QObject* viewport = pane->findChild<QObject*>(paneName + QStringLiteral("Viewport"));
        QObject* watermark = pane->findChild<QObject*>(paneName + QStringLiteral("Watermark"));
        if (viewport == nullptr || watermark == nullptr) return false;
        const qreal expectedSize = qMin(viewport->property("width").toReal(),
                                        viewport->property("height").toReal())
            / 2.0;
        return watermark->property("source").toUrl().isValid()
            && qAbs(watermark->property("opacity").toReal() - 0.12) <= 0.001
            && qAbs(watermark->property("width").toReal() - expectedSize) <= 1.0
            && qAbs(watermark->property("height").toReal() - expectedSize) <= 1.0;
    };
    test.expect(hasConfiguredWatermark(QStringLiteral("sourcePreview"))
                    && hasConfiguredWatermark(QStringLiteral("convertedPreview")),
                "both preview panes should carry a subdued half-size logo watermark");

    test.expect(window->findChild<QObject*>(QStringLiteral("mainMenuBar")) != nullptr,
                "application should expose its commands through a menu bar");
    const QObject* exitAction =
        window->findChild<QObject*>(QStringLiteral("exitAction"));
    const QObject* reloadAction =
        window->findChild<QObject*>(QStringLiteral("reloadAction"));
    test.expect(exitAction != nullptr
                    && exitAction->property("text").toString() == QStringLiteral("E&xit")
                    && window->findChild<QObject*>(QStringLiteral("exitMenuSeparator"))
                        != nullptr
                    && window->findChild<QObject*>(QStringLiteral("exitMenuItem")) != nullptr,
                "File menu should end with a separated Exit command");
    test.expect(reloadAction != nullptr && reloadAction->property("enabled").toBool()
                    && window->findChild<QObject*>(QStringLiteral("reloadMenuItem")) != nullptr,
                "File menu should expose Reload for a file-backed source");
    test.expect(window->property("previewLayout").toInt() == 1,
                "horizontal split should remain the default preview layout");
    test.expect(window->property("conversionPanelVisible").toBool()
                    && window->property("conversionPanelMode").toInt() == 0,
                "Side Panel should default to a visible adjacent panel");
    const QObject* screenImageModeItem =
        window->findChild<QObject*>(QStringLiteral("screenImageModeMenuItem"));
    const QObject* characterModeItem =
        window->findChild<QObject*>(QStringLiteral("characterEditorModeMenuItem"));
    const QObject* spriteModeItem =
        window->findChild<QObject*>(QStringLiteral("spriteEditorModeMenuItem"));
    const QObject* showSidePanelItem =
        window->findChild<QObject*>(QStringLiteral("showSidePanelMenuItem"));
    const QObject* sidePanelPlacementMenu =
        window->findChild<QObject*>(QStringLiteral("sidePanelPlacementMenu"));
    const QObject* exportAction =
        window->findChild<QObject*>(QStringLiteral("exportAction"));
    test.expect(window->property("workspaceMode").toInt() == 0
                    && screenImageModeItem != nullptr
                    && screenImageModeItem->property("checked").toBool()
                    && characterModeItem != nullptr
                    && !characterModeItem->property("checked").toBool()
                    && spriteModeItem != nullptr
                    && !spriteModeItem->property("checked").toBool(),
                "Mode menu should start with Screen Image exclusively selected");
    test.expect(showSidePanelItem != nullptr
                    && showSidePanelItem->property("text").toString().contains(
                        QStringLiteral("Side Panel"))
                    && sidePanelPlacementMenu != nullptr
                    && sidePanelPlacementMenu->property("title").toString().contains(
                        QStringLiteral("Side Panel")),
                "View should use Side Panel terminology");
    test.expect(window->findChild<QObject*>(QStringLiteral("loadRecipeAction")) != nullptr
                    && window->findChild<QObject*>(QStringLiteral("saveRecipeAction")) != nullptr
                    && window->findChild<QObject*>(
                           QStringLiteral("screenImageOutputProfilesGroup")) != nullptr,
                "File and Screen Image controls should expose the shared recipe and output-profile workflow");

    const auto hasDestinationShell = [](QObject* pane, const QString& paneName,
                                        const QString& expectedTitle) {
        if (pane == nullptr) return false;
        const QObject* title = pane->findChild<QObject*>(paneName + QStringLiteral("Title"));
        const QObject* toolbar = pane->findChild<QObject*>(
            paneName + QStringLiteral("DrawingToolbarSlot"));
        const QObject* viewport = pane->findChild<QObject*>(
            paneName + QStringLiteral("Viewport"));
        const QObject* controls = pane->findChild<QObject*>(
            paneName + QStringLiteral("Controls"));
        const QObject* zoom = pane->findChild<QObject*>(
            paneName + QStringLiteral("ZoomControls"));
        const QObject* zoomMenuButton = pane->findChild<QObject*>(
            paneName + QStringLiteral("ZoomMenuButton"));
        return title != nullptr && title->property("text").toString() == expectedTitle
            && toolbar != nullptr && viewport != nullptr && controls != nullptr
            && zoom != nullptr && zoomMenuButton != nullptr
            && !zoomMenuButton->property("enabled").toBool();
    };

    window->setProperty("workspaceMode", 1);
    test.expect(waitFor([&] {
                    return hasDestinationShell(
                               visiblePane(QStringLiteral("characterEditorWorkspace")),
                               QStringLiteral("characterEditorWorkspace"),
                               QStringLiteral("Character Editor"))
                        && visiblePane(QStringLiteral("characterEditorSidePanel")) != nullptr
                        && characterModeItem->property("checked").toBool()
                        && !screenImageModeItem->property("checked").toBool()
                        && exportAction != nullptr
                        && !exportAction->property("enabled").toBool();
                }),
                "Character Editor mode should replace the destination and Side Panel safely");
    QObject* characterPane = visiblePane(QStringLiteral("characterEditorWorkspace"));
    QObject* characterZoomMenu = characterPane != nullptr
        ? characterPane->findChild<QObject*>(
              QStringLiteral("characterEditorWorkspaceZoomMenuButton"))
        : nullptr;
    QObject* characterZoomIn = characterPane != nullptr
        ? characterPane->findChild<QObject*>(
              QStringLiteral("characterEditorWorkspaceZoomInButton"))
        : nullptr;
    QObject* characterZoomOut = characterPane != nullptr
        ? characterPane->findChild<QObject*>(
              QStringLiteral("characterEditorWorkspaceZoomOutButton"))
        : nullptr;
    QObject* characterWheelZoom = characterPane != nullptr
        ? characterPane->findChild<QObject*>(
              QStringLiteral("characterEditorWorkspaceWheelZoomHandler"))
        : nullptr;
    QObject* characterToolbar = characterPane != nullptr
        ? characterPane->findChild<QObject*>(
              QStringLiteral("characterEditorWorkspaceDrawingToolbarSlot"))
        : nullptr;
    QObject* characterPencil = characterToolbar != nullptr
        ? characterToolbar->findChild<QObject*>(
              QStringLiteral("characterPatternPencilButton"))
        : nullptr;
    QObject* characterEraser = characterToolbar != nullptr
        ? characterToolbar->findChild<QObject*>(
              QStringLiteral("characterPatternEraserButton"))
        : nullptr;
    QObject* characterColors = characterToolbar != nullptr
        ? characterToolbar->findChild<QObject*>(
              QStringLiteral("characterPatternColorControl"))
        : nullptr;
    QObject* characterRotate = characterToolbar != nullptr
        ? characterToolbar->findChild<QObject*>(
              QStringLiteral("rotateCharacterPatternButton"))
        : nullptr;
    QObject* characterMirror = characterToolbar != nullptr
        ? characterToolbar->findChild<QObject*>(
              QStringLiteral("mirrorCharacterPatternButton"))
        : nullptr;
    QObject* characterFlip = characterToolbar != nullptr
        ? characterToolbar->findChild<QObject*>(
              QStringLiteral("flipCharacterPatternButton"))
        : nullptr;
    QObject* characterBlank = characterToolbar != nullptr
        ? characterToolbar->findChild<QObject*>(
              QStringLiteral("blankCharacterPatternButton"))
        : nullptr;
    QObject* characterCopy = characterToolbar != nullptr
        ? characterToolbar->findChild<QObject*>(
              QStringLiteral("copyCharacterPatternButton"))
        : nullptr;
    QObject* characterPaste = characterToolbar != nullptr
        ? characterToolbar->findChild<QObject*>(
              QStringLiteral("pasteCharacterPatternButton"))
        : nullptr;
    QObject* characterSetGridButton = characterToolbar != nullptr
        ? characterToolbar->findChild<QObject*>(
              QStringLiteral("characterEditorWorkspaceSetGridButton"))
        : nullptr;
    QObject* characterTilingButton = characterToolbar != nullptr
        ? characterToolbar->findChild<QObject*>(
              QStringLiteral("characterTilingButton"))
        : nullptr;
    QObject* patternEditorModeIcon = characterTilingButton != nullptr
        ? characterTilingButton->findChild<QObject*>(
              QStringLiteral("patternEditorModeIcon"))
        : nullptr;
    QObject* tilingScreenModeIcon = characterTilingButton != nullptr
        ? characterTilingButton->findChild<QObject*>(
              QStringLiteral("tilingScreenModeIcon"))
        : nullptr;
    QObject* characterDetailsTrailing = characterPane != nullptr
        ? characterPane->findChild<QObject*>(
              QStringLiteral("characterEditorWorkspaceDetailsTrailing"))
        : nullptr;
    QObject* characterControls = characterPane != nullptr
        ? characterPane->findChild<QObject*>(
              QStringLiteral("characterEditorWorkspaceControls"))
        : nullptr;
    QObject* characterSelectionRow = characterPane != nullptr
        ? characterPane->findChild<QObject*>(
              QStringLiteral("characterPatternSelectionRow"))
        : nullptr;
    QObject* characterSetCombo = characterPane != nullptr
        ? characterPane->findChild<QObject*>(QStringLiteral("characterSetComboBox"))
        : nullptr;
    QObject* characterPatternSpin = characterPane != nullptr
        ? characterPane->findChild<QObject*>(QStringLiteral("characterPatternSpinBox"))
        : nullptr;
    QObject* characterPatternGrid = characterPane != nullptr
        ? characterPane->findChild<QObject*>(QStringLiteral("characterPatternGrid"))
        : nullptr;
    QObject* characterEditorTray = characterPane != nullptr
        ? characterPane->findChild<QObject*>(
              QStringLiteral("characterPatternEditorTray"))
        : nullptr;
    QObject* characterEditorGrid = characterPane != nullptr
        ? characterPane->findChild<QObject*>(
              QStringLiteral("characterPatternEditorGrid"))
        : nullptr;
    QObject* characterTilingView = characterPane != nullptr
        ? characterPane->findChild<QObject*>(QStringLiteral("characterTilingView"))
        : nullptr;
    QObject* characterTilingGrid = characterPane != nullptr
        ? characterPane->findChild<QObject*>(
              QStringLiteral("characterTilingScreenGrid"))
        : nullptr;
    QObject* addCharacterEditor = characterPane != nullptr
        ? characterPane->findChild<QObject*>(
              QStringLiteral("addCharacterEditorButton"))
        : nullptr;
    QObject* removeCharacterEditor = characterPane != nullptr
        ? characterPane->findChild<QObject*>(
              QStringLiteral("removeCharacterEditorButton"))
        : nullptr;
    QObject* characterLowerToolbar = characterPane != nullptr
        ? characterPane->findChild<QObject*>(
              QStringLiteral("characterEditorWorkspaceCustomLowerToolbar"))
        : nullptr;
    QObject* characterUndo = characterPane != nullptr
        ? characterPane->findChild<QObject*>(QStringLiteral("undoCharacterEditButton"))
        : nullptr;
    QObject* characterRedo = characterPane != nullptr
        ? characterPane->findChild<QObject*>(QStringLiteral("redoCharacterEditButton"))
        : nullptr;
    QObject* characterPan = characterPane != nullptr
        ? characterPane->findChild<QObject*>(QStringLiteral("characterPanButton"))
        : nullptr;
    QObject* characterPanLeft = characterPane != nullptr
        ? characterPane->findChild<QObject*>(QStringLiteral("characterPanLeftButton"))
        : nullptr;
    QObject* characterPanUp = characterPane != nullptr
        ? characterPane->findChild<QObject*>(QStringLiteral("characterPanUpButton"))
        : nullptr;
    QObject* characterPanCenter = characterPane != nullptr
        ? characterPane->findChild<QObject*>(QStringLiteral("characterPanCenterButton"))
        : nullptr;
    QObject* characterPanDown = characterPane != nullptr
        ? characterPane->findChild<QObject*>(QStringLiteral("characterPanDownButton"))
        : nullptr;
    QObject* characterPanRight = characterPane != nullptr
        ? characterPane->findChild<QObject*>(QStringLiteral("characterPanRightButton"))
        : nullptr;
    if (characterPane != nullptr) {
        characterPane->setProperty("fitToView", false);
        characterPane->setProperty("manualZoom", 1.0);
        const QVariant zoomFactor{2.0};
        QMetaObject::invokeMethod(characterPane, "zoomBy",
                                  Q_ARG(QVariant, zoomFactor));
    }
    test.expect(characterPane != nullptr
                    && characterPatternGrid != nullptr
                    && characterEditorTray != nullptr
                    && characterEditorGrid != nullptr
                    && characterEditorTray->property("renderedEditorCount").toInt()
                        == 1
                    && !characterPane->property("zoomInteractive").toBool()
                    && qAbs(characterPane->property("effectiveZoom").toReal() - 1.0)
                        < 0.001
                    && qAbs(characterPane->property("manualZoom").toReal() - 1.0)
                        < 0.001
                    && characterZoomMenu != nullptr
                    && characterZoomMenu->property("text").toString().contains(
                           QStringLiteral("100%"))
                    && !characterZoomMenu->property("enabled").toBool()
                    && characterZoomIn != nullptr
                    && !characterZoomIn->property("enabled").toBool()
                    && characterZoomOut != nullptr
                    && !characterZoomOut->property("enabled").toBool()
                    && characterWheelZoom != nullptr
                    && !characterWheelZoom->property("enabled").toBool()
                    && addCharacterEditor != nullptr
                    && removeCharacterEditor != nullptr
                    && !removeCharacterEditor->property("enabled").toBool()
                    && characterToolbar != nullptr
                    && characterPencil != nullptr && characterEraser != nullptr
                    && characterColors != nullptr
                    && characterRotate != nullptr && characterRotate->property("enabled").toBool()
                    && characterMirror != nullptr && characterMirror->property("enabled").toBool()
                    && characterFlip != nullptr && characterFlip->property("enabled").toBool()
                    && characterBlank != nullptr && characterBlank->property("enabled").toBool()
                    && characterCopy != nullptr && characterCopy->property("enabled").toBool()
                    && characterPaste != nullptr
                    && characterPaste->property("enabled").toBool()
                        == editorProject.canPasteCharacterPattern()
                    && characterSetGridButton != nullptr
                    && !characterSetGridButton->property("visible").toBool()
                    && characterTilingButton != nullptr
                    && !characterTilingButton->property("checked").toBool()
                    && characterTilingButton->property("modeHint").toString()
                        == QStringLiteral("Change to Tiling Screen")
                    && patternEditorModeIcon != nullptr
                    && tilingScreenModeIcon != nullptr
                    && patternEditorModeIcon->property("z").toReal()
                        > tilingScreenModeIcon->property("z").toReal()
                    && characterTilingView != nullptr
                    && !characterTilingView->property("visible").toBool()
                    && characterPencil->property("implicitWidth").toInt() == 26
                    && characterEraser->property("implicitWidth").toInt() == 26
                    && characterColors->property("implicitWidth").toInt() == 26
                    && characterDetailsTrailing != nullptr
                    && characterDetailsTrailing->property("text").toString().contains(
                           QStringLiteral("9918A baseline"))
                    && characterControls != nullptr
                    && characterDetailsTrailing->property("y").toReal()
                        < characterControls->property("y").toReal()
                    && characterSelectionRow != nullptr
                    && characterSetCombo != nullptr
                    && qRound(characterSetCombo->property("width").toReal()) == 55
                    && characterSetCombo->property("currentIndex").toInt()
                        == editorProject.activeCharacterSet()
                    && characterPatternSpin != nullptr
                    && qRound(characterPatternSpin->property("width").toReal()) == 48
                    && characterPatternSpin->property("value").toInt()
                        == editorProject.activeCharacterPattern()
                    && characterSelectionRow->property("y").toReal()
                        < characterPatternGrid->property("y").toReal()
                    && characterPane->findChild<QObject*>(
                           QStringLiteral("characterSetTabs")) == nullptr
                    && characterLowerToolbar != nullptr
                    && characterLowerToolbar->property("visible").toBool()
                    && characterUndo != nullptr && characterRedo != nullptr
                    && characterPan != nullptr && characterPan->property("enabled").toBool()
                    && characterPanLeft != nullptr && characterPanUp != nullptr
                    && characterPanCenter != nullptr && characterPanDown != nullptr
                    && characterPanRight != nullptr
                    && !characterPanLeft->property("enabled").toBool()
                    && !characterPanUp->property("enabled").toBool()
                    && !characterPanCenter->property("enabled").toBool()
                    && !characterPanDown->property("enabled").toBool()
                    && !characterPanRight->property("enabled").toBool()
                    && window->findChild<QObject*>(
                           QStringLiteral("characterEditorSidePanelOutputProfilesGroup"))
                        != nullptr,
                "Character Editor should expose its pattern tools in the upper toolbar, a reusable drawing tray, a 256-pattern set, and dual-profile controls");
    const bool panToggledOn = QMetaObject::invokeMethod(characterPan, "click");
    test.expect(panToggledOn && waitFor([&] {
                    return editorProject.characterPanActive()
                        && characterPan->property("checked").toBool()
                        && characterPanLeft->property("enabled").toBool()
                        && characterPanUp->property("enabled").toBool()
                        && characterPanDown->property("enabled").toBool()
                        && characterPanRight->property("enabled").toBool()
                        && !characterPencil->property("enabled").toBool()
                        && !characterRotate->property("enabled").toBool();
                }),
                "Pan should enable four-way positioning and suspend destructive pattern tools");
    const bool panRightInvoked = QMetaObject::invokeMethod(characterPanRight, "click");
    test.expect(panRightInvoked && waitFor([&] {
                    return editorProject.characterPanX() == 1
                        && characterPanCenter->property("enabled").toBool();
                }),
                "the lower position controls should move through the virtual grid");
    const bool panCentered = QMetaObject::invokeMethod(characterPanCenter, "click");
    test.expect(panCentered && waitFor([&] {
                    return editorProject.characterPanX() == 0
                        && editorProject.characterPanY() == 0
                        && !characterPanCenter->property("enabled").toBool();
                }),
                "the lower center control should restore the live pattern origin");
    const bool panToggledOff = QMetaObject::invokeMethod(characterPan, "click");
    test.expect(panToggledOff && waitFor([&] {
                    return !editorProject.characterPanActive()
                        && !characterPan->property("checked").toBool()
                        && editorProject.characterPanX() == 0
                        && editorProject.characterPanY() == 0
                        && characterPencil->property("enabled").toBool()
                        && characterRotate->property("enabled").toBool();
                }),
                "finishing Pan should reset its controls and restore drawing tools");
    const auto currentPreviewScale = [&] {
        QVariant scale;
        if (characterEditorTray == nullptr
            || !QMetaObject::invokeMethod(characterEditorTray,
                                          "currentActivePreviewScale",
                                          Q_RETURN_ARG(QVariant, scale))) {
            return 0;
        }
        return scale.toInt();
    };
    const auto changePreviewScale = [&](const char* method, int expected) {
        return characterEditorTray != nullptr
            && QMetaObject::invokeMethod(characterEditorTray, method)
            && currentPreviewScale() == expected;
    };
    const bool previewScaleCycle = characterEditorTray != nullptr
        && currentPreviewScale() == 4
        && changePreviewScale("decreaseActivePreviewScale", 3)
        && changePreviewScale("decreaseActivePreviewScale", 2)
        && changePreviewScale("decreaseActivePreviewScale", 1)
        && changePreviewScale("decreaseActivePreviewScale", 1)
        && changePreviewScale("increaseActivePreviewScale", 2)
        && changePreviewScale("increaseActivePreviewScale", 3)
        && changePreviewScale("increaseActivePreviewScale", 4)
        && changePreviewScale("increaseActivePreviewScale", 4);
    test.expect(previewScaleCycle,
                "pattern previews should cycle through bounded 1x, 2x, 3x, and 4x sizes");
    editorProject.setCharacterTilingMode(true);
    test.expect(waitFor([&] {
                    return characterTilingView != nullptr
                        && characterTilingView->property("visible").toBool()
                        && characterTilingView->property("renderedTileCount").toInt()
                            == 1
                        && characterTilingGrid != nullptr
                        && qRound(characterTilingGrid->property("width").toReal())
                            == 256
                        && qRound(characterTilingGrid->property("height").toReal())
                            == 192
                        && characterTilingButton->property("checked").toBool()
                        && characterTilingButton->property("modeHint").toString()
                            == QStringLiteral("Change to Pattern Editor")
                        && tilingScreenModeIcon->property("z").toReal()
                            > patternEditorModeIcon->property("z").toReal()
                        && !characterEditorGrid->property("visible").toBool();
                }),
                "Tiling should replace the editor tray with a 256 by 192 character grid");
    const qreal initialTilingHeight = characterEditorTray->property("height").toReal();
    const QVariant tilingZoomFactor{1.25};
    const bool tilingZoomInvoked = QMetaObject::invokeMethod(
        characterPane, "zoomBy", Q_ARG(QVariant, tilingZoomFactor));
    test.expect(tilingZoomInvoked && waitFor([&] {
                    return characterPane->property("zoomInteractive").toBool()
                        && characterZoomMenu->property("enabled").toBool()
                        && characterZoomIn->property("enabled").toBool()
                        && characterZoomOut->property("enabled").toBool()
                        && characterWheelZoom->property("enabled").toBool()
                        && qAbs(characterPane->property("effectiveZoom").toReal()
                                - 1.25) < 0.001
                        && qRound(characterTilingGrid->property("width").toReal())
                            == 320
                        && qRound(characterTilingGrid->property("height").toReal())
                            == 240
                        && characterEditorTray->property("height").toReal()
                            > initialTilingHeight;
                }),
                "Character zoom should scale the screen grid and grow the Tiling panel height");
    const QVariant tilingZoomResetFactor{0.8};
    QMetaObject::invokeMethod(characterPane, "zoomBy",
                              Q_ARG(QVariant, tilingZoomResetFactor));
    const bool tilingToggledOff = QMetaObject::invokeMethod(
        characterTilingButton, "click");
    test.expect(tilingToggledOff && waitFor([&] {
                    return !editorProject.characterTilingMode()
                        && characterEditorGrid->property("visible").toBool()
                        && !characterTilingView->property("visible").toBool()
                        && characterTilingButton->property("modeHint").toString()
                            == QStringLiteral("Change to Tiling Screen")
                        && patternEditorModeIcon->property("z").toReal()
                            > tilingScreenModeIcon->property("z").toReal()
                        && !characterPane->property("zoomInteractive").toBool()
                        && qAbs(characterPane->property("effectiveZoom").toReal() - 1.0)
                            < 0.001;
                }),
                "deselecting the Tiling button should return to Pattern Edit mode");
    const bool tilingToggledOn = QMetaObject::invokeMethod(
        characterTilingButton, "click");
    test.expect(tilingToggledOn && waitFor([&] {
                    return editorProject.characterTilingMode()
                        && characterTilingView->property("visible").toBool()
                        && characterTilingButton->property("modeHint").toString()
                            == QStringLiteral("Change to Pattern Editor")
                        && tilingScreenModeIcon->property("z").toReal()
                            > patternEditorModeIcon->property("z").toReal();
                }),
                "selecting the Tiling button should restore screen placement mode");
    editorProject.addCharacterEditor();
    test.expect(waitFor([&] {
                    return characterEditorTray != nullptr
                        && characterEditorTray->property("renderedEditorCount").toInt()
                            == 2
                        && characterEditorTray->property("activeEditorEmpty").toBool()
                        && characterTilingView->property("renderedTileCount").toInt()
                            == 2;
                }),
                "the shared add tool should create an active empty tile in Tiling view");
    const int sharedPattern = editorProject.activeCharacterPattern();
    editorProject.setActiveCharacterPattern(sharedPattern);
    editorProject.moveCharacterTile(1, 13, 19);
    const QVariantList tiledSlots = editorProject.characterEditorSlots();
    test.expect(waitFor([&] {
                    return characterTilingView->property("relatedTileCount").toInt()
                            == 2
                        && characterEditorTray->property("renderedEditorCount").toInt()
                            == 1;
                })
                    && tiledSlots.size() == 2
                    && tiledSlots.at(1).toMap().value(QStringLiteral("tileX")).toInt()
                        == 16
                    && tiledSlots.at(1).toMap().value(QStringLiteral("tileY")).toInt()
                        == 16,
                "duplicate pattern tiles should highlight together, snap independently, and share one Pattern Editor");
    editorProject.setCharacterTilingMode(false);
    test.expect(waitFor([&] {
                    return characterEditorGrid->property("visible").toBool()
                        && !characterTilingView->property("visible").toBool()
                        && editorProject.activeCharacterEditor() == 1
                        && editorProject.activeCharacterPattern() == sharedPattern
                        && characterEditorTray->property("renderedEditorCount").toInt()
                            == 1;
                }),
                "switching presentations should preserve the active tile while filtering duplicate pattern editors");
    editorProject.removeActiveCharacterEditor();
    window->setProperty("workspaceMode", 2);
    test.expect(waitFor([&] {
                    return hasDestinationShell(
                               visiblePane(QStringLiteral("spriteEditorWorkspace")),
                               QStringLiteral("spriteEditorWorkspace"),
                               QStringLiteral("Sprite Editor"))
                        && visiblePane(QStringLiteral("spriteEditorSidePanel")) != nullptr
                        && spriteModeItem->property("checked").toBool()
                        && !characterModeItem->property("checked").toBool();
                }),
                "Sprite Editor mode should replace the destination and Side Panel safely");
    QObject* spritePane = visiblePane(QStringLiteral("spriteEditorWorkspace"));
    QObject* spritePatternEditor = visiblePane(QStringLiteral("spritePatternEditor"));
    QObject* sprite8Grid = visiblePane(QStringLiteral("sprite8PatternGrid"));
    QObject* sprite16Grid = visiblePane(QStringLiteral("sprite16PatternGrid"));
    QObject* spritePencil = visiblePane(QStringLiteral("spritePatternPencilButton"));
    QObject* spriteModeButton = visiblePane(QStringLiteral("spritePlacementModeButton"));
    QObject* spriteSizeControl = visiblePane(
        QStringLiteral("spriteEditorSidePanelActiveSpriteSizeComboBox"));
    QObject* spriteDepthControl = visiblePane(
        QStringLiteral("spriteEditorSidePanelSpriteColorDepthComboBox"));
    test.expect(spritePane != nullptr && spritePatternEditor != nullptr
                    && sprite8Grid != nullptr && sprite16Grid != nullptr
                    && spritePencil != nullptr && spriteModeButton != nullptr
                    && spriteSizeControl != nullptr && spriteDepthControl != nullptr,
                "Sprite Editor should mirror the pattern workflow with an editor, tools, both 32-pattern banks, and chipset controls");
    const bool spritePlacementToggled = spriteModeButton != nullptr
        && QMetaObject::invokeMethod(spriteModeButton, "click");
    const bool placementVisible = waitFor([&] {
                    return visiblePane(QStringLiteral("spritePlacementWorkspace")) != nullptr;
                });
    if (!placementVisible) {
        const auto placements = window->findChildren<QObject*>(
            QStringLiteral("spritePlacementWorkspace"));
        std::cerr << "Placement workspace state: instances=" << placements.size()
                  << " mode=" << editorProject.spritePlacementMode() << '\n';
    }
    QObject* spritePlacementBox = visiblePane(QStringLiteral("spritePlacementBox"));
    const QVariant spriteZoomFactor{1.25};
    const bool spriteZoomInvoked = spritePane != nullptr
        && QMetaObject::invokeMethod(spritePane, "zoomBy",
                                     Q_ARG(QVariant, spriteZoomFactor));
    test.expect(spritePlacementToggled && placementVisible
                    && editorProject.activeSpritePlacements().size() == 32
                    && spritePlacementBox != nullptr && spriteZoomInvoked
                    && waitFor([&] {
                        return spritePane->property("zoomInteractive").toBool()
                            && qRound(spritePlacementBox->property("width").toReal())
                                == 320
                            && qRound(spritePlacementBox->property("height").toReal())
                                == 240;
                    }),
                "Sprite placement should overlap 32 sprites on a zoomable pixel-coordinate screen");
    const bool spriteEditorToggled = QMetaObject::invokeMethod(
        spriteModeButton, "click");
    test.expect(spriteEditorToggled && waitFor([&] {
                    return !editorProject.spritePlacementMode()
                        && visiblePane(QStringLiteral("spritePatternEditor")) != nullptr
                        && !spritePane->property("zoomInteractive").toBool()
                        && qAbs(spritePane->property("effectiveZoom").toReal() - 1.0)
                            < 0.001;
                }),
                "the shared Sprite Editor/Placement control should return to pixel editing and reset workspace zoom");
    window->setProperty("workspaceMode", 0);
    test.expect(waitFor([&] {
                    return visiblePane(QStringLiteral("convertedPreview")) != nullptr
                        && visiblePane(QStringLiteral("screenImageSidePanel")) != nullptr
                        && screenImageModeItem->property("checked").toBool();
                }),
                "Screen Image mode should restore conversion preview and controls");

    const auto verifyCollapsedSection = [&](const QString& toggleName,
                                            const QString& groupName,
                                            const QString& backgroundName,
                                            const char* existsMessage,
                                            const char* colorMessage,
                                            const char* collapsedMessage,
                                            const char* expandedMessage) {
        QObject* sectionToggle = nullptr;
        for (QObject* toggle : window->findChildren<QObject*>(toggleName)) {
            if (toggle->property("visible").toBool()) {
                sectionToggle = toggle;
                break;
            }
        }
        auto* sectionGroup = sectionToggle != nullptr
            ? sectionToggle->parent()->findChild<QObject*>(
                groupName, Qt::FindDirectChildrenOnly)
            : nullptr;
        const QObject* sectionBackground = sectionToggle != nullptr
            ? sectionToggle->findChild<QObject*>(backgroundName)
            : nullptr;
        const QColor sectionColor = sectionBackground != nullptr
            ? sectionBackground->property("color").value<QColor>()
            : QColor{};
        test.expect(sectionToggle != nullptr && sectionGroup != nullptr, existsMessage);
        test.expect(sectionColor.isValid() && sectionColor.blue() > sectionColor.red(),
                    colorMessage);
        if (sectionToggle == nullptr || sectionGroup == nullptr) return;

        test.expect(!sectionGroup->property("visible").toBool(), collapsedMessage);
        sectionToggle->setProperty("checked", true);
        test.expect(waitFor([&] {
                        const qreal gap = sectionGroup->property("y").toReal()
                            - sectionToggle->property("y").toReal()
                            - sectionToggle->property("height").toReal();
                        return sectionGroup->property("visible").toBool()
                            && sectionGroup->property("title").toString().isEmpty()
                            && gap >= 0.0 && gap <= 4.5;
                    }),
                    expandedMessage);
        sectionToggle->setProperty("checked", false);
        test.expect(waitFor([&] { return !sectionGroup->property("visible").toBool(); }),
                    collapsedMessage);
    };

    QObject* artStyleToggle = nullptr;
    for (QObject* toggle : window->findChildren<QObject*>(
             QStringLiteral("artStyleSettingsToggle"))) {
        if (toggle->property("visible").toBool()) {
            artStyleToggle = toggle;
            break;
        }
    }
    auto* artStyleGroup = artStyleToggle != nullptr
        ? artStyleToggle->parent()->findChild<QObject*>(
            QStringLiteral("recommendedSettingsGroup"), Qt::FindDirectChildrenOnly)
        : nullptr;
    const QObject* artStyleBackground = artStyleToggle != nullptr
        ? artStyleToggle->findChild<QObject*>(
              QStringLiteral("artStyleSettingsToggleBackground"))
        : nullptr;
    const QColor artStyleColor = artStyleBackground != nullptr
        ? artStyleBackground->property("color").value<QColor>()
        : QColor{};
    test.expect(artStyleToggle != nullptr && artStyleGroup != nullptr,
                "conversion panel should expose its Art Style section");
    test.expect(artStyleToggle != nullptr
                    && artStyleToggle->property("checked").toBool()
                    && artStyleToggle->property("text").toString().contains(
                        QStringLiteral("Art Style"))
                    && artStyleGroup != nullptr
                    && artStyleGroup->property("visible").toBool()
                    && artStyleGroup->property("title").toString().isEmpty(),
                "Art Style should replace Recommended starting point and start expanded");
    test.expect(artStyleColor.isValid() && artStyleColor.blue() > artStyleColor.red(),
                "Art Style expander should use a light-blue background");
    if (artStyleToggle != nullptr && artStyleGroup != nullptr) {
        artStyleToggle->setProperty("checked", false);
        test.expect(waitFor([&] { return !artStyleGroup->property("visible").toBool(); }),
                    "Art Style should collapse from its header");
        artStyleToggle->setProperty("checked", true);
    }

    verifyCollapsedSection(QStringLiteral("commonSettingsToggle"),
                           QStringLiteral("commonSettingsGroup"),
                           QStringLiteral("commonSettingsToggleBackground"),
                           "conversion panel should expose its common settings section",
                           "Common settings expander should use a light-blue background",
                           "common settings should collapse from their header",
                           "expanded common settings should have no redundant title and a tight gap");
    verifyCollapsedSection(QStringLiteral("framingSettingsToggle"),
                           QStringLiteral("framingSettingsGroup"),
                           QStringLiteral("framingSettingsToggleBackground"),
                           "conversion panel should expose its framing and scale section",
                           "Framing and scale expander should use a light-blue background",
                           "framing and scale settings should collapse from their header",
                           "expanded framing settings should have no redundant title and a tight gap");
    verifyCollapsedSection(QStringLiteral("workingPaletteSettingsToggle"),
                           QStringLiteral("workingPaletteSettingsGroup"),
                           QStringLiteral("workingPaletteSettingsToggleBackground"),
                           "conversion panel should expose its working palette section",
                           "Working palette expander should use a light-blue background",
                           "working palette settings should collapse from their header",
                           "expanded working palette should have no redundant title and a tight gap");
    verifyCollapsedSection(QStringLiteral("exportSettingsToggle"),
                           QStringLiteral("exportSettingsGroup"),
                           QStringLiteral("exportSettingsToggleBackground"),
                           "conversion panel should expose its export section",
                           "Export expander should use a light-blue background",
                           "export settings should collapse from their header",
                           "expanded export settings should have no redundant title and a tight gap");

    QObject* advancedToggle = nullptr;
    for (QObject* toggle : window->findChildren<QObject*>(
             QStringLiteral("advancedSettingsToggle"))) {
        if (toggle->property("visible").toBool()) {
            advancedToggle = toggle;
            break;
        }
    }
    auto* advancedGroup = advancedToggle != nullptr
        ? advancedToggle->parent()->findChild<QObject*>(
            QStringLiteral("advancedSettingsGroup"), Qt::FindDirectChildrenOnly)
        : nullptr;
    const QObject* advancedToggleBackground = advancedToggle != nullptr
        ? advancedToggle->findChild<QObject*>(
              QStringLiteral("advancedSettingsToggleBackground"))
        : nullptr;
    const QColor advancedToggleColor = advancedToggleBackground != nullptr
        ? advancedToggleBackground->property("color").value<QColor>()
        : QColor{};
    test.expect(advancedToggle != nullptr && advancedGroup != nullptr,
                "conversion panel should expose its advanced settings section");
    test.expect(advancedToggleColor.isValid()
                    && advancedToggleColor.blue() > advancedToggleColor.red(),
                "Advanced Settings expander should use a light-blue background");
    if (advancedToggle != nullptr && advancedGroup != nullptr) {
        advancedToggle->setProperty("checked", true);
        const QObject* errorAccumulationCombo = advancedGroup->findChild<QObject*>(
            QStringLiteral("errorAccumulationCombo"));
        const QObject* ditherModeCombo = window->findChild<QObject*>(
            QStringLiteral("ditherModeCombo"));
        const QObject* orderedMapSizeCombo = advancedGroup->findChild<QObject*>(
            QStringLiteral("orderedDitherMapSizeCombo"));
        const QObject* errorWeightGrid = advancedGroup->findChild<QObject*>(
            QStringLiteral("errorWeightGrid"));
        const QObject* errorRightSpinBox = advancedGroup->findChild<QObject*>(
            QStringLiteral("errorRightSpinBox"));
        const QObject* errorWeightTotalLabel = advancedGroup->findChild<QObject*>(
            QStringLiteral("errorWeightTotalLabel"));
        const QObject* orderedBrightnessSlider = advancedGroup->findChild<QObject*>(
            QStringLiteral("orderedBrightnessSlider"));
        const QObject* maximumColorShiftSlider = advancedGroup->findChild<QObject*>(
            QStringLiteral("maximumColorShiftSlider"));
        const QObject* perceptualWeightsGrid = advancedGroup->findChild<QObject*>(
            QStringLiteral("perceptualWeightsGrid"));
        const QObject* restorePerceptualWeightsButton = advancedGroup->findChild<QObject*>(
            QStringLiteral("restorePerceptualWeightsButton"));
        const QObject* paletteSelectionCombo = advancedGroup->findChild<QObject*>(
            QStringLiteral("paletteSelectionCombo"));
        const QObject* scanlineStaticColorCount = advancedGroup->findChild<QObject*>(
            QStringLiteral("scanlineStaticColorCountSpinBox"));
        test.expect(waitFor([&] {
                        const qreal gap = advancedGroup->property("y").toReal()
                            - advancedToggle->property("y").toReal()
                            - advancedToggle->property("height").toReal();
                        return advancedGroup->property("visible").toBool()
                            && advancedGroup->property("title").toString().isEmpty()
                            && gap >= 0.0 && gap <= 4.5;
                    }),
                    "expanded advanced settings should have no redundant title and a tight gap");
        test.expect(errorAccumulationCombo != nullptr
                        && errorAccumulationCombo->property("currentIndex").toInt()
                            == controller.errorAccumulationMode(),
                    "Advanced Settings should expose Average/Accumulate error handling");
        test.expect(ditherModeCombo != nullptr
                        && ditherModeCombo->property("count").toInt() == 8,
                    "Dithering choices should include the editable Custom mode");
        test.expect(orderedMapSizeCombo != nullptr
                        && orderedMapSizeCombo->property("currentIndex").toInt()
                            == (controller.orderedDitherMapSize() == 4 ? 1 : 0)
                        && orderedMapSizeCombo->property("enabled").toBool()
                            == (controller.ditherMode() == 5 || controller.ditherMode() == 6),
                    "Advanced Settings should expose the 2x2/4x4 ordered pattern");
        test.expect(errorWeightGrid != nullptr && errorRightSpinBox != nullptr
                        && errorWeightTotalLabel != nullptr
                        && errorRightSpinBox->property("value").toInt()
                            == controller.errorRight()
                        && errorWeightGrid->property("enabled").toBool()
                            == (controller.ditherMode() != 0
                                && controller.ditherMode() != 5),
                    "Advanced Settings should expose the editable six-cell error kernel");
        test.expect(orderedBrightnessSlider != nullptr
                        && orderedBrightnessSlider->property("from").toInt() == 0
                        && orderedBrightnessSlider->property("to").toInt() == 16
                        && maximumColorShiftSlider != nullptr
                        && maximumColorShiftSlider->property("to").toInt() == 100,
                    "legacy brightness and color-shift controls should expose their full ranges");
        test.expect(perceptualWeightsGrid != nullptr
                        && restorePerceptualWeightsButton != nullptr
                        && paletteSelectionCombo != nullptr
                        && scanlineStaticColorCount != nullptr,
                    "Advanced Settings should expose perceptual and F18A parity controls");
        advancedToggle->setProperty("checked", false);
    }

    window->setProperty("previewLayout", 0);
    test.expect(waitFor([&] {
                    const QObject* tabbed =
                        window->findChild<QObject*>(QStringLiteral("tabbedPreviewLayout"));
                    const QObject* sourceTab =
                        window->findChild<QObject*>(QStringLiteral("sourcePreviewTab"));
                    const QObject* convertedTab =
                        window->findChild<QObject*>(QStringLiteral("convertedPreviewTab"));
                    const QObject* sourceTitle =
                        window->findChild<QObject*>(QStringLiteral("sourcePreviewTitle"));
                    const QObject* convertedTitle =
                        window->findChild<QObject*>(QStringLiteral("convertedPreviewTitle"));
                    const QObject* sourceViewport =
                        window->findChild<QObject*>(QStringLiteral("sourcePreviewViewport"));
                    const QObject* sourceControls =
                        window->findChild<QObject*>(QStringLiteral("sourcePreviewControls"));
                    return tabbed != nullptr && sourceTab != nullptr && convertedTab != nullptr
                        && sourceTitle != nullptr && convertedTitle != nullptr
                        && !sourceTitle->property("visible").toBool()
                        && !convertedTitle->property("visible").toBool()
                        && sourceViewport != nullptr && sourceControls != nullptr
                        && sourceControls->property("y").toReal()
                            > sourceViewport->property("y").toReal()
                        && sourceTab->property("text").toString() == QStringLiteral("Source")
                        && convertedTab->property("text").toString()
                            == QStringLiteral("Screen Image");
                }),
                "tabbed layout should expose Source and Screen Image tabs");

    window->setProperty("previewLayout", 2);
    test.expect(waitFor([&] {
                    const QObject* vertical = window->findChild<QObject*>(
                        QStringLiteral("verticalPreviewLayout"));
                    const QObject* source = visiblePane(QStringLiteral("sourcePreview"));
                    const QObject* converted =
                        visiblePane(QStringLiteral("convertedPreview"));
                    return vertical != nullptr && source != nullptr && converted != nullptr
                        && qAbs(source->property("width").toReal()
                                - converted->property("width").toReal())
                            <= 1.0
                        && qAbs(source->property("height").toReal()
                                - converted->property("height").toReal())
                            <= 1.0;
                }),
                "vertical layout should start with two equal-size stacked panes");

    window->setProperty("previewLayout", 1);
    test.expect(waitFor([&] {
                    const QObject* horizontal = window->findChild<QObject*>(
                        QStringLiteral("horizontalPreviewLayout"));
                    const QObject* source = visiblePane(QStringLiteral("sourcePreview"));
                    const QObject* converted =
                        visiblePane(QStringLiteral("convertedPreview"));
                    bool hasVisibleSourceTitle = false;
                    bool hasNamedConvertedTitle = false;
                    for (const QObject* sourceTitle : window->findChildren<QObject*>(
                             QStringLiteral("sourcePreviewTitle"))) {
                        hasVisibleSourceTitle |= sourceTitle->property("visible").toBool();
                    }
                    for (const QObject* convertedTitle : window->findChildren<QObject*>(
                             QStringLiteral("convertedPreviewTitle"))) {
                        hasNamedConvertedTitle |= convertedTitle->property("visible").toBool()
                            && convertedTitle->property("text").toString()
                                == QStringLiteral("Screen Image");
                    }
                    return horizontal != nullptr && source != nullptr && converted != nullptr
                        && qAbs(source->property("width").toReal()
                                - converted->property("width").toReal())
                            <= 1.0
                        && qAbs(source->property("height").toReal()
                                - converted->property("height").toReal())
                            <= 1.0
                        && hasVisibleSourceTitle && hasNamedConvertedTitle;
                }),
                "horizontal layout should start with equal-size Source and Screen Image panes");

    QObject* sourcePane = visiblePane(QStringLiteral("sourcePreview"));
    const QObject* convertedPane = visiblePane(QStringLiteral("convertedPreview"));
    const QObject* updateConversionButton =
        window->findChild<QObject*>(QStringLiteral("updateConversionButton"));
    const QObject* livePreviewSwitch =
        window->findChild<QObject*>(QStringLiteral("livePreviewSwitch"));
    const QObject* sourceDetails = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewDetails"))
        : nullptr;
    const QObject* statusSourceName =
        window->findChild<QObject*>(QStringLiteral("statusSourceName"));
    test.expect(window->title() == QStringLiteral("New Convert 9918")
                    && sourceDetails != nullptr
                    && !sourceDetails->property("text").toString().contains(
                        controller.sourceName())
                    && statusSourceName != nullptr
                    && statusSourceName->property("text").toString()
                        == controller.sourceName(),
                "the source filename should appear only in the status bar");
    const QObject* sourceTools = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewSourceTools"))
        : nullptr;
    const QObject* drawingTools = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewDrawingTools"))
        : nullptr;
    const QObject* sourceDrawingToolbarSlot = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(
              QStringLiteral("sourcePreviewDrawingToolbarSlot"))
        : nullptr;
    const QObject* convertedDrawingToolbarSlot = convertedPane != nullptr
        ? convertedPane->findChild<QObject*>(
              QStringLiteral("convertedPreviewDrawingToolbarSlot"))
        : nullptr;
    const QObject* pencilButton = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewPencilButton"))
        : nullptr;
    const QObject* eraserButton = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewEraserButton"))
        : nullptr;
    const QObject* ellipseButton = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewEllipseButton"))
        : nullptr;
    const QObject* rectangleButton = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewRectangleButton"))
        : nullptr;
    const QObject* hardEdgeButton = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewHardEdgeButton"))
        : nullptr;
    const QObject* shapeFillButton = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewShapeFillButton"))
        : nullptr;
    const QObject* drawingUndoButton = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewDrawingUndoButton"))
        : nullptr;
    const QObject* drawingRedoButton = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewDrawingRedoButton"))
        : nullptr;
    const QObject* drawingDiameter = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(
              QStringLiteral("sourcePreviewDrawingDiameterField"))
        : nullptr;
    const QObject* drawingDiameterUp = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(
              QStringLiteral("sourcePreviewDrawingDiameterUpIndicator"))
        : nullptr;
    const QObject* drawingDiameterDown = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(
              QStringLiteral("sourcePreviewDrawingDiameterDownIndicator"))
        : nullptr;
    QObject* strokeOverlay = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewStrokeOverlay"))
        : nullptr;
    const QObject* brushCursorRing = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewBrushCursorRing"))
        : nullptr;
    const QObject* sourceColorSelector = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(
              QStringLiteral("sourcePreviewBackgroundColorButton"))
        : nullptr;
    const QObject* sourcePreviewImage = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewImage"))
        : nullptr;
    const QObject* sourceZoom = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewZoomControls"))
        : nullptr;
    const QObject* convertedTools = convertedPane != nullptr
        ? convertedPane->findChild<QObject*>(QStringLiteral("convertedPreviewConvertedTools"))
        : nullptr;
    const QObject* convertedAutoUpdate = convertedPane != nullptr
        ? convertedPane->findChild<QObject*>(
              QStringLiteral("convertedPreviewAutoUpdateSwitch"))
        : nullptr;
    const QObject* convertedZoom = convertedPane != nullptr
        ? convertedPane->findChild<QObject*>(QStringLiteral("convertedPreviewZoomControls"))
        : nullptr;
    const QObject* sourceZoomMenuButton = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewZoomMenuButton"))
        : nullptr;
    const QObject* sourceZoomIn = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewZoomInButton"))
        : nullptr;
    const QObject* sourceZoomOut = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewZoomOutButton"))
        : nullptr;
    const QObject* sourceFitMenuItem = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewFitMenuItem"))
        : nullptr;
    const QObject* sourceActualSizeMenuItem = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewActualSizeMenuItem"))
        : nullptr;
    const qreal zoomStepContentWidth = sourceZoomIn != nullptr && sourceZoomOut != nullptr
        ? qMax(sourceZoomIn->property("renderedContentWidth").toReal(),
               sourceZoomOut->property("renderedContentWidth").toReal())
        : 0.0;
    test.expect(ellipseButton != nullptr && ellipseButton->property("enabled").toBool()
                    && rectangleButton != nullptr
                    && rectangleButton->property("enabled").toBool()
                    && hardEdgeButton != nullptr
                    && hardEdgeButton->property("enabled").toBool()
                    && shapeFillButton != nullptr
                    && shapeFillButton->property("enabled").toBool(),
                "source drawing toolbar should expose shape, edge, and fill controls");
    test.expect(convertedTools != nullptr && convertedAutoUpdate != nullptr,
                "Converted lower toolbar should expose automatic conversion update");
    test.expect(convertedTools != nullptr && convertedZoom != nullptr
                    && convertedTools->parent() == convertedZoom->parent(),
                "Converted update and zoom controls should share a toolbar row");
    test.expect(convertedTools != nullptr && convertedZoom != nullptr
                    && qAbs(convertedTools->property("y").toReal()
                            - convertedZoom->property("y").toReal())
                        <= 1.0,
                "Converted update and zoom controls should align vertically");
    test.expect(sourcePane == nullptr
                    || sourcePane->findChild<QObject*>(
                           QStringLiteral("sourcePreviewAutoUpdateSwitch"))
                        == nullptr,
                "automatic conversion update should be removed from Source controls");
    bool constrainedShapePreview = false;
    if (strokeOverlay != nullptr) {
        const QVariant startX{10.0};
        const QVariant startY{12.0};
        const QVariant ellipse{true};
        const QVariant endX{36.0};
        const QVariant endY{20.0};
        const QVariant locked{true};
        const bool began = QMetaObject::invokeMethod(
            strokeOverlay, "beginShape", Q_ARG(QVariant, startX),
            Q_ARG(QVariant, startY), Q_ARG(QVariant, ellipse));
        const bool updated = QMetaObject::invokeMethod(
            strokeOverlay, "updateShape", Q_ARG(QVariant, endX),
            Q_ARG(QVariant, endY), Q_ARG(QVariant, locked));
        constrainedShapePreview = began && updated
            && qAbs(qAbs(strokeOverlay->property("shapeEndX").toReal()
                         - strokeOverlay->property("shapeStartX").toReal())
                    - qAbs(strokeOverlay->property("shapeEndY").toReal()
                           - strokeOverlay->property("shapeStartY").toReal()))
                < 0.01;
        QMetaObject::invokeMethod(strokeOverlay, "cancelShape");
    }
    test.expect(constrainedShapePreview,
                "Shift-drag shape preview should lock displayed width and height");
    test.expect(sourcePane != nullptr && convertedPane != nullptr
                    && sourceTools != nullptr && drawingTools != nullptr
                    && sourceDrawingToolbarSlot != nullptr
                    && convertedDrawingToolbarSlot != nullptr
                    && qAbs(sourceDrawingToolbarSlot->property("height").toReal()
                            - convertedDrawingToolbarSlot->property("height").toReal())
                        <= 1.0
                    && pencilButton != nullptr && pencilButton->property("enabled").toBool()
                    && eraserButton != nullptr && eraserButton->property("enabled").toBool()
                    && drawingUndoButton != nullptr
                    && drawingUndoButton->property("enabled").toBool()
                        == controller.canUndoDrawing()
                    && drawingRedoButton != nullptr
                    && drawingRedoButton->property("enabled").toBool()
                        == controller.canRedoDrawing()
                    && drawingDiameter != nullptr
                    && drawingDiameter->property("enabled").toBool()
                    && drawingDiameter->property("width").toReal() <= 48.0
                    && drawingDiameterUp != nullptr
                    && drawingDiameterUp->property("visible").toBool()
                    && drawingDiameterDown != nullptr
                    && drawingDiameterDown->property("visible").toBool()
                    && strokeOverlay != nullptr
                    && brushCursorRing != nullptr
                    && qAbs(brushCursorRing->property("cursorDiameter").toReal()
                            - qMax(1.0,
                                   sourcePane->property("drawingDiameter").toReal()
                                       * sourcePane->property("effectiveZoom").toReal()))
                        < 0.01
                    && sourcePreviewImage != nullptr
                    && !sourcePreviewImage->property("asynchronous").toBool()
                    && sourcePreviewImage->property("retainWhileLoading").toBool()
                    && sourceZoom != nullptr
                    && convertedZoom != nullptr
                    && sourceTools->parent() == sourceZoom->parent()
                    && qAbs(sourceTools->property("y").toReal()
                            - sourceZoom->property("y").toReal())
                        <= 1.0
                    && sourceColorSelector != nullptr
                    && sourceColorSelector->property("enabled").toBool()
                    && sourcePane->findChild<QObject*>(
                        QStringLiteral("sourcePreviewEyedropperButton"))
                        != nullptr
                    && sourcePane->findChild<QObject*>(
                        QStringLiteral("sourcePreviewCenterButton"))
                        != nullptr
                    && sourcePane->findChild<QObject*>(
                        QStringLiteral("sourcePreviewUpdateButton"))
                        == nullptr
                    && updateConversionButton != nullptr
                    && livePreviewSwitch != nullptr
                    && sourceZoomMenuButton != nullptr
                    && sourceZoomMenuButton->property("text").toString().contains(
                        QLatin1Char('%'))
                    && sourceZoomMenuButton->property("width").toReal()
                        <= sourceZoomMenuButton->property("renderedContentWidth").toReal()
                            + 5.0
                    && sourceZoomMenuButton->property("leftPadding").toReal() <= 2.0
                    && sourceZoomMenuButton->property("rightPadding").toReal() <= 2.0
                    && sourceZoomIn != nullptr && sourceZoomOut != nullptr
                    && sourceZoomIn->property("width").toReal()
                        <= zoomStepContentWidth + 5.0
                    && sourceZoomOut->property("width").toReal()
                        <= zoomStepContentWidth + 5.0
                    && sourceZoomIn->property("leftPadding").toReal() <= 2.0
                    && sourceZoomIn->property("rightPadding").toReal() <= 2.0
                    && sourceZoomOut->property("leftPadding").toReal() <= 2.0
                    && sourceZoomOut->property("rightPadding").toReal() <= 2.0
                    && sourceZoomIn->property("height").toReal()
                        <= sourceZoomMenuButton->property("height").toReal() / 2.0 + 1.0
                    && sourceZoomOut->property("height").toReal()
                        <= sourceZoomMenuButton->property("height").toReal() / 2.0 + 1.0
                    && sourceFitMenuItem != nullptr
                    && !sourceFitMenuItem->property("checkable").toBool()
                    && sourceActualSizeMenuItem != nullptr
                    && !sourceActualSizeMenuItem->property("checkable").toBool()
                    && convertedPane->findChild<QObject*>(
                        QStringLiteral("convertedPreviewZoomMenuButton"))
                        != nullptr
                    && convertedPane->findChild<QObject*>(
                        QStringLiteral("convertedPreviewFitMenuItem"))
                        != nullptr,
                "both panes should expose compact popup-based zoom controls");

    QObject* colorPicker = sourcePane != nullptr
        ? sourcePane->findChild<QObject*>(QStringLiteral("sourcePreviewColorPickerPopup"))
        : nullptr;
    QObject* colorPickerTabs = colorPicker != nullptr
        ? colorPicker->findChild<QObject*>(QStringLiteral("sourcePreviewColorPickerTabs"))
        : nullptr;
    QObject* usedColorGrid = colorPicker != nullptr
        ? colorPicker->findChild<QObject*>(QStringLiteral("sourcePreviewUsedColorGrid"))
        : nullptr;
    const bool pickerOpened = colorPicker != nullptr
        && QMetaObject::invokeMethod(colorPicker, "open");
    if (colorPickerTabs != nullptr) colorPickerTabs->setProperty("currentIndex", 2);
    test.expect(pickerOpened && colorPickerTabs != nullptr && usedColorGrid != nullptr
                    && waitFor([&] {
                           return usedColorGrid->property("visible").toBool()
                               && usedColorGrid->property("count").toInt()
                                   == controller.sourceUsedColors().size();
                       }),
                "used-color tab should display every unique source-image color");
    if (colorPicker != nullptr) QMetaObject::invokeMethod(colorPicker, "close");

    if (sourcePane != nullptr) {
        sourcePane->setProperty("manualZoom", 1.0);
        sourcePane->setProperty("fitToView", true);
        QCoreApplication::processEvents();
        const qreal fittedZoom = sourcePane->property("effectiveZoom").toReal();
        const QVariant zoomFactor{1.25};
        const bool zoomInvoked = QMetaObject::invokeMethod(
            sourcePane, "zoomBy", Q_ARG(QVariant, zoomFactor));
        test.expect(zoomInvoked && !sourcePane->property("fitToView").toBool()
                        && qAbs(sourcePane->property("manualZoom").toReal()
                                - qBound(0.125, fittedZoom * 1.25, 16.0))
                            < 0.001,
                    "first manual zoom step should start from the fitted zoom");
        sourcePane->setProperty("fitToView", true);
    }

    test.expect(window->findChild<QObject*>(QStringLiteral("powerPaintFramingCheckBox"))
                        != nullptr
                    && window->findChild<QObject*>(QStringLiteral("workingPaletteSettingsGroup"))
                        != nullptr
                    && window->findChild<QObject*>(QStringLiteral("resetWorkingPaletteButton"))
                        != nullptr,
                "the interface should expose PowerPaint framing and editable working palettes");

    auto* adjacentPanel =
        window->findChild<QObject*>(QStringLiteral("adjacentConversionPanel"));
    auto* overlayPanel =
        window->findChild<QObject*>(QStringLiteral("overlayConversionPanel"));
    auto* overlayRail =
        window->findChild<QObject*>(QStringLiteral("overlayExpandRail"));
    auto* overlayExpandButton =
        window->findChild<QObject*>(QStringLiteral("overlayExpandButton"));
    const QObject* overlayExpandButtonBackground = overlayExpandButton != nullptr
        ? overlayExpandButton->findChild<QObject*>(
              QStringLiteral("overlayExpandButtonBackground"))
        : nullptr;
    test.expect(adjacentPanel != nullptr && overlayPanel != nullptr
                    && overlayRail != nullptr && overlayExpandButton != nullptr,
                "both conversion-panel placements and the overlay reopen rail should exist");
    test.expect(overlayRail != nullptr
                    && !overlayRail->property("outlineColor").isValid(),
                "hidden overlay rail should not paint a full-height bounding frame");
    test.expect(overlayExpandButtonBackground != nullptr
                    && overlayExpandButtonBackground->property("color")
                           .value<QColor>().alpha() > 0,
                "hidden overlay expand button should keep a visible themed background");
    const QColor outlineColor = sourcePane->property("outlineColor").value<QColor>();
    bool themedOutlines = outlineColor.isValid() && outlineColor.alpha() > 0;
    for (const auto& name : {QStringLiteral("recommendedSettingsGroup"),
                             QStringLiteral("commonSettingsGroup"),
                             QStringLiteral("framingSettingsGroup"),
                             QStringLiteral("advancedSettingsGroup"),
                             QStringLiteral("exportSettingsGroup")}) {
        const QObject* box = window->findChild<QObject*>(name);
        themedOutlines &= box != nullptr
            && box->property("outlineColor").value<QColor>() == outlineColor;
    }
    themedOutlines &= convertedPane->property("outlineColor").value<QColor>() == outlineColor
        && adjacentPanel->property("outlineColor").value<QColor>() == outlineColor;
    test.expect(themedOutlines,
                "pane and settings outlines should share the active palette text color");
    window->setProperty("conversionPanelVisible", false);
    QCoreApplication::processEvents();
    test.expect(adjacentPanel != nullptr && !adjacentPanel->property("visible").toBool(),
                "conversion panel should be hideable");

    window->setProperty("conversionPanelMode", 1);
    test.expect(waitFor([&] {
                    return overlayRail->property("visible").toBool()
                        && !overlayPanel->property("opened").toBool();
                }),
                "hidden overlay should expose its right-edge expand control");
    window->setProperty("conversionPanelVisible", true);
    test.expect(waitFor([&] {
                    const QObject* hideButton =
                        visiblePane(QStringLiteral("overlayHideButton"));
                    return overlayPanel->property("opened").toBool()
                        && !overlayRail->property("visible").toBool()
                        && hideButton != nullptr && hideButton->property("visible").toBool()
                        && hideButton->property("text").toString() == QStringLiteral("›")
                        && overlayExpandButton->property("text").toString()
                            == QStringLiteral("‹")
                        && qAbs(overlayExpandButton->property("x").toReal()
                                + overlayExpandButton->property("width").toReal() / 2.0
                                - overlayRail->property("width").toReal() / 2.0 - 6.0)
                            <= 0.5;
                }),
                "overlay arrows and the expand control should align outward");
    test.expect(QMetaObject::invokeMethod(
                    overlayPanel, "handlePointerPresence", Q_ARG(QVariant, true))
                    && QMetaObject::invokeMethod(
                        overlayPanel, "handlePointerPresence", Q_ARG(QVariant, false)),
                "overlay pointer-presence handling should be callable");
    test.expect(waitFor([&] {
                    return !window->property("conversionPanelVisible").toBool()
                        && overlayRail->property("visible").toBool();
                }),
                "overlay should auto-hide after the pointer enters and leaves it");
    window->setProperty("conversionPanelVisible", true);
    test.expect(waitFor([&] { return overlayPanel->property("opened").toBool(); }),
                "overlay should reopen after automatic hiding");
    window->setProperty("conversionPanelVisible", false);
    test.expect(waitFor([&] {
                    return !overlayPanel->property("visible").toBool()
                        && overlayRail->property("visible").toBool();
                }),
                "hiding the overlay should restore the right-edge expand control");
    window->setProperty("conversionPanelVisible", true);
    window->setProperty("conversionPanelMode", 0);
    test.expect(waitFor([&] {
                    return !overlayPanel->property("opened").toBool()
                        && !overlayPanel->property("visible").toBool()
                        && adjacentPanel->property("visible").toBool();
                }),
                "adjacent placement should restore the docked conversion panel");

    window->setWidth(900);
    window->setHeight(640);
    window->setProperty("layoutWidth", 900);
    QCoreApplication::processEvents();
    test.expect(window->property("compactLayout").toBool(),
                "narrow window should report its compact breakpoint");

    window->setWidth(1280);
    window->setHeight(800);
    window->setProperty("layoutWidth", 1280);
    test.expect(waitFor([&] {
                    const QObject* sourcePreview =
                        window->findChild<QObject*>(QStringLiteral("sourcePreviewImage"));
                    const QObject* convertedPreview =
                        window->findChild<QObject*>(QStringLiteral("convertedPreviewImage"));
                    return sourcePreview != nullptr && convertedPreview != nullptr
                        && sourcePreview->property("status").toInt() == 1
                        && convertedPreview->property("status").toInt() == 1;
                }),
                "source and converted preview images should finish loading in QML");
    test.expect(!window->property("compactLayout").toBool(),
                "wide window should report its full-size breakpoint");
    const QImage screenshot = window->grabWindow();
    test.expect(QGuiApplication::primaryScreen() != nullptr
                    && QGuiApplication::primaryScreen()->devicePixelRatio() > 1.0,
                "fractional-scale test should run on a high-DPI virtual display");
    test.expect(!screenshot.isNull() && screenshot.width() >= 700
                    && screenshot.height() >= 500,
                "fractional-scale offscreen rendering should produce a complete frame");
    screenshot.save(QDir::current().filePath(QStringLiteral("phase6-interface.png")));
}

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("CiscoGarciaFL-Test"));
    application.setApplicationName(QStringLiteral("NewConvert9918-InterfaceTest"));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    QTemporaryDir settingsDirectory;
    if (!settingsDirectory.isValid()) {
        std::cerr << "Unable to create temporary settings directory\n";
        return 1;
    }
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsDirectory.path());
    QSettings().clear();

    TestContext test;
    previousMessageHandler = qInstallMessageHandler(captureQmlMessages);
    ImageInputController controller;
    testLiveWorkflow(test, controller);
    testExportWorkflow(test, controller);
    testEditorProjectRecipe(test);
    testResponsiveQml(test, controller);
    qInstallMessageHandler(previousMessageHandler);
    test.expect(qmlBindingErrors.load() == 0,
                "QML bindings should remain valid through engine shutdown");

    QSettings().clear();
    if (test.failures != 0) {
        std::cerr << test.failures << " interface workflow test(s) failed\n";
        return 1;
    }
    std::cout << "All interface workflow tests passed\n";
    return 0;
}
