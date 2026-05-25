#include "bot_state.h"

BotState bot_state_default(void) {
    BotState state;

    state.running = false;
    state.mode = BOT_MODE_PAUSED;
    state.eur_balance = 1000.00;
    state.btc_balance = 0.0;
    state.current_price = 0.0;
    state.used_slots = 0;
    state.max_slots = 6;

    return state;
}
