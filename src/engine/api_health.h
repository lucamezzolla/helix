#ifndef HELIX_API_HEALTH_H
#define HELIX_API_HEALTH_H

#include "bot_state.h"
#include "settings.h"

typedef struct {
    int ok;
    int blocking_count;
    int warning_count;
    char status[64];
    char reason[512];
} ApiHealthReport;

/*
 * Lightweight API health check.
 *
 * This check does not place orders and does not perform heavy network calls.
 * It only verifies whether the current runtime mode has the basic prerequisites
 * to use Coinbase safely: credentials, valid market price and sane settings.
 */
ApiHealthReport api_health_check_light(
    const BotState *state,
    const StrategySettings *settings
);

#endif
