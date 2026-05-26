#ifndef HELIX_FINAL_LIVE_GATE_H
#define HELIX_FINAL_LIVE_GATE_H

#include "bot_state.h"
#include "settings.h"
#include "../exchange/order_executor.h"

typedef struct {
    int allowed;
    char decision[32];
    char reason[256];
} FinalLiveGateCheck;

/*
 * Final gate before any order path.
 *
 * Current phase: dry-run only.
 * This module does not place orders and does not unlock LIVE_TRADING.
 * It centralizes the last safety checks that must stay green before
 * the executor path can be considered valid.
 */
FinalLiveGateCheck final_live_gate_check_dry_run(
    const BotState *state,
    const StrategySettings *settings,
    const OrderExecutionPlan *plan
);

#endif
