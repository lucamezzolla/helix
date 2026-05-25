#ifndef HELIX_WALLET_H
#define HELIX_WALLET_H

#include "bot_state.h"

int wallet_can_buy(
    BotState *state,
    double amount,
    double min_liquidity_percent
);

void wallet_buy(BotState *state, double eur_amount);

int wallet_can_sell(BotState *state);
void wallet_sell_all(BotState *state);

#endif
