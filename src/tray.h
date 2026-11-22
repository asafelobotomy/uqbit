// uQbit's tray icon (a StatusNotifierItem, which uQ's Tray island shows): there while any tray
// setting is on and a tray is there to hold it. Click: show or hide the window. Menu: show,
// pause all, resume all, quit.
#pragma once

#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QTimer>

class QQuickWindow;

namespace uqbit {

class App;

class Tray : public QObject
{
    Q_OBJECT
public:
    Tray(App *app, QQuickWindow *window, QObject *parent = nullptr);

    /// Whether the icon is in a tray now (closing or minimising to it is safe).
    bool shown() const { return m_icon.isVisible(); }
    /// Whether the tray settings ask for an icon.
    static bool wanted(const QVariantMap &settings);

private:
    void update();
    void tooltip();
    void toggle();

    App *m_app;
    QPointer<QQuickWindow> m_window;
    QSystemTrayIcon m_icon;
    QMenu m_menu;
    QAction *m_show = nullptr;
    QTimer m_tip;
};

} // namespace uqbit
