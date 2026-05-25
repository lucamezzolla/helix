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

#include <stdio.h>
#include <string.h>
#include <time.h>

#define AUDIT_COOLDOWN_SECONDS 300
#define AUDIT_CLEANUP_INTERVAL_SECONDS 3600
#define COINBASE_ORDER_PREVIEW_INTERVAL_SECONDS 300

static double estimate_fee_rate(StrategySettings *settings);
static double current_cost_basis(BotState *state);
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
        strcmp(event_type, "SELL_PREVIEW") == 0
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
    static char last_signature[512] = "";
    static time_t last_insert_time = 0;
    char signature[512];
    time_t now = time(NULL);
    int same_signature;
    int high_priority;

    snprintf(
        signature,
        sizeof(signature),
        "%s|%s|%s",
        event_type ? event_type : "",
        decision ? decision : "",
        reason ? reason : ""
    );

    same_signature = strcmp(signature, last_signature) == 0;
    high_priority = is_high_priority_audit_event(event_type, decision);

    /*
     * Regola anti-spam:
     * - se la decisione è identica alla precedente, non riscrivere ogni tick;
     * - per gli eventi importanti permettiamo comunque un nuovo log dopo 5 minuti;
     * - per stati ripetitivi tipo SYNC/WAITING, logghiamo solo quando cambia il messaggio.
     */
    if (same_signature) {
        if (!high_priority) {
            return;
        }

        if (last_insert_time > 0 && difftime(now, last_insert_time) < AUDIT_COOLDOWN_SECONDS) {
            return;
        }
    }

    snprintf(last_signature, sizeof(last_signature), "%s", signature);
    last_insert_time = now;

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


static void audit_order_execution_plan(
    const char *event_type,
    OrderExecutionPlan plan,
    BotState *state
) {
    if (state == NULL) {
        return;
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

        if (safety.allowed) {
            OrderExecutionPlan plan = order_executor_plan_market_sell_dry_run(
                "BTC-EUR",
                state->btc_balance,
                preview
            );
            audit_order_execution_plan("ORDER_EXECUTOR_SELL", plan, state);
        }

        return;
    }

    if (state->eur_balance >= settings->slot_amount_eur && settings->slot_amount_eur > 0.0) {
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

        if (safety.allowed) {
            OrderExecutionPlan plan = order_executor_plan_market_buy_dry_run(
                "BTC-EUR",
                settings->slot_amount_eur,
                preview
            );
            audit_order_execution_plan("ORDER_EXECUTOR_BUY", plan, state);
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
                position_summary->cost_basis_eur
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

void helix_engine_tick(BotState *state) {
    StrategySettings settings = settings_load();

    audit_engine_cleanup_if_needed(&settings);

    if (!state->running) {
        state->mode = BOT_MODE_PAUSED;
        return;
    }

    state->max_slots = settings.max_slots;
    state->current_price = market_data_get_price(state);

    if (settings.runtime_mode == RUNTIME_MODE_LIVE_READONLY) {
        WalletInfo remote_wallet =
            coinbase_get_wallet_info_readonly();

        CoinbasePositionSummary position_summary =
            coinbase_get_btc_eur_position_summary_readonly();

        if (remote_wallet.connected) {
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

    if (
        wallet_can_buy(
            state,
            settings.slot_amount_eur,
            settings.min_liquidity_percent
        ) &&
        price_dropped_enough(state, &settings)
    ) {
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
