#include "runtime_safety.h"
#include "liquidity_reserve.h"
#include "emergency_stop.h"

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

static double wallet_total_value_eur(const BotState *state) {
    double btc_value = 0.0;

    if (state == NULL) {
        return 0.0;
    }

    if (isfinite(state->current_price) && state->current_price > 0.0) {
        btc_value = state->btc_balance * state->current_price;
    }

    if (!isfinite(btc_value) || btc_value < 0.0) {
        btc_value = 0.0;
    }

    return state->eur_balance + btc_value;
}

static RuntimeSafetyCheck check_min_liquidity_after_buy(
    const BotState *state,
    const StrategySettings *settings,
    double eur_amount
) {
    double total_value;
    double eur_after_buy;
    double min_eur_required;

    if (settings->min_liquidity_percent < 0.0 || settings->min_liquidity_percent >= 100.0) {
        return make_check(0, "BLOCKED", "Safety BUY: min_liquidity_percent non valida");
    }

    total_value = wallet_total_value_eur(state);
    if (!isfinite(total_value) || total_value <= 0.0) {
        return make_check(0, "BLOCKED", "Safety BUY: valore wallet non valido per liquidità minima");
    }

    eur_after_buy = state->eur_balance - eur_amount;
    min_eur_required = total_value * (settings->min_liquidity_percent / 100.0);

    if (eur_after_buy < min_eur_required) {
        char reason[160];

        snprintf(
            reason,
            sizeof(reason),
            "Safety BUY: liquidità minima non rispettata | dopo BUY %.2f EUR | richiesta %.2f EUR",
            eur_after_buy,
            min_eur_required
        );

        return make_check(0, "BLOCKED", reason);
    }

    return make_check(1, "ALLOWED", "Safety BUY: liquidità minima rispettata");
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

    if (emergency_stop_is_active(settings)) {
        return make_check(0, "BLOCKED", "Safety runtime: emergency stop attivo");
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

    RuntimeSafetyCheck min_liquidity = check_min_liquidity_after_buy(
        state,
        settings,
        eur_amount
    );

    if (!min_liquidity.allowed) {
        return min_liquidity;
    }

    if (settings->liquidity_reserve_percent < 0.0 || settings->liquidity_reserve_percent > 95.0) {
        return make_check(0, "BLOCKED", "Safety BUY: liquidity_reserve_percent non valida");
    }

    LiquidityReserveCheck reserve = liquidity_reserve_check_buy(
        state,
        settings,
        eur_amount
    );

    if (!reserve.can_buy) {
        return make_check(0, "BLOCKED", reserve.reason);
    }

    return make_check(1, "ALLOWED", reserve.reason);
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
    if (settings == NULL) {
        return make_check(
            0,
            "BLOCKED",
            "Safety arm: settings non disponibili"
        );
    }

    if (emergency_stop_is_active(settings)) {
        return make_check(
            0,
            "BLOCKED",
            "Safety arm: emergency stop attivo"
        );
    }

    if (!settings->live_trading_armed) {
        return make_check(
            0,
            "BLOCKED",
            "Safety arm: LIVE_TRADING non armato manualmente dall\'utente"
        );
    }

    return make_check(
        0,
        "BLOCKED",
        "Safety arm: LIVE_TRADING armato manualmente, ma invio ordini reali ancora non implementato"
    );
}
