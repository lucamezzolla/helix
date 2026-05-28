#include "post_order_reconciliation.h"
#include "order_journal.h"
#include "../exchange/coinbase_client.h"
#include "../wallet/wallet_info.h"
#include "../db/database.h"

#include <stdio.h>
#include <string.h>

static PostOrderReconciliationCheck make_check(
    int allowed,
    const char *decision,
    const char *reason
) {
    PostOrderReconciliationCheck check;

    memset(&check, 0, sizeof(check));
    check.allowed = allowed;
    snprintf(check.decision, sizeof(check.decision), "%s", decision ? decision : "UNKNOWN");
    snprintf(check.reason, sizeof(check.reason), "%s", reason ? reason : "Nessun dettaglio post-order reconciliation");

    return check;
}

PostOrderReconciliationCheck post_order_reconciliation_check_after_plan(
    const BotState *state,
    const StrategySettings *settings,
    const OrderExecutionPlan *plan
) {
    (void)state;

    if (settings == NULL || plan == NULL) {
        return make_check(
            0,
            "BLOCKED",
            "Post-order reconciliation: settings o piano ordine non disponibili"
        );
    }

    if (!plan->allowed) {
        return make_check(
            0,
            "BLOCKED",
            "Post-order reconciliation: piano ordine non autorizzato"
        );
    }

    if (plan->dry_run) {
        return make_check(
            1,
            "DRY_RUN_OK",
            "Post-order reconciliation: dry-run, nessun ordine reale da riconciliare"
        );
    }

    if (settings->runtime_mode != RUNTIME_MODE_LIVE_TRADING) {
        return make_check(
            0,
            "BLOCKED",
            "Post-order reconciliation: piano reale fuori da LIVE_TRADING"
        );
    }

    /*
     * Hard-stop intentional: real post-order reconciliation must be implemented
     * before any non-dry-run order can be considered completed.
     */
    return make_check(
        0,
        "BLOCKED",
        "Post-order reconciliation reale non ancora implementata: blocco obbligatorio"
    );
}


static int order_status_is_complete(const CoinbaseOrderStatus *order_status) {
    if (order_status == NULL || !order_status->connected || !order_status->found) {
        return 0;
    }

    if (
        strcmp(order_status->status, "FILLED") == 0 ||
        strcmp(order_status->status, "SETTLED") == 0 ||
        strcmp(order_status->status, "DONE") == 0 ||
        order_status->completion_percentage >= 100.0
    ) {
        return 1;
    }

    return 0;
}

