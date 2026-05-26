#ifndef HELIX_COINBASE_CLIENT_H
#define HELIX_COINBASE_CLIENT_H

#include "../wallet/wallet_info.h"


typedef struct {
    int connected;
    int found;
    long http_code;
    char order_id[128];
    char product_id[32];
    char side[16];
    char status[64];
    double filled_size;
    double average_filled_price;
    double total_fees;
    double completion_percentage;
    char message[256];
} CoinbaseOrderStatus;

typedef struct {
    int connected;
    int fill_count;
    double btc_open;
    double cost_basis_eur;
    double avg_buy_price;
    double total_buy_fees_eur;
    double total_sell_fees_eur;
} CoinbasePositionSummary;

double coinbase_get_btc_eur_spot_price(void);
int coinbase_has_credentials(void);

WalletInfo coinbase_get_wallet_info_readonly(void);
CoinbasePositionSummary coinbase_get_btc_eur_position_summary_readonly(void);
CoinbaseOrderStatus coinbase_get_order_status_readonly(const char *order_id);

#endif
