#ifndef HELIX_BOT_STATE_H
#define HELIX_BOT_STATE_H

#include <stdbool.h>

typedef enum {
    BOT_MODE_PAUSED,
    BOT_MODE_READY,
    BOT_MODE_BUYING,
    BOT_MODE_WAITING_SELL,
    BOT_MODE_SELLING,
    BOT_MODE_ERROR
} BotMode;

typedef struct {
    bool running;
    BotMode mode;
    double eur_balance;
    double btc_balance;
    double current_price;
    double last_buy_price;
    double avg_buy_price;
    char last_trade[128];
    int used_slots;
    int max_slots;
} BotState;

BotState bot_state_default(void);

#endif
