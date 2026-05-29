#include "order_preview.h"
#include "coinbase_auth.h"
#include "../config/env_loader.h"

#include <curl/curl.h>
#include <cjson/cJSON.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define COINBASE_ORDER_PREVIEW_URL \
    "https://api.coinbase.com/api/v3/brokerage/orders/preview"

#define COINBASE_ORDER_PREVIEW_PATH \
    "/api/v3/brokerage/orders/preview"

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

/* Parser decimale indipendente dalla locale: Coinbase usa stringhe con punto. */
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

static const char *json_string_value(cJSON *item) {
    if (item && cJSON_IsString(item)) {
        return item->valuestring;
    }

    return NULL;
}

static void copy_json_string(char *dest, size_t dest_size, cJSON *item) {
    const char *value = json_string_value(item);

    if (!dest || dest_size == 0) {
        return;
    }

    if (!value) {
        dest[0] = '\0';
        return;
    }

    snprintf(dest, dest_size, "%s", value);
}

static int json_array_has_items(cJSON *item) {
    return item && cJSON_IsArray(item) && cJSON_GetArraySize(item) > 0;
}

static void preview_set_message(
    CoinbaseOrderPreview *preview,
    const char *message
) {
    snprintf(
        preview->message,
        sizeof(preview->message),
        "%s",
        message ? message : ""
    );
}

static void save_buy_preview_debug_response(const char *response_body) {
    FILE *file;

    if (response_body == NULL || response_body[0] == '\0') {
        return;
    }

    /*
     * Debug locale intenzionale.
     * Il file e' sotto data/ e non deve essere committato.
     * Serve per leggere il motivo reale restituito da Coinbase quando
     * l'endpoint preview risponde HTTP 200 ma contiene errs.
     */
    mkdir("data", 0755);

    file = fopen("data/coinbase_buy_preview_last.json", "w");
    if (file == NULL) {
        return;
    }

    fputs(response_body, file);
    fputc('\n', file);
    fclose(file);
}


static void save_buy_preview_debug_request(const char *request_body) {
    FILE *file;

    if (request_body == NULL || request_body[0] == '\0') {
        return;
    }

    /*
     * Debug locale intenzionale.
     * Serve per verificare che i BUY verso Coinbase usino quote_size EUR
     * con separatore decimale punto, non base_size BTC e non virgola locale.
     */
    mkdir("data", 0755);

    file = fopen("data/coinbase_buy_preview_request_last.json", "w");
    if (file == NULL) {
        return;
    }

    fputs(request_body, file);
    fputc('\n', file);
    fclose(file);
}

static void save_sell_preview_debug_response(const char *response_body) {
    FILE *file;

    if (response_body == NULL || response_body[0] == '\0') {
        return;
    }

    mkdir("data", 0755);

    file = fopen("data/coinbase_sell_preview_last.json", "w");
    if (file == NULL) {
        return;
    }

    fputs(response_body, file);
    fputc('\n', file);
    fclose(file);
}

static void save_sell_preview_debug_request(const char *request_body) {
    FILE *file;

    if (request_body == NULL || request_body[0] == '\0') {
        return;
    }

    mkdir("data", 0755);

    file = fopen("data/coinbase_sell_preview_request_last.json", "w");
    if (file == NULL) {
        return;
    }

    fputs(request_body, file);
    fputc('\n', file);
    fclose(file);
}

static void format_decimal_dot(
    char *buffer,
    size_t buffer_size,
    double value,
    int decimals
) {
    size_t i;

    if (buffer == NULL || buffer_size == 0) {
        return;
    }

    if (decimals <= 2) {
        snprintf(buffer, buffer_size, "%.2f", value);
    } else {
        snprintf(buffer, buffer_size, "%.8f", value);
    }

    /*
     * snprintf segue LC_NUMERIC: in ambiente italiano potrebbe produrre
     * "10,00". Coinbase vuole sempre decimali con punto nelle stringhe JSON.
     */
    for (i = 0; buffer[i] != '\0'; ++i) {
        if (buffer[i] == ',') {
            buffer[i] = '.';
        }
    }
}

