#ifndef HELIX_ORDER_EXECUTOR_H
#define HELIX_ORDER_EXECUTOR_H

#include "order_preview.h"

typedef enum {
    ORDER_EXECUTOR_SIDE_BUY = 0,
    ORDER_EXECUTOR_SIDE_SELL = 1
} OrderExecutorSide;

typedef struct {
    int allowed;
    int dry_run;
    OrderExecutorSide side;
    char product_id[32];
    char reason[256];
    char client_order_id[96];
    double requested_quote_size;
    double requested_base_size;
    double preview_total_eur;
    double preview_fee_eur;
    double preview_base_size;
    double preview_avg_price;
} OrderExecutionPlan;

OrderExecutionPlan order_executor_plan_market_buy_dry_run(
    const char *product_id,
    double quote_size_eur,
    CoinbaseOrderPreview preview
);

OrderExecutionPlan order_executor_plan_market_sell_dry_run(
    const char *product_id,
    double base_size_btc,
    CoinbaseOrderPreview preview
);

#endif
