#ifndef HELIX_LIQUIDITY_RESERVE_H
#define HELIX_LIQUIDITY_RESERVE_H

#include "bot_state.h"
#include "settings.h"

typedef struct {
    double eur_balance;
    double slot_amount_eur;
    double reserve_percent;
    double protected_eur;
    double released_eur;
    double operational_eur;
    double eur_after_buy;
    int base_operational_slots;
    int released_slots;
    int total_available_slots;
    int can_buy;
    char reason[160];
} LiquidityReserveCheck;

LiquidityReserveCheck liquidity_reserve_check_buy(
    const BotState *state,
    const StrategySettings *settings,
    double eur_amount
);

double liquidity_reserve_protected_eur(
    double eur_balance,
    double reserve_percent
);

double liquidity_reserve_released_eur(
    int released_slots,
    double slot_amount_eur
);

#endif
