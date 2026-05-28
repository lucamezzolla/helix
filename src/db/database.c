#include "database.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#define DB_PATH "data/helix.db"
#define ENGINE_AUDIT_MAX_RECORDS 5000

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

    /*
     * Schema compatibile con:
     * - trade simulati Helix
     * - import storico reale da Excel/Coinbase
     *
     * Le colonne extra sono nullable, quindi db_log_trade continua a funzionare.
     */
    const char *trades_sql =
        "CREATE TABLE IF NOT EXISTS trades ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "type TEXT NOT NULL,"
        "price REAL,"
        "eur_amount REAL NOT NULL,"
        "btc_amount REAL NOT NULL,"
        "fee_amount REAL DEFAULT 0,"
        "net_total REAL,"
        "reference TEXT,"
        "source TEXT DEFAULT 'HELIX',"
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP"
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

    const char *order_state_sql =
        "CREATE TABLE IF NOT EXISTS order_state ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "client_order_id TEXT NOT NULL UNIQUE,"
        "side TEXT NOT NULL,"
        "status TEXT NOT NULL,"
        "product_id TEXT NOT NULL,"
        "dry_run INTEGER NOT NULL DEFAULT 1,"
        "requested_quote_size REAL DEFAULT 0,"
        "requested_base_size REAL DEFAULT 0,"
        "preview_total_eur REAL DEFAULT 0,"
        "preview_fee_eur REAL DEFAULT 0,"
        "preview_base_size REAL DEFAULT 0,"
        "preview_avg_price REAL DEFAULT 0,"
        "reason TEXT DEFAULT '',"
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP,"
        "updated_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ");";

    if (sqlite3_exec(db, order_state_sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "Errore SQL order_state: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return 0;
    }

    const char *audit_sql =
        "CREATE TABLE IF NOT EXISTS engine_audit ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "event_type TEXT NOT NULL,"
        "decision TEXT NOT NULL,"
        "reason TEXT NOT NULL,"
        "price REAL DEFAULT 0,"
        "btc_amount REAL DEFAULT 0,"
        "eur_amount REAL DEFAULT 0,"
        "estimated_fee REAL DEFAULT 0,"
        "net_profit REAL DEFAULT 0,"
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ");";

    if (sqlite3_exec(db, audit_sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "Errore SQL engine_audit: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return 0;
    }


    const char *position_slots_sql =
        "CREATE TABLE IF NOT EXISTS position_slots ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "buy_order_id TEXT NOT NULL UNIQUE,"
        "buy_client_order_id TEXT NOT NULL,"
        "base_size_btc REAL NOT NULL,"
        "cost_eur REAL NOT NULL,"
        "buy_fee_eur REAL DEFAULT 0,"
        "avg_buy_price REAL DEFAULT 0,"
        "status TEXT NOT NULL DEFAULT 'OPEN',"
        "opened_at TEXT DEFAULT CURRENT_TIMESTAMP,"
        "closed_at TEXT DEFAULT '',"
        "sell_order_id TEXT DEFAULT '',"
        "sell_net_eur REAL DEFAULT 0,"
        "realized_profit_eur REAL DEFAULT 0"
        ");";

    if (sqlite3_exec(db, position_slots_sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "Errore SQL position_slots: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return 0;
    }

    sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS idx_position_slots_status ON position_slots(status);", NULL, NULL, NULL);

    const char *paper_position_slots_sql =
        "CREATE TABLE IF NOT EXISTS paper_position_slots ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "label TEXT NOT NULL UNIQUE,"
        "base_size_btc REAL NOT NULL,"
        "cost_eur REAL NOT NULL,"
        "buy_fee_eur REAL DEFAULT 0,"
        "avg_buy_price REAL DEFAULT 0,"
        "status TEXT NOT NULL DEFAULT 'OPEN',"
        "opened_at TEXT DEFAULT CURRENT_TIMESTAMP,"
        "closed_at TEXT DEFAULT '',"
        "sell_net_eur REAL DEFAULT 0,"
        "realized_profit_eur REAL DEFAULT 0"
        ");";

    if (sqlite3_exec(db, paper_position_slots_sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "Errore SQL paper_position_slots: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return 0;
    }

    sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS idx_paper_position_slots_status ON paper_position_slots(status);", NULL, NULL, NULL);

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

        const unsigned char *trade =
            sqlite3_column_text(stmt, 7);

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
        "(type, price, eur_amount, btc_amount, source) "
        "VALUES (?, ?, ?, ?, 'HELIX');";

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
        "SELECT id, type, COALESCE(price, 0), eur_amount, btc_amount, created_at "
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


int db_log_engine_audit(
    const char *event_type,
    const char *decision,
    const char *reason,
    double price,
    double btc_amount,
    double eur_amount,
    double estimated_fee,
    double net_profit
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "INSERT INTO engine_audit "
        "(event_type, decision, reason, price, btc_amount, eur_amount, estimated_fee, net_profit) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, event_type ? event_type : "UNKNOWN", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, decision ? decision : "UNKNOWN", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, reason ? reason : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, price);
    sqlite3_bind_double(stmt, 5, btc_amount);
    sqlite3_bind_double(stmt, 6, eur_amount);
    sqlite3_bind_double(stmt, 7, estimated_fee);
    sqlite3_bind_double(stmt, 8, net_profit);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (ok) {
        db_prune_engine_audits_max_records(ENGINE_AUDIT_MAX_RECORDS);
    }

    return ok;
}

int db_get_recent_engine_audits(
    EngineAuditRecord *records,
    int max_records
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (records == NULL || max_records <= 0) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "SELECT id, event_type, decision, reason, price, btc_amount, eur_amount, "
        "estimated_fee, net_profit, created_at "
        "FROM engine_audit "
        "ORDER BY id DESC "
        "LIMIT ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_int(stmt, 1, max_records);

    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_records) {
        const unsigned char *event_type = sqlite3_column_text(stmt, 1);
        const unsigned char *decision = sqlite3_column_text(stmt, 2);
        const unsigned char *reason = sqlite3_column_text(stmt, 3);
        const unsigned char *created_at = sqlite3_column_text(stmt, 9);

        records[count].id = sqlite3_column_int(stmt, 0);

        snprintf(records[count].event_type, sizeof(records[count].event_type), "%s", event_type ? (const char *)event_type : "");
        snprintf(records[count].decision, sizeof(records[count].decision), "%s", decision ? (const char *)decision : "");
        snprintf(records[count].reason, sizeof(records[count].reason), "%s", reason ? (const char *)reason : "");

        records[count].price = sqlite3_column_double(stmt, 4);
        records[count].btc_amount = sqlite3_column_double(stmt, 5);
        records[count].eur_amount = sqlite3_column_double(stmt, 6);
        records[count].estimated_fee = sqlite3_column_double(stmt, 7);
        records[count].net_profit = sqlite3_column_double(stmt, 8);

        snprintf(records[count].created_at, sizeof(records[count].created_at), "%s", created_at ? (const char *)created_at : "");

        count++;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return count;
}


static int db_count_engine_audit_matches(const char *where_clause, int days) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char sql[512];
    int count = 0;

    if (where_clause == NULL || days <= 0) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    snprintf(
        sql,
        sizeof(sql),
        "SELECT COUNT(*) FROM engine_audit "
        "WHERE created_at >= datetime('now', '-' || ? || ' days') "
        "AND (%s);",
        where_clause
    );

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, days);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return count;
}

int db_get_engine_audit_summary_last_days(
    EngineAuditSummary *summary,
    int days
) {
    if (summary == NULL || days <= 0) {
        return 0;
    }

    memset(summary, 0, sizeof(*summary));

    summary->total_last_days =
        db_count_engine_audit_matches("1 = 1", days);

    summary->reconciliation_blocks =
        db_count_engine_audit_matches("event_type = 'RECONCILIATION' AND decision LIKE '%BLOCK%'", days);

    summary->order_recovery_blocks =
        db_count_engine_audit_matches("event_type = 'ORDER_RECOVERY' AND decision LIKE '%BLOCK%'", days);

    summary->volatility_blocks =
        db_count_engine_audit_matches("event_type = 'VOLATILITY_PROTECTION' AND decision LIKE '%BLOCK%'", days);

    summary->operational_limits_blocks =
        db_count_engine_audit_matches("event_type = 'OPERATIONAL_LIMITS' AND decision LIKE '%BLOCK%'", days);

    summary->risk_guard_blocks =
        db_count_engine_audit_matches("event_type = 'RISK_GUARD' AND decision LIKE '%BLOCK%'", days);

    summary->final_live_gate_blocks =
        db_count_engine_audit_matches("event_type = 'FINAL_LIVE_GATE' AND decision LIKE '%BLOCK%'", days);

    summary->anti_duplicate_blocks =
        db_count_engine_audit_matches("event_type = 'ANTI_DUPLICATE_ORDER' AND decision LIKE '%BLOCK%'", days);

    summary->prelive_validation_blocks =
        db_count_engine_audit_matches("event_type = 'PRELIVE_VALIDATION' AND decision LIKE '%BLOCK%'", days);

    summary->real_executor_blocks =
        db_count_engine_audit_matches("event_type = 'REAL_EXECUTOR' AND decision LIKE '%BLOCK%'", days);

    summary->post_order_reconciliation_blocks =
        db_count_engine_audit_matches("event_type = 'POST_ORDER_RECONCILIATION' AND decision LIKE '%BLOCK%'", days);

    summary->emergency_stop_events =
        db_count_engine_audit_matches("event_type = 'EMERGENCY_STOP'", days);

    summary->live_trading_arm_events =
        db_count_engine_audit_matches("event_type = 'LIVE_TRADING_ARM'", days);

    return 1;
}


int db_prune_engine_audits(int retention_days) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (retention_days <= 0) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "DELETE FROM engine_audit "
        "WHERE created_at < datetime('now', '-' || ? || ' days');";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_int(stmt, 1, retention_days);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return ok;
}



int db_prune_engine_audits_max_records(int max_records) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (max_records <= 0) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "DELETE FROM engine_audit "
        "WHERE id IN ("
        "    SELECT id FROM engine_audit "
        "    ORDER BY id DESC "
        "    LIMIT -1 OFFSET ?"
        ");";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_int(stmt, 1, max_records);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return ok;
}


