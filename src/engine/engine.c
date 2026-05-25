#include "engine.h"
#include "wallet.h"

void helix_engine_tick(BotState *state) {
    if (!state->running) {
        state->mode = BOT_MODE_PAUSED;
        return;
    }

    if (state->current_price <= 0.0) {
        state->current_price = 60000.0;
    } else {
        state->current_price -= 50.0;
    }

    state->mode = BOT_MODE_READY;

    if (wallet_can_buy(state, 100.0)) {
        state->mode = BOT_MODE_BUYING;
        wallet_buy(state, 100.0);
        state->mode = BOT_MODE_WAITING_SELL;
    }
}
