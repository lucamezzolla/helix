#ifndef HELIX_RECONCILIATION_H
#define HELIX_RECONCILIATION_H

#include "bot_state.h"
#include "../wallet/wallet_info.h"
#include "../exchange/coinbase_client.h"

typedef enum {
    RECONCILIATION_STATUS_OK = 0,
    RECONCILIATION_STATUS_WARNING = 1,
    RECONCILIATION_STATUS_BLOCKED = 2
} ReconciliationStatus;

typedef struct {
    ReconciliationStatus status;
    int trading_allowed;
    char decision[32];
    char reason[192];
    double eur_delta;
    double btc_delta;
    double reconstructed_cost_basis;
    double reconstructed_avg_buy_price;
} ReconciliationReport;

ReconciliationReport reconciliation_check_live_readonly(
    const BotState *state,
    const WalletInfo *remote_wallet,
    const CoinbasePositionSummary *position_summary
);

#endif
