#include "risk_guard.h"
#include "../db/database.h"

#include <stdio.h>
#include <string.h>

static RiskGuardCheck make_check(
    int allowed,
    const char *decision,
    const char *reason,
    double market_value_eur,
    double cost_basis_eur,
    double unrealized_pnl_eur,
    double drawdown_percent
) {
    RiskGuardCheck check;

    check.allowed = allowed;
    check.market_value_eur = market_value_eur;
    check.cost_basis_eur = cost_basis_eur;
    check.unrealized_pnl_eur = unrealized_pnl_eur;
    check.drawdown_percent = drawdown_percent;

    snprintf(check.decision, sizeof(check.decision), "%s", decision ? decision : "UNKNOWN");
    snprintf(check.reason, sizeof(check.reason), "%s", reason ? reason : "Nessun dettaglio");

    return check;
}

RiskGuardCheck risk_guard_check(const BotState *state, const StrategySettings *settings) {
    double cost_basis;
    double market_value;
    double pnl;
    double drawdown_percent;
    char reason[256];

    if (state == NULL || settings == NULL) {
        return make_check(0, "BLOCKED", "Risk guard: stato o settings non disponibili", 0.0, 0.0, 0.0, 0.0);
    }

    if (settings->max_daily_loss_eur < 0.0 || settings->max_drawdown_percent < 0.0) {
        return make_check(0, "BLOCKED", "Risk guard: limiti rischio non validi", 0.0, 0.0, 0.0, 0.0);
    }

    if (state->btc_balance <= 0.0) {
        return make_check(1, "ALLOWED", "Risk guard: nessuna posizione BTC aperta", 0.0, 0.0, 0.0, 0.0);
    }

    if (state->current_price <= 0.0) {
        return make_check(0, "BLOCKED", "Risk guard: prezzo corrente non valido", 0.0, 0.0, 0.0, 0.0);
    }

    if (state->avg_buy_price <= 0.0) {
        return make_check(0, "BLOCKED", "Risk guard: costo medio non disponibile, impossibile calcolare drawdown", 0.0, 0.0, 0.0, 0.0);
    }

    cost_basis = state->btc_balance * state->avg_buy_price;
    market_value = state->btc_balance * state->current_price;
    pnl = market_value - cost_basis;
    drawdown_percent = 0.0;

    if (cost_basis > 0.0 && pnl < 0.0) {
        drawdown_percent = ((cost_basis - market_value) / cost_basis) * 100.0;
    }

    if (settings->max_daily_loss_eur > 0.0 && pnl < 0.0 && (cost_basis - market_value) >= settings->max_daily_loss_eur) {
        snprintf(
            reason,
            sizeof(reason),
            "Risk guard: perdita non realizzata %.2f EUR supera limite %.2f EUR",
            cost_basis - market_value,
            settings->max_daily_loss_eur
        );

        return make_check(0, "BLOCKED", reason, market_value, cost_basis, pnl, drawdown_percent);
    }

    if (settings->max_drawdown_percent > 0.0 && drawdown_percent >= settings->max_drawdown_percent) {
        snprintf(
            reason,
            sizeof(reason),
            "Risk guard: drawdown %.2f%% supera limite %.2f%%",
            drawdown_percent,
            settings->max_drawdown_percent
        );

        return make_check(0, "BLOCKED", reason, market_value, cost_basis, pnl, drawdown_percent);
    }

    snprintf(
        reason,
        sizeof(reason),
        "Risk guard OK: market %.2f EUR | cost %.2f EUR | P/L %.2f EUR | drawdown %.2f%%",
        market_value,
        cost_basis,
        pnl,
        drawdown_percent
    );

    return make_check(1, "ALLOWED", reason, market_value, cost_basis, pnl, drawdown_percent);
}

void risk_guard_audit_if_blocked(
    RiskGuardCheck check,
    double price,
    double btc_balance,
    double eur_balance
) {
    if (check.allowed) {
        return;
    }

    db_log_engine_audit(
        "RISK_GUARD",
        check.decision,
        check.reason,
        price,
        btc_balance,
        eur_balance,
        0.0,
        check.unrealized_pnl_eur
    );
}
