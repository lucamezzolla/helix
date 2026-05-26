#include "engine.h"
#include "wallet.h"
#include "settings.h"
#include "trade_preview.h"
#include "../db/database.h"
#include "../market/market_data.h"
#include "../wallet/wallet_info.h"
#include "../exchange/coinbase_client.h"
#include "../exchange/order_preview.h"
#include "../exchange/order_executor.h"
#include "exchange_safety.h"
#include "runtime_safety.h"
#include "reconciliation.h"
#include "volatility_protection.h"
#include "order_state_recovery.h"
#include "anti_duplicate_order.h"
#include "order_journal.h"
#include "final_live_gate.h"
#include "operational_limits.h"
#include "risk_guard.h"
#include "post_order_reconciliation.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define AUDIT_COOLDOWN_SECONDS 300
#define AUDIT_LOW_PRIORITY_COOLDOWN_SECONDS 900
#define AUDIT_THROTTLE_BUCKETS 64
#define AUDIT_CLEANUP_INTERVAL_SECONDS 3600
#define COINBASE_ORDER_PREVIEW_INTERVAL_SECONDS 300

static double estimate_fee_rate(StrategySettings *settings);
static double current_cost_basis(BotState *state);
static void audit_volatility_protection(VolatilityProtectionCheck check, BotState *state);

static void audit_runtime_safety_block(const char *event_type, RuntimeSafetyCheck safety, BotState *state);

static int is_high_priority_audit_event(const char *event_type, const char *decision) {
    if (decision && (
        strcmp(decision, "ALLOWED_SIMULATED") == 0 ||
        strcmp(decision, "ERROR") == 0 ||
        strcmp(decision, "BLOCKED") == 0
    )) {
        return 1;
    }

    if (event_type && (
        strcmp(event_type, "LIVE_TRADING") == 0 ||
        strcmp(event_type, "BUY_PREVIEW") == 0 ||
        strcmp(event_type, "SELL_PREVIEW") == 0 ||
        strcmp(event_type, "VOLATILITY_PROTECTION") == 0 ||
        strcmp(event_type, "ORDER_RECOVERY") == 0 ||
        strcmp(event_type, "ANTI_DUPLICATE_ORDER") == 0 ||
        strcmp(event_type, "FINAL_LIVE_GATE") == 0 ||
        strcmp(event_type, "OPERATIONAL_LIMITS") == 0 ||
        strcmp(event_type, "RISK_GUARD") == 0 ||
        strcmp(event_type, "POST_ORDER_RECONCILIATION") == 0
    )) {
        return 1;
    }

    return 0;
}

static void audit_engine_decision(
    const char *event_type,
    const char *decision,
    const char *reason,
    double price,
    double btc_amount,
    double eur_amount,
    double estimated_fee,
    double net_profit
) {
    typedef struct {
        char signature[512];
        time_t last_insert_time;
    } AuditThrottleEntry;

    static AuditThrottleEntry throttle_entries[AUDIT_THROTTLE_BUCKETS];
    static int next_throttle_slot = 0;

    char signature[512];
    time_t now = time(NULL);
    int high_priority = is_high_priority_audit_event(event_type, decision);
    int cooldown = high_priority ? AUDIT_COOLDOWN_SECONDS : AUDIT_LOW_PRIORITY_COOLDOWN_SECONDS;
    int free_slot = -1;
    int oldest_slot = 0;

    snprintf(
        signature,
        sizeof(signature),
        "%s|%s|%s",
        event_type ? event_type : "",
        decision ? decision : "",
        reason ? reason : ""
    );

    /*
     * Throttle globale per firma.
     *
     * La vecchia logica confrontava solo con l'audit immediatamente precedente:
     * due eventi ripetitivi alternati, ad esempio LIVE_READONLY/SYNC e
     * RECONCILIATION/OK, finivano comunque nel DB a ogni tick.
     *
     * Qui invece ogni firma event_type|decision|reason ha il proprio cooldown.
     * Gli eventi non critici vengono loggati al massimo ogni 15 minuti.
     * Gli eventi critici restano più reattivi, ma sempre protetti da spam.
     */
    for (int i = 0; i < AUDIT_THROTTLE_BUCKETS; i++) {
        if (throttle_entries[i].signature[0] == '\0') {
            if (free_slot < 0) {
                free_slot = i;
            }
            continue;
        }

        if (strcmp(throttle_entries[i].signature, signature) == 0) {
            if (
                throttle_entries[i].last_insert_time > 0 &&
                difftime(now, throttle_entries[i].last_insert_time) < cooldown
            ) {
                return;
            }

            throttle_entries[i].last_insert_time = now;
            db_log_engine_audit(
                event_type,
                decision,
                reason,
                price,
                btc_amount,
                eur_amount,
                estimated_fee,
                net_profit
            );
            return;
        }

        if (
            throttle_entries[i].last_insert_time <
            throttle_entries[oldest_slot].last_insert_time
        ) {
            oldest_slot = i;
        }
    }

    int slot = free_slot >= 0 ? free_slot : oldest_slot;

    if (free_slot < 0) {
        slot = next_throttle_slot;
        next_throttle_slot = (next_throttle_slot + 1) % AUDIT_THROTTLE_BUCKETS;
    }

    snprintf(throttle_entries[slot].signature, sizeof(throttle_entries[slot].signature), "%s", signature);
    throttle_entries[slot].last_insert_time = now;

    db_log_engine_audit(
        event_type,
        decision,
        reason,
        price,
        btc_amount,
        eur_amount,
        estimated_fee,
        net_profit
    );
}

