#include "notify.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QVariantMap>

namespace uqbit {

void notifyFinished(const QString &name, const QString &path)
{
    QDBusMessage m = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    QVariantMap hints{{"desktop-entry", QStringLiteral("io.github.asafelobotomy.uqbit")},
                      {"category", QStringLiteral("transfer.complete")}};
    m << QStringLiteral("uQbit") << quint32(0) << QStringLiteral("io.github.asafelobotomy.uqbit")
      << QStringLiteral("Download finished") << (path.isEmpty() ? name : name + "\n" + path)
      << QStringList() << hints << qint32(-1);
    QDBusConnection::sessionBus().call(m, QDBus::Block, 3000);
}

} // namespace uqbit
