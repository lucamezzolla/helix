#include "coinbase_auth.h"

#include <jwt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COINBASE_JWT_ISSUER "cdp"
#define COINBASE_JWT_HOST "api.coinbase.com"

static void build_uri(
    char *buffer,
    size_t buffer_size,
    const char *method,
    const char *path
) {
    snprintf(
        buffer,
        buffer_size,
        "%s %s%s",
        method,
        COINBASE_JWT_HOST,
        path
    );
}

static void build_nonce(char *buffer, size_t buffer_size) {
    const char *hex = "0123456789abcdef";
    static int random_seeded = 0;

    if (buffer_size < 33) {
        if (buffer_size > 0) {
            buffer[0] = '\0';
        }

        return;
    }

    if (!random_seeded) {
        srand((unsigned int)time(NULL));
        random_seeded = 1;
    }

    for (int i = 0; i < 32; i++) {
        buffer[i] = hex[rand() % 16];
    }

    buffer[32] = '\0';
}

static char *normalize_secret_newlines(const char *api_secret) {
    size_t len = strlen(api_secret);
    char *result = malloc(len + 1);

    if (!result) {
        return NULL;
    }

    size_t j = 0;

    for (size_t i = 0; i < len; i++) {
        if (
            api_secret[i] == '\\' &&
            i + 1 < len &&
            api_secret[i + 1] == 'n'
        ) {
            result[j++] = '\n';
            i++;
        } else {
            result[j++] = api_secret[i];
        }
    }

    result[j] = '\0';

    return result;
}

char *coinbase_build_rest_jwt(
    const char *method,
    const char *path,
    const char *api_key,
    const char *api_secret
) {
    jwt_t *jwt = NULL;
    char *token = NULL;
    char *normalized_secret = NULL;

    char uri[512];
    char nonce[64];

    time_t now = time(NULL);

    if (
        method == NULL ||
        path == NULL ||
        api_key == NULL ||
        api_secret == NULL
    ) {
        return NULL;
    }

    normalized_secret = normalize_secret_newlines(api_secret);

    if (!normalized_secret) {
        return NULL;
    }

    if (jwt_new(&jwt) != 0) {
        free(normalized_secret);
        return NULL;
    }

    build_uri(uri, sizeof(uri), method, path);
    build_nonce(nonce, sizeof(nonce));

    jwt_add_grant(jwt, "sub", api_key);
    jwt_add_grant(jwt, "iss", COINBASE_JWT_ISSUER);
    jwt_add_grant(jwt, "uri", uri);
    jwt_add_grant_int(jwt, "nbf", (long)now);
    jwt_add_grant_int(jwt, "exp", (long)(now + 120));

    jwt_add_header(jwt, "kid", api_key);
    jwt_add_header(jwt, "nonce", nonce);

    if (
        jwt_set_alg(
            jwt,
            JWT_ALG_ES256,
            (unsigned char *)normalized_secret,
            strlen(normalized_secret)
        ) != 0
    ) {
        jwt_free(jwt);
        free(normalized_secret);
        return NULL;
    }

    token = jwt_encode_str(jwt);

    jwt_free(jwt);
    free(normalized_secret);

    return token;
}
