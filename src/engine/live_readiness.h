#ifndef HELIX_LIVE_READINESS_H
#define HELIX_LIVE_READINESS_H

#include "bot_state.h"
#include "settings.h"

typedef struct {
    int ready;
    int blocking_count;
    int warning_count;
    char status[64];
    char reason[512];
} LiveReadinessReport;

/*
 * Centralized pre-live readiness check.
 *
 * This module does not place orders and does not unlock LIVE_TRADING.
 * It explains why Helix is or is not ready to even approach real execution.
 */
LiveReadinessReport live_readiness_check(
    const BotState *state,
    const StrategySettings *settings
);

#endif
