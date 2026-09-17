#include "ImageInputController.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>

#include <functional>
#include <iostream>

namespace {

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
    // Keep this end-to-end UI test fast in unoptimized developer builds. The
    // exhaustive non-zero shift search is covered independently by core tests.
    controller.applyPreset(1); // Pixel art: no dithering or color-shift search.
    controller.openUrl(QUrl::fromLocalFile(goldenSource(u"source/tiny-rgba.png")));
    test.expect(controller.hasImage(), "controller should load a source image");
    test.expect(waitFor([&] { return controller.hasConversion() && !controller.busy(); }),
                "loaded image should produce a debounced background preview");
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

    controller.setConversionMode(3);
    controller.setConversionMode(4);
    controller.setConversionMode(0);
    test.expect(waitFor([&] { return controller.hasConversion() && !controller.busy(); }),
                "rapid changes should settle on one current conversion");
    test.expect(controller.conversionMode() == 0
                    && controller.conversionDetails().contains(QStringLiteral("Bitmap 9918A")),
                "stale preview jobs should not replace the newest requested mode");

    controller.setConversionMode(8);
    test.expect(waitFor([&] { return controller.hasConversion() && !controller.busy(); }, 30'000),
                "scanline-palette mode should produce a background preview");
    test.expect(controller.scanlinePaletteAvailable()
                    && controller.palettePreview().startsWith(QStringLiteral("data:image/png;base64,")),
                "scanline-palette mode should expose its optional palette visualization");
    controller.setConversionMode(0);
    test.expect(waitFor([&] { return controller.hasConversion() && !controller.busy(); }),
                "controller should return to the requested export mode");

    controller.setGamma(1.4);
    test.expect(controller.canUndo(), "changing a setting should enable undo");
    QSettings persisted;
    const double persistedGamma =
        persisted.value(QStringLiteral("conversion/gamma"), -1.0).toDouble();
    if (!qFuzzyCompare(persistedGamma, 1.4)) {
        std::cerr << "Persisted gamma was " << persistedGamma
                  << "; settings status " << persisted.status() << '\n';
    }
    test.expect(qFuzzyCompare(persistedGamma, 1.4),
                "conversion settings should persist through QSettings");
    controller.undoSettings();
    test.expect(!qFuzzyCompare(controller.gamma(), 1.4),
                "undo should restore the prior settings snapshot");
    controller.setGamma(1.2);
    test.expect(controller.canUndo(),
                "a new settings change immediately after undo should create a new undo step");
    controller.undoSettings();
    test.expect(!qFuzzyCompare(controller.gamma(), 1.2),
                "the new post-undo settings step should be reversible");
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

void testResponsiveQml(TestContext& test, ImageInputController& controller)
{
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("imageInput"), &controller);
    const QString mainQml = QDir(QStringLiteral(NEWCONVERT9918_QML_DIR))
                                .filePath(QStringLiteral("Main.qml"));
    engine.load(QUrl::fromLocalFile(mainQml));
    test.expect(engine.rootObjects().size() == 1,
                "Phase 6 QML should load as one application window");
    if (engine.rootObjects().isEmpty()) return;

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().front());
    test.expect(window != nullptr, "Phase 6 root should be a QQuickWindow");
    if (window == nullptr) return;

    window->setWidth(900);
    window->setHeight(640);
    window->setProperty("layoutWidth", 900);
    QCoreApplication::processEvents();
    test.expect(window->property("compactLayout").toBool(),
                "narrow window should switch to the settings drawer layout");

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
                "wide window should show the persistent settings layout");
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
    ImageInputController controller;
    testLiveWorkflow(test, controller);
    testExportWorkflow(test, controller);
    testResponsiveQml(test, controller);

    QSettings().clear();
    if (test.failures != 0) {
        std::cerr << test.failures << " interface workflow test(s) failed\n";
        return 1;
    }
    std::cout << "All interface workflow tests passed\n";
    return 0;
}
