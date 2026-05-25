#include "market_data.h"

#include "../engine/settings.h"
#include "../exchange/coinbase_client.h"

static int direction = -1;

static double simulated_price(BotState *state) {
    double price = state->current_price;

    if (price <= 0.0) {
        price = 60000.0;
    }

    price += (150.0 * direction);

    if (price <= 57000.0) {
        direction = 1;
    }

    if (price >= 63000.0) {
        direction = -1;
    }

    return price;
}

double market_data_get_price(BotState *state) {
    StrategySettings settings = settings_load();

    if (settings.runtime_mode == RUNTIME_MODE_SIMULATION) {
        return simulated_price(state);
    }

    /*
     * LIVE_READONLY:
     * legge il prezzo reale da Coinbase
     * senza fare operazioni.
     */
    if (settings.runtime_mode == RUNTIME_MODE_LIVE_READONLY) {
        double live_price =
            coinbase_get_btc_eur_spot_price();

        if (live_price > 0.0) {
            return live_price;
        }

        return state->current_price;
    }

    /*
     * LIVE_TRADING ancora bloccato.
     */
    return state->current_price;
}
