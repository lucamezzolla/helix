#ifndef HELIX_SETTINGS_H
#define HELIX_SETTINGS_H

typedef enum {
    RUNTIME_MODE_SIMULATION,
    RUNTIME_MODE_LIVE_READONLY,
    RUNTIME_MODE_LIVE_TRADING
} RuntimeMode;

typedef struct {
    double slot_amount_eur;
    double buy_drop_percent;
    double sell_profit_percent;
    double estimated_fee_percent;
    double min_profit_eur;
    double min_profit_percent;
    double min_liquidity_percent;
    double liquidity_reserve_percent;
    int max_slots;
    int reserve_released_slots;
    int audit_retention_days;
    int emergency_stop_enabled;
    int live_trading_armed;
    int volatility_window_seconds;
    double volatility_max_move_percent;
    int max_orders_per_day;
    int order_cooldown_seconds;
    double max_daily_loss_eur;
    double max_drawdown_percent;
    RuntimeMode runtime_mode;
} StrategySettings;

StrategySettings settings_default(void);
StrategySettings settings_load(void);
void settings_save(StrategySettings *settings);
void settings_save_defaults_if_missing(void);

const char *runtime_mode_to_string(RuntimeMode mode);
RuntimeMode runtime_mode_from_string(const char *value);

#endif