int db_upsert_order_state(
    const char *client_order_id,
    const char *side,
    const char *status,
    const char *product_id,
    int dry_run,
    double requested_quote_size,
    double requested_base_size,
    double preview_total_eur,
    double preview_fee_eur,
    double preview_base_size,
    double preview_avg_price,
    const char *reason
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (client_order_id == NULL || client_order_id[0] == '\0') {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "INSERT INTO order_state "
        "(client_order_id, side, status, product_id, dry_run, "
        "requested_quote_size, requested_base_size, preview_total_eur, "
        "preview_fee_eur, preview_base_size, preview_avg_price, reason) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(client_order_id) DO UPDATE SET "
        "status=excluded.status,"
        "preview_total_eur=excluded.preview_total_eur,"
        "preview_fee_eur=excluded.preview_fee_eur,"
        "preview_base_size=excluded.preview_base_size,"
        "preview_avg_price=excluded.preview_avg_price,"
        "reason=excluded.reason,"
        "updated_at=CURRENT_TIMESTAMP;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, client_order_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, side ? side : "UNKNOWN", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, status ? status : "UNKNOWN", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, product_id ? product_id : "BTC-EUR", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, dry_run ? 1 : 0);
    sqlite3_bind_double(stmt, 6, requested_quote_size);
    sqlite3_bind_double(stmt, 7, requested_base_size);
    sqlite3_bind_double(stmt, 8, preview_total_eur);
    sqlite3_bind_double(stmt, 9, preview_fee_eur);
    sqlite3_bind_double(stmt, 10, preview_base_size);
    sqlite3_bind_double(stmt, 11, preview_avg_price);
    sqlite3_bind_text(stmt, 12, reason ? reason : "", -1, SQLITE_TRANSIENT);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return ok;
}

int db_get_active_order_state(OrderStateRecord *record) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (record == NULL) {
        return 0;
    }

    memset(record, 0, sizeof(*record));

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "SELECT id, client_order_id, side, status, product_id, dry_run, "
        "requested_quote_size, requested_base_size, preview_total_eur, "
        "preview_fee_eur, preview_base_size, preview_avg_price, reason, "
        "created_at, updated_at "
        "FROM order_state "
        "WHERE status IN ('PLANNED', 'SUBMITTED', 'PENDING', 'PARTIAL_FILL') "
        "ORDER BY id DESC LIMIT 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    int found = 0;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *client_order_id = sqlite3_column_text(stmt, 1);
        const unsigned char *side = sqlite3_column_text(stmt, 2);
        const unsigned char *status = sqlite3_column_text(stmt, 3);
        const unsigned char *product_id = sqlite3_column_text(stmt, 4);
        const unsigned char *reason = sqlite3_column_text(stmt, 12);
        const unsigned char *created_at = sqlite3_column_text(stmt, 13);
        const unsigned char *updated_at = sqlite3_column_text(stmt, 14);

        record->id = sqlite3_column_int(stmt, 0);
        snprintf(record->client_order_id, sizeof(record->client_order_id), "%s", client_order_id ? (const char *)client_order_id : "");
        snprintf(record->side, sizeof(record->side), "%s", side ? (const char *)side : "");
        snprintf(record->status, sizeof(record->status), "%s", status ? (const char *)status : "");
        snprintf(record->product_id, sizeof(record->product_id), "%s", product_id ? (const char *)product_id : "");
        record->dry_run = sqlite3_column_int(stmt, 5);
        record->requested_quote_size = sqlite3_column_double(stmt, 6);
        record->requested_base_size = sqlite3_column_double(stmt, 7);
        record->preview_total_eur = sqlite3_column_double(stmt, 8);
        record->preview_fee_eur = sqlite3_column_double(stmt, 9);
        record->preview_base_size = sqlite3_column_double(stmt, 10);
        record->preview_avg_price = sqlite3_column_double(stmt, 11);
        snprintf(record->reason, sizeof(record->reason), "%s", reason ? (const char *)reason : "");
        snprintf(record->created_at, sizeof(record->created_at), "%s", created_at ? (const char *)created_at : "");
        snprintf(record->updated_at, sizeof(record->updated_at), "%s", updated_at ? (const char *)updated_at : "");

        found = 1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return found;
}

