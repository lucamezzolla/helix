#include "coinbase_client.h"
#include "coinbase_auth.h"
#include "../config/env_loader.h"

#include <curl/curl.h>
#include <cjson/cJSON.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define COINBASE_SPOT_URL \
    "https://api.coinbase.com/v2/prices/BTC-EUR/spot"

#define COINBASE_ACCOUNTS_URL \
    "https://api.coinbase.com/api/v3/brokerage/accounts"

#define COINBASE_ACCOUNTS_PATH \
    "/api/v3/brokerage/accounts"

#define COINBASE_FILLS_URL \
    "https://api.coinbase.com/api/v3/brokerage/orders/historical/fills?product_id=BTC-EUR&limit=100"

#define COINBASE_FILLS_PATH \
    "/api/v3/brokerage/orders/historical/fills"

#define COINBASE_ORDER_STATUS_URL_PREFIX \
    "https://api.coinbase.com"

#define COINBASE_ORDER_STATUS_PATH_PREFIX \
    "/api/v3/brokerage/orders/historical/"

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

    if (response == NULL || real_size > SIZE_MAX - response->size - 1) {
        return 0;
    }

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

static int json_bool_value(cJSON *item) {
    if (!item) {
        return 0;
    }

    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item);
    }

    if (cJSON_IsNumber(item)) {
        return item->valuedouble != 0.0;
    }

    if (cJSON_IsString(item) && item->valuestring) {
        return (
            strcmp(item->valuestring, "true") == 0 ||
            strcmp(item->valuestring, "TRUE") == 0 ||
            strcmp(item->valuestring, "1") == 0
        );
    }

    return 0;
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


static double parse_fill_fee_eur(cJSON *fill) {
    double fee = 0.0;

    fee += parse_decimal_item(cJSON_GetObjectItem(fill, "commission"));
    fee += parse_decimal_item(cJSON_GetObjectItem(fill, "fee"));

    cJSON *commission_detail = cJSON_GetObjectItem(fill, "commission_detail_total");
    if (commission_detail && cJSON_IsObject(commission_detail)) {
        fee += parse_money_object(commission_detail);
    }

    return fee;
}

typedef struct {
    double btc;
    double cost_eur;
} OpenLot;

static void fifo_reduce_lots(OpenLot *lots, int lot_count, double sell_btc) {
    for (int i = 0; i < lot_count && sell_btc > 0.0; i++) {
        if (lots[i].btc <= 0.0) {
            continue;
        }

        if (lots[i].btc <= sell_btc) {
            sell_btc -= lots[i].btc;
            lots[i].btc = 0.0;
            lots[i].cost_eur = 0.0;
        } else {
            double ratio = sell_btc / lots[i].btc;
            lots[i].cost_eur -= lots[i].cost_eur * ratio;
            lots[i].btc -= sell_btc;
            sell_btc = 0.0;
        }
    }
}

static void process_fill_for_position(cJSON *fill, OpenLot *lots, int *lot_count, CoinbasePositionSummary *summary) {
    const char *side = json_string_value(cJSON_GetObjectItem(fill, "side"));

    if (!side) {
        return;
    }

    double price = parse_decimal_item(cJSON_GetObjectItem(fill, "price"));
    double raw_size = parse_decimal_item(cJSON_GetObjectItem(fill, "size"));
    double fee = parse_fill_fee_eur(fill);
    int size_in_quote = json_bool_value(cJSON_GetObjectItem(fill, "size_in_quote"));

    /*
     * Coinbase può restituire fill market BUY con size espresso in quote
     * currency (EUR) invece che in base currency (BTC).
     *
     * Se trattiamo un size EUR come BTC, il cost basis esplode:
     *   size=100 EUR, price=90000 EUR/BTC -> 9.000.000 EUR
     *
     * Quando size_in_quote è true convertiamo:
     *   base_size = quote_size / price
     *   gross_cost = quote_size
     *
     * Il fallback euristico copre vecchie/varianti di payload dove il flag non
     * è presente ma il size è chiaramente troppo grande per essere BTC.
     */
    int looks_like_quote_size = (!size_in_quote && price > 1000.0 && raw_size > 1.0);

    double base_size = (size_in_quote || looks_like_quote_size)
        ? raw_size / price
        : raw_size;

    double gross_quote_value = (size_in_quote || looks_like_quote_size)
        ? raw_size
        : raw_size * price;

    if (price <= 0.0 || raw_size <= 0.0 || base_size <= 0.0) {
        return;
    }

    summary->fill_count++;

    if (strcmp(side, "BUY") == 0) {
        double real_cost = gross_quote_value + fee;

        if (*lot_count < 256) {
            lots[*lot_count].btc = base_size;
            lots[*lot_count].cost_eur = real_cost;
            (*lot_count)++;
        }

        summary->total_buy_fees_eur += fee;
    } else if (strcmp(side, "SELL") == 0) {
        fifo_reduce_lots(lots, *lot_count, base_size);
        summary->total_sell_fees_eur += fee;
    }
}

