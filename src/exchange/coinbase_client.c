#include "coinbase_client.h"

#include <curl/curl.h>
#include <cjson/cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COINBASE_SPOT_URL \
    "https://api.coinbase.com/v2/prices/BTC-EUR/spot"

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

    memcpy(
        &(response->memory[response->size]),
        contents,
        real_size
    );

    response->size += real_size;
    response->memory[response->size] = '\0';

    return real_size;
}

double coinbase_get_btc_eur_spot_price(void) {
    CURL *curl;
    CURLcode result;

    HttpResponse response;

    response.memory = malloc(1);
    response.size = 0;

    curl = curl_easy_init();

    if (!curl) {
        free(response.memory);
        return -1.0;
    }

    curl_easy_setopt(curl, CURLOPT_URL, COINBASE_SPOT_URL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Helix/0.1");

    result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        curl_easy_cleanup(curl);
        free(response.memory);

        return -1.0;
    }

    cJSON *json = cJSON_Parse(response.memory);

    if (!json) {
        curl_easy_cleanup(curl);
        free(response.memory);

        return -1.0;
    }

    cJSON *data = cJSON_GetObjectItem(json, "data");

    if (!data) {
        cJSON_Delete(json);
        curl_easy_cleanup(curl);
        free(response.memory);

        return -1.0;
    }

    cJSON *amount = cJSON_GetObjectItem(data, "amount");

    if (!amount || !cJSON_IsString(amount)) {
        cJSON_Delete(json);
        curl_easy_cleanup(curl);
        free(response.memory);

        return -1.0;
    }

    double price = atof(amount->valuestring);

    cJSON_Delete(json);
    curl_easy_cleanup(curl);
    free(response.memory);

    return price;
}
