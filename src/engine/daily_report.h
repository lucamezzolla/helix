#ifndef HELIX_DAILY_REPORT_H
#define HELIX_DAILY_REPORT_H

#include "bot_state.h"
#include "settings.h"

void daily_report_maybe_send(
    const BotState *state,
    const StrategySettings *settings
);

#endif
