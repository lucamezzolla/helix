#ifndef HELIX_MARKET_DATA_H
#define HELIX_MARKET_DATA_H

#include "../engine/bot_state.h"

double market_data_get_price(BotState *state);

int market_data_simulated_price_override_active(double *price_out);

#endif