PostOrderReconciliationCheck post_order_reconciliation_check_latest_real_order(
    BotState *state,
    const StrategySettings *settings
) {
    RealOrderJournalRecord record;

    if (settings == NULL) {
        return make_check(
            0,
            "BLOCKED",
            "Post-order reconciliation reale: settings non disponibili"
        );
    }

    if (!order_journal_get_latest_unreconciled_real_order(&record)) {
        return make_check(
            1,
            "NO_REAL_ORDER",
            "Post-order reconciliation reale: nessun ordine reale pendente da riconciliare"
        );
    }

    if (record.coinbase_order_id[0] == '\0') {
        PostOrderReconciliationCheck check = make_check(
            0,
            "POST_ORDER_RECON_BLOCKED",
            "Post-order reconciliation reale: coinbase_order_id mancante nel journal"
        );

        order_journal_record_post_order_reconciliation(
            &record,
            check.allowed,
            check.decision,
            check.reason
        );

        return check;
    }

    CoinbaseOrderStatus order_status =
        coinbase_get_order_status_readonly(record.coinbase_order_id);

    if (!order_status.connected || !order_status.found) {
        char reason[256];

        snprintf(
            reason,
            sizeof(reason),
            "Post-order reconciliation reale: ordine Coinbase non confermato | http %ld | %.120s",
            order_status.http_code,
            order_status.message
        );

        PostOrderReconciliationCheck check = make_check(
            0,
            "POST_ORDER_RECON_BLOCKED",
            reason
        );

        order_journal_record_post_order_reconciliation(
            &record,
            check.allowed,
            check.decision,
            check.reason
        );

        return check;
    }

    if (!order_status_is_complete(&order_status)) {
        char reason[256];

        snprintf(
            reason,
            sizeof(reason),
            "Post-order reconciliation reale: ordine non completo | status %s | completion %.2f%%",
            order_status.status[0] ? order_status.status : "UNKNOWN",
            order_status.completion_percentage
        );

        PostOrderReconciliationCheck check = make_check(
            0,
            "POST_ORDER_RECON_BLOCKED",
            reason
        );

        order_journal_record_post_order_reconciliation(
            &record,
            check.allowed,
            check.decision,
            check.reason
        );

        return check;
    }

    WalletInfo wallet = coinbase_get_wallet_info_readonly();

    if (!wallet.connected) {
        PostOrderReconciliationCheck check = make_check(
            0,
            "POST_ORDER_RECON_BLOCKED",
            "Post-order reconciliation reale: wallet Coinbase non leggibile dopo ordine"
        );

        order_journal_record_post_order_reconciliation(
            &record,
            check.allowed,
            check.decision,
            check.reason
        );

        return check;
    }

    CoinbasePositionSummary position_summary =
        coinbase_get_btc_eur_position_summary_readonly();

    if (!position_summary.connected || position_summary.fill_count <= 0) {
        PostOrderReconciliationCheck check = make_check(
            0,
            "POST_ORDER_RECON_BLOCKED",
            "Post-order reconciliation reale: fills Coinbase non leggibili dopo ordine"
        );

        order_journal_record_post_order_reconciliation(
            &record,
            check.allowed,
            check.decision,
            check.reason
        );

        return check;
    }

    if (state != NULL) {
        state->eur_balance = wallet.eur_balance;
        state->btc_balance = wallet.btc_balance;

        if (position_summary.avg_buy_price > 0.0) {
            state->avg_buy_price = position_summary.avg_buy_price;
        }

        if (wallet.btc_balance > 0.0) {
            state->mode = BOT_MODE_WAITING_SELL;
        } else {
            state->mode = BOT_MODE_READY;
        }
    }

    char reason[256];

    snprintf(
        reason,
        sizeof(reason),
        "Post-order reconciliation reale OK | order %.80s | status %.32s | wallet EUR %.2f | BTC %.8f | fills %d",
        record.coinbase_order_id,
        order_status.status[0] ? order_status.status : "UNKNOWN",
        wallet.eur_balance,
        wallet.btc_balance,
        position_summary.fill_count
    );

    if (strcmp(record.side, "SELL") == 0) {
        PositionSlotRecord slot;
        double sell_net_eur = record.preview_total_eur - record.preview_fee_eur;
        double realized_profit_eur = 0.0;

        if (
            record.requested_base_size > 0.0 &&
            sell_net_eur > 0.0 &&
            db_find_open_position_slot_by_base_size(record.requested_base_size, &slot)
        ) {
            realized_profit_eur = sell_net_eur - slot.cost_eur;

            if (db_close_position_slot(
                slot.id,
                record.coinbase_order_id,
                sell_net_eur,
                realized_profit_eur
            )) {
                char slot_reason[320];

                snprintf(
                    slot_reason,
                    sizeof(slot_reason),
                    "Slot reale chiuso dopo SELL reconciliation OK | slot #%d | sell order %.80s | base %.8f | net %.2f | cost %.2f | profit %.2f",
                    slot.id,
                    record.coinbase_order_id,
                    record.requested_base_size,
                    sell_net_eur,
                    slot.cost_eur,
                    realized_profit_eur
                );

                db_log_engine_audit(
                    "SLOT_CLOSE_RECONCILIATION",
                    "SLOT_CLOSED_AFTER_SELL_RECON",
                    slot_reason,
                    state ? state->current_price : 0.0,
                    record.requested_base_size,
                    sell_net_eur,
                    record.preview_fee_eur,
                    realized_profit_eur
                );
            } else {
                db_log_engine_audit(
                    "SLOT_CLOSE_RECONCILIATION",
                    "SLOT_CLOSE_FAILED_AFTER_SELL_RECON",
                    "SELL reconciliation OK ma chiusura slot reale fallita",
                    state ? state->current_price : 0.0,
                    record.requested_base_size,
                    sell_net_eur,
                    record.preview_fee_eur,
                    realized_profit_eur
                );
            }
        } else {
            db_log_engine_audit(
                "SLOT_CLOSE_RECONCILIATION",
                "SLOT_CLOSE_SKIPPED_NO_MATCHING_SLOT",
                "SELL reconciliation OK ma nessuno slot OPEN compatibile trovato",
                state ? state->current_price : 0.0,
                record.requested_base_size,
                sell_net_eur,
                record.preview_fee_eur,
                0.0
            );
        }
    }

    PostOrderReconciliationCheck check = make_check(
        1,
        "POST_ORDER_RECON_OK",
        reason
    );

    order_journal_record_post_order_reconciliation(
        &record,
        check.allowed,
        check.decision,
        check.reason
    );

    return check;
}

