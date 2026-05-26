#include "order_state_recovery.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#define HELIX_DB_PATH "data/helix.db"

static void set_reason(char *reason, size_t reason_size, const char *message) {
    if (reason == NULL || reason_size == 0) {
        return;
    }

    snprintf(reason, reason_size, "%s", message ? message : "");
}

static int ensure_order_state_table(sqlite3 *db) {
    char *err = NULL;

    const char *sql =
        "CREATE TABLE IF NOT EXISTS order_state ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "client_order_id TEXT,"
        "product_id TEXT DEFAULT 'BTC-EUR',"
        "side TEXT,"
        "status TEXT NOT NULL,"
        "dry_run INTEGER DEFAULT 1,"
        "base_size REAL DEFAULT 0,"
        "quote_size REAL DEFAULT 0,"
        "order_total REAL DEFAULT 0,"
        "commission_total REAL DEFAULT 0,"
        "preview_id TEXT,"
        "reason TEXT,"
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP,"
        "updated_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ");";

    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "Errore SQL order_state: %s\n", err ? err : "unknown");
        sqlite3_free(err);
        return 0;
    }

    return 1;
}

OrderRecoveryStatus order_state_recovery_check(char *reason, size_t reason_size) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    int active_count = 0;

    set_reason(reason, reason_size, "ORDER_RECOVERY_OK");

    if (sqlite3_open(HELIX_DB_PATH, &db) != SQLITE_OK) {
        set_reason(reason, reason_size, "ORDER_RECOVERY_DB_OPEN_FAILED");
        if (db != NULL) {
            sqlite3_close(db);
        }
        return ORDER_RECOVERY_ERROR;
    }

    if (!ensure_order_state_table(db)) {
        sqlite3_close(db);
        set_reason(reason, reason_size, "ORDER_RECOVERY_TABLE_INIT_FAILED");
        return ORDER_RECOVERY_ERROR;
    }

    /*
     * Dry-run records are informational and must not block the engine.
     * Real future orders with unresolved status must block the engine
     * until reconciliation marks them FILLED/CANCELED/FAILED/RECONCILED.
     */
    const char *sql =
        "SELECT COUNT(*) "
        "FROM order_state "
        "WHERE dry_run = 0 "
        "AND status IN ('ACTIVE', 'PENDING', 'SUBMITTED', 'OPEN', 'PARTIAL_FILL');";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        set_reason(reason, reason_size, "ORDER_RECOVERY_QUERY_FAILED");
        return ORDER_RECOVERY_ERROR;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        active_count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (active_count > 0) {
        snprintf(
            reason,
            reason_size,
            "ORDER_RECOVERY_BLOCKED unresolved_real_orders=%d",
            active_count
        );
        return ORDER_RECOVERY_BLOCKED;
    }

    set_reason(reason, reason_size, "ORDER_RECOVERY_OK no_unresolved_real_orders");
    return ORDER_RECOVERY_OK;
}
