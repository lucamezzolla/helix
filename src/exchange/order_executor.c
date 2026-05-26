#include "order_executor.h"
#include "coinbase_auth.h"
#include "../engine/live_execution_lock.h"
#include "../config/env_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef HELIX_ENABLE_REAL_COINBASE_ORDERS
#include <curl/curl.h>
#include <cjson/cJSON.h>
#endif

#define COINBASE_CREATE_ORDER_URL \
    "https://api.coinbase.com/api/v3/brokerage/orders"

#define COINBASE_CREATE_ORDER_PATH \
    "/api/v3/brokerage/orders"

static void safe_copy(char *dst, size_t dst_size, const char *src) {
    if (dst == NULL || dst_size == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    snprintf(dst, dst_size, "%s", src);
}

static void make_client_order_id(char *buffer, size_t buffer_size, const char *side) {
    time_t now = time(NULL);

    if (buffer == NULL || buffer_size == 0) {
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "helix-dryrun-%s-%ld",
        side ? side : "unknown",
        (long) now
    );
}

static OrderExecutionPlan empty_plan(OrderExecutorSide side, const char *product_id) {
    OrderExecutionPlan plan;

    memset(&plan, 0, sizeof(plan));
    plan.side = side;
    plan.dry_run = 1;
    safe_copy(plan.product_id, sizeof(plan.product_id), product_id ? product_id : "BTC-EUR");

    return plan;
}

OrderExecutionPlan order_executor_plan_market_buy_dry_run(
    const char *product_id,
    double quote_size_eur,
    CoinbaseOrderPreview preview
) {
    OrderExecutionPlan plan = empty_plan(ORDER_EXECUTOR_SIDE_BUY, product_id);

    plan.requested_quote_size = quote_size_eur;
    plan.preview_total_eur = preview.order_total;
    plan.preview_fee_eur = preview.commission_total;
    plan.preview_base_size = preview.base_size;
    plan.preview_avg_price = preview.est_average_filled_price;
    safe_copy(plan.preview_id, sizeof(plan.preview_id), preview.preview_id);
    make_client_order_id(plan.client_order_id, sizeof(plan.client_order_id), "buy");

    if (quote_size_eur <= 0.0) {
        safe_copy(plan.reason, sizeof(plan.reason), "BUY dry-run bloccato: quote size non valida");
        return plan;
    }

    if (!preview.connected || !preview.allowed) {
        snprintf(
            plan.reason,
            sizeof(plan.reason),
            "BUY dry-run bloccato: preview Coinbase non valida | http %ld | %.120s",
            preview.http_code,
            preview.message
        );
        return plan;
    }

    if (preview.base_size <= 0.0) {
        safe_copy(plan.reason, sizeof(plan.reason), "BUY dry-run bloccato: base size preview non valida");
        return plan;
    }

    plan.allowed = 1;
    snprintf(
        plan.reason,
        sizeof(plan.reason),
        "BUY dry-run pronto | quote %.2f | base %.8f | fee %.2f | avg %.2f",
        quote_size_eur,
        preview.base_size,
        preview.commission_total,
        preview.est_average_filled_price
    );

    return plan;
}

OrderExecutionPlan order_executor_plan_market_sell_dry_run(
    const char *product_id,
    double base_size_btc,
    CoinbaseOrderPreview preview
) {
    OrderExecutionPlan plan = empty_plan(ORDER_EXECUTOR_SIDE_SELL, product_id);

    plan.requested_base_size = base_size_btc;
    plan.preview_total_eur = preview.order_total;
    plan.preview_fee_eur = preview.commission_total;
    plan.preview_base_size = preview.base_size;
    plan.preview_avg_price = preview.est_average_filled_price;
    safe_copy(plan.preview_id, sizeof(plan.preview_id), preview.preview_id);
    make_client_order_id(plan.client_order_id, sizeof(plan.client_order_id), "sell");

    if (base_size_btc <= 0.0) {
        safe_copy(plan.reason, sizeof(plan.reason), "SELL dry-run bloccato: base size non valida");
        return plan;
    }

    if (!preview.connected || !preview.allowed) {
        snprintf(
            plan.reason,
            sizeof(plan.reason),
            "SELL dry-run bloccato: preview Coinbase non valida | http %ld | %.120s",
            preview.http_code,
            preview.message
        );
        return plan;
    }

    if (preview.order_total <= 0.0) {
        safe_copy(plan.reason, sizeof(plan.reason), "SELL dry-run bloccato: totale preview non valido");
        return plan;
    }

    plan.allowed = 1;
    snprintf(
        plan.reason,
        sizeof(plan.reason),
        "SELL dry-run pronto | base %.8f | total %.2f | fee %.2f | avg %.2f",
        base_size_btc,
        preview.order_total,
        preview.commission_total,
        preview.est_average_filled_price
    );

    return plan;
}

static void set_execution_result(
    OrderExecutionResult *result,
    int allowed,
    const char *decision,
    const char *reason,
    const OrderExecutionPlan *plan
) {
    if (result == NULL) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->allowed = allowed;
    result->attempted_real_execution = 0;
    result->sent_to_coinbase = 0;
    result->http_code = 0;

    safe_copy(result->decision, sizeof(result->decision), decision ? decision : "BLOCKED");
    safe_copy(result->reason, sizeof(result->reason), reason ? reason : "");
    safe_copy(
        result->client_order_id,
        sizeof(result->client_order_id),
        plan ? plan->client_order_id : ""
    );
}

static void validate_plan_before_transport(
    OrderExecutionResult *result,
    const OrderExecutionPlan *plan,
    const StrategySettings *settings
) {
    set_execution_result(
        result,
        0,
        "BLOCKED",
        "REAL_EXECUTION_BLOCKED invalid input",
        plan
    );

    if (plan == NULL || settings == NULL) {
        return;
    }

    if (!plan->allowed) {
        set_execution_result(
            result,
            0,
            "BLOCKED",
            "REAL_EXECUTION_BLOCKED execution plan is not allowed",
            plan
        );
        return;
    }

    if (plan->dry_run) {
        set_execution_result(
            result,
            0,
            "BLOCKED",
            "REAL_EXECUTION_BLOCKED plan is dry-run only",
            plan
        );
        return;
    }

    if (plan->client_order_id[0] == '\0') {
        set_execution_result(
            result,
            0,
            "BLOCKED",
            "REAL_EXECUTION_BLOCKED missing client_order_id",
            plan
        );
        return;
    }

    if (plan->product_id[0] == '\0') {
        set_execution_result(
            result,
            0,
            "BLOCKED",
            "REAL_EXECUTION_BLOCKED missing product_id",
            plan
        );
        return;
    }

    if (
        plan->side == ORDER_EXECUTOR_SIDE_BUY &&
        plan->requested_quote_size <= 0.0
    ) {
        set_execution_result(
            result,
            0,
            "BLOCKED",
            "REAL_EXECUTION_BLOCKED BUY quote size is invalid",
            plan
        );
        return;
    }

    if (
        plan->side == ORDER_EXECUTOR_SIDE_SELL &&
        plan->requested_base_size <= 0.0
    ) {
        set_execution_result(
            result,
            0,
            "BLOCKED",
            "REAL_EXECUTION_BLOCKED SELL base size is invalid",
            plan
        );
        return;
    }

    LiveExecutionLockResult live_lock = live_execution_lock_check(plan, settings);

    if (!live_lock.allowed) {
        set_execution_result(
            result,
            0,
            live_lock.decision,
            live_lock.reason,
            plan
        );
        return;
    }

    set_execution_result(
        result,
        1,
        "READY",
        "REAL_EXECUTION_READY transport may run",
        plan
    );
}

#ifndef HELIX_ENABLE_REAL_COINBASE_ORDERS

OrderExecutionResult order_executor_execute_real(
    const OrderExecutionPlan *plan,
    const StrategySettings *settings
) {
    OrderExecutionResult result;

    (void)settings;

    set_execution_result(
        &result,
        0,
        "BLOCKED",
        "REAL_EXECUTION_BLOCKED compile flag HELIX_ENABLE_REAL_COINBASE_ORDERS is not enabled",
        plan
    );

    return result;
}

#else

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

static const char *json_string_value(cJSON *item) {
    if (item && cJSON_IsString(item)) {
        return item->valuestring;
    }

    return NULL;
}

static void result_copy_json_string(
    char *dest,
    size_t dest_size,
    cJSON *item
) {
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

static void build_market_ioc_order_body(
    char *buffer,
    size_t buffer_size,
    const OrderExecutionPlan *plan
) {
    char amount_text[64];
    const char *side_text = plan->side == ORDER_EXECUTOR_SIDE_BUY ? "BUY" : "SELL";
    const char *size_field = plan->side == ORDER_EXECUTOR_SIDE_BUY ? "quote_size" : "base_size";
    double amount = plan->side == ORDER_EXECUTOR_SIDE_BUY ?
        plan->requested_quote_size :
        plan->requested_base_size;

    snprintf(amount_text, sizeof(amount_text), "%.8f", amount);

    if (plan->preview_id[0] != '\0') {
        snprintf(
            buffer,
            buffer_size,
            "{"
            "\"client_order_id\":\"%s\","
            "\"product_id\":\"%s\","
            "\"side\":\"%s\","
            "\"preview_id\":\"%s\","
            "\"order_configuration\":{"
            "\"market_market_ioc\":{"
            "\"%s\":\"%s\","
            "\"rfq_disabled\":true"
            "}"
            "}"
            "}",
            plan->client_order_id,
            plan->product_id,
            side_text,
            plan->preview_id,
            size_field,
            amount_text
        );
    } else {
        snprintf(
            buffer,
            buffer_size,
            "{"
            "\"client_order_id\":\"%s\","
            "\"product_id\":\"%s\","
            "\"side\":\"%s\","
            "\"order_configuration\":{"
            "\"market_market_ioc\":{"
            "\"%s\":\"%s\","
            "\"rfq_disabled\":true"
            "}"
            "}"
            "}",
            plan->client_order_id,
            plan->product_id,
            side_text,
            size_field,
            amount_text
        );
    }
}

static void parse_create_order_response(
    OrderExecutionResult *result,
    const char *response_body
) {
    cJSON *json = cJSON_Parse(response_body);

    if (!json) {
        result->allowed = 0;
        safe_copy(result->decision, sizeof(result->decision), "ERROR");
        safe_copy(result->reason, sizeof(result->reason), "Coinbase create-order JSON non valido");
        return;
    }

    cJSON *success_item = cJSON_GetObjectItem(json, "success");
    int success = cJSON_IsTrue(success_item);

    if (success) {
        cJSON *success_response = cJSON_GetObjectItem(json, "success_response");

        result->allowed = 1;
        result->sent_to_coinbase = 1;
        safe_copy(result->decision, sizeof(result->decision), "SENT");
        safe_copy(result->reason, sizeof(result->reason), "Coinbase create-order accepted");

        result_copy_json_string(
            result->coinbase_order_id,
            sizeof(result->coinbase_order_id),
            cJSON_GetObjectItem(success_response, "order_id")
        );
    } else {
        cJSON *error_response = cJSON_GetObjectItem(json, "error_response");
        const char *error = json_string_value(cJSON_GetObjectItem(error_response, "error"));
        const char *message = json_string_value(cJSON_GetObjectItem(error_response, "message"));
        const char *details = json_string_value(cJSON_GetObjectItem(error_response, "error_details"));

        result->allowed = 0;
        result->sent_to_coinbase = 1;
        safe_copy(result->decision, sizeof(result->decision), "REJECTED");

        snprintf(
            result->reason,
            sizeof(result->reason),
            "Coinbase create-order rejected | %.80s | %.100s | %.60s",
            error ? error : "unknown_error",
            message ? message : "no_message",
            details ? details : "no_details"
        );
    }

    cJSON_Delete(json);
}

OrderExecutionResult order_executor_execute_real(
    const OrderExecutionPlan *plan,
    const StrategySettings *settings
) {
    OrderExecutionResult result;

    validate_plan_before_transport(&result, plan, settings);

    if (!result.allowed) {
        return result;
    }

    result.allowed = 0;
    result.attempted_real_execution = 1;
    result.sent_to_coinbase = 0;

    CoinbaseCredentials credentials = env_load_coinbase_credentials();

    if (!credentials.loaded) {
        safe_copy(result.decision, sizeof(result.decision), "BLOCKED");
        safe_copy(result.reason, sizeof(result.reason), "REAL_EXECUTION_BLOCKED Coinbase credentials missing");
        return result;
    }

    char *jwt_token = coinbase_build_rest_jwt(
        "POST",
        COINBASE_CREATE_ORDER_PATH,
        credentials.api_key,
        credentials.api_secret
    );

    if (!jwt_token) {
        safe_copy(result.decision, sizeof(result.decision), "ERROR");
        safe_copy(result.reason, sizeof(result.reason), "REAL_EXECUTION_ERROR JWT Coinbase create-order non generato");
        return result;
    }

    CURL *curl = curl_easy_init();

    if (!curl) {
        free(jwt_token);
        safe_copy(result.decision, sizeof(result.decision), "ERROR");
        safe_copy(result.reason, sizeof(result.reason), "REAL_EXECUTION_ERROR curl non inizializzato");
        return result;
    }

    HttpResponse response;

    if (!http_response_init(&response)) {
        curl_easy_cleanup(curl);
        free(jwt_token);
        safe_copy(result.decision, sizeof(result.decision), "ERROR");
        safe_copy(result.reason, sizeof(result.reason), "REAL_EXECUTION_ERROR memoria risposta non allocata");
        return result;
    }

    char request_body[2048];
    char auth_header[4096];
    struct curl_slist *headers = NULL;

    build_market_ioc_order_body(request_body, sizeof(request_body), plan);
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", jwt_token);

    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, COINBASE_CREATE_ORDER_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Helix/0.1");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode curl_result = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    result.http_code = http_code;

    if (curl_result != CURLE_OK) {
        result.allowed = 0;
        result.sent_to_coinbase = 0;
        safe_copy(result.decision, sizeof(result.decision), "ERROR");
        snprintf(
            result.reason,
            sizeof(result.reason),
            "REAL_EXECUTION_ERROR curl create-order failed: %s",
            curl_easy_strerror(curl_result)
        );
    } else if (http_code < 200 || http_code >= 300) {
        result.allowed = 0;
        result.sent_to_coinbase = 1;
        safe_copy(result.decision, sizeof(result.decision), "HTTP_ERROR");
        snprintf(
            result.reason,
            sizeof(result.reason),
            "Coinbase create-order HTTP %ld | %.120s",
            http_code,
            response.memory ? response.memory : ""
        );
    } else {
        parse_create_order_response(&result, response.memory);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    http_response_free(&response);
    free(jwt_token);

    return result;
}

#endif

OrderExecutionResult order_executor_execute_real_blocked(
    const OrderExecutionPlan *plan,
    const StrategySettings *settings
) {
    return order_executor_execute_real(plan, settings);
}
