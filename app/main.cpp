#include "ImageInputController.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

#include <cstdlib>

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    application.setApplicationDisplayName(QStringLiteral("New Convert 9918"));
    application.setOrganizationName(QStringLiteral("CiscoGarciaFL"));

    QQmlApplicationEngine engine;
    ImageInputController imageInput;
    engine.rootContext()->setContextProperty(QStringLiteral("imageInput"), &imageInput);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("NewConvert9918"), QStringLiteral("Main"));

    if (argc > 1) {
        imageInput.openUrl(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    }

    return application.exec();
}
