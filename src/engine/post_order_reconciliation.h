#ifndef HELIX_POST_ORDER_RECONCILIATION_H
#define HELIX_POST_ORDER_RECONCILIATION_H

#include "bot_state.h"
#include "settings.h"
#include "../exchange/order_executor.h"

/*
 * Post-order reconciliation gate.
 *
 * This module prepares the future real-order lifecycle: after a real order is
 * attempted, Helix must not trust local state again until Coinbase wallet/order
 * state has been reconciled. In the current pre-live phase, dry-run plans are
 * allowed and real plans are blocked unless the future reconciliation path is
 * implemented explicitly.
 */
typedef struct {
    int allowed;
    char decision[48];
    char reason[256];
} PostOrderReconciliationCheck;

PostOrderReconciliationCheck post_order_reconciliation_check_after_plan(
    const BotState *state,
    const StrategySettings *settings,
    const OrderExecutionPlan *plan
);

PostOrderReconciliationCheck post_order_reconciliation_check_latest_real_order(
    BotState *state,
    const StrategySettings *settings
);

#endif