int db_mark_dry_run_orders_recovered(void) {
    sqlite3 *db;
    char *err = NULL;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "UPDATE order_state "
        "SET status='DRY_RUN_RECOVERED', updated_at=CURRENT_TIMESTAMP "
        "WHERE dry_run=1 AND status IN ('PLANNED', 'SUBMITTED', 'PENDING', 'PARTIAL_FILL');";

    int ok = sqlite3_exec(db, sql, NULL, NULL, &err) == SQLITE_OK;

    if (!ok && err) {
        fprintf(stderr, "Errore SQL db_mark_dry_run_orders_recovered: %s\n", err);
        sqlite3_free(err);
    }

    sqlite3_close(db);

    return ok;
}


int db_create_position_slot_from_buy(
    const char *buy_order_id,
    const char *buy_client_order_id,
    double base_size_btc,
    double cost_eur,
    double buy_fee_eur,
    double avg_buy_price
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (buy_order_id == NULL || buy_order_id[0] == '\0') {
        return 0;
    }

    if (buy_client_order_id == NULL || buy_client_order_id[0] == '\0') {
        return 0;
    }

    if (base_size_btc <= 0.0 || cost_eur <= 0.0) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "INSERT OR IGNORE INTO position_slots "
        "(buy_order_id, buy_client_order_id, base_size_btc, cost_eur, buy_fee_eur, avg_buy_price, status) "
        "VALUES (?, ?, ?, ?, ?, ?, 'OPEN');";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, buy_order_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, buy_client_order_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, base_size_btc);
    sqlite3_bind_double(stmt, 4, cost_eur);
    sqlite3_bind_double(stmt, 5, buy_fee_eur);
    sqlite3_bind_double(stmt, 6, avg_buy_price);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return ok;
}

