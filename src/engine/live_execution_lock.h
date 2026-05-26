#ifndef HELIX_LIVE_EXECUTION_LOCK_H
#define HELIX_LIVE_EXECUTION_LOCK_H

#include "settings.h"
#include "../exchange/order_executor.h"

typedef struct {
    int allowed;
    char decision[32];
    char reason[256];
} LiveExecutionLockResult;

/*
 * Final pre-execution lock for real Coinbase orders.
 *
 * This module does not send orders. It only verifies that every explicit
 * live-trading consent/safety gate required before a real executor path is
 * satisfied.
 */
LiveExecutionLockResult live_execution_lock_check(
    const OrderExecutionPlan *plan,
    const StrategySettings *settings
);

#endif
