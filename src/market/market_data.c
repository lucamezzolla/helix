#include "market_data.h"

#include "../engine/settings.h"
#include "../exchange/coinbase_client.h"
#include "../config/env_loader.h"

#include <math.h>
#include <stddef.h>

static int direction = -1;

int market_data_simulated_price_override_active(double *price_out) {
    double simulated_price_eur;

    if (!env_load_bool_flag("HELIX_SIMULATED_MARKET_PRICE_ENABLED", 0)) {
        return 0;
    }

    simulated_price_eur = env_load_double_value("HELIX_SIMULATED_MARKET_PRICE_EUR", 0.0);

    if (!isfinite(simulated_price_eur) || simulated_price_eur <= 0.0) {
        return 0;
    }

    if (price_out != NULL) {
        *price_out = simulated_price_eur;
    }

    return 1;
}

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
    double simulated_override_price = 0.0;

    if (
        settings.runtime_mode != RUNTIME_MODE_LIVE_TRADING &&
        market_data_simulated_price_override_active(&simulated_override_price)
    ) {
        return simulated_override_price;
    }

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
