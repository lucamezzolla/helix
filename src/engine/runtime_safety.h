#ifndef HELIX_RUNTIME_SAFETY_H
#define HELIX_RUNTIME_SAFETY_H

#include "bot_state.h"
#include "settings.h"

typedef struct {
    int allowed;
    char decision[32];
    char reason[160];
} RuntimeSafetyCheck;

RuntimeSafetyCheck runtime_safety_check_buy(
    const BotState *state,
    const StrategySettings *settings,
    double eur_amount
);

RuntimeSafetyCheck runtime_safety_check_sell(
    const BotState *state,
    const StrategySettings *settings,
    double btc_amount
);

RuntimeSafetyCheck runtime_safety_check_live_trading_arm(
    const StrategySettings *settings
);

#endif
