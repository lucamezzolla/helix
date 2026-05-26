CC=gcc
CFLAGS=-Wall -Wextra -g `pkg-config --cflags gtk4`
LIBS=`pkg-config --libs gtk4` -lsqlite3 -lcurl -lcjson -ljwt -lm

SRC=src/main.c \
	src/ui/window.c \
	src/engine/bot_state.c \
	src/engine/engine.c \
	src/engine/trade_preview.c \
	src/engine/exchange_safety.c \
	src/engine/runtime_safety.c \
	src/engine/liquidity_reserve.c \
	src/engine/emergency_stop.c \
	src/engine/reconciliation.c \
	src/engine/volatility_protection.c \
	src/engine/order_state_recovery.c \
	src/engine/anti_duplicate_order.c \
	src/engine/order_journal.c \
	src/engine/final_live_gate.c \
	src/engine/live_readiness.c \
	src/engine/api_health.c \
	src/engine/operational_limits.c \
	src/engine/risk_guard.c \
	src/engine/post_order_reconciliation.c \
	src/engine/prelive_validation.c \
	src/engine/live_execution_lock.c \
	src/engine/settings.c \
	src/engine/wallet.c \
	src/db/database.c \
	src/market/market_data.c \
	src/exchange/coinbase_client.c \
	src/exchange/coinbase_auth.c \
	src/exchange/order_preview.c \
	src/exchange/order_executor.c \
	src/config/env_loader.c \
	src/wallet/wallet_info.c

OUT=helix

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LIBS)

run: all
	GTK_A11Y=none ./$(OUT)

clean:
	rm -f $(OUT)