static void audit_engine_cleanup_if_needed(StrategySettings *settings) {
    static time_t last_cleanup_time = 0;
    time_t now = time(NULL);
    int retention_days;

    if (settings == NULL) {
        return;
    }

    retention_days = settings->audit_retention_days;
    if (retention_days <= 0) {
        retention_days = 60;
    }

    if (last_cleanup_time > 0 && difftime(now, last_cleanup_time) < AUDIT_CLEANUP_INTERVAL_SECONDS) {
        return;
    }

    db_prune_engine_audits(retention_days);
    last_cleanup_time = now;
}


static void audit_volatility_protection(
    VolatilityProtectionCheck check,
    BotState *state
) {
    char reason[320];

    if (state == NULL) {
        return;
    }

    snprintf(
        reason,
        sizeof(reason),
        "%s | ref %.2f | current %.2f | move %.2f%% | elapsed %d sec",
        check.reason,
        check.reference_price,
        check.current_price,
        check.move_percent,
        check.elapsed_seconds
    );

    audit_engine_decision(
        "VOLATILITY_PROTECTION",
        check.decision,
        reason,
        state->current_price,
        state->btc_balance,
        state->eur_balance,
        0.0,
        0.0
    );
}

static void audit_runtime_safety_block(
    const char *event_type,
    RuntimeSafetyCheck safety,
    BotState *state
) {
    if (state == NULL) {
        return;
    }

    snprintf(
        state->last_trade,
        sizeof(state->last_trade),
        "%.120s",
        safety.reason
    );

    audit_engine_decision(
        event_type,
        safety.decision,
        safety.reason,
        state->current_price,
        state->btc_balance,
        state->eur_balance,
        0.0,
        0.0
    );
}



static void audit_reconciliation_report(
    ReconciliationReport report,
    BotState *state
) {
    char reason_message[121];
    char reason[320];

    if (state == NULL) {
        return;
    }

    snprintf(reason_message, sizeof(reason_message), "%.120s", report.reason);

    snprintf(
        reason,
        sizeof(reason),
        "%s | EUR delta %.6f | BTC delta %.10f | cost %.2f | avg %.2f",
        reason_message,
        report.eur_delta,
        report.btc_delta,
        report.reconstructed_cost_basis,
        report.reconstructed_avg_buy_price
    );

    audit_engine_decision(
        "RECONCILIATION",
        report.decision,
        reason,
        state->current_price,
        state->btc_balance,
        state->eur_balance,
        0.0,
        0.0
    );
}

