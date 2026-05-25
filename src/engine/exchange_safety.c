#include "exchange_safety.h"

#include <stdio.h>
#include <string.h>

static double abs_double(double value) {
    return value < 0.0 ? -value : value;
}

static double percent_deviation(double expected, double actual) {
    if (expected <= 0.0) {
        return actual > 0.0 ? 100.0 : 0.0;
    }

    return (abs_double(actual - expected) / expected) * 100.0;
}

static void init_check(
    ExchangeSafetyCheck *check,
    ExchangeSafetySide side
) {
    memset(check, 0, sizeof(*check));
    check->side = side;
    snprintf(check->decision, sizeof(check->decision), "BLOCKED");
    snprintf(check->reason, sizeof(check->reason), "not evaluated");
}

static int exchange_preview_is_usable(CoinbaseOrderPreview exchange_preview) {
    return exchange_preview.connected &&
           exchange_preview.allowed &&
           exchange_preview.http_code >= 200 &&
           exchange_preview.http_code < 300;
}

ExchangeSafetyCheck exchange_safety_check_sell(
    TradePreview local_preview,
    CoinbaseOrderPreview exchange_preview,
    double btc_amount
) {
    ExchangeSafetyCheck check;
    double local_net_value;

    init_check(&check, EXCHANGE_SAFETY_SIDE_SELL);

    check.local_total_eur = local_preview.net_value;
    check.local_fee_eur = local_preview.estimated_fee;
    check.local_btc_amount = btc_amount;
    check.exchange_total_eur = exchange_preview.order_total;
    check.exchange_fee_eur = exchange_preview.commission_total;
    check.exchange_btc_amount = exchange_preview.base_size;
    check.exchange_avg_price = exchange_preview.est_average_filled_price;

    if (!local_preview.allowed) {
        snprintf(check.decision, sizeof(check.decision), "BLOCKED_LOCAL");
        snprintf(check.reason, sizeof(check.reason), "local SELL preview is not allowed");
        return check;
    }

    if (!exchange_preview_is_usable(exchange_preview)) {
        snprintf(check.decision, sizeof(check.decision), "BLOCKED_EXCHANGE");
        snprintf(
            check.reason,
            sizeof(check.reason),
            "Coinbase SELL preview unavailable: http %ld %.96s",
            exchange_preview.http_code,
            exchange_preview.message
        );
        return check;
    }

    local_net_value = local_preview.net_value;
    check.total_deviation_percent = percent_deviation(local_net_value, exchange_preview.order_total);
    check.fee_deviation_percent = percent_deviation(local_preview.estimated_fee, exchange_preview.commission_total);

    if (exchange_preview.order_total <= 0.0 || exchange_preview.commission_total < 0.0) {
        snprintf(check.decision, sizeof(check.decision), "BLOCKED_EXCHANGE_VALUES");
        snprintf(check.reason, sizeof(check.reason), "Coinbase SELL preview returned invalid values");
        return check;
    }

    if (check.total_deviation_percent > EXCHANGE_SAFETY_MAX_PRICE_DEVIATION_PERCENT) {
        snprintf(check.decision, sizeof(check.decision), "BLOCKED_TOTAL_DEVIATION");
        snprintf(
            check.reason,
            sizeof(check.reason),
            "SELL total deviation %.2f%% exceeds %.2f%%",
            check.total_deviation_percent,
            EXCHANGE_SAFETY_MAX_PRICE_DEVIATION_PERCENT
        );
        return check;
    }

    if (local_preview.estimated_fee > 0.0 &&
        check.fee_deviation_percent > EXCHANGE_SAFETY_MAX_FEE_DEVIATION_PERCENT) {
        snprintf(check.decision, sizeof(check.decision), "BLOCKED_FEE_DEVIATION");
        snprintf(
            check.reason,
            sizeof(check.reason),
            "SELL fee deviation %.2f%% exceeds %.2f%%",
            check.fee_deviation_percent,
            EXCHANGE_SAFETY_MAX_FEE_DEVIATION_PERCENT
        );
        return check;
    }

    check.allowed = 1;
    snprintf(check.decision, sizeof(check.decision), "ALLOWED_DRY_RUN");
    snprintf(
        check.reason,
        sizeof(check.reason),
        "SELL safety OK | local %.2f EUR | exchange %.2f EUR | fee %.2f EUR | dev %.2f%%",
        local_preview.net_value,
        exchange_preview.order_total,
        exchange_preview.commission_total,
        check.total_deviation_percent
    );

    return check;
}

