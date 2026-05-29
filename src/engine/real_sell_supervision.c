#include "real_sell_supervision.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REAL_SELL_SUPERVISION_DB_PATH "data/helix.db"

static int env_allows_real_slot_sell_supervision(void) {
    const char *value = getenv("HELIX_ALLOW_REAL_SLOT_SELL");

    if (value == NULL) {
        return 0;
    }

    return
        strcmp(value, "true") == 0 ||
        strcmp(value, "TRUE") == 0 ||
        strcmp(value, "1") == 0 ||
        strcmp(value, "yes") == 0 ||
        strcmp(value, "YES") == 0;
}

static void set_check(
    RealSellSupervisionCheck *check,
    int allowed,
    const char *decision,
    const char *reason
) {
    if (check == NULL) {
        return;
    }

    check->allowed = allowed;
    snprintf(check->decision, sizeof(check->decision), "%s", decision ? decision : "UNKNOWN");
    snprintf(check->reason, sizeof(check->reason), "%s", reason ? reason : "");
}

static int count_int_query(const char *sql, int *out_count) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int ok = 0;

    if (out_count == NULL || sql == NULL) {
        return 0;
    }

    *out_count = 0;

    if (sqlite3_open(REAL_SELL_SUPERVISION_DB_PATH, &db) != SQLITE_OK) {
        return 0;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            *out_count = sqlite3_column_int(stmt, 0);
            ok = 1;
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return ok;
}

RealSellSupervisionCheck real_sell_supervision_check_one_shot(
    const StrategySettings *settings
) {
    RealSellSupervisionCheck check;
    int real_sell_sent_today = 0;
    int real_sell_closed_slots_today = 0;

    set_check(
        &check,
        0,
        "BLOCKED_INITIAL_STATE",
        "SELL reale one-shot non ancora valutato"
    );

    if (settings == NULL) {
        set_check(
            &check,
            0,
            "BLOCKED_SETTINGS_UNAVAILABLE",
            "SELL reale one-shot bloccato: settings non disponibili"
        );
        return check;
    }

    if (!env_allows_real_slot_sell_supervision()) {
        set_check(
            &check,
            0,
            "BLOCKED_ENV_GATE",
            "SELL reale one-shot bloccato: HELIX_ALLOW_REAL_SLOT_SELL non e true/1/yes"
        );
        return check;
    }

    if (settings->runtime_mode != RUNTIME_MODE_LIVE_TRADING) {
        set_check(
            &check,
            0,
            "BLOCKED_NOT_LIVE_TRADING",
            "SELL reale one-shot bloccato: modalita operativa diversa da LIVE_TRADING"
        );
        return check;
    }

    if (!settings->live_trading_armed) {
        set_check(
            &check,
            0,
            "BLOCKED_NOT_ARMED",
            "SELL reale one-shot bloccato: LIVE_TRADING non armato manualmente"
        );
        return check;
    }

    if (!count_int_query(
        "SELECT COUNT(*) "
        "FROM order_journal "
        "WHERE side='SELL' "
        "  AND dry_run=0 "
        "  AND (status='REAL_SENT' OR execution_decision='SENT') "
        "  AND date(COALESCE(NULLIF(executed_at, ''), created_at)) = date('now', 'localtime');",
        &real_sell_sent_today
    )) {
        set_check(
            &check,
            0,
            "BLOCKED_AUDIT_UNAVAILABLE",
            "SELL reale one-shot bloccato: impossibile leggere order_journal"
        );
        return check;
    }

    if (real_sell_sent_today > 0) {
        char reason[REAL_SELL_SUPERVISION_REASON_SIZE];
        snprintf(
            reason,
            sizeof(reason),
            "SELL reale one-shot bloccato: risultano gia %d SELL reali inviati oggi",
            real_sell_sent_today
        );
        set_check(&check, 0, "BLOCKED_ALREADY_SENT_TODAY", reason);
        return check;
    }

    if (!count_int_query(
        "SELECT COUNT(*) "
        "FROM position_slots "
        "WHERE status='CLOSED' "
        "  AND COALESCE(sell_order_id, '') <> '' "
        "  AND date(COALESCE(NULLIF(closed_at, ''), '1970-01-01')) = date('now', 'localtime');",
        &real_sell_closed_slots_today
    )) {
        set_check(
            &check,
            0,
            "BLOCKED_SLOT_AUDIT_UNAVAILABLE",
            "SELL reale one-shot bloccato: impossibile leggere position_slots"
        );
        return check;
    }

    if (real_sell_closed_slots_today > 0) {
        char reason[REAL_SELL_SUPERVISION_REASON_SIZE];
        snprintf(
            reason,
            sizeof(reason),
            "SELL reale one-shot bloccato: risultano gia %d slot chiusi oggi da SELL reale",
            real_sell_closed_slots_today
        );
        set_check(&check, 0, "BLOCKED_SLOT_ALREADY_CLOSED_TODAY", reason);
        return check;
    }

    set_check(
        &check,
        1,
        "ALLOWED_PRE_EXECUTION",
        "SELL reale one-shot consentibile: env abilitato, LIVE_TRADING armato, nessun SELL reale gia inviato oggi"
    );

    return check;
}
