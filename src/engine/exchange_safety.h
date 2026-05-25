#ifndef HELIX_EXCHANGE_SAFETY_H
#define HELIX_EXCHANGE_SAFETY_H

#include "trade_preview.h"
#include "../exchange/order_preview.h"

#define EXCHANGE_SAFETY_REASON_SIZE 256

#define EXCHANGE_SAFETY_MAX_PRICE_DEVIATION_PERCENT 2.0
#define EXCHANGE_SAFETY_MAX_FEE_DEVIATION_PERCENT 2.0

typedef enum {
    EXCHANGE_SAFETY_SIDE_BUY,
    EXCHANGE_SAFETY_SIDE_SELL
} ExchangeSafetySide;

typedef struct {
    int allowed;
    ExchangeSafetySide side;

    double local_total_eur;
    double exchange_total_eur;
    double local_fee_eur;
    double exchange_fee_eur;
    double local_btc_amount;
    double exchange_btc_amount;
    double exchange_avg_price;
    double total_deviation_percent;
    double fee_deviation_percent;

    char decision[64];
    char reason[EXCHANGE_SAFETY_REASON_SIZE];
} ExchangeSafetyCheck;

ExchangeSafetyCheck exchange_safety_check_sell(
    TradePreview local_preview,
    CoinbaseOrderPreview exchange_preview,
    double btc_amount
);

ExchangeSafetyCheck exchange_safety_check_buy(
    TradePreview local_preview,
    CoinbaseOrderPreview exchange_preview,
    double eur_amount
);

#endif