static void audit_order_execution_plan(
    const char *event_type,
    OrderExecutionPlan plan,
    BotState *state,
    StrategySettings *settings
) {
    if (state == NULL) {
        return;
    }

    if (plan.allowed) {
        AntiDuplicateOrderCheck duplicate_check =
            anti_duplicate_order_check_plan(&plan, AUDIT_COOLDOWN_SECONDS);

        if (!duplicate_check.allowed) {
            audit_engine_decision(
                "ANTI_DUPLICATE_ORDER",
                duplicate_check.decision,
                duplicate_check.reason,
                state->current_price,
                plan.side == ORDER_EXECUTOR_SIDE_SELL ? plan.requested_base_size : plan.preview_base_size,
                plan.side == ORDER_EXECUTOR_SIDE_BUY ? plan.requested_quote_size : plan.preview_total_eur,
                plan.preview_fee_eur,
                0.0
            );
            return;
        }

        FinalLiveGateCheck final_gate = final_live_gate_check_dry_run(
            state,
            settings,
            &plan
        );

        if (!final_gate.allowed) {
            audit_engine_decision(
                "FINAL_LIVE_GATE",
                final_gate.decision,
                final_gate.reason,
                state->current_price,
                plan.side == ORDER_EXECUTOR_SIDE_SELL ? plan.requested_base_size : plan.preview_base_size,
                plan.side == ORDER_EXECUTOR_SIDE_BUY ? plan.requested_quote_size : plan.preview_total_eur,
                plan.preview_fee_eur,
                0.0
            );

            order_journal_record_execution_plan(
                &plan,
                "PRE_EXECUTION",
                "FINAL_GATE_BLOCKED"
            );
            return;
        }

        anti_duplicate_order_record_dry_run_plan(&plan);
        order_journal_record_execution_plan(
            &plan,
            "PRE_EXECUTION",
            "FINAL_GATE_OK"
        );

        PostOrderReconciliationCheck post_order =
            post_order_reconciliation_check_after_plan(
                state,
                settings,
                &plan
            );

        audit_engine_decision(
            "POST_ORDER_RECONCILIATION",
            post_order.decision,
            post_order.reason,
            state->current_price,
            plan.side == ORDER_EXECUTOR_SIDE_SELL ? plan.requested_base_size : plan.preview_base_size,
            plan.side == ORDER_EXECUTOR_SIDE_BUY ? plan.requested_quote_size : plan.preview_total_eur,
            plan.preview_fee_eur,
            0.0
        );

        if (!post_order.allowed) {
            order_journal_record_execution_plan(
                &plan,
                "POST_EXECUTION",
                "POST_ORDER_RECON_BLOCKED"
            );
            return;
        }

        order_journal_record_execution_plan(
            &plan,
            "POST_EXECUTION",
            "POST_ORDER_RECON_OK"
        );
    } else {
        order_journal_record_execution_plan(
            &plan,
            "PRE_EXECUTION",
            "PLAN_BLOCKED"
        );
    }

    audit_engine_decision(
        event_type,
        plan.allowed ? "DRY_RUN_READY" : "DRY_RUN_BLOCKED",
        plan.reason,
        state->current_price,
        plan.side == ORDER_EXECUTOR_SIDE_SELL ? plan.requested_base_size : plan.preview_base_size,
        plan.side == ORDER_EXECUTOR_SIDE_BUY ? plan.requested_quote_size : plan.preview_total_eur,
        plan.preview_fee_eur,
        0.0
    );
}


static void record_blocked_preview_candidate(
    OrderExecutionPlan plan,
    const char *decision,
    const char *reason
) {
    plan.allowed = 0;

    if (reason != NULL && reason[0] != '\0') {
        snprintf(plan.reason, sizeof(plan.reason), "%.255s", reason);
    }

    order_journal_record_execution_plan(
        &plan,
        "PREVIEW",
        decision ? decision : "CANDIDATE_BLOCKED"
    );
}

