#include "liquidity_reserve.h"

#include <math.h>
#include <stdio.h>

static double clamp_percent(double value) {
    if (!isfinite(value) || value < 0.0) {
        return 0.0;
    }

    if (value > 95.0) {
        return 95.0;
    }

    return value;
}

static int safe_floor_slots(double value) {
    if (!isfinite(value) || value <= 0.0) {
        return 0;
    }

    return (int)floor(value);
}

double liquidity_reserve_protected_eur(
    double eur_balance,
    double reserve_percent
) {
    double percent = clamp_percent(reserve_percent);

    if (!isfinite(eur_balance) || eur_balance <= 0.0) {
        return 0.0;
    }

    return eur_balance * (percent / 100.0);
}

double liquidity_reserve_released_eur(
    int released_slots,
    double slot_amount_eur
) {
    if (released_slots <= 0 || !isfinite(slot_amount_eur) || slot_amount_eur <= 0.0) {
        return 0.0;
    }

    return (double)released_slots * slot_amount_eur;
}

LiquidityReserveCheck liquidity_reserve_check_buy(
    const BotState *state,
    const StrategySettings *settings,
    double eur_amount
) {
    LiquidityReserveCheck check;
    double base_operational_eur;
    double protected_eur;
    double released_eur;
    double operational_eur;
    double eur_after_buy;
    int base_operational_slots;
    int total_available_slots;

    check.eur_balance = 0.0;
    check.slot_amount_eur = 0.0;
    check.reserve_percent = 0.0;
    check.protected_eur = 0.0;
    check.released_eur = 0.0;
    check.operational_eur = 0.0;
    check.eur_after_buy = 0.0;
    check.base_operational_slots = 0;
    check.released_slots = 0;
    check.total_available_slots = 0;
    check.can_buy = 0;
    snprintf(check.reason, sizeof(check.reason), "Liquidity reserve: controllo non inizializzato");

    if (state == NULL || settings == NULL) {
        snprintf(check.reason, sizeof(check.reason), "Liquidity reserve: stato o settings non disponibili");
        return check;
    }

    check.eur_balance = state->eur_balance;
    check.slot_amount_eur = settings->slot_amount_eur;
    check.reserve_percent = clamp_percent(settings->liquidity_reserve_percent);
    check.released_slots = settings->reserve_released_slots;

    if (!isfinite(eur_amount) || eur_amount <= 0.0) {
        snprintf(check.reason, sizeof(check.reason), "Liquidity reserve: importo BUY non valido");
        return check;
    }

    if (!isfinite(settings->slot_amount_eur) || settings->slot_amount_eur <= 0.0) {
        snprintf(check.reason, sizeof(check.reason), "Liquidity reserve: slot_amount_eur non valido");
        return check;
    }

    if (settings->reserve_released_slots < 0) {
        snprintf(check.reason, sizeof(check.reason), "Liquidity reserve: reserve_released_slots non valido");
        return check;
    }

    if (state->used_slots < 0 || state->used_slots > settings->max_slots) {
        snprintf(check.reason, sizeof(check.reason), "Liquidity reserve: used_slots incoerente");
        return check;
    }

    if (state->eur_balance < eur_amount) {
        snprintf(check.reason, sizeof(check.reason), "Liquidity reserve: saldo EUR insufficiente");
        return check;
    }

    protected_eur = liquidity_reserve_protected_eur(
        state->eur_balance,
        settings->liquidity_reserve_percent
    );

    released_eur = liquidity_reserve_released_eur(
        settings->reserve_released_slots,
        settings->slot_amount_eur
    );

    if (released_eur > protected_eur) {
        released_eur = protected_eur;
    }

    base_operational_eur = state->eur_balance - protected_eur;
    if (base_operational_eur < 0.0) {
        base_operational_eur = 0.0;
    }

    operational_eur = base_operational_eur + released_eur;
    if (operational_eur > state->eur_balance) {
        operational_eur = state->eur_balance;
    }

    eur_after_buy = state->eur_balance - eur_amount;

    base_operational_slots = safe_floor_slots(base_operational_eur / settings->slot_amount_eur);
    total_available_slots = base_operational_slots + settings->reserve_released_slots;

    if (total_available_slots > settings->max_slots) {
        total_available_slots = settings->max_slots;
    }

    check.protected_eur = protected_eur;
    check.released_eur = released_eur;
    check.operational_eur = operational_eur;
    check.eur_after_buy = eur_after_buy;
    check.base_operational_slots = base_operational_slots;
    check.total_available_slots = total_available_slots;

    if (state->used_slots >= total_available_slots) {
        snprintf(
            check.reason,
            sizeof(check.reason),
            "Liquidity reserve: slot operativi esauriti (%d/%d), sbloccare slot riserva manualmente",
            state->used_slots,
            total_available_slots
        );
        return check;
    }

    if (eur_amount > operational_eur) {
        snprintf(
            check.reason,
            sizeof(check.reason),
            "Liquidity reserve: BUY %.2f supera liquidità operativa %.2f, riserva protetta %.2f",
            eur_amount,
            operational_eur,
            protected_eur - released_eur
        );
        return check;
    }

    check.can_buy = 1;
    snprintf(
        check.reason,
        sizeof(check.reason),
        "Liquidity reserve: BUY consentito | operativa %.2f | protetta %.2f | rilasciata %.2f | slot %d/%d",
        operational_eur,
        protected_eur - released_eur,
        released_eur,
        state->used_slots,
        total_available_slots
    );

    return check;
}
