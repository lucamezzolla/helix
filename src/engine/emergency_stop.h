#ifndef HELIX_EMERGENCY_STOP_H
#define HELIX_EMERGENCY_STOP_H

#include "settings.h"

int emergency_stop_is_active(const StrategySettings *settings);
void emergency_stop_activate(void);
void emergency_stop_reset(void);

#endif
