#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <cstdlib>

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    application.setApplicationDisplayName(QStringLiteral("New Convert 9918"));
    application.setOrganizationName(QStringLiteral("CiscoGarciaFL"));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("NewConvert9918"), QStringLiteral("Main"));

    return application.exec();
}
