#include "engine.h"
#include "wallet.h"

#define SLOT_AMOUNT_EUR 100.0
#define BUY_DROP_PERCENT 2.0
#define SELL_PROFIT_PERCENT 1.5

static int price_dropped_enough(BotState *state) {
    if (state->last_buy_price <= 0.0) {
        return 1;
    }

    double trigger_price = state->last_buy_price * (1.0 - BUY_DROP_PERCENT / 100.0);

    return state->current_price <= trigger_price;
}

static int price_high_enough_to_sell(BotState *state) {
    if (state->avg_buy_price <= 0.0) {
        return 0;
    }

    double target_price = state->avg_buy_price * (1.0 + SELL_PROFIT_PERCENT / 100.0);

    return state->current_price >= target_price;
}

void helix_engine_tick(BotState *state) {
    if (!state->running) {
        state->mode = BOT_MODE_PAUSED;
        return;
    }

    /*
     * Prezzo simulato.
     * Per ora scende, così testiamo gli acquisti progressivi.
     */
    if (state->current_price <= 0.0) {
        state->current_price = 60000.0;
    } else {
        state->current_price -= 50.0;
    }

    state->mode = BOT_MODE_READY;

    if (
        wallet_can_sell(state) &&
        price_high_enough_to_sell(state)
    ) {
        state->mode = BOT_MODE_SELLING;
        wallet_sell_all(state);
        state->mode = BOT_MODE_READY;
        return;
    }

    if (
        wallet_can_buy(state, SLOT_AMOUNT_EUR) &&
        price_dropped_enough(state)
    ) {
        state->mode = BOT_MODE_BUYING;
        wallet_buy(state, SLOT_AMOUNT_EUR);
        state->mode = BOT_MODE_WAITING_SELL;
        return;
    }

    if (state->btc_balance > 0.0) {
        state->mode = BOT_MODE_WAITING_SELL;
    }
}
