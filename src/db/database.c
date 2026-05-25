#include "database.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

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
        "last_trade TEXT NOT NULL,"
        "used_slots INTEGER NOT NULL,"
        "max_slots INTEGER NOT NULL"
        ");";

    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "Errore SQL bot_state: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return 0;
    }

    const char *trades_sql =
        "CREATE TABLE IF NOT EXISTS trades ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "type TEXT NOT NULL,"
        "price REAL NOT NULL,"
        "eur_amount REAL NOT NULL,"
        "btc_amount REAL NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    if (sqlite3_exec(db, trades_sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "Errore SQL trades: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return 0;
    }

    const char *settings_sql =
        "CREATE TABLE IF NOT EXISTS settings ("
        "key TEXT PRIMARY KEY,"
        "value TEXT NOT NULL"
        ");";

    if (sqlite3_exec(db, settings_sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "Errore SQL settings: %s\n", err);
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
        "(id, running, mode, eur_balance, btc_balance, current_price, "
        "last_buy_price, avg_buy_price, last_trade, used_slots, max_slots) "
        "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "running=excluded.running,"
        "mode=excluded.mode,"
        "eur_balance=excluded.eur_balance,"
        "btc_balance=excluded.btc_balance,"
        "current_price=excluded.current_price,"
        "last_buy_price=excluded.last_buy_price,"
        "avg_buy_price=excluded.avg_buy_price,"
        "last_trade=excluded.last_trade,"
        "used_slots=excluded.used_slots,"
        "max_slots=excluded.max_slots;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_int(stmt, 1, state->running ? 1 : 0);
    sqlite3_bind_int(stmt, 2, state->mode);
    sqlite3_bind_double(stmt, 3, state->eur_balance);
    sqlite3_bind_double(stmt, 4, state->btc_balance);
    sqlite3_bind_double(stmt, 5, state->current_price);
    sqlite3_bind_double(stmt, 6, state->last_buy_price);
    sqlite3_bind_double(stmt, 7, state->avg_buy_price);
    sqlite3_bind_text(stmt, 8, state->last_trade, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 9, state->used_slots);
    sqlite3_bind_int(stmt, 10, state->max_slots);

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
        "SELECT running, mode, eur_balance, btc_balance, current_price, "
        "last_buy_price, avg_buy_price, last_trade, used_slots, max_slots "
        "FROM bot_state WHERE id = 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    int found = 0;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        state->running = sqlite3_column_int(stmt, 0) == 1;
        state->mode = sqlite3_column_int(stmt, 1);
        state->eur_balance = sqlite3_column_double(stmt, 2);
        state->btc_balance = sqlite3_column_double(stmt, 3);
        state->current_price = sqlite3_column_double(stmt, 4);
        state->last_buy_price = sqlite3_column_double(stmt, 5);
        state->avg_buy_price = sqlite3_column_double(stmt, 6);

        const unsigned char *trade = sqlite3_column_text(stmt, 7);

        if (trade) {
            snprintf(
                state->last_trade,
                sizeof(state->last_trade),
                "%s",
                (const char *)trade
            );
        }

        state->used_slots = sqlite3_column_int(stmt, 8);
        state->max_slots = sqlite3_column_int(stmt, 9);

        found = 1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return found;
}

int db_log_trade(
    const char *type,
    double price,
    double eur_amount,
    double btc_amount
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "INSERT INTO trades "
        "(type, price, eur_amount, btc_amount) "
        "VALUES (?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, price);
    sqlite3_bind_double(stmt, 3, eur_amount);
    sqlite3_bind_double(stmt, 4, btc_amount);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return ok;
}

int db_get_recent_trades(
    TradeRecord *trades,
    int max_trades
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (trades == NULL || max_trades <= 0) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "SELECT id, type, price, eur_amount, btc_amount, created_at "
        "FROM trades "
        "ORDER BY id DESC "
        "LIMIT ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_int(stmt, 1, max_trades);

    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_trades) {
        trades[count].id = sqlite3_column_int(stmt, 0);

        const unsigned char *type = sqlite3_column_text(stmt, 1);
        const unsigned char *created_at = sqlite3_column_text(stmt, 5);

        snprintf(
            trades[count].type,
            sizeof(trades[count].type),
            "%s",
            type ? (const char *)type : ""
        );

        trades[count].price = sqlite3_column_double(stmt, 2);
        trades[count].eur_amount = sqlite3_column_double(stmt, 3);
        trades[count].btc_amount = sqlite3_column_double(stmt, 4);

        snprintf(
            trades[count].created_at,
            sizeof(trades[count].created_at),
            "%s",
            created_at ? (const char *)created_at : ""
        );

        count++;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return count;
}

int db_set_setting(
    const char *key,
    const char *value
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "INSERT INTO settings (key, value) "
        "VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET "
        "value=excluded.value;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value, -1, SQLITE_TRANSIENT);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return ok;
}

int db_get_setting(
    const char *key,
    char *buffer,
    int buffer_size
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (buffer == NULL || buffer_size <= 0) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "SELECT value FROM settings "
        "WHERE key = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);

    int found = 0;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *value = sqlite3_column_text(stmt, 0);

        if (value) {
            snprintf(
                buffer,
                buffer_size,
                "%s",
                (const char *)value
            );

            found = 1;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return found;
}
