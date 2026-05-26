#ifndef HELIX_ORDER_STATE_RECOVERY_H
#define HELIX_ORDER_STATE_RECOVERY_H

#include <stddef.h>

typedef enum {
    ORDER_RECOVERY_OK = 0,
    ORDER_RECOVERY_BLOCKED = 1,
    ORDER_RECOVERY_ERROR = 2
} OrderRecoveryStatus;

/*
 * Read-only recovery guard.
 *
 * This module does not place orders and does not contact Coinbase.
 * It only checks the local SQLite order_state table, when present.
 *
 * If a future real order is left in ACTIVE/PENDING/SUBMITTED state,
 * Helix must stop before making new decisions until reconciliation
 * explicitly resolves that state.
 */
OrderRecoveryStatus order_state_recovery_check(char *reason, size_t reason_size);

#endif