int db_rebuild_position_slots_from_real_buys(void) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int created = 0;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "INSERT OR IGNORE INTO position_slots "
        "(buy_order_id, buy_client_order_id, base_size_btc, cost_eur, "
        " buy_fee_eur, avg_buy_price, status, opened_at) "
        "SELECT "
        "  coinbase_order_id, "
        "  client_order_id, "
        "  preview_base_size, "
        "  CASE WHEN requested_quote_size > 0 THEN requested_quote_size ELSE preview_total_eur END, "
        "  preview_fee_eur, "
        "  preview_avg_price, "
        "  'OPEN', "
        "  created_at "
        "FROM order_journal "
        "WHERE side = 'BUY' "
        "  AND dry_run = 0 "
        "  AND status = 'REAL_SENT' "
        "  AND phase = 'REAL_EXECUTION' "
        "  AND decision = 'SENT' "
        "  AND coinbase_order_id IS NOT NULL "
        "  AND coinbase_order_id <> '' "
        "  AND preview_base_size > 0 "
        "  AND (CASE WHEN requested_quote_size > 0 THEN requested_quote_size ELSE preview_total_eur END) > 0;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        created = sqlite3_changes(db);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return created;
}

int db_get_open_position_slots(
    PositionSlotRecord *records,
    int max_records
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int count = 0;

    if (records == NULL || max_records <= 0) {
        return 0;
    }

    memset(records, 0, sizeof(PositionSlotRecord) * (size_t)max_records);

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "SELECT id, buy_order_id, buy_client_order_id, base_size_btc, cost_eur, "
        "       buy_fee_eur, avg_buy_price, status, opened_at, closed_at, "
        "       sell_order_id, sell_net_eur, realized_profit_eur "
        "FROM position_slots "
        "WHERE status = 'OPEN' "
        "ORDER BY opened_at ASC, id ASC "
        "LIMIT ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_int(stmt, 1, max_records);

    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_records) {
        PositionSlotRecord *record = &records[count];
        const unsigned char *text;

        record->id = sqlite3_column_int(stmt, 0);

        text = sqlite3_column_text(stmt, 1);
        snprintf(record->buy_order_id, sizeof(record->buy_order_id), "%s", text ? (const char *)text : "");
        text = sqlite3_column_text(stmt, 2);
        snprintf(record->buy_client_order_id, sizeof(record->buy_client_order_id), "%s", text ? (const char *)text : "");

        record->base_size_btc = sqlite3_column_double(stmt, 3);
        record->cost_eur = sqlite3_column_double(stmt, 4);
        record->buy_fee_eur = sqlite3_column_double(stmt, 5);
        record->avg_buy_price = sqlite3_column_double(stmt, 6);

        text = sqlite3_column_text(stmt, 7);
        snprintf(record->status, sizeof(record->status), "%s", text ? (const char *)text : "");
        text = sqlite3_column_text(stmt, 8);
        snprintf(record->opened_at, sizeof(record->opened_at), "%s", text ? (const char *)text : "");
        text = sqlite3_column_text(stmt, 9);
        snprintf(record->closed_at, sizeof(record->closed_at), "%s", text ? (const char *)text : "");
        text = sqlite3_column_text(stmt, 10);
        snprintf(record->sell_order_id, sizeof(record->sell_order_id), "%s", text ? (const char *)text : "");

        record->sell_net_eur = sqlite3_column_double(stmt, 11);
        record->realized_profit_eur = sqlite3_column_double(stmt, 12);

        count++;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return count;
}


