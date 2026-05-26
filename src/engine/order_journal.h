#ifndef HELIX_ORDER_JOURNAL_H
#define HELIX_ORDER_JOURNAL_H

#include "../exchange/order_executor.h"



typedef struct {
    int dry_run_last_7_days;
    int buy_dry_run_last_7_days;
    int sell_dry_run_last_7_days;
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

#endif
