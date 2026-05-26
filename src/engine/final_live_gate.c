#include "final_live_gate.h"
#include "emergency_stop.h"
#include "runtime_safety.h"

#include <stdio.h>
#include <string.h>

static void set_result(
    FinalLiveGateCheck *check,
    int allowed,
    const char *decision,
    const char *reason
) {
    if (check == NULL) {
        return;
    }

    memset(check, 0, sizeof(*check));
    check->allowed = allowed;
    snprintf(check->decision, sizeof(check->decision), "%s", decision ? decision : "UNKNOWN");
    snprintf(check->reason, sizeof(check->reason), "%s", reason ? reason : "");
}

FinalLiveGateCheck final_live_gate_check_dry_run(
    const BotState *state,
    const StrategySettings *settings,
    const OrderExecutionPlan *plan
) {
    FinalLiveGateCheck check;

    set_result(&check, 0, "BLOCKED", "FINAL_LIVE_GATE_BLOCKED invalid input");

    if (state == NULL || settings == NULL || plan == NULL) {
        return check;
    }

    if (emergency_stop_is_active(settings)) {
        set_result(&check, 0, "BLOCKED", "FINAL_LIVE_GATE_BLOCKED emergency stop active");
        return check;
    }

    if (!plan->allowed) {
        set_result(&check, 0, "BLOCKED", "FINAL_LIVE_GATE_BLOCKED execution plan not allowed");
        return check;
    }

    if (!plan->dry_run) {
        set_result(&check, 0, "BLOCKED", "FINAL_LIVE_GATE_BLOCKED real order execution disabled");
        return check;
    }

    if (plan->product_id[0] == '\0') {
        set_result(&check, 0, "BLOCKED", "FINAL_LIVE_GATE_BLOCKED missing product id");
        return check;
    }

    if (state->current_price <= 0.0) {
        set_result(&check, 0, "BLOCKED", "FINAL_LIVE_GATE_BLOCKED stale or invalid market price");
        return check;
    }

    if (settings->runtime_mode == RUNTIME_MODE_LIVE_TRADING) {
        RuntimeSafetyCheck live_safety = runtime_safety_check_live_trading_arm(settings);

        snprintf(
            check.reason,
            sizeof(check.reason),
            "FINAL_LIVE_GATE_BLOCKED LIVE_TRADING still blocked | %.160s",
            live_safety.reason
        );
        snprintf(check.decision, sizeof(check.decision), "%s", "BLOCKED");
        check.allowed = 0;
        return check;
    }

    if (plan->side == ORDER_EXECUTOR_SIDE_BUY) {
        if (plan->requested_quote_size <= 0.0) {
            set_result(&check, 0, "BLOCKED", "FINAL_LIVE_GATE_BLOCKED invalid BUY quote size");
            return check;
        }

        if (state->eur_balance + 0.000001 < plan->requested_quote_size) {
            set_result(&check, 0, "BLOCKED", "FINAL_LIVE_GATE_BLOCKED insufficient EUR balance for BUY plan");
            return check;
        }

        if (plan->preview_base_size <= 0.0 || plan->preview_total_eur <= 0.0) {
            set_result(&check, 0, "BLOCKED", "FINAL_LIVE_GATE_BLOCKED invalid BUY Coinbase preview values");
            return check;
        }
    } else if (plan->side == ORDER_EXECUTOR_SIDE_SELL) {
        if (plan->requested_base_size <= 0.0) {
            set_result(&check, 0, "BLOCKED", "FINAL_LIVE_GATE_BLOCKED invalid SELL base size");
            return check;
        }

        if (state->btc_balance + 0.00000001 < plan->requested_base_size) {
            set_result(&check, 0, "BLOCKED", "FINAL_LIVE_GATE_BLOCKED insufficient BTC balance for SELL plan");
            return check;
        }

        if (plan->preview_total_eur <= 0.0) {
            set_result(&check, 0, "BLOCKED", "FINAL_LIVE_GATE_BLOCKED invalid SELL Coinbase preview total");
            return check;
        }
    } else {
        set_result(&check, 0, "BLOCKED", "FINAL_LIVE_GATE_BLOCKED unknown order side");
        return check;
    }

    set_result(&check, 1, "ALLOWED_DRY_RUN", "FINAL_LIVE_GATE_OK dry-run order path allowed");
    return check;
}
