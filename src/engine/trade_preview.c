#include "trade_preview.h"

static double safe_percent(double part, double total) {
    if (total <= 0.0) {
        return 0.0;
    }

    return (part / total) * 100.0;
}

static double normalize_fee_rate(double estimated_fee_rate) {
    if (estimated_fee_rate < 0.0) {
        return 0.0;
    }

    if (estimated_fee_rate > 1.0) {
        return 1.0;
    }

    return estimated_fee_rate;
}

TradePreview trade_preview_sell(
    double btc_amount,
    double current_price,
    double cost_basis,
    double estimated_fee_rate,
    double min_profit_eur,
    double min_profit_percent
) {
    TradePreview preview;
    double fee_rate = normalize_fee_rate(estimated_fee_rate);

    preview.gross_value = 0.0;
    preview.estimated_fee = 0.0;
    preview.net_value = 0.0;
    preview.cost_basis = cost_basis;
    preview.net_profit = 0.0;
    preview.net_profit_percent = 0.0;
    preview.allowed = 0;

    if (btc_amount <= 0.0 || current_price <= 0.0 || cost_basis <= 0.0) {
        return preview;
    }

    preview.gross_value = btc_amount * current_price;
    preview.estimated_fee = preview.gross_value * fee_rate;
    preview.net_value = preview.gross_value - preview.estimated_fee;
    preview.net_profit = preview.net_value - preview.cost_basis;
    preview.net_profit_percent = safe_percent(preview.net_profit, preview.cost_basis);

    if (
        preview.net_profit >= min_profit_eur &&
        preview.net_profit_percent >= min_profit_percent
    ) {
        preview.allowed = 1;
    }

    return preview;
}

TradePreview trade_preview_buy(
    double eur_amount,
    double current_price,
    double estimated_fee_rate
) {
    TradePreview preview;
    double fee_rate = normalize_fee_rate(estimated_fee_rate);
    double eur_after_fee;

    preview.gross_value = eur_amount;
    preview.estimated_fee = 0.0;
    preview.net_value = 0.0;
    preview.cost_basis = eur_amount;
    preview.net_profit = 0.0;
    preview.net_profit_percent = 0.0;
    preview.allowed = 0;

    if (eur_amount <= 0.0 || current_price <= 0.0) {
        return preview;
    }

    preview.estimated_fee = eur_amount * fee_rate;
    eur_after_fee = eur_amount - preview.estimated_fee;

    if (eur_after_fee <= 0.0) {
        return preview;
    }

    /* For BUY, net_value is the estimated BTC amount received. */
    preview.net_value = eur_after_fee / current_price;
    preview.allowed = 1;

    return preview;
}
