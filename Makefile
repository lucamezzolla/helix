CC=gcc
CFLAGS=-Wall -Wextra -g `pkg-config --cflags gtk4`
LIBS=`pkg-config --libs gtk4` -lsqlite3 -lcurl -lcjson -ljwt

SRC=src/main.c \
	src/ui/window.c \
	src/engine/bot_state.c \
	src/engine/engine.c \
	src/engine/trade_preview.c \
	src/engine/settings.c \
	src/engine/wallet.c \
	src/db/database.c \
	src/market/market_data.c \
	src/exchange/coinbase_client.c \
	src/exchange/coinbase_auth.c \
	src/config/env_loader.c \
	src/wallet/wallet_info.c

OUT=helix

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LIBS)

run: all
	GTK_A11Y=none ./$(OUT)

clean:
	rm -f $(OUT)