static void audit_coinbase_order_preview_if_needed(
    BotState *state,
    StrategySettings *settings
) {
    static time_t last_preview_time = 0;
    time_t now = time(NULL);

    if (!state || !settings) {
        return;
    }

    if (last_preview_time > 0 && difftime(now, last_preview_time) < COINBASE_ORDER_PREVIEW_INTERVAL_SECONDS) {
        return;
    }

    if (state->current_price <= 0.0) {
        return;
    }

    last_preview_time = now;

    if (state->btc_balance > 0.0) {
        double cost_basis = current_cost_basis(state);
        TradePreview local_preview = trade_preview_sell(
            state->btc_balance,
            state->current_price,
            cost_basis,
            estimate_fee_rate(settings),
            settings->min_profit_eur,
            settings->min_profit_percent
        );
        CoinbaseOrderPreview preview =
            coinbase_order_preview_market_sell_btc("BTC-EUR", state->btc_balance);
        ExchangeSafetyCheck safety = exchange_safety_check_sell(
            local_preview,
            preview,
            state->btc_balance
        );

        audit_engine_decision(
            "EXCHANGE_SAFETY_SELL",
            safety.decision,
            safety.reason,
            state->current_price,
            state->btc_balance,
            safety.exchange_total_eur,
            safety.exchange_fee_eur,
            local_preview.net_profit
        );

        {
            OrderExecutionPlan plan = order_executor_plan_market_sell_dry_run(
                "BTC-EUR",
                state->btc_balance,
                preview
            );

            if (safety.allowed) {
                audit_order_execution_plan("ORDER_EXECUTOR_SELL", plan, state, settings);
            } else {
                char blocked_reason[256];

                snprintf(
                    blocked_reason,
                    sizeof(blocked_reason),
                    "SELL candidate bloccato | %.140s | net %.2f | profit %.2f EUR %.2f%%",
                    safety.reason,
                    local_preview.net_value,
                    local_preview.net_profit,
                    local_preview.net_profit_percent
                );

                record_blocked_preview_candidate(
                    plan,
                    "SELL_CANDIDATE_BLOCKED",
                    blocked_reason
                );
            }
        }

        return;
    }

    if (state->eur_balance >= settings->slot_amount_eur && settings->slot_amount_eur > 0.0) {
        RuntimeSafetyCheck runtime_safety = runtime_safety_check_buy(
            state,
            settings,
            settings->slot_amount_eur
        );

        if (!runtime_safety.allowed) {
            audit_runtime_safety_block("RUNTIME_SAFETY_BUY_PREVIEW", runtime_safety, state);
            return;
        }

        TradePreview local_preview = trade_preview_buy(
            settings->slot_amount_eur,
            state->current_price,
            estimate_fee_rate(settings)
        );
        CoinbaseOrderPreview preview =
            coinbase_order_preview_market_buy_eur("BTC-EUR", settings->slot_amount_eur);
        ExchangeSafetyCheck safety = exchange_safety_check_buy(
            local_preview,
            preview,
            settings->slot_amount_eur
        );

        audit_engine_decision(
            "EXCHANGE_SAFETY_BUY",
            safety.decision,
            safety.reason,
            state->current_price,
            safety.exchange_btc_amount,
            settings->slot_amount_eur,
            safety.exchange_fee_eur,
            0.0
        );

        {
            OrderExecutionPlan plan = order_executor_plan_market_buy_dry_run(
                "BTC-EUR",
                settings->slot_amount_eur,
                preview
            );

            if (safety.allowed) {
                audit_order_execution_plan("ORDER_EXECUTOR_BUY", plan, state, settings);
            } else {
                char blocked_reason[256];

                snprintf(
                    blocked_reason,
                    sizeof(blocked_reason),
                    "BUY candidate bloccato | %.150s | quote %.2f | fee %.2f",
                    safety.reason,
                    settings->slot_amount_eur,
                    safety.exchange_fee_eur
                );

                record_blocked_preview_candidate(
                    plan,
                    "BUY_CANDIDATE_BLOCKED",
                    blocked_reason
                );
            }
        }
    }
}

static int price_dropped_enough(BotState *state, StrategySettings *settings) {
    if (state->last_buy_price <= 0.0) {
        return 1;
    }

    double trigger_price =
        state->last_buy_price * (1.0 - settings->buy_drop_percent / 100.0);

    return state->current_price <= trigger_price;
}

static int price_high_enough_to_sell(BotState *state, StrategySettings *settings) {
    if (state->avg_buy_price <= 0.0) {
        return 0;
    }

    double target_price =
        state->avg_buy_price * (1.0 + settings->sell_profit_percent / 100.0);

    return state->current_price >= target_price;
}

static double estimate_fee_rate(StrategySettings *settings) {
    if (settings->estimated_fee_percent <= 0.0) {
        return 0.0;
    }

    return settings->estimated_fee_percent / 100.0;
}

static double current_cost_basis(BotState *state) {
    if (state->btc_balance <= 0.0 || state->avg_buy_price <= 0.0) {
        return 0.0;
    }

    return state->btc_balance * state->avg_buy_price;
}

