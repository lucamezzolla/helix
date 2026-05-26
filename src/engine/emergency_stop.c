#include "emergency_stop.h"
#include "settings.h"

#include <stddef.h>

int emergency_stop_is_active(const StrategySettings *settings) {
    if (settings == NULL) {
        return 1;
    }

    return settings->emergency_stop_enabled ? 1 : 0;
}

void emergency_stop_activate(void) {
    StrategySettings settings = settings_load();

    settings.emergency_stop_enabled = 1;
    settings_save(&settings);
}

void emergency_stop_reset(void) {
    StrategySettings settings = settings_load();

    settings.emergency_stop_enabled = 0;
    settings_save(&settings);
}
