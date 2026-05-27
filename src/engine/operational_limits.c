#include "operational_limits.h"
#include "../db/database.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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

static OperationalLimitCheck make_check(
    int allowed,
    const char *decision,
    const char *reason,
    int orders_today,
    int seconds_since_last_order
) {
    OperationalLimitCheck check;

    check.allowed = allowed;
    check.orders_today = orders_today;
    check.seconds_since_last_order = seconds_since_last_order;

    snprintf(check.decision, sizeof(check.decision), "%s", decision ? decision : "UNKNOWN");
    snprintf(check.reason, sizeof(check.reason), "%s", reason ? reason : "Nessun dettaglio");

    return check;
}

static int ensure_order_journal_exists(sqlite3 *db) {
    char *err = NULL;

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

    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        if (err) {
            fprintf(stderr, "Errore SQL operational_limits ensure_order_journal: %s\n", err);
            sqlite3_free(err);
        }
        return 0;
    }

    return 1;
}

static int count_real_orders_today(sqlite3 *db) {
    sqlite3_stmt *stmt;
    int count = 0;

    /*
     * Operational limits must count only orders that actually reached
     * Coinbase. Dry-run FINAL_GATE_OK/DRY_RUN_READY entries are diagnostics
     * and must never consume the daily real-order allowance.
     */
    const char *sql =
        "SELECT COUNT(DISTINCT client_order_id) "
        "FROM order_journal "
        "WHERE date(created_at) = date('now', 'localtime') "
        "  AND dry_run = 0 "
        "  AND status = 'REAL_SENT' "
        "  AND phase = 'REAL_EXECUTION' "
        "  AND COALESCE(coinbase_order_id, '') <> '';";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

static int seconds_since_last_real_order(sqlite3 *db) {
    sqlite3_stmt *stmt;
    int seconds = 999999;

    /*
     * Cooldown applies to real Coinbase submissions only.
     * Preview/dry-run diagnostics must not keep the bot artificially blocked.
     */
    const char *sql =
        "SELECT CAST((julianday('now') - julianday(MAX(created_at))) * 86400 AS INTEGER) "
        "FROM order_journal "
        "WHERE dry_run = 0 "
        "  AND status = 'REAL_SENT' "
        "  AND phase = 'REAL_EXECUTION' "
        "  AND COALESCE(coinbase_order_id, '') <> '';";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 999999;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        seconds = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return seconds;
}

OperationalLimitCheck operational_limits_check(
    const StrategySettings *settings,
    OrderExecutorSide side
) {
    sqlite3 *db;
    int orders_today;
    int elapsed;
    char reason[256];

    if (settings == NULL) {
        return make_check(0, "BLOCKED", "Operational limits: settings non disponibili", 0, 0);
    }

    if (settings->max_orders_per_day <= 0) {
        return make_check(0, "BLOCKED", "Operational limits: max_orders_per_day non valido", 0, 0);
    }

    if (settings->order_cooldown_seconds < 0) {
        return make_check(0, "BLOCKED", "Operational limits: order_cooldown_seconds non valido", 0, 0);
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return make_check(0, "BLOCKED", "Operational limits: database non disponibile", 0, 0);
    }

    if (!ensure_order_journal_exists(db)) {
        sqlite3_close(db);
        return make_check(0, "BLOCKED", "Operational limits: order_journal non disponibile", 0, 0);
    }

    orders_today = count_real_orders_today(db);
    elapsed = seconds_since_last_real_order(db);

    sqlite3_close(db);

    if (orders_today >= settings->max_orders_per_day) {
        snprintf(
            reason,
            sizeof(reason),
            "Operational limits %s: limite ordini reali giornalieri raggiunto %d/%d",
            side_to_string(side),
            orders_today,
            settings->max_orders_per_day
        );

        return make_check(0, "BLOCKED", reason, orders_today, elapsed);
    }

    if (elapsed < settings->order_cooldown_seconds) {
        snprintf(
            reason,
            sizeof(reason),
            "Operational limits %s: cooldown attivo, trascorsi %d sec su %d sec richiesti",
            side_to_string(side),
            elapsed,
            settings->order_cooldown_seconds
        );

        return make_check(0, "BLOCKED", reason, orders_today, elapsed);
    }

    snprintf(
        reason,
        sizeof(reason),
        "Operational limits %s: OK | ordini oggi %d/%d | ultimo ordine %d sec fa",
        side_to_string(side),
        orders_today,
        settings->max_orders_per_day,
        elapsed
    );

    return make_check(1, "ALLOWED", reason, orders_today, elapsed);
}

void operational_limits_audit_if_blocked(
    OperationalLimitCheck check,
    double price,
    double btc_balance,
    double eur_balance
) {
    if (check.allowed) {
        return;
    }

    db_log_engine_audit(
        "OPERATIONAL_LIMITS",
        check.decision,
        check.reason,
        price,
        btc_balance,
        eur_balance,
        0.0,
        0.0
    );
}
