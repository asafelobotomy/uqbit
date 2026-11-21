#include "single.h"
#include "app.h"
#include "torrentfile.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QFileInfo>

namespace uqbit {

Remote::Remote(App *app) : QObject(app), m_app(app) {}

bool Remote::claim()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return true; // no bus: nothing to share the window with
    if (!bus.registerService(QString::fromLatin1(kBusName)))
        return false;
    bus.registerObject(QString::fromLatin1(kBusPath), this, QDBusConnection::ExportScriptableSlots);
    return true;
}

void Remote::Open(const QStringList &inputs)
{
    m_app->openInputs(inputs);
    m_app->raise();
}

void Remote::Activate() { m_app->raise(); }

QStringList absoluteInputs(const QStringList &inputs)
{
    QStringList out;
    for (const QString &in : inputs) {
        if (isTorrentLink(in) || in.contains(QLatin1String("://")))
            out.append(in);
        else
            out.append(QFileInfo(in).absoluteFilePath());
    }
    return out;
}

bool forwardToRunning(const QStringList &inputs)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()
        || !bus.interface()->isServiceRegistered(QString::fromLatin1(kBusName)).value())
        return false;
    QDBusMessage m = QDBusMessage::createMethodCall(
        QString::fromLatin1(kBusName), QString::fromLatin1(kBusPath),
        QString::fromLatin1(kBusName), inputs.isEmpty() ? "Activate" : "Open");
    if (!inputs.isEmpty())
        m << absoluteInputs(inputs);
    const QDBusMessage reply = bus.call(m, QDBus::Block, 5000);
    return reply.type() == QDBusMessage::ReplyMessage;
}

} // namespace uqbit
