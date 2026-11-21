// One window per session: the first uQbit owns io.github.asafelobotomy.uqbit on the session
// bus; a second `uqbit MAGNET` hands its links and files to it and exits.
#pragma once

#include <QObject>
#include <QStringList>

namespace uqbit {

inline constexpr auto kBusName = "io.github.asafelobotomy.uqbit";
inline constexpr auto kBusPath = "/io/github/asafelobotomy/uqbit";

class App;

class Remote : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.asafelobotomy.uqbit")
public:
    explicit Remote(App *app);
    /// Takes the bus name. False when another uQbit has it.
    bool claim();

public slots:
    Q_SCRIPTABLE void Open(const QStringList &inputs);
    Q_SCRIPTABLE void Activate();

private:
    App *m_app;
};

/// Inputs made absolute (relative files) so another process can read them.
QStringList absoluteInputs(const QStringList &inputs);
/// Hands `inputs` to the running uQbit. False when there is none.
bool forwardToRunning(const QStringList &inputs);

} // namespace uqbit
