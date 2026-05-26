#include "api_health.h"
#include "../exchange/coinbase_client.h"

#include <stdio.h>
#include <string.h>

static void append_reason(ApiHealthReport *report, const char *level, const char *message) {
    char item[192];
    size_t used;
    size_t remaining;

    if (report == NULL || message == NULL) {
        return;
    }

    snprintf(item, sizeof(item), "%s%s", level ? level : "", message);

    used = strlen(report->reason);
    if (used >= sizeof(report->reason) - 1) {
        return;
    }

    remaining = sizeof(report->reason) - used;

    if (used > 0) {
        snprintf(report->reason + used, remaining, " | %.160s", item);
    } else {
        snprintf(report->reason + used, remaining, "%.180s", item);
    }
}

ApiHealthReport api_health_check_light(
    const BotState *state,
    const StrategySettings *settings
) {
    ApiHealthReport report;
    int live_mode;

    memset(&report, 0, sizeof(report));
    snprintf(report.status, sizeof(report.status), "%s", "OK");

    if (state == NULL || settings == NULL) {
        report.ok = 0;
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "state/settings non disponibili");
        snprintf(report.status, sizeof(report.status), "%s", "BLOCKED");
        return report;
    }

    live_mode =
        settings->runtime_mode == RUNTIME_MODE_LIVE_READONLY ||
        settings->runtime_mode == RUNTIME_MODE_LIVE_TRADING;

    if (state->current_price <= 0.0) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "prezzo BTC-EUR non valido o non aggiornato");
    }

    if (live_mode && !coinbase_has_credentials()) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "credenziali Coinbase mancanti in modalità live");
    }

    if (!live_mode && !coinbase_has_credentials()) {
        report.warning_count++;
        append_reason(&report, "WARN: ", "credenziali Coinbase mancanti, ok in SIMULATION");
    }

    if (settings->runtime_mode == RUNTIME_MODE_LIVE_TRADING) {
        report.warning_count++;
        append_reason(&report, "WARN: ", "LIVE_TRADING selezionato ma invio reale ancora bloccato dal codice");
    }

    if (settings->estimated_fee_percent < 0.0 || settings->estimated_fee_percent >= 100.0) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "fee stimata non valida");
    }

    if (settings->slot_amount_eur <= 0.0 || settings->max_slots <= 0) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "slot amount/max slots non validi");
    }

    if (report.blocking_count > 0) {
        report.ok = 0;
        snprintf(report.status, sizeof(report.status), "%s", "BLOCKED");
    } else if (report.warning_count > 0) {
        report.ok = 1;
        snprintf(report.status, sizeof(report.status), "%s", "WARN");
    } else {
        report.ok = 1;
        snprintf(report.status, sizeof(report.status), "%s", "OK");
        snprintf(report.reason, sizeof(report.reason), "%s", "Controlli API base validi");
    }

    return report;
}
