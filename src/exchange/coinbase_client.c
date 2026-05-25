#include "coinbase_client.h"
#include "coinbase_auth.h"
#include "../config/env_loader.h"

#include <curl/curl.h>
#include <cjson/cJSON.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COINBASE_SPOT_URL \
    "https://api.coinbase.com/v2/prices/BTC-EUR/spot"

#define COINBASE_ACCOUNTS_URL \
    "https://api.coinbase.com/api/v3/brokerage/accounts"

#define COINBASE_ACCOUNTS_PATH \
    "/api/v3/brokerage/accounts"

typedef struct {
    char *memory;
    size_t size;
} HttpResponse;

static size_t write_callback(
    void *contents,
    size_t size,
    size_t nmemb,
    void *userp
) {
    size_t real_size = size * nmemb;
    HttpResponse *response = (HttpResponse *)userp;

    char *ptr = realloc(
        response->memory,
        response->size + real_size + 1
    );

    if (!ptr) {
        return 0;
    }

    response->memory = ptr;
    memcpy(&(response->memory[response->size]), contents, real_size);

    response->size += real_size;
    response->memory[response->size] = '\0';

    return real_size;
}

static int http_response_init(HttpResponse *response) {
    response->memory = malloc(1);
    response->size = 0;

    if (!response->memory) {
        return 0;
    }

    response->memory[0] = '\0';

    return 1;
}

static void http_response_free(HttpResponse *response) {
    free(response->memory);
    response->memory = NULL;
    response->size = 0;
}

/*
 * Parser decimale indipendente dalla locale.
 *
 * Su alcuni sistemi italiani strtod/atof possono aspettarsi la virgola
 * e quindi "0.00679761" viene letto come 0.
 *
 * Coinbase usa sempre il punto decimale nelle stringhe JSON.
 */
static double parse_decimal_string(const char *text) {
    double result = 0.0;
    double divisor = 10.0;
    int sign = 1;
    int after_decimal = 0;

    if (!text) {
        return 0.0;
    }

    while (isspace((unsigned char)*text)) {
        text++;
    }

    if (*text == '-') {
        sign = -1;
        text++;
    } else if (*text == '+') {
        text++;
    }

    while (*text) {
        if (*text >= '0' && *text <= '9') {
            int digit = *text - '0';

            if (after_decimal) {
                result += digit / divisor;
                divisor *= 10.0;
            } else {
                result = result * 10.0 + digit;
            }
        } else if (*text == '.' || *text == ',') {
            if (after_decimal) {
                break;
            }

            after_decimal = 1;
        } else {
            break;
        }

        text++;
    }

    return result * sign;
}

static double parse_decimal_item(cJSON *item) {
    if (!item) {
        return 0.0;
    }

    if (cJSON_IsNumber(item)) {
        return item->valuedouble;
    }

    if (cJSON_IsString(item) && item->valuestring) {
        return parse_decimal_string(item->valuestring);
    }

    return 0.0;
}

int coinbase_has_credentials(void) {
    CoinbaseCredentials credentials = env_load_coinbase_credentials();

    return credentials.loaded;
}

static const char *json_string_value(cJSON *item) {
    if (item && cJSON_IsString(item)) {
        return item->valuestring;
    }

    return NULL;
}

static const char *get_account_currency(cJSON *account) {
    cJSON *currency = cJSON_GetObjectItem(account, "currency");
    const char *value = json_string_value(currency);

    if (value) {
        return value;
    }

    if (currency && cJSON_IsObject(currency)) {
        cJSON *code = cJSON_GetObjectItem(currency, "code");
        value = json_string_value(code);

        if (value) {
            return value;
        }
    }

    cJSON *available_balance =
        cJSON_GetObjectItem(account, "available_balance");

    if (available_balance && cJSON_IsObject(available_balance)) {
        cJSON *balance_currency =
            cJSON_GetObjectItem(available_balance, "currency");

        value = json_string_value(balance_currency);

        if (value) {
            return value;
        }
    }

    return NULL;
}

static double parse_money_object(cJSON *object) {
    if (!object || !cJSON_IsObject(object)) {
        return 0.0;
    }

    cJSON *value = cJSON_GetObjectItem(object, "value");

    if (value) {
        double parsed = parse_decimal_item(value);

        if (parsed != 0.0) {
            return parsed;
        }
    }

    cJSON *amount = cJSON_GetObjectItem(object, "amount");

    if (amount) {
        double parsed = parse_decimal_item(amount);

        if (parsed != 0.0) {
            return parsed;
        }
    }

    return 0.0;
}

