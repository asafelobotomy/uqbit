#include "tray.h"

#include "app.h"
#include "torrents.h"

#include <QCoreApplication>
#include <QIcon>
#include <QQuickWindow>

namespace uqbit {

static QString rate(double bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B/s").arg(qRound(bytes));
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KiB/s").arg(qRound(bytes / 1024));
    return QStringLiteral("%1 MiB/s").arg(bytes / 1048576.0, 0, 'f', 1);
}

bool Tray::wanted(const QVariantMap &s)
{
    return s.value("close_to_tray").toBool() || s.value("minimize_to_tray").toBool()
           || (s.value("start_in_tray").toBool() && s.value("launch_on_login").toBool());
}

Tray::Tray(App *app, QQuickWindow *window, QObject *parent)
    : QObject(parent), m_app(app), m_window(window)
{
    m_icon.setIcon(QGuiApplication::windowIcon());
    m_icon.setToolTip(QStringLiteral("uQbit"));
    m_show = m_menu.addAction(tr("Show uQbit"), this, &Tray::toggle);
    m_menu.addSeparator();
    m_menu.addAction(tr("Pause all"), this, [this] { m_app->setRunning({QStringLiteral("all")}, false); });
    m_menu.addAction(tr("Resume all"), this, [this] { m_app->setRunning({QStringLiteral("all")}, true); });
    m_menu.addSeparator();
    m_menu.addAction(tr("Quit uQbit"), this, [] { QCoreApplication::quit(); });
    m_icon.setContextMenu(&m_menu);
    connect(&m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick)
            toggle();
    });
    connect(app, &App::settingsChanged, this, &Tray::update);
    if (window)
        connect(window, &QWindow::visibleChanged, this, [this](bool v) {
            m_show->setText(v ? tr("Hide uQbit") : tr("Show uQbit"));
        });
    m_tip.setInterval(3000);
    connect(&m_tip, &QTimer::timeout, this, &Tray::tooltip);
    update();
}

void Tray::update()
{
    const bool on = wanted(m_app->settings()) && QSystemTrayIcon::isSystemTrayAvailable();
    if (on == m_icon.isVisible())
        return;
    m_icon.setVisible(on);
    if (on) {
        tooltip();
        m_tip.start();
    } else {
        m_tip.stop();
        // Nowhere to come back from: never leave the window hidden without an icon.
        if (m_window && !m_window->isVisible())
            m_app->raise();
    }
    m_app->setTrayShown(on);
}

void Tray::tooltip()
{
    const QVariantMap s = m_app->torrents()->server();
    m_icon.setToolTip(tr("uQbit · ↓ %1 · ↑ %2")
                          .arg(rate(s.value("dl_info_speed").toDouble()),
                               rate(s.value("up_info_speed").toDouble())));
}

void Tray::toggle()
{
    if (m_window && m_window->isVisible() && m_window->visibility() != QWindow::Minimized)
        m_window->hide();
    else
        m_app->raise();
}

} // namespace uqbit
