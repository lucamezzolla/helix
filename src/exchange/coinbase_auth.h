#ifndef HELIX_COINBASE_AUTH_H
#define HELIX_COINBASE_AUTH_H

char *coinbase_build_rest_jwt(
    const char *method,
    const char *path,
    const char *api_key,
    const char *api_secret
);

#endif