static const char *first_json_string(cJSON *object, const char *a, const char *b, const char *c) {
    cJSON *item;

    if (object == NULL) {
        return NULL;
    }

    item = cJSON_GetObjectItem(object, a);
    if (item != NULL && cJSON_IsString(item) && item->valuestring != NULL) {
        return item->valuestring;
    }

    if (b != NULL) {
        item = cJSON_GetObjectItem(object, b);
        if (item != NULL && cJSON_IsString(item) && item->valuestring != NULL) {
            return item->valuestring;
        }
    }

    if (c != NULL) {
        item = cJSON_GetObjectItem(object, c);
        if (item != NULL && cJSON_IsString(item) && item->valuestring != NULL) {
            return item->valuestring;
        }
    }

    return NULL;
}

static void preview_set_error_message_from_json(CoinbaseOrderPreview *preview, cJSON *json) {
    cJSON *errs;
    cJSON *first_error;
    const char *code = NULL;
    const char *message = NULL;

    if (preview == NULL || json == NULL) {
        return;
    }

    errs = cJSON_GetObjectItem(json, "errs");
    if (errs != NULL && cJSON_IsArray(errs) && cJSON_GetArraySize(errs) > 0) {
        first_error = cJSON_GetArrayItem(errs, 0);

        if (first_error != NULL && cJSON_IsString(first_error) && first_error->valuestring != NULL) {
            snprintf(
                preview->message,
                sizeof(preview->message),
                "Coinbase preview error: %.180s",
                first_error->valuestring
            );
            return;
        }

        code = first_json_string(first_error, "error_code", "error", "code");
        message = first_json_string(first_error, "message", "error_message", "description");

        if (code != NULL || message != NULL) {
            snprintf(
                preview->message,
                sizeof(preview->message),
                "Coinbase preview error: %.64s %.160s",
                code ? code : "ERR",
                message ? message : ""
            );
            return;
        }

        preview_set_message(preview, "Coinbase preview ha restituito errs senza dettaglio leggibile");
        return;
    }

    code = first_json_string(json, "error", "error_code", "code");
    message = first_json_string(json, "message", "error_message", "description");

    if (code != NULL || message != NULL) {
        snprintf(
            preview->message,
            sizeof(preview->message),
            "Coinbase preview error: %.64s %.160s",
            code ? code : "ERR",
            message ? message : ""
        );
        return;
    }

    preview_set_message(preview, "Coinbase preview ha restituito errori");
}

static void parse_preview_response(
    CoinbaseOrderPreview *preview,
    const char *response_body
) {
    cJSON *json = cJSON_Parse(response_body);

    if (!json) {
        preview_set_message(preview, "preview JSON non valido");
        return;
    }

    preview->order_total = parse_decimal_item(cJSON_GetObjectItem(json, "order_total"));
    preview->commission_total = parse_decimal_item(cJSON_GetObjectItem(json, "commission_total"));
    preview->quote_size = parse_decimal_item(cJSON_GetObjectItem(json, "quote_size"));
    preview->base_size = parse_decimal_item(cJSON_GetObjectItem(json, "base_size"));
    preview->best_bid = parse_decimal_item(cJSON_GetObjectItem(json, "best_bid"));
    preview->best_ask = parse_decimal_item(cJSON_GetObjectItem(json, "best_ask"));
    preview->est_average_filled_price = parse_decimal_item(cJSON_GetObjectItem(json, "est_average_filled_price"));
    preview->slippage = parse_decimal_item(cJSON_GetObjectItem(json, "slippage"));

    copy_json_string(
        preview->preview_id,
        sizeof(preview->preview_id),
        cJSON_GetObjectItem(json, "preview_id")
    );

    if (json_array_has_items(cJSON_GetObjectItem(json, "errs"))) {
        preview->allowed = 0;
        preview_set_error_message_from_json(preview, json);
    } else {
        preview->allowed = 1;
        preview_set_message(preview, "Coinbase preview OK");
    }

    preview->connected = 1;

    cJSON_Delete(json);
}

