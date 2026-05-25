CC=gcc
CFLAGS=-Wall -Wextra -g `pkg-config --cflags gtk4`
LIBS=`pkg-config --libs gtk4` -lsqlite3 -lcurl -lcjson

SRC=src/main.c \
	src/ui/window.c \
	src/engine/bot_state.c \
	src/engine/engine.c \
	src/engine/settings.c \
	src/engine/wallet.c \
	src/db/database.c \
	src/market/market_data.c \
	src/exchange/coinbase_client.c

OUT=helix

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LIBS)

run: all
	GTK_A11Y=none GTK_IM_MODULE= ./$(OUT)

clean:
	rm -f $(OUT)
