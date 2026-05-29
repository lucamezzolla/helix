#include "daily_report.h"

#include "email_delivery.h"
#include "../db/database.h"

#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef HELIX_MAX_OPEN_POSITION_SLOTS
#define HELIX_MAX_OPEN_POSITION_SLOTS 64
#endif

static const char *daily_report_bot_mode_to_string(BotMode mode) {
    switch (mode) {
        case BOT_MODE_PAUSED:
            return "PAUSED";
        case BOT_MODE_READY:
            return "READY";
        case BOT_MODE_BUYING:
            return "BUYING";
        case BOT_MODE_WAITING_SELL:
            return "WAITING_SELL";
        case BOT_MODE_SELLING:
            return "SELLING";
        case BOT_MODE_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

#define DAILY_REPORT_BODY_SIZE 20000
#define DAILY_REPORT_MESSAGE_SIZE 384
#define DAILY_REPORT_LAST_DATE_KEY "email.last_daily_report_date"
#define DAILY_REPORT_DB_PATH "data/helix.db"

static void append_text(char *buffer, size_t buffer_size, const char *text) {
    size_t used;

    if (buffer == NULL || buffer_size == 0 || text == NULL) {
        return;
    }

    used = strlen(buffer);
    if (used >= buffer_size - 1) {
        return;
    }

    snprintf(buffer + used, buffer_size - used, "%s", text);
}

static void append_format(char *buffer, size_t buffer_size, const char *format, ...) {
    size_t used;
    va_list args;

    if (buffer == NULL || buffer_size == 0 || format == NULL) {
        return;
    }

    used = strlen(buffer);
    if (used >= buffer_size - 1) {
        return;
    }

    va_start(args, format);
    vsnprintf(buffer + used, buffer_size - used, format, args);
    va_end(args);
}

static const char *safe_column_text(sqlite3_stmt *stmt, int index) {
    const unsigned char *text = sqlite3_column_text(stmt, index);

    return text ? (const char *)text : "";
}

static void append_recent_orders(char *body, size_t body_size) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT created_at, side, dry_run, status, phase, decision, "
        "COALESCE(client_order_id, ''), COALESCE(coinbase_order_id, ''), COALESCE(reason, '') "
        "FROM order_journal "
        "WHERE dry_run = 0 "
        "ORDER BY id DESC "
        "LIMIT 8;";
    int rows = 0;

    append_text(body, body_size, "\n== Ordini reali recenti ==\n");

    if (sqlite3_open(DAILY_REPORT_DB_PATH, &db) != SQLITE_OK) {
        append_text(body, body_size, "Impossibile aprire il database per leggere order_journal.\n");
        return;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        append_text(body, body_size, "Impossibile preparare query order_journal.\n");
        sqlite3_close(db);
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        rows++;
        append_format(
            body,
            body_size,
            "- %s | %s | dry_run=%d | %s/%s/%s | client=%s | coinbase=%s | %.120s\n",
            safe_column_text(stmt, 0),
            safe_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            safe_column_text(stmt, 3),
            safe_column_text(stmt, 4),
            safe_column_text(stmt, 5),
            safe_column_text(stmt, 6),
            safe_column_text(stmt, 7),
            safe_column_text(stmt, 8)
        );
    }

    if (rows == 0) {
        append_text(body, body_size, "Nessun ordine reale recente.\n");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

static void append_recent_audit(
    char *body,
    size_t body_size,
    const char *title,
    const char *event_filter_sql
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char sql[1024];
    int rows = 0;

    append_format(body, body_size, "\n== %s ==\n", title);

    snprintf(
        sql,
        sizeof(sql),
        "SELECT created_at, event_type, decision, reason, eur_amount, estimated_fee, net_profit "
        "FROM engine_audit "
        "WHERE event_type IN (%s) "
        "ORDER BY id DESC "
        "LIMIT 8;",
        event_filter_sql
    );

    if (sqlite3_open(DAILY_REPORT_DB_PATH, &db) != SQLITE_OK) {
        append_text(body, body_size, "Impossibile aprire il database per leggere engine_audit.\n");
        return;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        append_text(body, body_size, "Impossibile preparare query engine_audit.\n");
        sqlite3_close(db);
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        rows++;
        append_format(
            body,
            body_size,
            "- %s | %s | %s | EUR %.2f | fee %.2f | net %.2f | %.140s\n",
            safe_column_text(stmt, 0),
            safe_column_text(stmt, 1),
            safe_column_text(stmt, 2),
            sqlite3_column_double(stmt, 4),
            sqlite3_column_double(stmt, 5),
            sqlite3_column_double(stmt, 6),
            safe_column_text(stmt, 3)
        );
    }

    if (rows == 0) {
        append_text(body, body_size, "Nessun evento recente.\n");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

static void build_daily_report_body(
    const BotState *state,
    const StrategySettings *settings,
    const char *date_text,
    char *body,
    size_t body_size
) {
    PositionSlotRecord slots[HELIX_MAX_OPEN_POSITION_SLOTS];
    int slot_count;
    double open_btc = 0.0;
    double open_cost = 0.0;
    double open_fees = 0.0;

    body[0] = '\0';

    append_text(body, body_size, "Helix - report giornaliero\n");
    append_text(body, body_size, "===========================\n\n");
    append_format(body, body_size, "Data locale: %s\n", date_text);
    append_text(body, body_size, "Modalità: report informativo. Nessun ordine viene eseguito da questa email.\n\n");

    if (state != NULL) {
        append_format(
            body,
            body_size,
            "== Stato attuale ==\n"
            "Running: %s\n"
            "Mode: %s\n"
            "EUR: %.2f\n"
            "BTC: %.8f\n"
            "Prezzo BTC/EUR: %.2f\n"
            "Prezzo medio: %.2f\n"
            "Slot usati: %d / %d\n"
            "Ultima operazione: %.160s\n\n",
            state->running ? "sì" : "no",
            daily_report_bot_mode_to_string(state->mode),
            state->eur_balance,
            state->btc_balance,
            state->current_price,
            state->avg_buy_price,
            state->used_slots,
            state->max_slots,
            state->last_trade
        );
    } else {
        append_text(body, body_size, "== Stato attuale ==\nStato Helix non disponibile.\n\n");
    }

    if (settings != NULL) {
        append_format(
            body,
            body_size,
            "== Impostazioni principali ==\n"
            "Max slot: %d\n"
            "Slot amount EUR: %.2f\n"
            "Riserva protetta: %.2f%%\n"
            "Slot riserva sbloccati: %d\n"
            "Micro-live: %s\n"
            "Max micro ordine: %.2f EUR\n"
            "Stop dopo ordine reale: %s\n"
            "Email giornaliera: %s alle %02d:%02d\n\n",
            settings->max_slots,
            settings->slot_amount_eur,
            settings->liquidity_reserve_percent,
            settings->reserve_released_slots,
            settings->micro_live_enabled ? "attivo" : "disattivato",
            settings->micro_live_max_order_eur,
            settings->micro_live_stop_after_real_order ? "attivo" : "disattivato",
            settings->email_daily_enabled ? "attiva" : "disattivata",
            settings->email_report_hour,
            settings->email_report_minute
        );
    }

    memset(slots, 0, sizeof(slots));
    slot_count = db_get_open_position_slots(slots, HELIX_MAX_OPEN_POSITION_SLOTS);

    append_text(body, body_size, "== Slot reali aperti ==\n");
    if (slot_count <= 0) {
        append_text(body, body_size, "Nessuno slot reale aperto.\n");
    } else {
        for (int i = 0; i < slot_count; i++) {
            open_btc += slots[i].base_size_btc;
            open_cost += slots[i].cost_eur;
            open_fees += slots[i].buy_fee_eur;

            append_format(
                body,
                body_size,
                "- #%d | BTC %.8f | costo %.2f | fee %.2f | avg %.2f | %s | %.32s\n",
                slots[i].id,
                slots[i].base_size_btc,
                slots[i].cost_eur,
                slots[i].buy_fee_eur,
                slots[i].avg_buy_price,
                slots[i].status,
                slots[i].opened_at
            );
        }

        append_format(
            body,
            body_size,
            "Totale slot aperti: %d | BTC %.8f | costo %.2f | fee %.2f | allocato %.2f\n",
            slot_count,
            open_btc,
            open_cost,
            open_fees,
            open_cost + open_fees
        );
    }

    append_recent_orders(body, body_size);
    append_recent_audit(
        body,
        body_size,
        "Audit BUY portfolio",
        "'PORTFOLIO_SLOT_BUY','PORTFOLIO_SLOT_BUY_PREVIEW'"
    );
    append_recent_audit(
        body,
        body_size,
        "Audit SELL slot",
        "'SELL_SLOT_SELECTION','REAL_SLOT_SELL_PLAN','EXCHANGE_SAFETY_SELL','SLOT_CLOSE_RECONCILIATION'"
    );

    append_text(body, body_size, "\n--\nHelix daily report\n");
}

void daily_report_maybe_send(
    const BotState *state,
    const StrategySettings *settings
) {
    static int last_checked_yday = -1;
    static int last_checked_hour = -1;
    static int last_checked_minute = -1;

    time_t now;
    struct tm *local_now;
    char today[16];
    char date_text[64];
    char last_sent_date[32];
    char body[DAILY_REPORT_BODY_SIZE];
    char message[DAILY_REPORT_MESSAGE_SIZE];
    char reason[512];
    int sent;
    int now_minutes;
    int target_minutes;

    if (settings == NULL || !settings->email_daily_enabled) {
        return;
    }

    now = time(NULL);
    local_now = localtime(&now);
    if (local_now == NULL) {
        return;
    }

    if (
        local_now->tm_yday == last_checked_yday &&
        local_now->tm_hour == last_checked_hour &&
        local_now->tm_min == last_checked_minute
    ) {
        return;
    }

    now_minutes = local_now->tm_hour * 60 + local_now->tm_min;
    target_minutes = settings->email_report_hour * 60 + settings->email_report_minute;

    /*
     * Finestra tollerante: se il timer non cade esattamente sul minuto,
     * il report può partire entro 5 minuti dall'orario configurato.
     */
    if (now_minutes < target_minutes || now_minutes > target_minutes + 5) {
        return;
    }

    last_checked_yday = local_now->tm_yday;
    last_checked_hour = local_now->tm_hour;
    last_checked_minute = local_now->tm_min;

    strftime(today, sizeof(today), "%Y-%m-%d", local_now);
    strftime(date_text, sizeof(date_text), "%Y-%m-%d %H:%M:%S", local_now);

    last_sent_date[0] = '\0';
    if (db_get_setting(DAILY_REPORT_LAST_DATE_KEY, last_sent_date, sizeof(last_sent_date))) {
        if (strcmp(last_sent_date, today) == 0) {
            return;
        }
    }

    build_daily_report_body(state, settings, date_text, body, sizeof(body));

    sent = email_delivery_send_message(
        settings,
        "Helix - report giornaliero",
        body,
        message,
        sizeof(message)
    );

    if (sent) {
        db_set_setting(DAILY_REPORT_LAST_DATE_KEY, today);
        snprintf(
            reason,
            sizeof(reason),
            "Report giornaliero inviato a %s | data %s | %.180s",
            settings->email_recipient,
            today,
            message
        );
        db_log_engine_audit(
            "DAILY_EMAIL_REPORT",
            "SENT",
            reason,
            state ? state->current_price : 0.0,
            state ? state->btc_balance : 0.0,
            state ? state->eur_balance : 0.0,
            0.0,
            0.0
        );
    } else {
        snprintf(
            reason,
            sizeof(reason),
            "Report giornaliero non inviato | data %s | %.220s",
            today,
            message
        );
        db_log_engine_audit(
            "DAILY_EMAIL_REPORT",
            "FAILED",
            reason,
            state ? state->current_price : 0.0,
            state ? state->btc_balance : 0.0,
            state ? state->eur_balance : 0.0,
            0.0,
            0.0
        );
    }
}