static void apply_simulated_sell_all(BotState *state, TradePreview *preview) {
    state->eur_balance += preview->net_value;
    state->btc_balance = 0.0;
    state->used_slots = 0;
    state->last_buy_price = 0.0;
    state->avg_buy_price = 0.0;
}

static void apply_simulated_buy(BotState *state, double eur_amount, TradePreview *preview) {
    double old_cost_basis = current_cost_basis(state);
    double new_btc_total = state->btc_balance + preview->net_value;
    double new_cost_basis = old_cost_basis + eur_amount;

    state->eur_balance -= eur_amount;
    state->btc_balance = new_btc_total;
    state->used_slots++;
    state->last_buy_price = state->current_price;

    if (new_btc_total > 0.0) {
        state->avg_buy_price = new_cost_basis / new_btc_total;
    }
}

static void sync_state_from_remote_wallet(
    BotState *state,
    WalletInfo *remote_wallet,
    CoinbasePositionSummary *position_summary
) {
    state->eur_balance = remote_wallet->eur_balance;
    state->btc_balance = remote_wallet->btc_balance;

    if (
        position_summary &&
        position_summary->connected &&
        position_summary->btc_open > 0.0 &&
        position_summary->avg_buy_price > 0.0
    ) {
        state->avg_buy_price = position_summary->avg_buy_price;
    }

    if (state->btc_balance > 0.0) {
        if (state->eur_balance < 1.0) {
            state->used_slots = state->max_slots;
        } else if (state->used_slots <= 0) {
            state->used_slots = 1;
        }

        state->mode = BOT_MODE_WAITING_SELL;

        if (position_summary && position_summary->connected && state->avg_buy_price > 0.0) {
            snprintf(
                state->last_trade,
                sizeof(state->last_trade),
                "LIVE_READONLY sync: BTC rilevato | avg %.2f EUR | cost %.2f EUR",
                state->avg_buy_price,
                current_cost_basis(state)
            );
        } else {
            snprintf(
                state->last_trade,
                sizeof(state->last_trade),
                "LIVE_READONLY sync: posizione BTC rilevata, costo non ricostruito"
            );
        }
    } else {
        state->used_slots = 0;
        state->mode = BOT_MODE_READY;

        snprintf(
            state->last_trade,
            sizeof(state->last_trade),
            "LIVE_READONLY sync: nessuna posizione BTC"
        );
    }
}


static int enforce_micro_live_stop_after_real_order(
    BotState *state,
    StrategySettings *settings
) {
    if (state == NULL || settings == NULL) {
        return 0;
    }

    if (!settings->micro_live_stop_after_real_order) {
        return 0;
    }

    if (order_journal_real_sent_last_24h() <= 0) {
        return 0;
    }

    PostOrderReconciliationCheck post_recon =
        post_order_reconciliation_check_latest_real_order(state, settings);

    if (strcmp(post_recon.decision, "NO_REAL_ORDER") != 0) {
        audit_engine_decision(
            "POST_ORDER_RECONCILIATION",
            post_recon.decision,
            post_recon.reason,
            state->current_price,
            state->btc_balance,
            state->eur_balance,
            0.0,
            0.0
        );
    }

    if (!state->running && !settings->live_trading_armed) {
        return 1;
    }

    state->running = false;
    state->mode = BOT_MODE_PAUSED;

    snprintf(
        state->last_trade,
        sizeof(state->last_trade),
        "Micro-live: stop automatico dopo ordine reale"
    );

    if (settings->live_trading_armed) {
        settings->live_trading_armed = 0;
        settings_save(settings);
    }

    db_save_state(state);

    audit_engine_decision(
        "MICRO_LIVE",
        "STOP_AFTER_REAL_ORDER",
        "Bot fermato e LIVE_TRADING disarmato dopo ordine reale registrato nel journal",
        state->current_price,
        state->btc_balance,
        state->eur_balance,
        0.0,
        0.0
    );

    return 1;
}

