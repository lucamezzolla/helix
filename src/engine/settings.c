#include "settings.h"
#include "../db/database.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_SLOT_AMOUNT_EUR "strategy.slot_amount_eur"
#define KEY_BUY_DROP_PERCENT "strategy.buy_drop_percent"
#define KEY_SELL_PROFIT_PERCENT "strategy.sell_profit_percent"
#define KEY_MIN_LIQUIDITY_PERCENT "strategy.min_liquidity_percent"
#define KEY_MAX_SLOTS "strategy.max_slots"
#define KEY_RUNTIME_MODE "runtime.mode"

static double get_double_setting(const char *key, double fallback) {
    char buffer[64];

    if (!db_get_setting(key, buffer, sizeof(buffer))) {
        return fallback;
    }

    return atof(buffer);
}

static int get_int_setting(const char *key, int fallback) {
    char buffer[64];

    if (!db_get_setting(key, buffer, sizeof(buffer))) {
        return fallback;
    }

    return atoi(buffer);
}

static void set_double_setting(const char *key, double value) {
    char buffer[64];

    snprintf(buffer, sizeof(buffer), "%.8f", value);
    db_set_setting(key, buffer);
}

static void set_int_setting(const char *key, int value) {
    char buffer[64];

    snprintf(buffer, sizeof(buffer), "%d", value);
    db_set_setting(key, buffer);
}

const char *runtime_mode_to_string(RuntimeMode mode) {
    switch (mode) {
        case RUNTIME_MODE_SIMULATION:
            return "SIMULATION";
        case RUNTIME_MODE_LIVE_READONLY:
            return "LIVE_READONLY";
        case RUNTIME_MODE_LIVE_TRADING:
            return "LIVE_TRADING";
        default:
            return "SIMULATION";
    }
}

RuntimeMode runtime_mode_from_string(const char *value) {
    if (value == NULL) {
        return RUNTIME_MODE_SIMULATION;
    }

    if (strcmp(value, "LIVE_READONLY") == 0) {
        return RUNTIME_MODE_LIVE_READONLY;
    }

    if (strcmp(value, "LIVE_TRADING") == 0) {
        return RUNTIME_MODE_LIVE_TRADING;
    }

    return RUNTIME_MODE_SIMULATION;
}

static RuntimeMode get_runtime_mode_setting(RuntimeMode fallback) {
    char buffer[64];

    if (!db_get_setting(KEY_RUNTIME_MODE, buffer, sizeof(buffer))) {
        return fallback;
    }

    return runtime_mode_from_string(buffer);
}

static void set_runtime_mode_setting(RuntimeMode mode) {
    db_set_setting(KEY_RUNTIME_MODE, runtime_mode_to_string(mode));
}

StrategySettings settings_default(void) {
    StrategySettings settings;

    settings.slot_amount_eur = 100.0;
    settings.buy_drop_percent = 2.0;
    settings.sell_profit_percent = 1.5;
    settings.min_liquidity_percent = 25.0;
    settings.max_slots = 6;
    settings.runtime_mode = RUNTIME_MODE_SIMULATION;

    return settings;
}

void settings_save(StrategySettings *settings) {
    set_double_setting(KEY_SLOT_AMOUNT_EUR, settings->slot_amount_eur);
    set_double_setting(KEY_BUY_DROP_PERCENT, settings->buy_drop_percent);
    set_double_setting(KEY_SELL_PROFIT_PERCENT, settings->sell_profit_percent);
    set_double_setting(KEY_MIN_LIQUIDITY_PERCENT, settings->min_liquidity_percent);
    set_int_setting(KEY_MAX_SLOTS, settings->max_slots);
    set_runtime_mode_setting(settings->runtime_mode);
}

void settings_save_defaults_if_missing(void) {
    StrategySettings defaults = settings_default();
    char buffer[64];

    if (!db_get_setting(KEY_SLOT_AMOUNT_EUR, buffer, sizeof(buffer))) {
        set_double_setting(KEY_SLOT_AMOUNT_EUR, defaults.slot_amount_eur);
    }

    if (!db_get_setting(KEY_BUY_DROP_PERCENT, buffer, sizeof(buffer))) {
        set_double_setting(KEY_BUY_DROP_PERCENT, defaults.buy_drop_percent);
    }

    if (!db_get_setting(KEY_SELL_PROFIT_PERCENT, buffer, sizeof(buffer))) {
        set_double_setting(KEY_SELL_PROFIT_PERCENT, defaults.sell_profit_percent);
    }

    if (!db_get_setting(KEY_MIN_LIQUIDITY_PERCENT, buffer, sizeof(buffer))) {
        set_double_setting(KEY_MIN_LIQUIDITY_PERCENT, defaults.min_liquidity_percent);
    }

    if (!db_get_setting(KEY_MAX_SLOTS, buffer, sizeof(buffer))) {
        set_int_setting(KEY_MAX_SLOTS, defaults.max_slots);
    }

    if (!db_get_setting(KEY_RUNTIME_MODE, buffer, sizeof(buffer))) {
        set_runtime_mode_setting(defaults.runtime_mode);
    }
}

StrategySettings settings_load(void) {
    StrategySettings defaults = settings_default();
    StrategySettings settings;

    settings.slot_amount_eur =
        get_double_setting(KEY_SLOT_AMOUNT_EUR, defaults.slot_amount_eur);

    settings.buy_drop_percent =
        get_double_setting(KEY_BUY_DROP_PERCENT, defaults.buy_drop_percent);

    settings.sell_profit_percent =
        get_double_setting(KEY_SELL_PROFIT_PERCENT, defaults.sell_profit_percent);

    settings.min_liquidity_percent =
        get_double_setting(KEY_MIN_LIQUIDITY_PERCENT, defaults.min_liquidity_percent);

    settings.max_slots =
        get_int_setting(KEY_MAX_SLOTS, defaults.max_slots);

    settings.runtime_mode =
        get_runtime_mode_setting(defaults.runtime_mode);

    return settings;
}
