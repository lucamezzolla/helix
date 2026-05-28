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
#define KEY_LIQUIDITY_RESERVE_PERCENT "strategy.liquidity_reserve_percent"
#define KEY_MAX_SLOTS "strategy.max_slots"
#define KEY_RESERVE_RELEASED_SLOTS "strategy.reserve_released_slots"
#define KEY_AUDIT_RETENTION_DAYS "audit.retention_days"
#define KEY_EMERGENCY_STOP_ENABLED "safety.emergency_stop_enabled"
#define KEY_LIVE_TRADING_ARMED "safety.live_trading_armed"
#define KEY_VOLATILITY_WINDOW_SECONDS "safety.volatility_window_seconds"
#define KEY_VOLATILITY_MAX_MOVE_PERCENT "safety.volatility_max_move_percent"
#define KEY_MAX_ORDERS_PER_DAY "safety.max_orders_per_day"
#define KEY_ORDER_COOLDOWN_SECONDS "safety.order_cooldown_seconds"
#define KEY_MAX_DAILY_LOSS_EUR "safety.max_daily_loss_eur"
#define KEY_MAX_DRAWDOWN_PERCENT "safety.max_drawdown_percent"
#define KEY_MICRO_LIVE_ENABLED "micro_live.enabled"
#define KEY_MICRO_LIVE_MAX_ORDER_EUR "micro_live.max_order_eur"
#define KEY_MICRO_LIVE_STOP_AFTER_REAL_ORDER "micro_live.stop_after_real_order"
#define KEY_MICRO_LIVE_ALLOW_ACCUMULATION "micro_live.allow_accumulation"
#define KEY_MICRO_LIVE_LAST_REAL_ORDER_ACKNOWLEDGED "micro_live.last_real_order_acknowledged"
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

static void get_string_setting(const char *key, char *buffer, int buffer_size, const char *fallback) {
    if (buffer == NULL || buffer_size <= 0) {
        return;
    }

    if (!db_get_setting(key, buffer, buffer_size)) {
        snprintf(buffer, (size_t)buffer_size, "%s", fallback ? fallback : "");
    }
}