void helix_engine_tick(BotState *state) {
    static int order_journal_ready = 0;
    StrategySettings settings = settings_load();

    if (!order_journal_ready) {
        order_journal_ready = order_journal_init();
    }

    audit_engine_cleanup_if_needed(&settings);

    if (enforce_micro_live_stop_after_real_order(state, &settings)) {
        return;
    }

    if (!state->running) {
        state->mode = BOT_MODE_PAUSED;
        return;
    }

    state->max_slots = settings.max_slots;
    state->current_price = market_data_get_price(state);

    char recovery_reason[256];

    if (order_state_recovery_check(
            recovery_reason,
            sizeof(recovery_reason)
        ) != ORDER_RECOVERY_OK) {

        state->mode = BOT_MODE_ERROR;
        snprintf(
            state->last_trade,
            sizeof(state->last_trade),
            "%.120s",
            recovery_reason
        );

        audit_engine_decision(
            "ORDER_RECOVERY",
            "BLOCKED",
            recovery_reason,
            state->current_price,
            state->btc_balance,
            state->eur_balance,
            0.0,
            0.0
        );

        return;
    }

    VolatilityProtectionCheck volatility = volatility_protection_check(state, &settings);
    if (!volatility.allowed) {
        state->mode = BOT_MODE_ERROR;
        snprintf(
            state->last_trade,
            sizeof(state->last_trade),
            "%.120s",
            volatility.reason
        );
        audit_volatility_protection(volatility, state);
        return;
    }

    if (settings.runtime_mode == RUNTIME_MODE_LIVE_READONLY) {
        WalletInfo remote_wallet =
            coinbase_get_wallet_info_readonly();

        CoinbasePositionSummary position_summary =
            coinbase_get_btc_eur_position_summary_readonly();

        if (remote_wallet.connected) {
            ReconciliationReport reconciliation;

            sync_state_from_remote_wallet(state, &remote_wallet, &position_summary);

            audit_engine_decision(
                "LIVE_READONLY",
                "SYNC",
                state->last_trade,
                state->current_price,
                state->btc_balance,
                state->eur_balance,
                0.0,
                0.0
            );

            reconciliation = reconciliation_check_live_readonly(
                state,
                &remote_wallet,
                &position_summary
            );
            audit_reconciliation_report(reconciliation, state);

            if (reconciliation.status == RECONCILIATION_STATUS_BLOCKED) {
                state->mode = BOT_MODE_ERROR;
                snprintf(
                    state->last_trade,
                    sizeof(state->last_trade),
                    "%.120s",
                    reconciliation.reason
                );
                return;
            }

            audit_coinbase_order_preview_if_needed(state, &settings);
        } else {
            state->mode = BOT_MODE_ERROR;

            snprintf(
                state->last_trade,
                sizeof(state->last_trade),
                "LIVE_READONLY errore: wallet remoto non connesso"
            );

            audit_engine_decision(
                "LIVE_READONLY",
                "ERROR",
                state->last_trade,
                state->current_price,
                state->btc_balance,
                state->eur_balance,
                0.0,
                0.0
            );
        }

        return;
    }

    if (settings.runtime_mode == RUNTIME_MODE_LIVE_TRADING) {
        RuntimeSafetyCheck live_safety = runtime_safety_check_live_trading_arm(&settings);

        state->mode = BOT_MODE_ERROR;
        audit_runtime_safety_block("LIVE_TRADING", live_safety, state);

        return;
    }

    RiskGuardCheck risk_guard = risk_guard_check(state, &settings);
    if (!risk_guard.allowed) {
        state->mode = BOT_MODE_ERROR;
        snprintf(
            state->last_trade,
            sizeof(state->last_trade),
            "Risk guard: %.100s",
            risk_guard.reason
        );
        risk_guard_audit_if_blocked(
            risk_guard,
            state->current_price,
            state->btc_balance,
            state->eur_balance
        );
        return;
    }

    state->mode = BOT_MODE_READY;

    if (
        wallet_can_sell(state) &&
        price_high_enough_to_sell(state, &settings)
    ) {
        RuntimeSafetyCheck safety = runtime_safety_check_sell(
            state,
            &settings,
            state->btc_balance
        );
        double btc_before = state->btc_balance;
        double price = state->current_price;
        double cost_basis = current_cost_basis(state);

        if (!safety.allowed) {
            state->mode = BOT_MODE_WAITING_SELL;
            audit_runtime_safety_block("RUNTIME_SAFETY_SELL", safety, state);
            return;
        }

        OperationalLimitCheck limits = operational_limits_check(
            &settings,
            ORDER_EXECUTOR_SIDE_SELL
        );

        if (!limits.allowed) {
            state->mode = BOT_MODE_WAITING_SELL;
            snprintf(
                state->last_trade,
                sizeof(state->last_trade),
                "SELL bloccata: %.90s",
                limits.reason
            );
            operational_limits_audit_if_blocked(
                limits,
                state->current_price,
                state->btc_balance,
                state->eur_balance
            );
            return;
        }

        TradePreview preview = trade_preview_sell(
            btc_before,
            price,
            cost_basis,
            estimate_fee_rate(&settings),
            settings.min_profit_eur,
            settings.min_profit_percent
        );

        if (!preview.allowed) {
            state->mode = BOT_MODE_WAITING_SELL;

            snprintf(
                state->last_trade,
                sizeof(state->last_trade),
                "SELL preview bloccata: netto %.2f | profit %.2f EUR %.2f%%",
                preview.net_value,
                preview.net_profit,
                preview.net_profit_percent
            );

            audit_engine_decision(
                "SELL_PREVIEW",
                "BLOCKED",
                state->last_trade,
                price,
                btc_before,
                preview.net_value,
                preview.estimated_fee,
                preview.net_profit
            );

            return;
        }

        state->mode = BOT_MODE_SELLING;

        apply_simulated_sell_all(state, &preview);

        snprintf(
            state->last_trade,
            sizeof(state->last_trade),
            "SELL preview OK @ %.2f | profit %.2f EUR",
            price,
            preview.net_profit
        );

        db_log_trade(
            "SELL",
            price,
            preview.net_value,
            btc_before
        );

        audit_engine_decision(
            "SELL_PREVIEW",
            "ALLOWED_SIMULATED",
            state->last_trade,
            price,
            btc_before,
            preview.net_value,
            preview.estimated_fee,
            preview.net_profit
        );

        state->mode = BOT_MODE_READY;

        return;
    }

    if (price_dropped_enough(state, &settings)) {
        RuntimeSafetyCheck safety = runtime_safety_check_buy(
            state,
            &settings,
            settings.slot_amount_eur
        );
        double price = state->current_price;

        if (!safety.allowed) {
            state->mode = BOT_MODE_READY;
            audit_runtime_safety_block("RUNTIME_SAFETY_BUY", safety, state);
            return;
        }

        OperationalLimitCheck limits = operational_limits_check(
            &settings,
            ORDER_EXECUTOR_SIDE_BUY
        );

        if (!limits.allowed) {
            state->mode = BOT_MODE_READY;
            snprintf(
                state->last_trade,
                sizeof(state->last_trade),
                "BUY bloccato: %.90s",
                limits.reason
            );
            operational_limits_audit_if_blocked(
                limits,
                state->current_price,
                state->btc_balance,
                state->eur_balance
            );
            return;
        }

        TradePreview preview = trade_preview_buy(
            settings.slot_amount_eur,
            price,
            estimate_fee_rate(&settings)
        );

        if (!preview.allowed) {
            state->mode = BOT_MODE_READY;

            snprintf(
                state->last_trade,
                sizeof(state->last_trade),
                "BUY preview bloccata: fee stimata non valida"
            );

            audit_engine_decision(
                "BUY_PREVIEW",
                "BLOCKED",
                state->last_trade,
                price,
                preview.net_value,
                settings.slot_amount_eur,
                preview.estimated_fee,
                0.0
            );

            return;
        }

        state->mode = BOT_MODE_BUYING;

        apply_simulated_buy(state, settings.slot_amount_eur, &preview);

        snprintf(
            state->last_trade,
            sizeof(state->last_trade),
            "BUY preview OK @ %.2f | fee %.2f EUR",
            price,
            preview.estimated_fee
        );

        db_log_trade(
            "BUY",
            price,
            settings.slot_amount_eur,
            preview.net_value
        );

        audit_engine_decision(
            "BUY_PREVIEW",
            "ALLOWED_SIMULATED",
            state->last_trade,
            price,
            preview.net_value,
            settings.slot_amount_eur,
            preview.estimated_fee,
            0.0
        );

        state->mode = BOT_MODE_WAITING_SELL;

        return;
    }

    if (state->btc_balance > 0.0) {
        state->mode = BOT_MODE_WAITING_SELL;
    }
}
