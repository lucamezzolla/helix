#include "volatility_protection.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static double reference_price = 0.0;
static time_t reference_time = 0;

static VolatilityProtectionCheck make_check(
    int allowed,
    const char *decision,
    const char *reason,
    double current,
    double reference,
    double move_percent,
    int elapsed_seconds
) {
    VolatilityProtectionCheck check;

    check.allowed = allowed;
    snprintf(check.decision, sizeof(check.decision), "%s", decision ? decision : "UNKNOWN");
    snprintf(check.reason, sizeof(check.reason), "%s", reason ? reason : "Nessun dettaglio");
    check.current_price = current;
    check.reference_price = reference;
    check.move_percent = move_percent;
    check.elapsed_seconds = elapsed_seconds;

    return check;
}

void volatility_protection_reset(void) {
    reference_price = 0.0;
    reference_time = 0;
}

VolatilityProtectionCheck volatility_protection_check(
    const BotState *state,
    const StrategySettings *settings
) {
    time_t now = time(NULL);
    int elapsed_seconds = 0;
    double current_price;
    double move_percent = 0.0;
    char reason[180];

    if (state == NULL || settings == NULL) {
        return make_check(0, "BLOCKED", "Volatility: stato o settings non disponibili", 0.0, 0.0, 0.0, 0);
    }

    current_price = state->current_price;

    if (!isfinite(current_price) || current_price <= 0.0) {
        return make_check(0, "BLOCKED", "Volatility: prezzo corrente non valido", current_price, reference_price, 0.0, 0);
    }

    if (settings->volatility_window_seconds <= 0 || settings->volatility_max_move_percent <= 0.0) {
        return make_check(1, "ALLOWED", "Volatility: protezione disattivata", current_price, reference_price, 0.0, 0);
    }

    if (reference_price <= 0.0 || reference_time == 0) {
        reference_price = current_price;
        reference_time = now;

        return make_check(1, "ALLOWED", "Volatility: baseline inizializzata", current_price, reference_price, 0.0, 0);
    }

    elapsed_seconds = (int)difftime(now, reference_time);

    if (elapsed_seconds >= settings->volatility_window_seconds) {
        reference_price = current_price;
        reference_time = now;

        return make_check(1, "ALLOWED", "Volatility: baseline aggiornata", current_price, reference_price, 0.0, elapsed_seconds);
    }

    move_percent = ((current_price - reference_price) / reference_price) * 100.0;

    if (fabs(move_percent) >= settings->volatility_max_move_percent) {
        snprintf(
            reason,
            sizeof(reason),
            "Volatility: movimento %.2f%% in %d sec supera limite %.2f%%",
            move_percent,
            elapsed_seconds,
            settings->volatility_max_move_percent
        );

        return make_check(0, "BLOCKED", reason, current_price, reference_price, move_percent, elapsed_seconds);
    }

    snprintf(
        reason,
        sizeof(reason),
        "Volatility: movimento %.2f%% in %d sec entro limite %.2f%%",
        move_percent,
        elapsed_seconds,
        settings->volatility_max_move_percent
    );

    return make_check(1, "ALLOWED", reason, current_price, reference_price, move_percent, elapsed_seconds);
}
