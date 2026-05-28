#include "order_journal.h"
#include "../db/database.h"

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

    sqlite3_exec(db, position_slots_sql, NULL, NULL, NULL);
    sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS idx_position_slots_status ON position_slots(status);", NULL, NULL, NULL);

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

    {
        const char *status;

        if (plan->dry_run) {
            status = plan->allowed ? "DRY_RUN_READY" : "DRY_RUN_BLOCKED";
        } else if (
            (decision && strstr(decision, "BLOCK") != NULL) ||
            !plan->allowed
        ) {
            status = "REAL_BLOCKED";
        } else {
            /*
             * A live plan that reached PRE_EXECUTION/FINAL_GATE_OK is not a
             * dry-run. It is only a real candidate until order_executor writes
             * REAL_SENT / REAL_REJECTED / REAL_BLOCKED.
             */
            status = "REAL_PLAN_READY";
        }

        sqlite3_bind_text(stmt, 5, status, -1, SQLITE_TRANSIENT);
    }

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

    if (
        ok &&
        result->sent_to_coinbase &&
        result->allowed &&
        plan->side == ORDER_EXECUTOR_SIDE_BUY &&
        result->coinbase_order_id[0] != '\0' &&
        plan->preview_base_size > 0.0
    ) {
        double slot_cost_eur = plan->requested_quote_size > 0.0 ?
            plan->requested_quote_size :
            plan->preview_total_eur;

        db_create_position_slot_from_buy(
            result->coinbase_order_id,
            plan->client_order_id,
            plan->preview_base_size,
            slot_cost_eur,
            plan->preview_fee_eur,
            plan->preview_avg_price
        );
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return ok;
}

static int count_order_journal_matches(const char *where_clause) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char sql[512];
    int count = 0;

    if (where_clause == NULL) {
        return 0;
    }

    if (!order_journal_init()) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    snprintf(
        sql,
        sizeof(sql),
        "SELECT COUNT(*) FROM order_journal WHERE created_at >= datetime('now', '-7 days') AND (%s);",
        where_clause
    );

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return count;
}

static void load_last_order_journal_block(PreliveReport *report) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (report == NULL) {
        return;
    }

    if (!order_journal_init()) {
        return;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return;
    }

    const char *sql =
        "SELECT COALESCE(NULLIF(execution_reason, ''), reason, ''), created_at "
        "FROM order_journal "
        "WHERE status LIKE '%BLOCK%' "
        "   OR decision LIKE '%BLOCK%' "
        "   OR execution_decision LIKE '%BLOCK%' "
        "ORDER BY id DESC LIMIT 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *reason = sqlite3_column_text(stmt, 0);
            const unsigned char *created_at = sqlite3_column_text(stmt, 1);

            snprintf(
                report->last_block_reason,
                sizeof(report->last_block_reason),
                "%s",
                reason ? (const char *)reason : ""
            );

            snprintf(
                report->last_blocked_at,
                sizeof(report->last_blocked_at),
                "%s",
                created_at ? (const char *)created_at : ""
            );
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

static void load_last_order_journal_entry(PreliveReport *report) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (report == NULL) {
        return;
    }

    if (!order_journal_init()) {
        return;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return;
    }

    const char *sql =
        "SELECT client_order_id, created_at "
        "FROM order_journal "
        "ORDER BY id DESC LIMIT 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *client_order_id = sqlite3_column_text(stmt, 0);
            const unsigned char *created_at = sqlite3_column_text(stmt, 1);

            snprintf(
                report->last_client_order_id,
                sizeof(report->last_client_order_id),
                "%s",
                client_order_id ? (const char *)client_order_id : ""
            );

            snprintf(
                report->last_journal_at,
                sizeof(report->last_journal_at),
                "%s",
                created_at ? (const char *)created_at : ""
            );
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}



int order_journal_real_sent_last_24h(void) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int count = 0;

    if (!order_journal_init()) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "SELECT COUNT(*) "
        "FROM order_journal "
        "WHERE created_at >= datetime('now', '-1 day') "
        "  AND status = 'REAL_SENT';";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return count;
}


int order_journal_get_latest_real_sent_order_id(
    char *buffer,
    int buffer_size
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int found = 0;

    if (buffer == NULL || buffer_size <= 0) {
        return 0;
    }

    buffer[0] = '\0';

    if (!order_journal_init()) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "SELECT COALESCE(NULLIF(coinbase_order_id, ''), client_order_id) "
        "FROM order_journal "
        "WHERE status = 'REAL_SENT' "
        "ORDER BY id DESC LIMIT 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *value = sqlite3_column_text(stmt, 0);
            if (value && value[0] != '\0') {
                snprintf(buffer, (size_t)buffer_size, "%s", (const char *)value);
                found = 1;
            }
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return found;
}


