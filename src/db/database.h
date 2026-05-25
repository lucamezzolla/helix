#ifndef HELIX_DATABASE_H
#define HELIX_DATABASE_H

#include "../engine/bot_state.h"

int db_init(void);
int db_save_state(const BotState *state);
int db_load_state(BotState *state);

#endif