static void build_market_ioc_body(
    char *buffer,
    size_t buffer_size,
    const char *product_id,
    OrderPreviewSide side,
    double amount
) {
    char amount_text[64];
    const char *side_text = side == ORDER_PREVIEW_SIDE_BUY ? "BUY" : "SELL";
    const char *size_field = side == ORDER_PREVIEW_SIDE_BUY ? "quote_size" : "base_size";

    format_decimal_dot(
        amount_text,
        sizeof(amount_text),
        amount,
        side == ORDER_PREVIEW_SIDE_BUY ? 2 : 8
    );

    snprintf(
        buffer,
        buffer_size,
        "{"
        "\"product_id\":\"%s\"," 
        "\"side\":\"%s\"," 
        "\"order_configuration\":{"
        "\"market_market_ioc\":{"
        "\"%s\":\"%s\"," 
        "\"rfq_disabled\":true"
        "}"
        "}"
        "}",
        product_id,
        side_text,
        size_field,
        amount_text
    );
}

static CoinbaseOrderPreview order_preview_empty(OrderPreviewSide side) {
    CoinbaseOrderPreview preview;
    memset(&preview, 0, sizeof(preview));
    preview.side = side;
    preview.allowed = 0;
    preview.connected = 0;
    preview.http_code = 0;
    preview_set_message(&preview, "preview non eseguita");
    return preview;
}

static CoinbaseOrderPreview coinbase_order_preview_market(
    const char *product_id,
    OrderPreviewSide side,
    double amount
) {
    CoinbaseOrderPreview preview = order_preview_empty(side);

    if (!product_id || amount <= 0.0) {
        preview_set_message(&preview, "parametri preview non validi");
        return preview;
    }

    CoinbaseCredentials credentials = env_load_coinbase_credentials();

    if (!credentials.loaded) {
        preview_set_message(&preview, "credenziali Coinbase mancanti");
        return preview;
    }

    char *jwt_token = coinbase_build_rest_jwt(
        "POST",
        COINBASE_ORDER_PREVIEW_PATH,
        credentials.api_key,
        credentials.api_secret
    );

    if (!jwt_token) {
        preview_set_message(&preview, "JWT Coinbase preview non generato");
        return preview;
    }

    CURL *curl = curl_easy_init();

    if (!curl) {
        free(jwt_token);
        preview_set_message(&preview, "curl non inizializzato");
        return preview;
    }

    HttpResponse response;

    if (!http_response_init(&response)) {
        curl_easy_cleanup(curl);
        free(jwt_token);
        preview_set_message(&preview, "memoria risposta preview non allocata");
        return preview;
    }

    char request_body[1024];
    char auth_header[4096];
    struct curl_slist *headers = NULL;

    build_market_ioc_body(
        request_body,
        sizeof(request_body),
        product_id,
        side,
        amount
    );

    if (side == ORDER_PREVIEW_SIDE_BUY) {
        save_buy_preview_debug_request(request_body);
    } else if (side == ORDER_PREVIEW_SIDE_SELL) {
        save_sell_preview_debug_request(request_body);
    }

    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", jwt_token);

    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, COINBASE_ORDER_PREVIEW_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Helix/0.1");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode result = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    preview.http_code = http_code;

    if (result != CURLE_OK) {
        preview_set_message(&preview, "errore curl durante order preview");
    } else if (http_code < 200 || http_code >= 300) {
        snprintf(
            preview.message,
            sizeof(preview.message),
            "Coinbase order preview HTTP %ld",
            http_code
        );
    } else {
        if (side == ORDER_PREVIEW_SIDE_BUY) {
            save_buy_preview_debug_response(response.memory);
        } else if (side == ORDER_PREVIEW_SIDE_SELL) {
            save_sell_preview_debug_response(response.memory);
        }
        parse_preview_response(&preview, response.memory);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    http_response_free(&response);
    free(jwt_token);

    return preview;
}

CoinbaseOrderPreview coinbase_order_preview_market_buy_eur(
    const char *product_id,
    double eur_amount
) {
    return coinbase_order_preview_market(
        product_id,
        ORDER_PREVIEW_SIDE_BUY,
        eur_amount
    );
}

CoinbaseOrderPreview coinbase_order_preview_market_sell_btc(
    const char *product_id,
    double btc_amount
) {
    return coinbase_order_preview_market(
        product_id,
        ORDER_PREVIEW_SIDE_SELL,
        btc_amount
    );
}