int db_find_open_position_slot_by_base_size(
    double base_size_btc,
    PositionSlotRecord *record
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int found = 0;

    if (record == NULL || base_size_btc <= 0.0) {
        return 0;
    }

    memset(record, 0, sizeof(*record));

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    /*
     * We use a small tolerance because Coinbase preview/execution quantities
     * may be rounded at 8 decimal places.
     */
    const char *sql =
        "SELECT id, buy_order_id, buy_client_order_id, base_size_btc, cost_eur, "
        "       buy_fee_eur, avg_buy_price, status, opened_at, closed_at, "
        "       sell_order_id, sell_net_eur, realized_profit_eur "
        "FROM position_slots "
        "WHERE status = 'OPEN' "
        "  AND ABS(base_size_btc - ?) <= 0.00000001 "
        "ORDER BY opened_at ASC, id ASC "
        "LIMIT 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_double(stmt, 1, base_size_btc);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *text;

        record->id = sqlite3_column_int(stmt, 0);

        text = sqlite3_column_text(stmt, 1);
        snprintf(record->buy_order_id, sizeof(record->buy_order_id), "%s", text ? (const char *)text : "");

        text = sqlite3_column_text(stmt, 2);
        snprintf(record->buy_client_order_id, sizeof(record->buy_client_order_id), "%s", text ? (const char *)text : "");

        record->base_size_btc = sqlite3_column_double(stmt, 3);
        record->cost_eur = sqlite3_column_double(stmt, 4);
        record->buy_fee_eur = sqlite3_column_double(stmt, 5);
        record->avg_buy_price = sqlite3_column_double(stmt, 6);

        text = sqlite3_column_text(stmt, 7);
        snprintf(record->status, sizeof(record->status), "%s", text ? (const char *)text : "");

        text = sqlite3_column_text(stmt, 8);
        snprintf(record->opened_at, sizeof(record->opened_at), "%s", text ? (const char *)text : "");

        text = sqlite3_column_text(stmt, 9);
        snprintf(record->closed_at, sizeof(record->closed_at), "%s", text ? (const char *)text : "");

        text = sqlite3_column_text(stmt, 10);
        snprintf(record->sell_order_id, sizeof(record->sell_order_id), "%s", text ? (const char *)text : "");

        record->sell_net_eur = sqlite3_column_double(stmt, 11);
        record->realized_profit_eur = sqlite3_column_double(stmt, 12);

        found = 1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return found;
}

