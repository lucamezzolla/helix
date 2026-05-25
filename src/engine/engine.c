#include "engine.h"
#include "wallet.h"
#include "settings.h"
#include "../db/database.h"
#include "../market/market_data.h"

#include <stdio.h>

static int price_dropped_enough(BotState *state, StrategySettings *settings) {
    if (state->last_buy_price <= 0.0) {
        return 1;
    }

    double trigger_price =
        state->last_buy_price * (1.0 - settings->buy_drop_percent / 100.0);

    return state->current_price <= trigger_price;
}

static int price_high_enough_to_sell(BotState *state, StrategySettings *settings) {
    if (state->avg_buy_price <= 0.0) {
        return 0;
    }

    double target_price =
        state->avg_buy_price * (1.0 + settings->sell_profit_percent / 100.0);

    return state->current_price >= target_price;
}

void helix_engine_tick(BotState *state) {
    StrategySettings settings = settings_load();

    if (!state->running) {
        state->mode = BOT_MODE_PAUSED;
        return;
    }

    state->max_slots = settings.max_slots;

    state->current_price = market_data_get_price(state);

    if (settings.runtime_mode == RUNTIME_MODE_LIVE_READONLY) {
        state->mode = BOT_MODE_READY;
        return;
    }

    if (settings.runtime_mode == RUNTIME_MODE_LIVE_TRADING) {
        state->mode = BOT_MODE_ERROR;

        snprintf(
            state->last_trade,
            sizeof(state->last_trade),
            "LIVE_TRADING bloccato: safety checks mancanti"
        );

        return;
    }

    state->mode = BOT_MODE_READY;

    if (
        wallet_can_sell(state) &&
        price_high_enough_to_sell(state, &settings)
    ) {
        double eur_before = state->eur_balance;
        double btc_before = state->btc_balance;
        double price = state->current_price;

        state->mode = BOT_MODE_SELLING;

        wallet_sell_all(state);

        snprintf(
            state->last_trade,
            sizeof(state->last_trade),
            "SELL @ %.2f",
            price
        );

        db_log_trade(
            "SELL",
            price,
            state->eur_balance - eur_before,
            btc_before
        );

        state->mode = BOT_MODE_READY;

        return;
    }

    if (
        wallet_can_buy(
            state,
            settings.slot_amount_eur,
            settings.min_liquidity_percent
        ) &&
        price_dropped_enough(state, &settings)
    ) {
        double price = state->current_price;

        state->mode = BOT_MODE_BUYING;

        wallet_buy(state, settings.slot_amount_eur);

        snprintf(
            state->last_trade,
            sizeof(state->last_trade),
            "BUY @ %.2f",
            price
        );

        db_log_trade(
            "BUY",
            price,
            settings.slot_amount_eur,
            settings.slot_amount_eur / price
        );

        state->mode = BOT_MODE_WAITING_SELL;

        return;
    }

    if (state->btc_balance > 0.0) {
        state->mode = BOT_MODE_WAITING_SELL;
    }
}
