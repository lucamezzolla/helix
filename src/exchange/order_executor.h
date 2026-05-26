#ifndef HELIX_ORDER_EXECUTOR_H
#define HELIX_ORDER_EXECUTOR_H

#include "order_preview.h"
#include "../engine/settings.h"

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
    char preview_id[128];
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

typedef struct {
    int allowed;
    int attempted_real_execution;
    int sent_to_coinbase;
    long http_code;
    char decision[32];
    char reason[256];
    char client_order_id[96];
    char coinbase_order_id[128];
} OrderExecutionResult;

/*
 * Real Coinbase order executor.
 *
 * Safe by default:
 * - if HELIX_ENABLE_REAL_COINBASE_ORDERS is NOT defined at compile time,
 *   this function never sends anything;
 * - even when compiled in, it must pass live_execution_lock_check();
 * - it expects a non-dry-run plan, explicit .env consent flags, LIVE_TRADING,
 *   manual arm, and every final safety gate.
 */
OrderExecutionResult order_executor_execute_real(
    const OrderExecutionPlan *plan,
    const StrategySettings *settings
);

/*
 * Compatibility wrapper used by earlier pre-live code.
 * It delegates to order_executor_execute_real(), which is still blocked unless
 * every explicit safety gate is satisfied and the compile flag is enabled.
 */
OrderExecutionResult order_executor_execute_real_blocked(
    const OrderExecutionPlan *plan,
    const StrategySettings *settings
);

#endif
