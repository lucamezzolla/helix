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

static double live_plan_notional_eur(const OrderExecutionPlan *plan) {
    if (plan == NULL) {
        return 0.0;
    }

    if (plan->side == ORDER_EXECUTOR_SIDE_BUY) {
        return plan->requested_quote_size;
    }

    return plan->preview_total_eur;
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

    if (!settings->micro_live_enabled) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK micro-live mode is not enabled"
        );
        return result;
    }

    if (settings->micro_live_max_order_eur <= 0.0 || settings->micro_live_max_order_eur > 50.0) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK micro-live max order must be between 0 and 50 EUR"
        );
        return result;
    }

    if (live_plan_notional_eur(plan) <= 0.0 || live_plan_notional_eur(plan) > settings->micro_live_max_order_eur) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK order size exceeds micro-live max order"
        );
        return result;
    }

    if (settings->max_orders_per_day > 1) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK max orders per day must be 1 in micro-live"
        );
        return result;
    }

    if (settings->order_cooldown_seconds < 900) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK order cooldown must be at least 900 sec in micro-live"
        );
        return result;
    }

    if (settings->liquidity_reserve_percent < 80.0) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK liquidity reserve must be at least 80% in micro-live"
        );
        return result;
    }

    if (!settings->micro_live_stop_after_real_order) {
        live_lock_set(
            &result,
            0,
            "BLOCKED",
            "LIVE_EXECUTION_LOCK stop-after-real-order must be enabled in micro-live"
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
