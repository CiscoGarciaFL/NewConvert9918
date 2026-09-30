#include "AppPreferencesController.hpp"
#include "EditorProjectController.hpp"
#include "ImageInputController.hpp"

#include <QApplication>
#include <QColor>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QPropertyAnimation>
#include <QSettings>
#include <QSplashScreen>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cstdlib>
#include <string_view>

namespace {

void importLegacySettings()
{
    QSettings current;
    if (!current.allKeys().isEmpty()) return;

    // Keep the retired identifier only at this migration boundary. All new
    // settings are written under the RetroVDPStudio application identity.
    QSettings legacy(QSettings::NativeFormat,
                     QSettings::UserScope,
                     QStringLiteral("CiscoGarciaFL"),
                     QStringLiteral("NewConvert9918"));
    const QStringList legacyKeys = legacy.allKeys();
    if (legacyKeys.isEmpty()) return;

    for (const QString& key : legacyKeys) current.setValue(key, legacy.value(key));
    current.setValue(QStringLiteral("migration/importedLegacySettings"), true);
    current.sync();
}

QPixmap createSplashPixmap()
{
    constexpr int splashWidth = 640;
    constexpr int splashHeight = 360;
    QPixmap splashPixmap(splashWidth, splashHeight);
    splashPixmap.fill(QColor(QStringLiteral("#101820")));

    QPainter painter(&splashPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QPixmap sourceLogo(QStringLiteral(
        ":/qt/qml/retrovdp-studio/assets/icons/RetroVDPStudio-256.png"));
    const QPixmap logo = sourceLogo.scaled(
        224, 224, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    painter.drawPixmap((splashWidth - logo.width()) / 2, 24, logo);

    QFont titleFont;
    titleFont.setPixelSize(30);
    titleFont.setWeight(QFont::DemiBold);
    painter.setFont(titleFont);
    painter.setPen(QColor(QStringLiteral("#f4f7fa")));
    painter.drawText(QRect(24, 270, splashWidth - 48, 52),
                     Qt::AlignCenter,
                     QStringLiteral("RetroVDP Studio"));

    return splashPixmap;
}

} // namespace

int main(int argc, char* argv[])
{
    const bool smokeTest = std::any_of(
        argv + 1, argv + argc,
        [](const char* argument) {
            return std::string_view(argument) == "--smoke-test";
        });
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("RetroVDPStudio"));
    application.setApplicationDisplayName(QStringLiteral("RetroVDP Studio"));
    application.setApplicationVersion(QStringLiteral(RETROVDP_VERSION));
    application.setOrganizationName(QStringLiteral("CiscoGarciaFL"));
    importLegacySettings();
    application.setWindowIcon(QIcon(QStringLiteral(
        ":/qt/qml/retrovdp-studio/assets/icons/RetroVDPStudio-256.png")));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    QElapsedTimer splashLifetime;
    splashLifetime.start();
    QSplashScreen splash(
        createSplashPixmap(),
        Qt::SplashScreen | Qt::FramelessWindowHint
            | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    if (!smokeTest) {
        splash.show();
        splash.raise();
        application.processEvents();
    }

    // The context property must outlive the QML engine so bindings cannot
    // observe a null imageInput while the object tree is being destroyed.
    ImageInputController imageInput;
    AppPreferencesController appPreferences(&imageInput);
    appPreferences.applyStartupPreferences();
    EditorProjectController editorProject(&imageInput);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("imageInput"), &imageInput);
    engine.rootContext()->setContextProperty(QStringLiteral("appPreferences"), &appPreferences);
    engine.rootContext()->setContextProperty(QStringLiteral("editorProject"), &editorProject);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("RetroVDPStudio"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }
    if (smokeTest) {
        // Loading the complete QML object tree and draining its startup events
        // verifies the packaged runtime without leaving a GUI process open on
        // a headless release runner.
        application.processEvents(QEventLoop::AllEvents, 1000);
        splash.close();
        return EXIT_SUCCESS;
    }

    constexpr int splashFullOpacityDurationMs = 1500;
    constexpr int splashFadeDurationMs = 750;
    QPropertyAnimation splashFade(&splash, "windowOpacity");
    splashFade.setDuration(splashFadeDurationMs);
    splashFade.setStartValue(1.0);
    splashFade.setEndValue(0.0);
    splashFade.setEasingCurve(QEasingCurve::InOutQuad);
    QObject::connect(&splashFade, &QPropertyAnimation::finished,
                     &splash, &QSplashScreen::close);

    bool splashExitScheduled = false;
    const auto scheduleSplashExit = [&] {
        if (splashExitScheduled) return;
        splashExitScheduled = true;
        const int remainingFullOpacity = std::max(
            0, splashFullOpacityDurationMs
                   - static_cast<int>(splashLifetime.elapsed()));
        QTimer::singleShot(remainingFullOpacity, &splash, [&splashFade] {
            splashFade.start();
        });
    };
    if (auto* mainWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0))) {
        splash.raise();
        QObject::connect(mainWindow, &QQuickWindow::frameSwapped,
                         &splash, scheduleSplashExit, Qt::SingleShotConnection);
        QTimer::singleShot(3000, &splash, scheduleSplashExit);
    } else {
        splash.close();
    }

    if (argc > 1) {
        const std::string_view firstArgument(argv[1]);
        imageInput.openUrl(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    }

    return application.exec();
}
