#ifndef HELIX_COINBASE_CLIENT_H
#define HELIX_COINBASE_CLIENT_H

#include "../wallet/wallet_info.h"

double coinbase_get_btc_eur_spot_price(void);
int coinbase_has_credentials(void);

WalletInfo coinbase_get_wallet_info_readonly(void);

#endif