static void set_string_setting(const char *key, const char *value) {
    db_set_setting(key, value ? value : "");
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
    settings.liquidity_reserve_percent = 40.0;
    settings.max_slots = 6;
    settings.reserve_released_slots = 0;
    settings.audit_retention_days = 60;
    settings.emergency_stop_enabled = 0;
    settings.live_trading_armed = 0;
    settings.volatility_window_seconds = 60;
    settings.volatility_max_move_percent = 3.0;
    settings.max_orders_per_day = 8;
    settings.order_cooldown_seconds = 300;
    settings.max_daily_loss_eur = 25.0;
    settings.max_drawdown_percent = 8.0;
    settings.micro_live_enabled = 0;
    settings.micro_live_max_order_eur = 20.0;
    settings.micro_live_stop_after_real_order = 1;
    settings.micro_live_allow_accumulation = 0;
    settings.micro_live_last_real_order_acknowledged[0] = '\0';
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
    set_double_setting(KEY_LIQUIDITY_RESERVE_PERCENT, settings->liquidity_reserve_percent);
    set_int_setting(KEY_MAX_SLOTS, settings->max_slots);
    set_int_setting(KEY_RESERVE_RELEASED_SLOTS, settings->reserve_released_slots);
    set_int_setting(KEY_AUDIT_RETENTION_DAYS, settings->audit_retention_days);
    set_int_setting(KEY_EMERGENCY_STOP_ENABLED, settings->emergency_stop_enabled ? 1 : 0);
    set_int_setting(KEY_LIVE_TRADING_ARMED, settings->live_trading_armed ? 1 : 0);
    set_int_setting(KEY_VOLATILITY_WINDOW_SECONDS, settings->volatility_window_seconds);
    set_double_setting(KEY_VOLATILITY_MAX_MOVE_PERCENT, settings->volatility_max_move_percent);
    set_int_setting(KEY_MAX_ORDERS_PER_DAY, settings->max_orders_per_day);
    set_int_setting(KEY_ORDER_COOLDOWN_SECONDS, settings->order_cooldown_seconds);
    set_double_setting(KEY_MAX_DAILY_LOSS_EUR, settings->max_daily_loss_eur);
    set_double_setting(KEY_MAX_DRAWDOWN_PERCENT, settings->max_drawdown_percent);
    set_int_setting(KEY_MICRO_LIVE_ENABLED, settings->micro_live_enabled ? 1 : 0);
    set_double_setting(KEY_MICRO_LIVE_MAX_ORDER_EUR, settings->micro_live_max_order_eur);
    set_int_setting(KEY_MICRO_LIVE_STOP_AFTER_REAL_ORDER, settings->micro_live_stop_after_real_order ? 1 : 0);
    set_int_setting(KEY_MICRO_LIVE_ALLOW_ACCUMULATION, settings->micro_live_allow_accumulation ? 1 : 0);
    set_string_setting(KEY_MICRO_LIVE_LAST_REAL_ORDER_ACKNOWLEDGED, settings->micro_live_last_real_order_acknowledged);
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

    if (!db_get_setting(KEY_LIQUIDITY_RESERVE_PERCENT, buffer, sizeof(buffer))) {
        set_double_setting(KEY_LIQUIDITY_RESERVE_PERCENT, defaults.liquidity_reserve_percent);
    }

    if (!db_get_setting(KEY_MAX_SLOTS, buffer, sizeof(buffer))) {
        set_int_setting(KEY_MAX_SLOTS, defaults.max_slots);
    }

    if (!db_get_setting(KEY_RESERVE_RELEASED_SLOTS, buffer, sizeof(buffer))) {
        set_int_setting(KEY_RESERVE_RELEASED_SLOTS, defaults.reserve_released_slots);
    }

    if (!db_get_setting(KEY_AUDIT_RETENTION_DAYS, buffer, sizeof(buffer))) {
        set_int_setting(KEY_AUDIT_RETENTION_DAYS, defaults.audit_retention_days);
    }

    if (!db_get_setting(KEY_EMERGENCY_STOP_ENABLED, buffer, sizeof(buffer))) {
        set_int_setting(KEY_EMERGENCY_STOP_ENABLED, defaults.emergency_stop_enabled);
    }

    if (!db_get_setting(KEY_LIVE_TRADING_ARMED, buffer, sizeof(buffer))) {
        set_int_setting(KEY_LIVE_TRADING_ARMED, defaults.live_trading_armed);
    }

    if (!db_get_setting(KEY_VOLATILITY_WINDOW_SECONDS, buffer, sizeof(buffer))) {
        set_int_setting(KEY_VOLATILITY_WINDOW_SECONDS, defaults.volatility_window_seconds);
    }

    if (!db_get_setting(KEY_VOLATILITY_MAX_MOVE_PERCENT, buffer, sizeof(buffer))) {
        set_double_setting(KEY_VOLATILITY_MAX_MOVE_PERCENT, defaults.volatility_max_move_percent);
    }

    if (!db_get_setting(KEY_MAX_ORDERS_PER_DAY, buffer, sizeof(buffer))) {
        set_int_setting(KEY_MAX_ORDERS_PER_DAY, defaults.max_orders_per_day);
    }

    if (!db_get_setting(KEY_ORDER_COOLDOWN_SECONDS, buffer, sizeof(buffer))) {
        set_int_setting(KEY_ORDER_COOLDOWN_SECONDS, defaults.order_cooldown_seconds);
    }

    if (!db_get_setting(KEY_MAX_DAILY_LOSS_EUR, buffer, sizeof(buffer))) {
        set_double_setting(KEY_MAX_DAILY_LOSS_EUR, defaults.max_daily_loss_eur);
    }

    if (!db_get_setting(KEY_MAX_DRAWDOWN_PERCENT, buffer, sizeof(buffer))) {
        set_double_setting(KEY_MAX_DRAWDOWN_PERCENT, defaults.max_drawdown_percent);
    }

    if (!db_get_setting(KEY_MICRO_LIVE_ENABLED, buffer, sizeof(buffer))) {
        set_int_setting(KEY_MICRO_LIVE_ENABLED, defaults.micro_live_enabled);
    }

    if (!db_get_setting(KEY_MICRO_LIVE_MAX_ORDER_EUR, buffer, sizeof(buffer))) {
        set_double_setting(KEY_MICRO_LIVE_MAX_ORDER_EUR, defaults.micro_live_max_order_eur);
    }

    if (!db_get_setting(KEY_MICRO_LIVE_STOP_AFTER_REAL_ORDER, buffer, sizeof(buffer))) {
        set_int_setting(KEY_MICRO_LIVE_STOP_AFTER_REAL_ORDER, defaults.micro_live_stop_after_real_order);
    }

    if (!db_get_setting(KEY_MICRO_LIVE_ALLOW_ACCUMULATION, buffer, sizeof(buffer))) {
        set_int_setting(KEY_MICRO_LIVE_ALLOW_ACCUMULATION, defaults.micro_live_allow_accumulation);
    }

    if (!db_get_setting(KEY_MICRO_LIVE_LAST_REAL_ORDER_ACKNOWLEDGED, buffer, sizeof(buffer))) {
        set_string_setting(KEY_MICRO_LIVE_LAST_REAL_ORDER_ACKNOWLEDGED, defaults.micro_live_last_real_order_acknowledged);
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

    settings.liquidity_reserve_percent =
        get_double_setting(KEY_LIQUIDITY_RESERVE_PERCENT, defaults.liquidity_reserve_percent);

    settings.max_slots =
        get_int_setting(KEY_MAX_SLOTS, defaults.max_slots);

    settings.reserve_released_slots =
        get_int_setting(KEY_RESERVE_RELEASED_SLOTS, defaults.reserve_released_slots);

    if (settings.reserve_released_slots < 0) {
        settings.reserve_released_slots = 0;
    }

    settings.audit_retention_days =
        get_int_setting(KEY_AUDIT_RETENTION_DAYS, defaults.audit_retention_days);

    if (settings.liquidity_reserve_percent < 0.0) {
        settings.liquidity_reserve_percent = defaults.liquidity_reserve_percent;
    }

    if (settings.liquidity_reserve_percent > 95.0) {
        settings.liquidity_reserve_percent = 95.0;
    }

    if (settings.reserve_released_slots > settings.max_slots) {
        settings.reserve_released_slots = settings.max_slots;
    }

    if (settings.audit_retention_days < 1) {
        settings.audit_retention_days = defaults.audit_retention_days;
    }

    settings.emergency_stop_enabled =
        get_int_setting(KEY_EMERGENCY_STOP_ENABLED, defaults.emergency_stop_enabled) ? 1 : 0;

    settings.live_trading_armed =
        get_int_setting(KEY_LIVE_TRADING_ARMED, defaults.live_trading_armed) ? 1 : 0;

    settings.volatility_window_seconds =
        get_int_setting(KEY_VOLATILITY_WINDOW_SECONDS, defaults.volatility_window_seconds);

    settings.volatility_max_move_percent =
    get_double_setting(KEY_VOLATILITY_MAX_MOVE_PERCENT, defaults.volatility_max_move_percent);

    if (settings.volatility_window_seconds < 10) {
        settings.volatility_window_seconds = defaults.volatility_window_seconds;
    }

    if (settings.volatility_max_move_percent <= 0.0) {
        settings.volatility_max_move_percent = defaults.volatility_max_move_percent;
    }

    settings.max_orders_per_day =
        get_int_setting(KEY_MAX_ORDERS_PER_DAY, defaults.max_orders_per_day);

    settings.order_cooldown_seconds =
        get_int_setting(KEY_ORDER_COOLDOWN_SECONDS, defaults.order_cooldown_seconds);

    settings.max_daily_loss_eur =
        get_double_setting(KEY_MAX_DAILY_LOSS_EUR, defaults.max_daily_loss_eur);

    settings.max_drawdown_percent =
        get_double_setting(KEY_MAX_DRAWDOWN_PERCENT, defaults.max_drawdown_percent);

    if (settings.max_orders_per_day <= 0) {
        settings.max_orders_per_day = defaults.max_orders_per_day;
    }

    if (settings.order_cooldown_seconds < 0) {
        settings.order_cooldown_seconds = defaults.order_cooldown_seconds;
    }

    if (settings.max_daily_loss_eur < 0.0) {
        settings.max_daily_loss_eur = defaults.max_daily_loss_eur;
    }

    if (settings.max_drawdown_percent < 0.0) {
        settings.max_drawdown_percent = defaults.max_drawdown_percent;
    }

    settings.micro_live_enabled =
        get_int_setting(KEY_MICRO_LIVE_ENABLED, defaults.micro_live_enabled) ? 1 : 0;

    settings.micro_live_max_order_eur =
        get_double_setting(KEY_MICRO_LIVE_MAX_ORDER_EUR, defaults.micro_live_max_order_eur);

    settings.micro_live_stop_after_real_order =
        get_int_setting(KEY_MICRO_LIVE_STOP_AFTER_REAL_ORDER, defaults.micro_live_stop_after_real_order) ? 1 : 0;

    settings.micro_live_allow_accumulation =
        get_int_setting(KEY_MICRO_LIVE_ALLOW_ACCUMULATION, defaults.micro_live_allow_accumulation) ? 1 : 0;

    get_string_setting(
        KEY_MICRO_LIVE_LAST_REAL_ORDER_ACKNOWLEDGED,
        settings.micro_live_last_real_order_acknowledged,
        sizeof(settings.micro_live_last_real_order_acknowledged),
        defaults.micro_live_last_real_order_acknowledged
    );

    if (settings.micro_live_max_order_eur <= 0.0 || settings.micro_live_max_order_eur > 50.0) {
        settings.micro_live_max_order_eur = defaults.micro_live_max_order_eur;
    }

    settings.runtime_mode =
        get_runtime_mode_setting(defaults.runtime_mode);

    return settings;
}
