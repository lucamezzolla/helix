#ifndef HELIX_WALLET_H
#define HELIX_WALLET_H

#include "bot_state.h"

int wallet_can_buy(BotState *state, double amount);
void wallet_buy(BotState *state, double eur_amount);

#endif
