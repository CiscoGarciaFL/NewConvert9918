#include "EditorProjectController.hpp"
#include "ImageInputController.hpp"

#include <QCoreApplication>
#include <QColor>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
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
    project.setActiveCharacterSet(2);
    project.setActiveCharacterPattern(255);

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
    test.expect(loaded && project.workspaceMode() == 2 && project.f18aEnabled()
                    && project.previewTarget() == 2 && project.editScope() == 1
                    && project.spriteSetNames().size() == 2
                    && project.activeSpriteSet() == 1 && project.activeSprite() == 3
                    && project.spritePlacementMode() && project.placementWidth() == 320
                    && project.placementHeight() == 200
                    && movedSprite.value(QStringLiteral("x")).toInt() == 91
                    && movedSprite.value(QStringLiteral("y")).toInt() == 47
                    && project.activeCharacterSet() == 2
                    && project.activeCharacterPattern() == 255
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
    test.expect(window->findChild<QObject*>(QStringLiteral("characterPatternGrid")) != nullptr
                    && window->findChild<QObject*>(
                           QStringLiteral("characterEditorSidePanelOutputProfilesGroup"))
                        != nullptr,
                "Character Editor should expose a 256-pattern set and dual-profile controls");
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
    editorProject.setSpritePlacementMode(true);
    const bool placementVisible = waitFor([&] {
                    return visiblePane(QStringLiteral("spritePlacementWorkspace")) != nullptr;
                });
    if (!placementVisible) {
        const auto placements = window->findChildren<QObject*>(
            QStringLiteral("spritePlacementWorkspace"));
        std::cerr << "Placement workspace state: instances=" << placements.size()
                  << " mode=" << editorProject.spritePlacementMode() << '\n';
    }
    test.expect(placementVisible && editorProject.activeSpritePlacements().size() == 32,
                "Sprite Editor should expose a persisted 32-sprite placement workspace");
    editorProject.setSpritePlacementMode(false);
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