static double parse_account_balance(cJSON *account) {
    double available =
        parse_money_object(cJSON_GetObjectItem(account, "available_balance"));

    double hold =
        parse_money_object(cJSON_GetObjectItem(account, "hold"));

    if ((available + hold) != 0.0) {
        return available + hold;
    }

    double total =
        parse_money_object(cJSON_GetObjectItem(account, "total_balance"));

    if (total != 0.0) {
        return total;
    }

    double balance =
        parse_money_object(cJSON_GetObjectItem(account, "balance"));

    return balance;
}

WalletInfo coinbase_get_wallet_info_readonly(void) {
    WalletInfo info = wallet_info_empty();

    CoinbaseCredentials credentials =
        env_load_coinbase_credentials();

    if (!credentials.loaded) {
        return info;
    }

    char *jwt_token =
        coinbase_build_rest_jwt(
            "GET",
            COINBASE_ACCOUNTS_PATH,
            credentials.api_key,
            credentials.api_secret
        );

    if (!jwt_token) {
        return info;
    }

    CURL *curl = curl_easy_init();

    if (!curl) {
        free(jwt_token);
        return info;
    }

    HttpResponse response;

    if (!http_response_init(&response)) {
        curl_easy_cleanup(curl);
        free(jwt_token);
        return info;
    }

    struct curl_slist *headers = NULL;
    char auth_header[4096];

    snprintf(
        auth_header,
        sizeof(auth_header),
        "Authorization: Bearer %s",
        jwt_token
    );

    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, COINBASE_ACCOUNTS_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Helix/0.1");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode result = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (result != CURLE_OK || http_code < 200 || http_code >= 300) {
        fprintf(
            stderr,
            "Coinbase accounts error: curl=%d http=%ld response=%s\n",
            result,
            http_code,
            response.memory ? response.memory : ""
        );

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        http_response_free(&response);
        free(jwt_token);
        return info;
    }

    cJSON *json = cJSON_Parse(response.memory);

    if (!json) {
        fprintf(stderr, "Coinbase accounts error: invalid JSON\n");

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        http_response_free(&response);
        free(jwt_token);
        return info;
    }

    cJSON *accounts = cJSON_GetObjectItem(json, "accounts");

    if (accounts && cJSON_IsArray(accounts)) {
        cJSON *account = NULL;

        cJSON_ArrayForEach(account, accounts) {
            const char *currency = get_account_currency(account);

            if (!currency) {
                continue;
            }

            double balance = parse_account_balance(account);

            if (strcmp(currency, "EUR") == 0) {
                info.eur_balance += balance;
            } else if (strcmp(currency, "BTC") == 0) {
                info.btc_balance += balance;
            }
        }

        info.connected = 1;
    } else {
        fprintf(stderr, "Coinbase accounts warning: missing accounts array\n");
    }

    cJSON_Delete(json);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    http_response_free(&response);
    free(jwt_token);

    return info;
}

double coinbase_get_btc_eur_spot_price(void) {
    CURL *curl;
    CURLcode result;
    HttpResponse response;

    if (!http_response_init(&response)) {
        return -1.0;
    }

    curl = curl_easy_init();

    if (!curl) {
        http_response_free(&response);
        return -1.0;
    }

    curl_easy_setopt(curl, CURLOPT_URL, COINBASE_SPOT_URL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Helix/0.1");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        curl_easy_cleanup(curl);
        http_response_free(&response);
        return -1.0;
    }

    cJSON *json = cJSON_Parse(response.memory);

    if (!json) {
        curl_easy_cleanup(curl);
        http_response_free(&response);
        return -1.0;
    }

    cJSON *data = cJSON_GetObjectItem(json, "data");

    if (!data) {
        cJSON_Delete(json);
        curl_easy_cleanup(curl);
        http_response_free(&response);
        return -1.0;
    }

    cJSON *amount = cJSON_GetObjectItem(data, "amount");

    if (!amount || !cJSON_IsString(amount)) {
        cJSON_Delete(json);
        curl_easy_cleanup(curl);
        http_response_free(&response);
        return -1.0;
    }

    double price = parse_decimal_item(amount);

    cJSON_Delete(json);
    curl_easy_cleanup(curl);
    http_response_free(&response);

    return price;
}
