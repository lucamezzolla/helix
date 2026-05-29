#ifndef HELIX_REAL_SELL_SUPERVISION_H
#define HELIX_REAL_SELL_SUPERVISION_H

#include "settings.h"

#define REAL_SELL_SUPERVISION_REASON_SIZE 512

typedef struct {
    int allowed;
    char decision[64];
    char reason[REAL_SELL_SUPERVISION_REASON_SIZE];
} RealSellSupervisionCheck;

RealSellSupervisionCheck real_sell_supervision_check_one_shot(
    const StrategySettings *settings
);

#endif
