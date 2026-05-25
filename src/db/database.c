#include "database.h"
#include <sqlite3.h>
#include <stdio.h>

#define DB_PATH "data/helix.db"

int db_init(void) {
    sqlite3 *db;
    char *err = NULL;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        fprintf(stderr, "Errore apertura DB\n");
        return 0;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS bot_state ("
        "id INTEGER PRIMARY KEY CHECK(id = 1),"
        "running INTEGER NOT NULL,"
        "mode INTEGER NOT NULL,"
        "eur_balance REAL NOT NULL,"
        "btc_balance REAL NOT NULL,"
        "current_price REAL NOT NULL,"
        "last_buy_price REAL NOT NULL,"
        "avg_buy_price REAL NOT NULL,"
        "used_slots INTEGER NOT NULL,"
        "max_slots INTEGER NOT NULL"
        ");";

    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "Errore SQL: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return 0;
    }

    sqlite3_close(db);
    return 1;
}

int db_save_state(const BotState *state) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "INSERT INTO bot_state "
        "(id, running, mode, eur_balance, btc_balance, current_price, last_buy_price, avg_buy_price, used_slots, max_slots) "
        "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "running=excluded.running,"
        "mode=excluded.mode,"
        "eur_balance=excluded.eur_balance,"
        "btc_balance=excluded.btc_balance,"
        "current_price=excluded.current_price,"
        "last_buy_price=excluded.last_buy_price,"
        "avg_buy_price=excluded.avg_buy_price,"
        "used_slots=excluded.used_slots,"
        "max_slots=excluded.max_slots;";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, state->running ? 1 : 0);
    sqlite3_bind_int(stmt, 2, state->mode);
    sqlite3_bind_double(stmt, 3, state->eur_balance);
    sqlite3_bind_double(stmt, 4, state->btc_balance);
    sqlite3_bind_double(stmt, 5, state->current_price);
    sqlite3_bind_double(stmt, 6, state->last_buy_price);
    sqlite3_bind_double(stmt, 7, state->avg_buy_price);
    sqlite3_bind_int(stmt, 8, state->used_slots);
    sqlite3_bind_int(stmt, 9, state->max_slots);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return ok;
}

int db_load_state(BotState *state) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "SELECT running, mode, eur_balance, btc_balance, current_price, last_buy_price, avg_buy_price, used_slots, max_slots "
        "FROM bot_state WHERE id = 1;";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    int found = 0;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        state->running = sqlite3_column_int(stmt, 0) == 1;
        state->mode = sqlite3_column_int(stmt, 1);
        state->eur_balance = sqlite3_column_double(stmt, 2);
        state->btc_balance = sqlite3_column_double(stmt, 3);
        state->current_price = sqlite3_column_double(stmt, 4);
        state->last_buy_price = sqlite3_column_double(stmt, 5);
        state->avg_buy_price = sqlite3_column_double(stmt, 6);
        state->used_slots = sqlite3_column_int(stmt, 7);
        state->max_slots = sqlite3_column_int(stmt, 8);
        found = 1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return found;
}
