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
