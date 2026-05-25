#ifndef HELIX_ORDER_PREVIEW_H
#define HELIX_ORDER_PREVIEW_H

#define HELIX_ORDER_PREVIEW_MESSAGE_SIZE 256
#define HELIX_ORDER_PREVIEW_ID_SIZE 128

typedef enum {
    ORDER_PREVIEW_SIDE_BUY,
    ORDER_PREVIEW_SIDE_SELL
} OrderPreviewSide;

typedef struct {
    int connected;
    int allowed;
    long http_code;

    OrderPreviewSide side;

    double order_total;
    double commission_total;
    double quote_size;
    double base_size;
    double best_bid;
    double best_ask;
    double est_average_filled_price;
    double slippage;

    char preview_id[HELIX_ORDER_PREVIEW_ID_SIZE];
    char message[HELIX_ORDER_PREVIEW_MESSAGE_SIZE];
} CoinbaseOrderPreview;

CoinbaseOrderPreview coinbase_order_preview_market_buy_eur(
    const char *product_id,
    double eur_amount
);

CoinbaseOrderPreview coinbase_order_preview_market_sell_btc(
    const char *product_id,
    double btc_amount
);

#endif
