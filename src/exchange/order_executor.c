#include "order_executor.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static void safe_copy(char *dst, size_t dst_size, const char *src) {
    if (dst == NULL || dst_size == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    snprintf(dst, dst_size, "%s", src);
}

static void make_client_order_id(char *buffer, size_t buffer_size, const char *side) {
    time_t now = time(NULL);

    if (buffer == NULL || buffer_size == 0) {
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "helix-dryrun-%s-%ld",
        side ? side : "unknown",
        (long) now
    );
}

static OrderExecutionPlan empty_plan(OrderExecutorSide side, const char *product_id) {
    OrderExecutionPlan plan;

    memset(&plan, 0, sizeof(plan));
    plan.side = side;
    plan.dry_run = 1;
    safe_copy(plan.product_id, sizeof(plan.product_id), product_id ? product_id : "BTC-EUR");

    return plan;
}

OrderExecutionPlan order_executor_plan_market_buy_dry_run(
    const char *product_id,
    double quote_size_eur,
    CoinbaseOrderPreview preview
) {
    OrderExecutionPlan plan = empty_plan(ORDER_EXECUTOR_SIDE_BUY, product_id);

    plan.requested_quote_size = quote_size_eur;
    plan.preview_total_eur = preview.order_total;
    plan.preview_fee_eur = preview.commission_total;
    plan.preview_base_size = preview.base_size;
    plan.preview_avg_price = preview.est_average_filled_price;
    make_client_order_id(plan.client_order_id, sizeof(plan.client_order_id), "buy");

    if (quote_size_eur <= 0.0) {
        safe_copy(plan.reason, sizeof(plan.reason), "BUY dry-run bloccato: quote size non valida");
        return plan;
    }

    if (!preview.connected || !preview.allowed) {
        snprintf(
            plan.reason,
            sizeof(plan.reason),
            "BUY dry-run bloccato: preview Coinbase non valida | http %ld | %.120s",
            preview.http_code,
            preview.message
        );
        return plan;
    }

    if (preview.base_size <= 0.0) {
        safe_copy(plan.reason, sizeof(plan.reason), "BUY dry-run bloccato: base size preview non valida");
        return plan;
    }

    plan.allowed = 1;
    snprintf(
        plan.reason,
        sizeof(plan.reason),
        "BUY dry-run pronto | quote %.2f | base %.8f | fee %.2f | avg %.2f",
        quote_size_eur,
        preview.base_size,
        preview.commission_total,
        preview.est_average_filled_price
    );

    return plan;
}

OrderExecutionPlan order_executor_plan_market_sell_dry_run(
    const char *product_id,
    double base_size_btc,
    CoinbaseOrderPreview preview
) {
    OrderExecutionPlan plan = empty_plan(ORDER_EXECUTOR_SIDE_SELL, product_id);

    plan.requested_base_size = base_size_btc;
    plan.preview_total_eur = preview.order_total;
    plan.preview_fee_eur = preview.commission_total;
    plan.preview_base_size = preview.base_size;
    plan.preview_avg_price = preview.est_average_filled_price;
    make_client_order_id(plan.client_order_id, sizeof(plan.client_order_id), "sell");

    if (base_size_btc <= 0.0) {
        safe_copy(plan.reason, sizeof(plan.reason), "SELL dry-run bloccato: base size non valida");
        return plan;
    }

    if (!preview.connected || !preview.allowed) {
        snprintf(
            plan.reason,
            sizeof(plan.reason),
            "SELL dry-run bloccato: preview Coinbase non valida | http %ld | %.120s",
            preview.http_code,
            preview.message
        );
        return plan;
    }

    if (preview.order_total <= 0.0) {
        safe_copy(plan.reason, sizeof(plan.reason), "SELL dry-run bloccato: totale preview non valido");
        return plan;
    }

    plan.allowed = 1;
    snprintf(
        plan.reason,
        sizeof(plan.reason),
        "SELL dry-run pronto | base %.8f | total %.2f | fee %.2f | avg %.2f",
        base_size_btc,
        preview.order_total,
        preview.commission_total,
        preview.est_average_filled_price
    );

    return plan;
}