static cJSON *extract_fills_array(cJSON *json) {
    cJSON *fills = cJSON_GetObjectItem(json, "fills");

    if (fills && cJSON_IsArray(fills)) {
        return fills;
    }

    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (data && cJSON_IsObject(data)) {
        fills = cJSON_GetObjectItem(data, "fills");
        if (fills && cJSON_IsArray(fills)) {
            return fills;
        }
    }

    return NULL;
}

CoinbasePositionSummary coinbase_get_btc_eur_position_summary_readonly(void) {
    CoinbasePositionSummary summary = {0};

    /*
     * Se Coinbase risponde 401/403 sui fills, non continuiamo a richiamare
     * l'endpoint a ogni tick: la UI resta fluida e il wallet sync continua.
     * Il flag si resetta al prossimo avvio dell'applicazione.
     */
    static int disabled_after_auth_error = 0;
    static int auth_error_already_reported = 0;
    static int generic_error_already_reported = 0;

    if (disabled_after_auth_error) {
        return summary;
    }

    CoinbaseCredentials credentials = env_load_coinbase_credentials();

    if (!credentials.loaded) {
        return summary;
    }

    char *jwt_token = coinbase_build_rest_jwt(
        "GET",
        COINBASE_FILLS_PATH,
        credentials.api_key,
        credentials.api_secret
    );

    if (!jwt_token) {
        return summary;
    }

    CURL *curl = curl_easy_init();

    if (!curl) {
        free(jwt_token);
        return summary;
    }

    HttpResponse response;

    if (!http_response_init(&response)) {
        curl_easy_cleanup(curl);
        free(jwt_token);
        return summary;
    }

    struct curl_slist *headers = NULL;
    char auth_header[4096];

    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", jwt_token);

    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, COINBASE_FILLS_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Helix/0.1");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode result = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (result != CURLE_OK || http_code < 200 || http_code >= 300) {
        if (http_code == 401 || http_code == 403) {
            disabled_after_auth_error = 1;

            if (!auth_error_already_reported) {
                fprintf(
                    stderr,
                    "Coinbase fills disabled: endpoint non autorizzato "
                    "o JWT non accettato per fills (http=%ld). "
                    "Wallet read-only continuerà a funzionare.\n",
                    http_code
                );
                auth_error_already_reported = 1;
            }
        } else if (!generic_error_already_reported) {
            fprintf(
                stderr,
                "Coinbase fills warning: curl=%d http=%ld. "
                "Uso fallback wallet read-only.\n",
                result,
                http_code
            );
            generic_error_already_reported = 1;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        http_response_free(&response);
        free(jwt_token);
        return summary;
    }

    cJSON *json = cJSON_Parse(response.memory);

    if (!json) {
        if (!generic_error_already_reported) {
            fprintf(
                stderr,
                "Coinbase fills warning: JSON non valido. "
                "Uso fallback wallet read-only.\n"
            );
            generic_error_already_reported = 1;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        http_response_free(&response);
        free(jwt_token);
        return summary;
    }

    cJSON *fills = extract_fills_array(json);

    if (fills && cJSON_IsArray(fills)) {
        int count = cJSON_GetArraySize(fills);
        OpenLot lots[256];
        int lot_count = 0;

        memset(lots, 0, sizeof(lots));

        /* Coinbase normalmente restituisce i fill più recenti prima.
         * Per ricostruire il costo con FIFO li processiamo dal più vecchio
         * al più recente.
         */
        for (int i = count - 1; i >= 0; i--) {
            cJSON *fill = cJSON_GetArrayItem(fills, i);
            if (fill && cJSON_IsObject(fill)) {
                process_fill_for_position(fill, lots, &lot_count, &summary);
            }
        }

        for (int i = 0; i < lot_count; i++) {
            summary.btc_open += lots[i].btc;
            summary.cost_basis_eur += lots[i].cost_eur;
        }

        if (summary.btc_open > 0.0) {
            summary.avg_buy_price = summary.cost_basis_eur / summary.btc_open;
        }

        summary.connected = 1;
    } else if (!generic_error_already_reported) {
        fprintf(
            stderr,
            "Coinbase fills warning: array fills non trovato. "
            "Uso fallback wallet read-only.\n"
        );
        generic_error_already_reported = 1;
    }

    cJSON_Delete(json);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    http_response_free(&response);
    free(jwt_token);

    return summary;
}


static void safe_copy_client(char *dst, size_t dst_size, const char *src) {
    if (dst == NULL || dst_size == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    snprintf(dst, dst_size, "%s", src);
}

CoinbaseOrderStatus coinbase_get_order_status_readonly(const char *order_id) {
    CoinbaseOrderStatus status;
    memset(&status, 0, sizeof(status));

    if (order_id == NULL || order_id[0] == '\0') {
        safe_copy_client(status.message, sizeof(status.message), "Coinbase order status: order_id mancante");
        return status;
    }

    CoinbaseCredentials credentials = env_load_coinbase_credentials();

    if (!credentials.loaded) {
        safe_copy_client(status.message, sizeof(status.message), "Coinbase order status: credenziali mancanti");
        return status;
    }

    char path[256];
    char url[512];

    snprintf(
        path,
        sizeof(path),
        "%s%s",
        COINBASE_ORDER_STATUS_PATH_PREFIX,
        order_id
    );

    snprintf(
        url,
        sizeof(url),
        "%s%s",
        COINBASE_ORDER_STATUS_URL_PREFIX,
        path
    );

    char *jwt_token = coinbase_build_rest_jwt(
        "GET",
        path,
        credentials.api_key,
        credentials.api_secret
    );

    if (!jwt_token) {
        safe_copy_client(status.message, sizeof(status.message), "Coinbase order status: JWT non generato");
        return status;
    }

    CURL *curl = curl_easy_init();

    if (!curl) {
        free(jwt_token);
        safe_copy_client(status.message, sizeof(status.message), "Coinbase order status: curl init fallita");
        return status;
    }

    HttpResponse response;

    if (!http_response_init(&response)) {
        curl_easy_cleanup(curl);
        free(jwt_token);
        safe_copy_client(status.message, sizeof(status.message), "Coinbase order status: memoria risposta non disponibile");
        return status;
    }

    struct curl_slist *headers = NULL;
    char auth_header[4096];

    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", jwt_token);

    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Helix/0.2");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode result = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    status.http_code = http_code;

    if (result != CURLE_OK || http_code < 200 || http_code >= 300) {
        snprintf(
            status.message,
            sizeof(status.message),
            "Coinbase order status error: curl=%d http=%ld",
            result,
            http_code
        );

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        http_response_free(&response);
        free(jwt_token);
        return status;
    }

    cJSON *json = cJSON_Parse(response.memory);

    if (!json) {
        safe_copy_client(status.message, sizeof(status.message), "Coinbase order status: JSON non valido");

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        http_response_free(&response);
        free(jwt_token);
        return status;
    }

    cJSON *order = cJSON_GetObjectItem(json, "order");

    if (order && cJSON_IsObject(order)) {
        status.connected = 1;
        status.found = 1;

        safe_copy_client(
            status.order_id,
            sizeof(status.order_id),
            json_string_value(cJSON_GetObjectItem(order, "order_id"))
        );

        safe_copy_client(
            status.product_id,
            sizeof(status.product_id),
            json_string_value(cJSON_GetObjectItem(order, "product_id"))
        );

        safe_copy_client(
            status.side,
            sizeof(status.side),
            json_string_value(cJSON_GetObjectItem(order, "side"))
        );

        safe_copy_client(
            status.status,
            sizeof(status.status),
            json_string_value(cJSON_GetObjectItem(order, "status"))
        );

        status.filled_size =
            parse_decimal_item(cJSON_GetObjectItem(order, "filled_size"));

        status.average_filled_price =
            parse_decimal_item(cJSON_GetObjectItem(order, "average_filled_price"));

        status.total_fees =
            parse_decimal_item(cJSON_GetObjectItem(order, "total_fees"));

        status.completion_percentage =
            parse_decimal_item(cJSON_GetObjectItem(order, "completion_percentage"));

        snprintf(
            status.message,
            sizeof(status.message),
            "Coinbase order status OK: %s %.2f%%",
            status.status[0] ? status.status : "UNKNOWN",
            status.completion_percentage
        );
    } else {
        status.connected = 1;
        status.found = 0;
        safe_copy_client(status.message, sizeof(status.message), "Coinbase order status: oggetto order mancante");
    }

    cJSON_Delete(json);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    http_response_free(&response);
    free(jwt_token);

    return status;
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
