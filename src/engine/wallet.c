#include "wallet.h"

int wallet_can_buy(BotState *state, double amount) {
    if (state->used_slots >= state->max_slots) {
        return 0;
    }

    if (state->eur_balance < amount) {
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
