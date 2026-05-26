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
