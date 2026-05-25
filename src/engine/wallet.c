#include "wallet.h"

static double wallet_total_value_eur(BotState *state) {
    double btc_value = 0.0;

    if (state->current_price > 0.0) {
        btc_value = state->btc_balance * state->current_price;
    }

    return state->eur_balance + btc_value;
}

static int wallet_respects_min_liquidity(
    BotState *state,
    double amount,
    double min_liquidity_percent
) {
    double total_value = wallet_total_value_eur(state);

    if (total_value <= 0.0) {
        return 0;
    }

    double eur_after_buy = state->eur_balance - amount;
    double min_eur_required = total_value * (min_liquidity_percent / 100.0);

    return eur_after_buy >= min_eur_required;
}

int wallet_can_buy(
    BotState *state,
    double amount,
    double min_liquidity_percent
) {
    if (state->used_slots >= state->max_slots) {
        return 0;
    }

    if (state->eur_balance < amount) {
        return 0;
    }

    if (!wallet_respects_min_liquidity(state, amount, min_liquidity_percent)) {
        return 0;
    }

    return 1;
}

void wallet_buy(BotState *state, double eur_amount) {
    double btc_amount;

    if (state->current_price <= 0.0) {
        return;
    }

    btc_amount = eur_amount / state->current_price;

    double old_value = state->btc_balance * state->avg_buy_price;
    double new_value = eur_amount;
    double new_btc_total = state->btc_balance + btc_amount;

    state->eur_balance -= eur_amount;
    state->btc_balance = new_btc_total;
    state->used_slots++;
    state->last_buy_price = state->current_price;

    if (new_btc_total > 0.0) {
        state->avg_buy_price = (old_value + new_value) / new_btc_total;
    }
}

int wallet_can_sell(BotState *state) {
    return state->btc_balance > 0.0;
}

void wallet_sell_all(BotState *state) {
    if (state->current_price <= 0.0 || state->btc_balance <= 0.0) {
        return;
    }

    state->eur_balance += state->btc_balance * state->current_price;
    state->btc_balance = 0.0;
    state->used_slots = 0;
    state->last_buy_price = 0.0;
    state->avg_buy_price = 0.0;
}
