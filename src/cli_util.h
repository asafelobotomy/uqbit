// Helpers the CLI's command files share (cli.cpp, cli_status.cpp).
#pragma once

#include "client.h"
#include "config.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTextStream>

namespace uqbit {

void json(QTextStream &out, const QJsonObject &o);
int fail(QTextStream &err, const QString &msg, int code = 1);
QString self();
/// The engine's version, or "" when it doesn't answer.
QString engineVersion(Client &c, int timeoutMs = 4000);
/// Starts the engine when it isn't answering and waits up to 20 s for it; then gives it the
/// settings.
QString ensureEngine(Client &c);
/// Pushes the settings to a running engine (nothing when it isn't running).
void applyToEngine(const Config &cfg);

int cmdStatus(bool asJson, QTextStream &out, QTextStream &err);
int cmdAll(bool start, QTextStream &out, QTextStream &err);
int cmdShow(QTextStream &err);
int cmdBlocklist(const QStringList &args, bool asJson, QTextStream &out, QTextStream &err);
int cmdTrackers(const QStringList &args, bool asJson, QTextStream &out, QTextStream &err);
int cmdPlugins(const QStringList &args, bool asJson, QTextStream &out, QTextStream &err);
QJsonObject trackersJson(const Config &cfg);
QString refreshTrackers(const Config &cfg);

} // namespace uqbit
