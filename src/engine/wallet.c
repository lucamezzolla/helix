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

    state->eur_balance -= eur_amount;
    state->btc_balance += btc_amount;
    state->used_slots++;
}
