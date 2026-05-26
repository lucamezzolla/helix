#ifndef HELIX_VOLATILITY_PROTECTION_H
#define HELIX_VOLATILITY_PROTECTION_H

#include "bot_state.h"
#include "settings.h"

typedef struct {
    int allowed;
    char decision[32];
    char reason[180];
    double reference_price;
    double current_price;
    double move_percent;
    int elapsed_seconds;
} VolatilityProtectionCheck;

VolatilityProtectionCheck volatility_protection_check(
    const BotState *state,
    const StrategySettings *settings
);

void volatility_protection_reset(void);

#endif
