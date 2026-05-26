#ifndef HELIX_ANTI_DUPLICATE_ORDER_H
#define HELIX_ANTI_DUPLICATE_ORDER_H

#include "../exchange/order_executor.h"

#include <stddef.h>

typedef struct {
    int allowed;
    char decision[32];
    char reason[256];
} AntiDuplicateOrderCheck;

AntiDuplicateOrderCheck anti_duplicate_order_check_plan(
    const OrderExecutionPlan *plan,
    int cooldown_seconds
);

int anti_duplicate_order_record_dry_run_plan(const OrderExecutionPlan *plan);

#endif
