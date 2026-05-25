#include "runtime_safety.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static RuntimeSafetyCheck make_check(int allowed, const char *decision, const char *reason) {
    RuntimeSafetyCheck check;

    check.allowed = allowed;

    snprintf(check.decision, sizeof(check.decision), "%s", decision ? decision : "UNKNOWN");
    snprintf(check.reason, sizeof(check.reason), "%s", reason ? reason : "Nessun dettaglio");

    return check;
}

static int invalid_positive_double(double value) {
    return !isfinite(value) || value <= 0.0;
}

static RuntimeSafetyCheck check_common_state(
    const BotState *state,
    const StrategySettings *settings
) {
    if (state == NULL || settings == NULL) {
        return make_check(0, "BLOCKED", "Safety runtime: stato o settings non disponibili");
    }

    if (!state->running) {
        return make_check(0, "BLOCKED", "Safety runtime: bot non avviato");
    }

    if (invalid_positive_double(state->current_price)) {
        return make_check(0, "BLOCKED", "Safety runtime: prezzo corrente non valido");
    }

    if (settings->runtime_mode == RUNTIME_MODE_LIVE_TRADING) {
        return make_check(0, "BLOCKED", "Safety runtime: LIVE_TRADING ancora bloccato");
    }

    if (settings->max_slots <= 0) {
        return make_check(0, "BLOCKED", "Safety runtime: max_slots non valido");
    }

    if (state->used_slots < 0 || state->used_slots > settings->max_slots) {
        return make_check(0, "BLOCKED", "Safety runtime: used_slots incoerente");
    }

    return make_check(1, "ALLOWED", "Safety runtime: controlli comuni superati");
}

RuntimeSafetyCheck runtime_safety_check_buy(
    const BotState *state,
    const StrategySettings *settings,
    double eur_amount
) {
    RuntimeSafetyCheck common = check_common_state(state, settings);

    if (!common.allowed) {
        return common;
    }

    if (invalid_positive_double(eur_amount)) {
        return make_check(0, "BLOCKED", "Safety BUY: importo EUR non valido");
    }

    if (eur_amount > state->eur_balance) {
        return make_check(0, "BLOCKED", "Safety BUY: saldo EUR insufficiente");
    }

    if (state->used_slots >= settings->max_slots) {
        return make_check(0, "BLOCKED", "Safety BUY: max_slots raggiunto");
    }

    if (settings->min_liquidity_percent < 0.0 || settings->min_liquidity_percent >= 100.0) {
        return make_check(0, "BLOCKED", "Safety BUY: min_liquidity_percent non valida");
    }

    return make_check(1, "ALLOWED", "Safety BUY: autorizzato per preview/simulazione");
}

RuntimeSafetyCheck runtime_safety_check_sell(
    const BotState *state,
    const StrategySettings *settings,
    double btc_amount
) {
    RuntimeSafetyCheck common = check_common_state(state, settings);

    if (!common.allowed) {
        return common;
    }

    if (invalid_positive_double(btc_amount)) {
        return make_check(0, "BLOCKED", "Safety SELL: importo BTC non valido");
    }

    if (btc_amount > state->btc_balance) {
        return make_check(0, "BLOCKED", "Safety SELL: saldo BTC insufficiente");
    }

    if (state->avg_buy_price <= 0.0) {
        return make_check(0, "BLOCKED", "Safety SELL: prezzo medio/costo posizione non disponibile");
    }

    if (settings->min_profit_eur < 0.0 || settings->min_profit_percent < 0.0) {
        return make_check(0, "BLOCKED", "Safety SELL: margine minimo non valido");
    }

    return make_check(1, "ALLOWED", "Safety SELL: autorizzato per preview/simulazione");
}

RuntimeSafetyCheck runtime_safety_check_live_trading_arm(
    const StrategySettings *settings
) {
    (void)settings;

    return make_check(
        0,
        "BLOCKED",
        "Safety arm: esecuzione ordini reali non implementata e non autorizzata"
    );
}
