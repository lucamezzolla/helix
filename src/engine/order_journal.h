#ifndef HELIX_ORDER_JOURNAL_H
#define HELIX_ORDER_JOURNAL_H

#include "../exchange/order_executor.h"




typedef struct {
    int found;
    char client_order_id[96];
    char coinbase_order_id[128];
    char side[8];
    char product_id[32];
    char status[32];
    char created_at[32];
} RealOrderJournalRecord;


typedef struct {
    int found;
    char client_order_id[96];
    char coinbase_order_id[128];
    double quote_size_eur;
    double fee_eur;
    double base_size_btc;
    double avg_price_eur;
} RealBuySlotRecord;


typedef struct {
    int dry_run_last_7_days;
    int dry_run_ready_last_7_days;
    int dry_run_blocked_last_7_days;
    int buy_dry_run_last_7_days;
    int buy_dry_run_ready_last_7_days;
    int buy_dry_run_blocked_last_7_days;
    int sell_dry_run_last_7_days;
    int sell_dry_run_ready_last_7_days;
    int sell_dry_run_blocked_last_7_days;
    int sell_blocked_not_profitable_last_7_days;
    int journal_entries_last_24h;
    int real_sent_last_7_days;
    int final_gate_ok_last_7_days;
    int final_gate_blocked_last_7_days;
    int real_executor_blocked_last_7_days;
    int post_order_recon_ok_last_7_days;
    int post_order_recon_blocked_last_7_days;
    char last_block_reason[256];
    char last_blocked_at[32];
    char last_client_order_id[96];
    char last_journal_at[32];
} PreliveReport;

int order_journal_init(void);

int order_journal_record_execution_plan(
    const OrderExecutionPlan *plan,
    const char *phase,
    const char *decision
);

int order_journal_record_execution_result(
    const OrderExecutionPlan *plan,
    const OrderExecutionResult *result,
    const char *phase
);

int order_journal_get_prelive_report(PreliveReport *report);

int order_journal_real_sent_last_24h(void);

int order_journal_get_latest_real_sent_order_id(
    char *buffer,
    int buffer_size
);

int order_journal_get_latest_unreconciled_real_order(
    RealOrderJournalRecord *record
);

int order_journal_get_latest_real_buy_slot(
    RealBuySlotRecord *slot
);

int order_journal_record_post_order_reconciliation(
    const RealOrderJournalRecord *record,
    int allowed,
    const char *decision,
    const char *reason
);


#endif