int order_journal_get_latest_real_buy_slot(
    RealBuySlotRecord *slot
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (slot == NULL) {
        return 0;
    }

    memset(slot, 0, sizeof(*slot));

    if (!order_journal_init()) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    /*
     * Current slot model, v0.2.0-rc1:
     * use the latest real BUY accepted by Coinbase as the sellable micro-slot.
     *
     * This is intentionally narrower than selling the full wallet balance.
     * It prevents the SELL path from evaluating the whole historical BTC
     * position when only the latest micro-live BUY slot should be checked.
     */
    const char *sql =
        "SELECT client_order_id, coinbase_order_id, "
        "requested_quote_size, preview_fee_eur, preview_base_size, preview_avg_price "
        "FROM order_journal "
        "WHERE side = 'BUY' "
        "  AND status = 'REAL_SENT' "
        "  AND coinbase_order_id IS NOT NULL "
        "  AND coinbase_order_id <> '' "
        "  AND preview_base_size > 0 "
        "ORDER BY id DESC "
        "LIMIT 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *client_order_id = sqlite3_column_text(stmt, 0);
        const unsigned char *coinbase_order_id = sqlite3_column_text(stmt, 1);

        slot->found = 1;
        snprintf(
            slot->client_order_id,
            sizeof(slot->client_order_id),
            "%s",
            client_order_id ? (const char *)client_order_id : ""
        );
        snprintf(
            slot->coinbase_order_id,
            sizeof(slot->coinbase_order_id),
            "%s",
            coinbase_order_id ? (const char *)coinbase_order_id : ""
        );

        slot->quote_size_eur = sqlite3_column_double(stmt, 2);
        slot->fee_eur = sqlite3_column_double(stmt, 3);
        slot->base_size_btc = sqlite3_column_double(stmt, 4);
        slot->avg_price_eur = sqlite3_column_double(stmt, 5);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return slot->found;
}


int order_journal_get_latest_unreconciled_real_order(
    RealOrderJournalRecord *record
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (record == NULL) {
        return 0;
    }

    memset(record, 0, sizeof(*record));

    if (!order_journal_init()) {
        return 0;
    }

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "SELECT client_order_id, coinbase_order_id, side, product_id, status, created_at, "
        "       requested_quote_size, requested_base_size, preview_total_eur, "
        "       preview_fee_eur, preview_base_size, preview_avg_price "
        "FROM order_journal oj "
        "WHERE status = 'REAL_SENT' "
        "  AND NOT EXISTS ("
        "      SELECT 1 FROM order_journal r "
        "      WHERE r.client_order_id = oj.client_order_id "
        "        AND r.phase = 'POST_ORDER_RECONCILIATION'"
        "  ) "
        "ORDER BY id DESC LIMIT 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *client_order_id = sqlite3_column_text(stmt, 0);
            const unsigned char *coinbase_order_id = sqlite3_column_text(stmt, 1);
            const unsigned char *side = sqlite3_column_text(stmt, 2);
            const unsigned char *product_id = sqlite3_column_text(stmt, 3);
            const unsigned char *status = sqlite3_column_text(stmt, 4);
            const unsigned char *created_at = sqlite3_column_text(stmt, 5);

            record->found = 1;

            snprintf(record->client_order_id, sizeof(record->client_order_id), "%s", client_order_id ? (const char *)client_order_id : "");
            snprintf(record->coinbase_order_id, sizeof(record->coinbase_order_id), "%s", coinbase_order_id ? (const char *)coinbase_order_id : "");
            snprintf(record->side, sizeof(record->side), "%s", side ? (const char *)side : "");
            snprintf(record->product_id, sizeof(record->product_id), "%s", product_id ? (const char *)product_id : "BTC-EUR");
            snprintf(record->status, sizeof(record->status), "%s", status ? (const char *)status : "");
            snprintf(record->created_at, sizeof(record->created_at), "%s", created_at ? (const char *)created_at : "");

            record->requested_quote_size = sqlite3_column_double(stmt, 6);
            record->requested_base_size = sqlite3_column_double(stmt, 7);
            record->preview_total_eur = sqlite3_column_double(stmt, 8);
            record->preview_fee_eur = sqlite3_column_double(stmt, 9);
            record->preview_base_size = sqlite3_column_double(stmt, 10);
            record->preview_avg_price = sqlite3_column_double(stmt, 11);
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return record->found;
}

