// uQ's theme tokens (~/.local/state/uq/theme/tokens.json, written by uqd) for the QML Theme
// singleton, from the vendored uq-theme module (uqtheme.h/.cpp, copied unchanged from uQ's
// native/uq-theme; CI checks them with scripts/check-uq-native.sh). Without uQ, `present` is false
// and the window uses its own dark palette. The file is followed with a file watch, not polled.
#pragma once

#include "uqtheme.h"

namespace uqbit {

/// $UQBIT_UQ_TOKENS, else $XDG_STATE_HOME/uq/theme/tokens.json, else ~/.local/state/uq/...
inline QString tokensPath() { return uqtheme::tokensPath("UQBIT_UQ_TOKENS"); }

} // namespace uqbit
