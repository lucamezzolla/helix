#ifndef HELIX_RISK_GUARD_H
#define HELIX_RISK_GUARD_H

#include "bot_state.h"
#include "settings.h"

typedef struct {
    int allowed;
    double market_value_eur;
    double cost_basis_eur;
    double unrealized_pnl_eur;
    double drawdown_percent;
    char decision[32];
    char reason[256];
} RiskGuardCheck;

RiskGuardCheck risk_guard_check(const BotState *state, const StrategySettings *settings);

void risk_guard_audit_if_blocked(
    RiskGuardCheck check,
    double price,
    double btc_balance,
    double eur_balance
);

#endif
