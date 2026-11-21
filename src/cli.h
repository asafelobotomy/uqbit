// `uqbit config|default|engine|add|list|notify-finished ...`: everything the uQ Settings page
// and scripts need, without a window.
#pragma once

#include <QStringList>
#include <QTextStream>

namespace uqbit {

bool isCliCommand(const QStringList &args);
int runCli(const QStringList &args, QTextStream &out, QTextStream &err);

} // namespace uqbit
