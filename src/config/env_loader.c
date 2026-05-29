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

static int env_value_is_single_line(const char *value) {
    if (value == NULL || value[0] == '\0') {
        return 0;
    }

    return strchr(value, '\n') == NULL && strchr(value, '\r') == NULL;
}

int env_save_coinbase_credentials(
    const char *api_key,
    const char *api_secret
) {
    FILE *input;
    FILE *output;
    char preserved[32768];
    size_t preserved_len = 0;
    char line[4096];

    if (
        !env_value_is_single_line(api_key) ||
        !env_value_is_single_line(api_secret)
    ) {
        return 0;
    }

    preserved[0] = '\0';

    input = fopen(ENV_PATH, "r");
    if (input != NULL) {
        while (fgets(line, sizeof(line), input)) {
            char line_copy[4096];
            char *equals;
            char *key;

            snprintf(line_copy, sizeof(line_copy), "%s", line);
            trim(line_copy);

            equals = strchr(line_copy, '=');
            if (equals != NULL) {
                *equals = '\0';
                key = line_copy;
                trim(key);

                if (
                    strcmp(key, "COINBASE_API_KEY") == 0 ||
                    strcmp(key, "COINBASE_API_SECRET") == 0
                ) {
                    continue;
                }
            }

            size_t line_len = strlen(line);
            if (line_len < sizeof(preserved) - preserved_len - 1) {
                memcpy(preserved + preserved_len, line, line_len);
                preserved_len += line_len;
                preserved[preserved_len] = '\0';
            }
        }

        fclose(input);
    }

    output = fopen(ENV_PATH, "w");
    if (output == NULL) {
        return 0;
    }

    if (preserved_len > 0) {
        fputs(preserved, output);
        if (preserved[preserved_len - 1] != '\n') {
            fputc('\n', output);
        }
    } else {
        fprintf(output, "# Helix local configuration\n");
        fprintf(output, "# This file is local and must never be committed.\n");
    }

    fprintf(output, "COINBASE_API_KEY=%s\n", api_key);
    fprintf(output, "COINBASE_API_SECRET=%s\n", api_secret);

    if (fclose(output) != 0) {
        return 0;
    }

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
