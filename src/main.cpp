#include "app.h"
#include "cli.h"
#include "single.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTextStream>

int main(int argc, char **argv)
{
    QStringList args;
    for (int i = 1; i < argc; ++i)
        args.append(QString::fromLocal8Bit(argv[i]));

    if (uqbit::isCliCommand(args)) {
        QCoreApplication core(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("uqbit"));
        QTextStream out(stdout), err(stderr);
        return uqbit::runCli(args, out, err);
    }
    // "--" ends options: everything after it is a link or a file, even "add".
    args.removeAll(QStringLiteral("--"));

    QGuiApplication gui(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("uqbit"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("uQbit"));
    QGuiApplication::setApplicationVersion(QStringLiteral(UQBIT_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.asafelobotomy.uqbit"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("io.github.asafelobotomy.uqbit"),
                                                    QIcon(QStringLiteral(":/qt/qml/Uqbit/icons/uqbit.svg"))));
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
        QQuickStyle::setStyle(QStringLiteral("Basic"));

    // One window per session: links opened while uQbit is open go to that window.
    if (uqbit::forwardToRunning(args))
        return 0;

    uqbit::App app(uqbit::absoluteInputs(args));
    (new uqbit::Remote(&app))->claim();

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &gui,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Uqbit", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    app.setWindow(qobject_cast<QQuickWindow *>(engine.rootObjects().first()));
    return QGuiApplication::exec();
}