int db_close_position_slot(
    int slot_id,
    const char *sell_order_id,
    double sell_net_eur,
    double realized_profit_eur
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (slot_id <= 0 || sell_order_id == NULL || sell_order_id[0] == '\0') {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "UPDATE position_slots "
        "SET status = 'CLOSED', closed_at = CURRENT_TIMESTAMP, "
        "    sell_order_id = ?, sell_net_eur = ?, realized_profit_eur = ? "
        "WHERE id = ? AND status = 'OPEN';";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, sell_order_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, sell_net_eur);
    sqlite3_bind_double(stmt, 3, realized_profit_eur);
    sqlite3_bind_int(stmt, 4, slot_id);

    int ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) == 1;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return ok;
}


int db_create_paper_position_slot(
    const char *label,
    double base_size_btc,
    double cost_eur,
    double buy_fee_eur,
    double avg_buy_price
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (label == NULL || label[0] == '\0') {
        return 0;
    }

    if (base_size_btc <= 0.0 || cost_eur <= 0.0) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "INSERT OR IGNORE INTO paper_position_slots "
        "(label, base_size_btc, cost_eur, buy_fee_eur, avg_buy_price, status) "
        "VALUES (?, ?, ?, ?, ?, 'OPEN');";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, label, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, base_size_btc);
    sqlite3_bind_double(stmt, 3, cost_eur);
    sqlite3_bind_double(stmt, 4, buy_fee_eur);
    sqlite3_bind_double(stmt, 5, avg_buy_price);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return ok;
}

