#ifndef HELIX_OPERATIONAL_LIMITS_H
#define HELIX_OPERATIONAL_LIMITS_H

#include "settings.h"
#include "../exchange/order_executor.h"

typedef struct {
    int allowed;
    int orders_today;
    int seconds_since_last_order;
    char decision[32];
    char reason[256];
} OperationalLimitCheck;

OperationalLimitCheck operational_limits_check(
    const StrategySettings *settings,
    OrderExecutorSide side
);

void operational_limits_audit_if_blocked(
    OperationalLimitCheck check,
    double price,
    double btc_balance,
    double eur_balance
);

#endif
