#ifndef HELIX_WALLET_INFO_H
#define HELIX_WALLET_INFO_H

typedef struct {
    double eur_balance;
    double btc_balance;
    int connected;
} WalletInfo;

WalletInfo wallet_info_empty(void);

#endif
