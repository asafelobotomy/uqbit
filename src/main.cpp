#include "app.h"
#include "cli.h"
#include "single.h"
#include "tray.h"

#include <QCoreApplication>
#include <QApplication>
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
    // "--" ends options: everything after it is a link or a file, even "add". "--hidden" (the
    // login entry with "start in the tray") opens to the tray, not the window.
    const int dashes = int(args.indexOf(QStringLiteral("--")));
    const bool hidden = args.indexOf(QStringLiteral("--hidden")) >= 0
                        && (dashes < 0 || args.indexOf(QStringLiteral("--hidden")) < dashes);
    if (hidden)
        args.removeOne(QStringLiteral("--hidden"));
    args.removeAll(QStringLiteral("--"));

    // QApplication, not QGuiApplication: the tray icon's menu is a widget.
    QApplication gui(argc, argv);
    QApplication::setApplicationName(QStringLiteral("uqbit"));
    QApplication::setApplicationDisplayName(QStringLiteral("uQbit"));
    QApplication::setApplicationVersion(QStringLiteral(UQBIT_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("io.github.asafelobotomy.uqbit"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("io.github.asafelobotomy.uqbit"),
                                                    QIcon(QStringLiteral(":/qt/qml/Uqbit/icons/uqbit.svg"))));
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
        QQuickStyle::setStyle(QStringLiteral("Basic"));

    // One window per session: links opened while uQbit is open go to that window. A second
    // login start finds it running and leaves it be.
    if (hidden && args.isEmpty() && uqbit::isRunning())
        return 0;
    if (uqbit::forwardToRunning(args))
        return 0;
    // The window decides when uQbit quits (closing may only hide it to the tray).
    QApplication::setQuitOnLastWindowClosed(false);

    uqbit::App app(uqbit::absoluteInputs(args));
    (new uqbit::Remote(&app))->claim();

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &gui,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.setInitialProperties({{QStringLiteral("startHidden"), hidden}});
    engine.loadFromModule("Uqbit", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    app.setWindow(window);
    uqbit::Tray tray(&app, window);
    // Started for the tray but there is none to wait in: show the window after all.
    if (hidden && !tray.shown())
        app.raise();
    return QApplication::exec();
}
