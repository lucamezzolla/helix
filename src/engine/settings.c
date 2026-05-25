#include "settings.h"
#include "../db/database.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double parse_double_setting(const char *value, double fallback) {
    char buffer[64];
    size_t i;

    if (value == NULL || value[0] == '\0') {
        return fallback;
    }

    snprintf(buffer, sizeof(buffer), "%s", value);

    for (i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == ',') {
            buffer[i] = '.';
        }
    }

    char *endptr = NULL;
    double parsed = strtod(buffer, &endptr);

    if (endptr == buffer) {
        return fallback;
    }

    return parsed;
}

#define KEY_SLOT_AMOUNT_EUR "strategy.slot_amount_eur"
#define KEY_BUY_DROP_PERCENT "strategy.buy_drop_percent"
#define KEY_SELL_PROFIT_PERCENT "strategy.sell_profit_percent"
#define KEY_ESTIMATED_FEE_PERCENT "strategy.estimated_fee_percent"
#define KEY_MIN_PROFIT_EUR "strategy.min_profit_eur"
#define KEY_MIN_PROFIT_PERCENT "strategy.min_profit_percent"
#define KEY_MIN_LIQUIDITY_PERCENT "strategy.min_liquidity_percent"
#define KEY_MAX_SLOTS "strategy.max_slots"
#define KEY_AUDIT_RETENTION_DAYS "audit.retention_days"
#define KEY_RUNTIME_MODE "runtime.mode"

static double get_double_setting(const char *key, double fallback) {
    char buffer[64];

    if (!db_get_setting(key, buffer, sizeof(buffer))) {
        return fallback;
    }

    return parse_double_setting(buffer, fallback);
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
    settings.estimated_fee_percent = 0.60;
    settings.min_profit_eur = 1.0;
    settings.min_profit_percent = 0.20;
    settings.min_liquidity_percent = 25.0;
    settings.max_slots = 6;
    settings.audit_retention_days = 60;
    settings.runtime_mode = RUNTIME_MODE_SIMULATION;

    return settings;
}

void settings_save(StrategySettings *settings) {
    set_double_setting(KEY_SLOT_AMOUNT_EUR, settings->slot_amount_eur);
    set_double_setting(KEY_BUY_DROP_PERCENT, settings->buy_drop_percent);
    set_double_setting(KEY_SELL_PROFIT_PERCENT, settings->sell_profit_percent);
    set_double_setting(KEY_ESTIMATED_FEE_PERCENT, settings->estimated_fee_percent);
    set_double_setting(KEY_MIN_PROFIT_EUR, settings->min_profit_eur);
    set_double_setting(KEY_MIN_PROFIT_PERCENT, settings->min_profit_percent);
    set_double_setting(KEY_MIN_LIQUIDITY_PERCENT, settings->min_liquidity_percent);
    set_int_setting(KEY_MAX_SLOTS, settings->max_slots);
    set_int_setting(KEY_AUDIT_RETENTION_DAYS, settings->audit_retention_days);
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

    if (!db_get_setting(KEY_ESTIMATED_FEE_PERCENT, buffer, sizeof(buffer))) {
        set_double_setting(KEY_ESTIMATED_FEE_PERCENT, defaults.estimated_fee_percent);
    }

    if (!db_get_setting(KEY_MIN_PROFIT_EUR, buffer, sizeof(buffer))) {
        set_double_setting(KEY_MIN_PROFIT_EUR, defaults.min_profit_eur);
    }

    if (!db_get_setting(KEY_MIN_PROFIT_PERCENT, buffer, sizeof(buffer))) {
        set_double_setting(KEY_MIN_PROFIT_PERCENT, defaults.min_profit_percent);
    }

    if (!db_get_setting(KEY_MIN_LIQUIDITY_PERCENT, buffer, sizeof(buffer))) {
        set_double_setting(KEY_MIN_LIQUIDITY_PERCENT, defaults.min_liquidity_percent);
    }

    if (!db_get_setting(KEY_MAX_SLOTS, buffer, sizeof(buffer))) {
        set_int_setting(KEY_MAX_SLOTS, defaults.max_slots);
    }

    if (!db_get_setting(KEY_AUDIT_RETENTION_DAYS, buffer, sizeof(buffer))) {
        set_int_setting(KEY_AUDIT_RETENTION_DAYS, defaults.audit_retention_days);
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

    settings.estimated_fee_percent =
        get_double_setting(KEY_ESTIMATED_FEE_PERCENT, defaults.estimated_fee_percent);

    settings.min_profit_eur =
        get_double_setting(KEY_MIN_PROFIT_EUR, defaults.min_profit_eur);

    settings.min_profit_percent =
        get_double_setting(KEY_MIN_PROFIT_PERCENT, defaults.min_profit_percent);

    settings.min_liquidity_percent =
        get_double_setting(KEY_MIN_LIQUIDITY_PERCENT, defaults.min_liquidity_percent);

    settings.max_slots =
        get_int_setting(KEY_MAX_SLOTS, defaults.max_slots);

    settings.audit_retention_days =
        get_int_setting(KEY_AUDIT_RETENTION_DAYS, defaults.audit_retention_days);

    if (settings.audit_retention_days < 1) {
        settings.audit_retention_days = defaults.audit_retention_days;
    }

    settings.runtime_mode =
        get_runtime_mode_setting(defaults.runtime_mode);

    return settings;
}