ExchangeSafetyCheck exchange_safety_check_buy(
    TradePreview local_preview,
    CoinbaseOrderPreview exchange_preview,
    double eur_amount
) {
    ExchangeSafetyCheck check;

    init_check(&check, EXCHANGE_SAFETY_SIDE_BUY);

    check.local_total_eur = eur_amount;
    check.local_fee_eur = local_preview.estimated_fee;
    check.local_btc_amount = local_preview.net_value;
    check.exchange_total_eur = exchange_preview.order_total;
    check.exchange_fee_eur = exchange_preview.commission_total;
    check.exchange_btc_amount = exchange_preview.base_size;
    check.exchange_avg_price = exchange_preview.est_average_filled_price;

    if (!local_preview.allowed) {
        snprintf(check.decision, sizeof(check.decision), "BLOCKED_LOCAL");
        snprintf(check.reason, sizeof(check.reason), "local BUY preview is not allowed");
        return check;
    }

    if (!exchange_preview_is_usable(exchange_preview)) {
        snprintf(check.decision, sizeof(check.decision), "BLOCKED_EXCHANGE");
        snprintf(
            check.reason,
            sizeof(check.reason),
            "Coinbase BUY preview unavailable: http %ld %.96s",
            exchange_preview.http_code,
            exchange_preview.message
        );
        return check;
    }

    check.total_deviation_percent = percent_deviation(eur_amount, exchange_preview.order_total);
    check.fee_deviation_percent = percent_deviation(local_preview.estimated_fee, exchange_preview.commission_total);

    if (exchange_preview.order_total <= 0.0 || exchange_preview.base_size <= 0.0) {
        snprintf(check.decision, sizeof(check.decision), "BLOCKED_EXCHANGE_VALUES");
        snprintf(check.reason, sizeof(check.reason), "Coinbase BUY preview returned invalid values");
        return check;
    }

    if (check.total_deviation_percent > EXCHANGE_SAFETY_MAX_PRICE_DEVIATION_PERCENT) {
        snprintf(check.decision, sizeof(check.decision), "BLOCKED_TOTAL_DEVIATION");
        snprintf(
            check.reason,
            sizeof(check.reason),
            "BUY total deviation %.2f%% exceeds %.2f%%",
            check.total_deviation_percent,
            EXCHANGE_SAFETY_MAX_PRICE_DEVIATION_PERCENT
        );
        return check;
    }

    if (local_preview.estimated_fee > 0.0 &&
        check.fee_deviation_percent > EXCHANGE_SAFETY_MAX_FEE_DEVIATION_PERCENT) {
        snprintf(check.decision, sizeof(check.decision), "BLOCKED_FEE_DEVIATION");
        snprintf(
            check.reason,
            sizeof(check.reason),
            "BUY fee deviation %.2f%% exceeds %.2f%%",
            check.fee_deviation_percent,
            EXCHANGE_SAFETY_MAX_FEE_DEVIATION_PERCENT
        );
        return check;
    }

    check.allowed = 1;
    snprintf(check.decision, sizeof(check.decision), "ALLOWED_DRY_RUN");
    snprintf(
        check.reason,
        sizeof(check.reason),
        "BUY safety OK | local %.8f BTC | exchange %.8f BTC | fee %.2f EUR | dev %.2f%%",
        local_preview.net_value,
        exchange_preview.base_size,
        exchange_preview.commission_total,
        check.total_deviation_percent
    );

    return check;
}
