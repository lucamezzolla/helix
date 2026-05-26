#include "post_order_reconciliation.h"

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
