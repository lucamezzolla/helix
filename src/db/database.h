#ifndef HELIX_DATABASE_H
#define HELIX_DATABASE_H

#include "../engine/bot_state.h"

typedef struct {
    int id;
    char type[16];
    double price;
    double eur_amount;
    double btc_amount;
    char created_at[32];
} TradeRecord;



typedef struct {
    int id;
    char client_order_id[96];
    char side[8];
    char status[32];
    char product_id[32];
    int dry_run;
    double requested_quote_size;
    double requested_base_size;
    double preview_total_eur;
    double preview_fee_eur;
    double preview_base_size;
    double preview_avg_price;
    char reason[256];
    char created_at[32];
    char updated_at[32];
} OrderStateRecord;

typedef struct {
    int id;
    char event_type[32];
    char decision[32];
    char reason[256];
    double price;
    double btc_amount;
    double eur_amount;
    double estimated_fee;
    double net_profit;
    char created_at[32];
} EngineAuditRecord;





typedef struct {
    int id;
    char label[96];
    double base_size_btc;
    double cost_eur;
    double buy_fee_eur;
    double avg_buy_price;
    char status[16];
    char opened_at[32];
    char closed_at[32];
    double sell_net_eur;
    double realized_profit_eur;
} PaperPositionSlotRecord;

typedef struct {
    int id;
    char buy_order_id[128];
    char buy_client_order_id[96];
    double base_size_btc;
    double cost_eur;
    double buy_fee_eur;
    double avg_buy_price;
    char status[16];
    char opened_at[32];
    char closed_at[32];
    char sell_order_id[128];
    double sell_net_eur;
    double realized_profit_eur;
} PositionSlotRecord;

int db_create_position_slot_from_buy(
    const char *buy_order_id,
    const char *buy_client_order_id,
    double base_size_btc,
    double cost_eur,
    double buy_fee_eur,
    double avg_buy_price
);

int db_rebuild_position_slots_from_real_buys(void);

int db_get_open_position_slots(
    PositionSlotRecord *records,
    int max_records
);

int db_close_position_slot(
    int slot_id,
    const char *sell_order_id,
    double sell_net_eur,
    double realized_profit_eur
);

int db_find_open_position_slot_by_base_size(
    double base_size_btc,
    PositionSlotRecord *record
);


int db_create_paper_position_slot(
    const char *label,
    double base_size_btc,
    double cost_eur,
    double buy_fee_eur,
    double avg_buy_price
);

int db_get_open_paper_position_slots(
    PaperPositionSlotRecord *records,
    int max_records
);

int db_close_paper_position_slot(
    int slot_id,
    double sell_net_eur,
    double realized_profit_eur
);

int db_clear_paper_position_slots(void);

int db_seed_demo_paper_position_slots(void);

typedef struct {
    int total_last_days;
    int reconciliation_blocks;
    int order_recovery_blocks;
    int volatility_blocks;
    int operational_limits_blocks;
    int risk_guard_blocks;
    int final_live_gate_blocks;
    int anti_duplicate_blocks;
    int prelive_validation_blocks;
    int real_executor_blocks;
    int post_order_reconciliation_blocks;
    int emergency_stop_events;
    int live_trading_arm_events;
} EngineAuditSummary;

int db_init(void);

int db_save_state(const BotState *state);
int db_load_state(BotState *state);

int db_log_trade(
    const char *type,
    double price,
    double eur_amount,
    double btc_amount
);

int db_get_recent_trades(
    TradeRecord *trades,
    int max_trades
);

int db_log_engine_audit(
    const char *event_type,
    const char *decision,
    const char *reason,
    double price,
    double btc_amount,
    double eur_amount,
    double estimated_fee,
    double net_profit
);

int db_get_recent_engine_audits(
    EngineAuditRecord *records,
    int max_records
);


int db_get_engine_audit_summary_last_days(
    EngineAuditSummary *summary,
    int days
);


int db_upsert_order_state(
    const char *client_order_id,
    const char *side,
    const char *status,
    const char *product_id,
    int dry_run,
    double requested_quote_size,
    double requested_base_size,
    double preview_total_eur,
    double preview_fee_eur,
    double preview_base_size,
    double preview_avg_price,
    const char *reason
);

int db_get_active_order_state(OrderStateRecord *record);

int db_mark_dry_run_orders_recovered(void);

int db_prune_engine_audits(int retention_days);

int db_prune_engine_audits_max_records(int max_records);

int db_set_setting(
    const char *key,
    const char *value
);

int db_get_setting(
    const char *key,
    char *buffer,
    int buffer_size
);

#endif
