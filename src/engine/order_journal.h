#ifndef HELIX_ORDER_JOURNAL_H
#define HELIX_ORDER_JOURNAL_H

#include "../exchange/order_executor.h"

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

#endif
