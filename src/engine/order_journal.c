#include "order_journal.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#define DB_PATH "data/helix.db"

static const char *side_to_string(OrderExecutorSide side) {
    switch (side) {
        case ORDER_EXECUTOR_SIDE_BUY:
            return "BUY";
        case ORDER_EXECUTOR_SIDE_SELL:
            return "SELL";
        default:
            return "UNKNOWN";
    }
}

int order_journal_init(void) {
    sqlite3 *db;
    char *err = NULL;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS order_journal ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "client_order_id TEXT NOT NULL,"
        "side TEXT NOT NULL,"
        "product_id TEXT NOT NULL,"
        "dry_run INTEGER NOT NULL DEFAULT 1,"
        "status TEXT NOT NULL,"
        "phase TEXT NOT NULL,"
        "decision TEXT NOT NULL,"
        "requested_quote_size REAL DEFAULT 0,"
        "requested_base_size REAL DEFAULT 0,"
        "preview_total_eur REAL DEFAULT 0,"
        "preview_fee_eur REAL DEFAULT 0,"
        "preview_base_size REAL DEFAULT 0,"
        "preview_avg_price REAL DEFAULT 0,"
        "reason TEXT DEFAULT '',"
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP,"
        "UNIQUE(client_order_id, phase, decision)"
        ");";

    int ok = sqlite3_exec(db, sql, NULL, NULL, &err) == SQLITE_OK;

    if (!ok && err) {
        fprintf(stderr, "Errore SQL order_journal_init: %s\n", err);
        sqlite3_free(err);
        err = NULL;
    }

    /*
     * Safe migrations for existing local databases.
     * SQLite has no ADD COLUMN IF NOT EXISTS on older versions, so failures
     * for duplicate columns are intentionally ignored.
     */
    sqlite3_exec(db, "ALTER TABLE order_journal ADD COLUMN http_code INTEGER DEFAULT 0;", NULL, NULL, NULL);
    sqlite3_exec(db, "ALTER TABLE order_journal ADD COLUMN coinbase_order_id TEXT DEFAULT '';", NULL, NULL, NULL);
    sqlite3_exec(db, "ALTER TABLE order_journal ADD COLUMN execution_decision TEXT DEFAULT '';", NULL, NULL, NULL);
    sqlite3_exec(db, "ALTER TABLE order_journal ADD COLUMN execution_reason TEXT DEFAULT '';", NULL, NULL, NULL);
    sqlite3_exec(db, "ALTER TABLE order_journal ADD COLUMN executed_at TEXT DEFAULT '';", NULL, NULL, NULL);

    sqlite3_close(db);
    return ok;
}

int order_journal_record_execution_plan(
    const OrderExecutionPlan *plan,
    const char *phase,
    const char *decision
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (plan == NULL || plan->client_order_id[0] == '\0') {
        return 0;
    }

    if (!order_journal_init()) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "INSERT OR IGNORE INTO order_journal "
        "(client_order_id, side, product_id, dry_run, status, phase, decision, "
        "requested_quote_size, requested_base_size, preview_total_eur, "
        "preview_fee_eur, preview_base_size, preview_avg_price, reason) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, plan->client_order_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, side_to_string(plan->side), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, plan->product_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, plan->dry_run ? 1 : 0);
    sqlite3_bind_text(stmt, 5, plan->allowed ? "DRY_RUN_READY" : "DRY_RUN_BLOCKED", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, phase ? phase : "PRE_EXECUTION", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, decision ? decision : "UNKNOWN", -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 8, plan->requested_quote_size);
    sqlite3_bind_double(stmt, 9, plan->requested_base_size);
    sqlite3_bind_double(stmt, 10, plan->preview_total_eur);
    sqlite3_bind_double(stmt, 11, plan->preview_fee_eur);
    sqlite3_bind_double(stmt, 12, plan->preview_base_size);
    sqlite3_bind_double(stmt, 13, plan->preview_avg_price);
    sqlite3_bind_text(stmt, 14, plan->reason, -1, SQLITE_TRANSIENT);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return ok;
}

int order_journal_record_execution_result(
    const OrderExecutionPlan *plan,
    const OrderExecutionResult *result,
    const char *phase
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (plan == NULL || result == NULL || plan->client_order_id[0] == '\0') {
        return 0;
    }

    if (!order_journal_init()) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "INSERT OR IGNORE INTO order_journal "
        "(client_order_id, side, product_id, dry_run, status, phase, decision, "
        "requested_quote_size, requested_base_size, preview_total_eur, "
        "preview_fee_eur, preview_base_size, preview_avg_price, reason, "
        "http_code, coinbase_order_id, execution_decision, execution_reason, executed_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, plan->client_order_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, side_to_string(plan->side), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, plan->product_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, plan->dry_run ? 1 : 0);

    if (result->sent_to_coinbase) {
        sqlite3_bind_text(stmt, 5, result->allowed ? "REAL_SENT" : "REAL_REJECTED", -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_text(stmt, 5, "REAL_BLOCKED", -1, SQLITE_TRANSIENT);
    }

    sqlite3_bind_text(stmt, 6, phase ? phase : "REAL_EXECUTION", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, result->decision[0] ? result->decision : "BLOCKED", -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 8, plan->requested_quote_size);
    sqlite3_bind_double(stmt, 9, plan->requested_base_size);
    sqlite3_bind_double(stmt, 10, plan->preview_total_eur);
    sqlite3_bind_double(stmt, 11, plan->preview_fee_eur);
    sqlite3_bind_double(stmt, 12, plan->preview_base_size);
    sqlite3_bind_double(stmt, 13, plan->preview_avg_price);
    sqlite3_bind_text(stmt, 14, plan->reason, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 15, (sqlite3_int64) result->http_code);
    sqlite3_bind_text(stmt, 16, result->coinbase_order_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 17, result->decision, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 18, result->reason, -1, SQLITE_TRANSIENT);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return ok;
}
