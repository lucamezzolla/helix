CC=gcc
CFLAGS=-Wall -Wextra -g `pkg-config --cflags gtk4`
LIBS=`pkg-config --libs gtk4` -lsqlite3

SRC=src/main.c src/ui/window.c src/engine/bot_state.c src/engine/engine.c src/engine/wallet.c src/db/database.c
OUT=helix

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LIBS)

run: all
	./$(OUT)

clean:
	rm -f $(OUT)
