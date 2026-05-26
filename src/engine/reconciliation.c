#include "reconciliation.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define RECON_EUR_TOLERANCE 0.01
#define RECON_BTC_TOLERANCE 0.00000001

static ReconciliationReport make_report(
    ReconciliationStatus status,
    int trading_allowed,
    const char *decision,
    const char *reason,
    double eur_delta,
    double btc_delta,
    double cost_basis,
    double avg_buy_price
) {
    ReconciliationReport report;

    report.status = status;
    report.trading_allowed = trading_allowed;
    snprintf(report.decision, sizeof(report.decision), "%s", decision ? decision : "UNKNOWN");
    snprintf(report.reason, sizeof(report.reason), "%s", reason ? reason : "Nessun dettaglio reconciliation");
    report.eur_delta = eur_delta;
    report.btc_delta = btc_delta;
    report.reconstructed_cost_basis = cost_basis;
    report.reconstructed_avg_buy_price = avg_buy_price;

    return report;
}

static int invalid_balance(double value) {
    return !isfinite(value) || value < 0.0;
}

ReconciliationReport reconciliation_check_live_readonly(
    const BotState *state,
    const WalletInfo *remote_wallet,
    const CoinbasePositionSummary *position_summary
) {
    double eur_delta;
    double btc_delta;

    if (state == NULL || remote_wallet == NULL) {
        return make_report(
            RECONCILIATION_STATUS_BLOCKED,
            0,
            "BLOCKED",
            "Reconciliation: stato o wallet remoto non disponibile",
            0.0,
            0.0,
            0.0,
            0.0
        );
    }

    if (!remote_wallet->connected) {
        return make_report(
            RECONCILIATION_STATUS_BLOCKED,
            0,
            "BLOCKED",
            "Reconciliation: wallet Coinbase non connesso",
            0.0,
            0.0,
            0.0,
            0.0
        );
    }

    if (
        invalid_balance(state->eur_balance) ||
        invalid_balance(state->btc_balance) ||
        invalid_balance(remote_wallet->eur_balance) ||
        invalid_balance(remote_wallet->btc_balance)
    ) {
        return make_report(
            RECONCILIATION_STATUS_BLOCKED,
            0,
            "BLOCKED",
            "Reconciliation: saldo negativo/non numerico rilevato",
            0.0,
            0.0,
            0.0,
            0.0
        );
    }

    eur_delta = fabs(state->eur_balance - remote_wallet->eur_balance);
    btc_delta = fabs(state->btc_balance - remote_wallet->btc_balance);

    if (eur_delta > RECON_EUR_TOLERANCE || btc_delta > RECON_BTC_TOLERANCE) {
        char reason[192];

        snprintf(
            reason,
            sizeof(reason),
            "Reconciliation: mismatch wallet | EUR delta %.6f | BTC delta %.10f",
            eur_delta,
            btc_delta
        );

        return make_report(
            RECONCILIATION_STATUS_BLOCKED,
            0,
            "BLOCKED",
            reason,
            eur_delta,
            btc_delta,
            position_summary ? position_summary->cost_basis_eur : 0.0,
            position_summary ? position_summary->avg_buy_price : 0.0
        );
    }

    if (state->btc_balance > RECON_BTC_TOLERANCE) {
        if (state->used_slots <= 0) {
            return make_report(
                RECONCILIATION_STATUS_BLOCKED,
                0,
                "BLOCKED",
                "Reconciliation: BTC presente ma used_slots non coerente",
                eur_delta,
                btc_delta,
                position_summary ? position_summary->cost_basis_eur : 0.0,
                position_summary ? position_summary->avg_buy_price : 0.0
            );
        }

        if (
            position_summary == NULL ||
            !position_summary->connected ||
            position_summary->btc_open <= 0.0 ||
            position_summary->avg_buy_price <= 0.0
        ) {
            return make_report(
                RECONCILIATION_STATUS_WARNING,
                0,
                "WARNING",
                "Reconciliation: wallet allineato, ma costo posizione/fills non ricostruiti",
                eur_delta,
                btc_delta,
                position_summary ? position_summary->cost_basis_eur : 0.0,
                position_summary ? position_summary->avg_buy_price : 0.0
            );
        }

        return make_report(
            RECONCILIATION_STATUS_OK,
            1,
            "OK",
            "Reconciliation: wallet e posizione BTC coerenti",
            eur_delta,
            btc_delta,
            state->btc_balance * position_summary->avg_buy_price,
            position_summary->avg_buy_price
        );
    }

    if (state->used_slots != 0) {
        return make_report(
            RECONCILIATION_STATUS_BLOCKED,
            0,
            "BLOCKED",
            "Reconciliation: nessun BTC ma used_slots diverso da zero",
            eur_delta,
            btc_delta,
            position_summary ? position_summary->cost_basis_eur : 0.0,
            position_summary ? position_summary->avg_buy_price : 0.0
        );
    }

    return make_report(
        RECONCILIATION_STATUS_OK,
        1,
        "OK",
        "Reconciliation: wallet EUR/BTC coerente, nessuna posizione aperta",
        eur_delta,
        btc_delta,
        position_summary ? position_summary->cost_basis_eur : 0.0,
        position_summary ? position_summary->avg_buy_price : 0.0
    );
}