int order_journal_record_post_order_reconciliation(
    const RealOrderJournalRecord *record,
    int allowed,
    const char *decision,
    const char *reason
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (record == NULL || !record->found || record->client_order_id[0] == '\0') {
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
        "reason, coinbase_order_id, execution_decision, execution_reason, executed_at) "
        "VALUES (?, ?, ?, 0, ?, 'POST_ORDER_RECONCILIATION', ?, ?, ?, ?, ?, CURRENT_TIMESTAMP);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    const char *final_decision = decision ? decision : (allowed ? "POST_ORDER_RECON_OK" : "POST_ORDER_RECON_BLOCKED");
    const char *final_reason = reason ? reason : "";

    sqlite3_bind_text(stmt, 1, record->client_order_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, record->side[0] ? record->side : "UNKNOWN", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, record->product_id[0] ? record->product_id : "BTC-EUR", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, allowed ? "POST_ORDER_RECON_OK" : "POST_ORDER_RECON_BLOCKED", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, final_decision, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, final_reason, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, record->coinbase_order_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, final_decision, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, final_reason, -1, SQLITE_TRANSIENT);

    int ok = sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return ok;
}


int order_journal_get_prelive_report(PreliveReport *report) {
    if (report == NULL) {
        return 0;
    }

    memset(report, 0, sizeof(*report));

    report->dry_run_last_7_days =
        count_order_journal_matches("dry_run = 1");

    report->dry_run_ready_last_7_days =
        count_order_journal_matches("dry_run = 1 AND status = 'DRY_RUN_READY'");

    report->dry_run_blocked_last_7_days =
        count_order_journal_matches("dry_run = 1 AND status = 'DRY_RUN_BLOCKED'");

    report->buy_dry_run_last_7_days =
        count_order_journal_matches("dry_run = 1 AND side = 'BUY'");

    report->buy_dry_run_ready_last_7_days =
        count_order_journal_matches("dry_run = 1 AND side = 'BUY' AND status = 'DRY_RUN_READY'");

    report->buy_dry_run_blocked_last_7_days =
        count_order_journal_matches("dry_run = 1 AND side = 'BUY' AND status = 'DRY_RUN_BLOCKED'");

    report->sell_dry_run_last_7_days =
        count_order_journal_matches("dry_run = 1 AND side = 'SELL'");

    report->sell_dry_run_ready_last_7_days =
        count_order_journal_matches("dry_run = 1 AND side = 'SELL' AND status = 'DRY_RUN_READY'");

    report->sell_dry_run_blocked_last_7_days =
        count_order_journal_matches("dry_run = 1 AND side = 'SELL' AND status = 'DRY_RUN_BLOCKED'");

    report->sell_blocked_not_profitable_last_7_days =
        count_order_journal_matches("dry_run = 1 AND side = 'SELL' AND status = 'DRY_RUN_BLOCKED' AND (decision = 'SELL_CANDIDATE_BLOCKED' OR reason LIKE '%profit -%')");

    report->journal_entries_last_24h =
        count_order_journal_matches("created_at >= datetime('now', '-1 day')");

    report->real_sent_last_7_days =
        count_order_journal_matches("status = 'REAL_SENT'");

    report->final_gate_ok_last_7_days =
        count_order_journal_matches("dry_run = 0 AND (decision = 'FINAL_GATE_OK' OR phase = 'FINAL_GATE_OK')");

    report->final_gate_blocked_last_7_days =
        count_order_journal_matches("dry_run = 0 AND (decision = 'FINAL_GATE_BLOCKED' OR phase = 'FINAL_GATE_BLOCKED')");

    report->real_executor_blocked_last_7_days =
        count_order_journal_matches("status = 'REAL_BLOCKED' OR execution_decision = 'BLOCKED'");

    report->post_order_recon_ok_last_7_days =
        count_order_journal_matches("dry_run = 0 AND (decision = 'POST_ORDER_RECON_OK' OR phase = 'POST_ORDER_RECON_OK')");

    report->post_order_recon_blocked_last_7_days =
        count_order_journal_matches("dry_run = 0 AND (decision = 'POST_ORDER_RECON_BLOCKED' OR phase = 'POST_ORDER_RECON_BLOCKED')");

    load_last_order_journal_block(report);
    load_last_order_journal_entry(report);

    return 1;
}

