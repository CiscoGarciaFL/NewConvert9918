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

    const auto visiblePane = [window](const QString& objectName) -> QObject* {
        for (QObject* pane : window->findChildren<QObject*>(objectName)) {
            if (pane->property("visible").toBool()) return pane;
        }
        return nullptr;
    };

    test.expect(window->findChild<QObject*>(QStringLiteral("mainMenuBar")) != nullptr,
                "application should expose its commands through a menu bar");
    const QObject* exitAction =
        window->findChild<QObject*>(QStringLiteral("exitAction"));
    test.expect(exitAction != nullptr
                    && exitAction->property("text").toString() == QStringLiteral("E&xit")
                    && window->findChild<QObject*>(QStringLiteral("exitMenuSeparator"))
                        != nullptr
                    && window->findChild<QObject*>(QStringLiteral("exitMenuItem")) != nullptr,
                "File menu should end with a separated Exit command");
    test.expect(window->property("previewLayout").toInt() == 1,
                "horizontal split should remain the default preview layout");
    test.expect(window->property("conversionPanelVisible").toBool()
                    && window->property("conversionPanelMode").toInt() == 0,
                "conversion panel should default to a visible adjacent panel");

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
                            == QStringLiteral("Converted");
                }),
                "tabbed layout should expose Source and Converted tabs");

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
                                == QStringLiteral("Converted");
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
                "horizontal layout should start with equal-size Source and Converted panes");

    auto* adjacentPanel =
        window->findChild<QObject*>(QStringLiteral("adjacentConversionPanel"));
    auto* overlayPanel =
        window->findChild<QObject*>(QStringLiteral("overlayConversionPanel"));
    auto* overlayRail =
        window->findChild<QObject*>(QStringLiteral("overlayExpandRail"));
    auto* overlayExpandButton =
        window->findChild<QObject*>(QStringLiteral("overlayExpandButton"));
    test.expect(adjacentPanel != nullptr && overlayPanel != nullptr
                    && overlayRail != nullptr && overlayExpandButton != nullptr,
                "both conversion-panel placements and the overlay reopen rail should exist");
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
                        window->findChild<QObject*>(QStringLiteral("overlayHideButton"));
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
