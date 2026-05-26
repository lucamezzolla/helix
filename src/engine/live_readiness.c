#include "live_readiness.h"
#include "order_state_recovery.h"
#include "../exchange/coinbase_client.h"
#include "../config/env_loader.h"

#include <stdio.h>
#include <string.h>

static void append_reason(
    LiveReadinessReport *report,
    const char *level,
    const char *message
) {
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

static int env_real_trading_flags_enabled(void) {
    int flag_real_trading;
    int flag_orders;
    int flag_risk;

    flag_real_trading = env_load_bool_flag("HELIX_REAL_TRADING_ENABLED", 0);
    flag_orders = env_load_bool_flag("HELIX_ALLOW_COINBASE_ORDERS", 0);
    flag_risk = env_load_bool_flag("HELIX_I_UNDERSTAND_REAL_MONEY_RISK", 0);

    return flag_real_trading && flag_orders && flag_risk;
}

LiveReadinessReport live_readiness_check(
    const BotState *state,
    const StrategySettings *settings
) {
    LiveReadinessReport report;
    char recovery_reason[256];

    memset(&report, 0, sizeof(report));
    snprintf(report.status, sizeof(report.status), "%s", "NOT_READY");

    if (state == NULL || settings == NULL) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "state/settings non disponibili");
        return report;
    }

    if (settings->runtime_mode != RUNTIME_MODE_LIVE_TRADING) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "runtime mode non è LIVE_TRADING");
    }

    if (settings->emergency_stop_enabled) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "kill-switch attivo");
    }

    if (!settings->live_trading_armed) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "LIVE_TRADING non armato dalla UI");
    }

    if (!env_real_trading_flags_enabled()) {
        report.blocking_count++;
        append_reason(
            &report,
            "BLOCK: ",
            "flag .env real trading non tutti attivi"
        );
    }

    if (!coinbase_has_credentials()) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "credenziali Coinbase mancanti");
    }

    if (state->current_price <= 0.0) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "prezzo BTC-EUR non valido");
    }

    if (state->used_slots < 0 || state->used_slots > settings->max_slots) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "used_slots incoerente");
    }

    if (settings->max_slots <= 0) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "max_slots non valido");
    }

    if (settings->liquidity_reserve_percent < 0.0 || settings->liquidity_reserve_percent > 95.0) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "riserva liquidità non valida");
    }

    if (settings->volatility_window_seconds < 10 || settings->volatility_max_move_percent <= 0.0) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", "protezione volatilità non valida");
    }

    if (order_state_recovery_check(recovery_reason, sizeof(recovery_reason)) != ORDER_RECOVERY_OK) {
        report.blocking_count++;
        append_reason(&report, "BLOCK: ", recovery_reason);
    }

#ifndef HELIX_ENABLE_REAL_COINBASE_ORDERS
    /*
     * Normal builds must never be considered ready for real execution.
     * The live executor is compiled only in Makefile.live with the explicit
     * HELIX_ENABLE_REAL_COINBASE_ORDERS flag.
     */
    report.blocking_count++;
    append_reason(
        &report,
        "BLOCK: ",
        "build normale: HELIX_ENABLE_REAL_COINBASE_ORDERS non attivo"
    );
#endif

    if (state->running == 0) {
        report.warning_count++;
        append_reason(&report, "WARN: ", "bot fermo");
    }

    if (report.blocking_count == 0) {
        report.ready = 1;
        snprintf(report.status, sizeof(report.status), "%s", "READY");
        if (report.reason[0] == '\0') {
            snprintf(report.reason, sizeof(report.reason), "%s", "Tutti i controlli pre-live sono verdi");
        }
    } else {
        report.ready = 0;
        snprintf(report.status, sizeof(report.status), "%s", "NOT_READY");
    }

    return report;
}
