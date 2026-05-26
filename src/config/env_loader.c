#include "env_loader.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define ENV_PATH ".env"

static void trim(char *s) {
    char *start = s;
    char *end;

    while (isspace((unsigned char)*start)) {
        start++;
    }

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }

    if (*s == '\0') {
        return;
    }

    end = s + strlen(s) - 1;

    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
}

static void strip_optional_quotes(char *s) {
    size_t len = strlen(s);

    if (len < 2) {
        return;
    }

    if (
        (s[0] == '"' && s[len - 1] == '"') ||
        (s[0] == '\'' && s[len - 1] == '\'')
    ) {
        memmove(s, s + 1, len - 2);
        s[len - 2] = '\0';
    }
}

static void copy_value(char *dest, size_t dest_size, const char *value) {
    if (dest_size == 0) {
        return;
    }

    snprintf(dest, dest_size, "%s", value);
}

CoinbaseCredentials env_load_coinbase_credentials(void) {
    CoinbaseCredentials credentials;

    credentials.api_key[0] = '\0';
    credentials.api_secret[0] = '\0';
    credentials.loaded = 0;

    FILE *file = fopen(ENV_PATH, "r");

    if (!file) {
        return credentials;
    }

    char line[4096];

    while (fgets(line, sizeof(line), file)) {
        char *equals;
        char *key;
        char *value;

        trim(line);

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        equals = strchr(line, '=');

        if (!equals) {
            continue;
        }

        *equals = '\0';

        key = line;
        value = equals + 1;

        trim(key);
        trim(value);
        strip_optional_quotes(value);

        if (strcmp(key, "COINBASE_API_KEY") == 0) {
            copy_value(credentials.api_key, sizeof(credentials.api_key), value);
        } else if (strcmp(key, "COINBASE_API_SECRET") == 0) {
            copy_value(credentials.api_secret, sizeof(credentials.api_secret), value);
        }
    }

    fclose(file);

    if (
        credentials.api_key[0] != '\0' &&
        credentials.api_secret[0] != '\0'
    ) {
        credentials.loaded = 1;
    }

    return credentials;
}

int env_save_coinbase_credentials(
    const char *api_key,
    const char *api_secret
) {
    if (api_key == NULL || api_secret == NULL) {
        return 0;
    }

    FILE *file = fopen(ENV_PATH, "w");

    if (!file) {
        return 0;
    }

    fprintf(file, "# Helix Coinbase credentials\n");
    fprintf(file, "# This file is local and must never be committed.\n");
    fprintf(file, "COINBASE_API_KEY=%s\n", api_key);
    fprintf(file, "COINBASE_API_SECRET=%s\n", api_secret);

    fclose(file);

    return 1;
}


static int parse_bool_value(const char *value, int fallback) {
    if (value == NULL || value[0] == '\0') {
        return fallback;
    }

    if (
        strcmp(value, "1") == 0 ||
        strcmp(value, "true") == 0 ||
        strcmp(value, "TRUE") == 0 ||
        strcmp(value, "yes") == 0 ||
        strcmp(value, "YES") == 0 ||
        strcmp(value, "on") == 0 ||
        strcmp(value, "ON") == 0
    ) {
        return 1;
    }

    if (
        strcmp(value, "0") == 0 ||
        strcmp(value, "false") == 0 ||
        strcmp(value, "FALSE") == 0 ||
        strcmp(value, "no") == 0 ||
        strcmp(value, "NO") == 0 ||
        strcmp(value, "off") == 0 ||
        strcmp(value, "OFF") == 0
    ) {
        return 0;
    }

    return fallback;
}

int env_load_bool_flag(
    const char *target_key,
    int fallback
) {
    FILE *file;
    char line[4096];

    if (target_key == NULL || target_key[0] == '\0') {
        return fallback;
    }

    file = fopen(ENV_PATH, "r");

    if (!file) {
        return fallback;
    }

    while (fgets(line, sizeof(line), file)) {
        char *equals;
        char *key;
        char *value;

        trim(line);

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        equals = strchr(line, '=');

        if (!equals) {
            continue;
        }

        *equals = '\0';

        key = line;
        value = equals + 1;

        trim(key);
        trim(value);
        strip_optional_quotes(value);

        if (strcmp(key, target_key) == 0) {
            int parsed = parse_bool_value(value, fallback);
            fclose(file);
            return parsed;
        }
    }

    fclose(file);

    return fallback;
}
