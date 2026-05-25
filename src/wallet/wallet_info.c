#include "wallet_info.h"

WalletInfo wallet_info_empty(void) {
    WalletInfo info;

    info.eur_balance = 0.0;
    info.btc_balance = 0.0;
    info.connected = 0;

    return info;
}
