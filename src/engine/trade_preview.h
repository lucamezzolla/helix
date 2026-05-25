#ifndef HELIX_TRADE_PREVIEW_H
#define HELIX_TRADE_PREVIEW_H

typedef struct {
    double gross_value;
    double estimated_fee;
    double net_value;
    double cost_basis;
    double net_profit;
    double net_profit_percent;
    int allowed;
} TradePreview;

/*
 * estimated_fee_rate is decimal rate, not human percent.
 * Example:
 *   0.006  = 0.6%
 *   0.0012 = 0.12%
 */
TradePreview trade_preview_sell(
    double btc_amount,
    double current_price,
    double cost_basis,
    double estimated_fee_rate,
    double min_profit_eur,
    double min_profit_percent
);

TradePreview trade_preview_buy(
    double eur_amount,
    double current_price,
    double estimated_fee_rate
);

#endif
