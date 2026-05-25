#ifndef HELIX_ENV_LOADER_H
#define HELIX_ENV_LOADER_H

typedef struct {
    char api_key[512];
    char api_secret[2048];
    int loaded;
} CoinbaseCredentials;

CoinbaseCredentials env_load_coinbase_credentials(void);

int env_save_coinbase_credentials(
    const char *api_key,
    const char *api_secret
);

#endif
