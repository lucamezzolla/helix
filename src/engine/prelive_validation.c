#include "prelive_validation.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#define DB_PATH "data/helix.db"
#define PRELIVE_REQUIRED_SUCCESSFUL_DRY_RUNS 20

static void set_result(
    PreLiveValidationCheck *check,
    int allowed,
    int successful_dry_runs,
    const char *decision,
    const char *reason
) {
    if (check == NULL) {
        return;
    }

    memset(check, 0, sizeof(*check));
    check->allowed = allowed;
    check->required_successful_dry_runs = PRELIVE_REQUIRED_SUCCESSFUL_DRY_RUNS;
    check->successful_dry_runs = successful_dry_runs;
    snprintf(check->decision, sizeof(check->decision), "%s", decision ? decision : "UNKNOWN");
    snprintf(check->reason, sizeof(check->reason), "%s", reason ? reason : "");
}

static int count_successful_dry_runs(void) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int count = 0;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "SELECT COUNT(*) "
        "FROM order_journal "
        "WHERE dry_run = 1 "
        "AND status = 'DRY_RUN_READY' "
        "AND decision = 'FINAL_GATE_OK' "
        "AND created_at >= datetime('now', '-7 days');";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return count;
}

PreLiveValidationCheck prelive_validation_check(void) {
    PreLiveValidationCheck check;
    int successful_dry_runs = count_successful_dry_runs();

    if (successful_dry_runs < PRELIVE_REQUIRED_SUCCESSFUL_DRY_RUNS) {
        char reason[256];

        snprintf(
            reason,
            sizeof(reason),
            "Pre-live validation incomplete: %d/%d successful dry-run final-gate passes in the last 7 days",
            successful_dry_runs,
            PRELIVE_REQUIRED_SUCCESSFUL_DRY_RUNS
        );

        set_result(
            &check,
            0,
            successful_dry_runs,
            "BLOCKED",
            reason
        );
        return check;
    }

    set_result(
        &check,
        1,
        successful_dry_runs,
        "ALLOWED",
        "Pre-live validation passed: enough successful dry-run final-gate passes"
    );

    return check;
}
