#include "live_execution_lock.h"
#include "../config/env_loader.h"

#include <stdio.h>
#include <string.h>

static void live_lock_set(
    LiveExecutionLockResult *result,
    int allowed,
    const char *decision,
    const char *reason
) {
    if (result == NULL) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->allowed = allowed;
    snprintf(result->decision, sizeof(result->decision), "%s", decision ? decision : "BLOCKED");
    snprintf(result->reason, sizeof(result->reason), "%s", reason ? reason : "");
}

LiveExecutionLockResult live_execution_lock_check(
    const OrderExecutionPlan *plan,
    const StrategySettings *settings
) {
    LiveExecutionLockResult result;

    live_lock_set(
        &result,
        0,
        "BLOCKED",
        "LIVE_EXECUTION_LOCK invalid input"
    );

    if (plan == NULL || settings == NULL) {
        return result;
    }

    if (!plan->allowed) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK order plan is not allowed"
        );
        return result;
    }

    if (plan->dry_run) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK order plan is still dry-run"
        );
        return result;
    }

    if (settings->runtime_mode != RUNTIME_MODE_LIVE_TRADING) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK runtime mode is not LIVE_TRADING"
        );
        return result;
    }

    if (!settings->live_trading_armed) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK LIVE_TRADING is not manually armed"
        );
        return result;
    }

    if (settings->emergency_stop_enabled) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK emergency stop is active"
        );
        return result;
    }

    if (!env_load_bool_flag("HELIX_REAL_TRADING_ENABLED", 0)) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK missing HELIX_REAL_TRADING_ENABLED=true"
        );
        return result;
    }

    if (!env_load_bool_flag("HELIX_ALLOW_COINBASE_ORDERS", 0)) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK missing HELIX_ALLOW_COINBASE_ORDERS=true"
        );
        return result;
    }

    if (!env_load_bool_flag("HELIX_I_UNDERSTAND_REAL_MONEY_RISK", 0)) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK missing HELIX_I_UNDERSTAND_REAL_MONEY_RISK=true"
        );
        return result;
    }

#ifndef HELIX_ENABLE_REAL_COINBASE_ORDERS
    live_lock_set(
        &result,
        0,
        "BLOCKED",
        "LIVE_EXECUTION_LOCK compile flag HELIX_ENABLE_REAL_COINBASE_ORDERS is not enabled"
    );
    return result;
#endif

    live_lock_set(
        &result,
        1,
        "ALLOWED",
        "LIVE_EXECUTION_LOCK all explicit live execution gates passed"
    );

    return result;
}