int db_get_open_paper_position_slots(
    PaperPositionSlotRecord *records,
    int max_records
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int count = 0;

    if (records == NULL || max_records <= 0) {
        return 0;
    }

    memset(records, 0, sizeof(PaperPositionSlotRecord) * (size_t)max_records);

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "SELECT id, label, base_size_btc, cost_eur, buy_fee_eur, avg_buy_price, "
        "       status, opened_at, closed_at, sell_net_eur, realized_profit_eur "
        "FROM paper_position_slots "
        "WHERE status = 'OPEN' "
        "ORDER BY opened_at ASC, id ASC "
        "LIMIT ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_int(stmt, 1, max_records);

    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_records) {
        PaperPositionSlotRecord *record = &records[count];
        const unsigned char *text;

        record->id = sqlite3_column_int(stmt, 0);

        text = sqlite3_column_text(stmt, 1);
        snprintf(record->label, sizeof(record->label), "%s", text ? (const char *)text : "");

        record->base_size_btc = sqlite3_column_double(stmt, 2);
        record->cost_eur = sqlite3_column_double(stmt, 3);
        record->buy_fee_eur = sqlite3_column_double(stmt, 4);
        record->avg_buy_price = sqlite3_column_double(stmt, 5);

        text = sqlite3_column_text(stmt, 6);
        snprintf(record->status, sizeof(record->status), "%s", text ? (const char *)text : "");

        text = sqlite3_column_text(stmt, 7);
        snprintf(record->opened_at, sizeof(record->opened_at), "%s", text ? (const char *)text : "");

        text = sqlite3_column_text(stmt, 8);
        snprintf(record->closed_at, sizeof(record->closed_at), "%s", text ? (const char *)text : "");

        record->sell_net_eur = sqlite3_column_double(stmt, 9);
        record->realized_profit_eur = sqlite3_column_double(stmt, 10);

        count++;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return count;
}

int db_close_paper_position_slot(
    int slot_id,
    double sell_net_eur,
    double realized_profit_eur
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (slot_id <= 0) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "UPDATE paper_position_slots "
        "SET status = 'CLOSED', closed_at = CURRENT_TIMESTAMP, "
        "    sell_net_eur = ?, realized_profit_eur = ? "
        "WHERE id = ? AND status = 'OPEN';";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_double(stmt, 1, sell_net_eur);
    sqlite3_bind_double(stmt, 2, realized_profit_eur);
    sqlite3_bind_int(stmt, 3, slot_id);

    int ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) == 1;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return ok;
}

int db_clear_paper_position_slots(void) {
    sqlite3 *db;
    int ok = 0;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    ok = sqlite3_exec(db, "DELETE FROM paper_position_slots;", NULL, NULL, NULL) == SQLITE_OK;

    sqlite3_close(db);

    return ok;
}

int db_seed_demo_paper_position_slots(void) {
    int created = 0;

    created += db_create_paper_position_slot("paper-slot-a", 0.00015000, 10.00, 0.12, 66666.67) ? 1 : 0;
    created += db_create_paper_position_slot("paper-slot-b", 0.00016500, 10.00, 0.12, 60606.06) ? 1 : 0;
    created += db_create_paper_position_slot("paper-slot-c", 0.00018000, 10.00, 0.12, 55555.56) ? 1 : 0;

    return created;
}
