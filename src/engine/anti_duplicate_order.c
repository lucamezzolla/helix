#include "anti_duplicate_order.h"
#include "../db/database.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void set_result(
    AntiDuplicateOrderCheck *check,
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

static const char *side_to_string(OrderExecutorSide side) {
    switch (side) {
        case ORDER_EXECUTOR_SIDE_BUY:
            return "BUY";
        case ORDER_EXECUTOR_SIDE_SELL:
            return "SELL";
        default:
            return "UNKNOWN";
    }
}

static double requested_amount_for_signature(const OrderExecutionPlan *plan) {
    if (plan == NULL) {
        return 0.0;
    }

    if (plan->side == ORDER_EXECUTOR_SIDE_BUY) {
        return plan->requested_quote_size;
    }

    return plan->requested_base_size;
}

AntiDuplicateOrderCheck anti_duplicate_order_check_plan(
    const OrderExecutionPlan *plan,
    int cooldown_seconds
) {
    static char last_signature[160] = "";
    static time_t last_allowed_time = 0;

    AntiDuplicateOrderCheck check;
    OrderStateRecord active_order;
    char signature[160];
    time_t now = time(NULL);
    double requested_amount;

    set_result(&check, 0, "BLOCKED", "ANTI_DUPLICATE_ORDER_BLOCKED invalid plan");

    if (plan == NULL) {
        return check;
    }

    if (!plan->allowed) {
        set_result(&check, 0, "BLOCKED", "ANTI_DUPLICATE_ORDER_BLOCKED plan not allowed");
        return check;
    }

    if (plan->product_id[0] == '\0') {
        set_result(&check, 0, "BLOCKED", "ANTI_DUPLICATE_ORDER_BLOCKED missing product id");
        return check;
    }

    requested_amount = requested_amount_for_signature(plan);
    if (requested_amount <= 0.0) {
        set_result(&check, 0, "BLOCKED", "ANTI_DUPLICATE_ORDER_BLOCKED invalid requested amount");
        return check;
    }

    memset(&active_order, 0, sizeof(active_order));
    if (db_get_active_order_state(&active_order) && !active_order.dry_run) {
        snprintf(
            check.reason,
            sizeof(check.reason),
            "ANTI_DUPLICATE_ORDER_BLOCKED unresolved real order %s %s status %s",
            active_order.client_order_id,
            active_order.side,
            active_order.status
        );
        snprintf(check.decision, sizeof(check.decision), "%s", "BLOCKED");
        check.allowed = 0;
        return check;
    }

    snprintf(
        signature,
        sizeof(signature),
        "%s|%s|%.8f",
        plan->product_id,
        side_to_string(plan->side),
        requested_amount
    );

    if (
        last_signature[0] != '\0' &&
        strcmp(signature, last_signature) == 0 &&
        last_allowed_time > 0 &&
        difftime(now, last_allowed_time) < cooldown_seconds
    ) {
        snprintf(
            check.reason,
            sizeof(check.reason),
            "ANTI_DUPLICATE_ORDER_BLOCKED duplicate dry-run plan inside cooldown %d sec",
            cooldown_seconds
        );
        snprintf(check.decision, sizeof(check.decision), "%s", "BLOCKED");
        check.allowed = 0;
        return check;
    }

    snprintf(last_signature, sizeof(last_signature), "%s", signature);
    last_allowed_time = now;

    snprintf(
        check.reason,
        sizeof(check.reason),
        "ANTI_DUPLICATE_ORDER_OK %s %s amount %.8f",
        side_to_string(plan->side),
        plan->product_id,
        requested_amount
    );
    snprintf(check.decision, sizeof(check.decision), "%s", "ALLOWED");
    check.allowed = 1;

    return check;
}

int anti_duplicate_order_record_dry_run_plan(const OrderExecutionPlan *plan) {
    const char *side;

    if (plan == NULL || !plan->allowed || plan->client_order_id[0] == '\0') {
        return 0;
    }

    side = side_to_string(plan->side);

    return db_upsert_order_state(
        plan->client_order_id,
        side,
        "DRY_RUN_READY",
        plan->product_id,
        1,
        plan->requested_quote_size,
        plan->requested_base_size,
        plan->preview_total_eur,
        plan->preview_fee_eur,
        plan->preview_base_size,
        plan->preview_avg_price,
        plan->reason
    );
}
