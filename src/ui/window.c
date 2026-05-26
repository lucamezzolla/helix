#include "window.h"
#include "../engine/bot_state.h"
#include "../engine/settings.h"
#include "../db/database.h"
#include "../engine/engine.h"
#include "../engine/emergency_stop.h"
#include "../engine/live_readiness.h"
#include "../engine/api_health.h"
#include "../exchange/coinbase_client.h"
#include "../wallet/wallet_info.h"
#include "../config/env_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRADE_HISTORY_LIMIT 8
#define ENGINE_AUDIT_LIMIT 8

static double parse_decimal_input(const char *value) {
    char buffer[64];
    size_t i;

    if (value == NULL || value[0] == '\0') {
        return 0.0;
    }

    snprintf(buffer, sizeof(buffer), "%s", value);

    for (i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == ',') {
            buffer[i] = '.';
        }
    }

    return strtod(buffer, NULL);
}

typedef struct {
    BotState *state;
    GtkWidget *window;
    guint timer_id;
    gboolean shutting_down;
    GtkWidget *price_label;
    GtkWidget *eur_label;
    GtkWidget *btc_label;
    GtkWidget *slots_label;
    GtkWidget *mode_label;
    GtkWidget *runtime_mode_label;
    GtkWidget *live_readiness_label;
    GtkWidget *api_health_label;
    GtkWidget *coinbase_credentials_label;
    GtkWidget *remote_wallet_label;
    GtkWidget *last_trade_label;
    GtkWidget *settings_label;
    GtkWidget *status_label;
    GtkWidget *settings_expander;
    GtkWidget *coinbase_expander;
    GtkWidget *history_title;
    GtkWidget *history_scrolled_window;
    GtkWidget *audit_title;
    GtkWidget *audit_scrolled_window;
    GtkWidget *trade_list;
    GtkWidget *audit_list;

    GtkWidget *slot_amount_entry;
    GtkWidget *buy_drop_entry;
    GtkWidget *sell_profit_entry;
    GtkWidget *estimated_fee_entry;
    GtkWidget *min_profit_eur_entry;
    GtkWidget *min_profit_percent_entry;
    GtkWidget *min_liquidity_entry;
    GtkWidget *liquidity_reserve_entry;
    GtkWidget *reserve_released_slots_entry;
    GtkWidget *max_slots_entry;
    GtkWidget *audit_retention_days_entry;
    GtkWidget *volatility_window_seconds_entry;
    GtkWidget *volatility_max_move_percent_entry;
    GtkWidget *max_orders_per_day_entry;
    GtkWidget *order_cooldown_seconds_entry;
    GtkWidget *max_daily_loss_eur_entry;
    GtkWidget *max_drawdown_percent_entry;
    GtkWidget *emergency_stop_label;
    GtkWidget *live_trading_arm_label;
    GtkWidget *live_trading_arm_buttons_box;
    GtkWidget *arm_live_trading_button;
    GtkWidget *disarm_live_trading_button;
    GtkWidget *runtime_mode_dropdown;

    GtkWidget *coinbase_api_key_entry;
    GtkWidget *coinbase_api_secret_entry;
} AppWidgets;

static void load_app_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();

    gtk_css_provider_load_from_string(
        provider,
        ".status-running {"
        "  color: #008000;"
        "  font-weight: bold;"
        "}"
        ".status-stopped {"
        "  color: #cc0000;"
        "  font-weight: bold;"
        "}"
        ".credentials-ok {"
        "  color: #008000;"
        "  font-weight: bold;"
        "}"
        ".credentials-missing {"
        "  color: #cc8800;"
        "  font-weight: bold;"
        "}"
    );

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    g_object_unref(provider);
}

static const char *bot_mode_to_string(BotMode mode) {
    switch (mode) {
        case BOT_MODE_PAUSED:
            return "PAUSED";
        case BOT_MODE_READY:
            return "READY";
        case BOT_MODE_BUYING:
            return "BUYING";
        case BOT_MODE_WAITING_SELL:
            return "WAITING_SELL";
        case BOT_MODE_SELLING:
            return "SELLING";
        case BOT_MODE_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

static void free_app_widgets(gpointer data) {
    AppWidgets *widgets = data;

    if (widgets == NULL) {
        return;
    }

    widgets->shutting_down = TRUE;

    if (widgets->timer_id != 0) {
        g_source_remove(widgets->timer_id);
        widgets->timer_id = 0;
    }

    if (widgets->state != NULL) {
        g_free(widgets->state);
        widgets->state = NULL;
    }

    g_free(widgets);
}

static gboolean on_window_close_request(GtkWindow *window, gpointer user_data) {
    (void)window;

    AppWidgets *widgets = user_data;

    if (widgets != NULL) {
        widgets->shutting_down = TRUE;

        if (widgets->timer_id != 0) {
            g_source_remove(widgets->timer_id);
            widgets->timer_id = 0;
        }
    }

    return FALSE;
}

static void refresh_status(AppWidgets *widgets) {
    GtkWidget *label = widgets->status_label;

    gtk_widget_remove_css_class(label, "status-running");
    gtk_widget_remove_css_class(label, "status-stopped");

    if (widgets->state->running) {
        gtk_label_set_text(GTK_LABEL(label), "Stato: bot avviato");
        gtk_widget_add_css_class(label, "status-running");
    } else {
        gtk_label_set_text(GTK_LABEL(label), "Stato: bot fermo");
        gtk_widget_add_css_class(label, "status-stopped");
    }
}

static void refresh_coinbase_credentials_status(AppWidgets *widgets) {
    GtkWidget *label = widgets->coinbase_credentials_label;

    gtk_widget_remove_css_class(label, "credentials-ok");
    gtk_widget_remove_css_class(label, "credentials-missing");

    if (coinbase_has_credentials()) {
        gtk_label_set_text(GTK_LABEL(label), "Credenziali Coinbase: presenti");
        gtk_widget_add_css_class(label, "credentials-ok");
    } else {
        gtk_label_set_text(GTK_LABEL(label), "Credenziali Coinbase: mancanti");
        gtk_widget_add_css_class(label, "credentials-missing");
    }
}

static void refresh_remote_wallet_status(AppWidgets *widgets) {
    WalletInfo info = coinbase_get_wallet_info_readonly();

    char text[256];

    if (info.connected) {
        snprintf(
            text,
            sizeof(text),
            "Wallet Coinbase read-only: connesso | EUR %.2f | BTC %.8f",
            info.eur_balance,
            info.btc_balance
        );
    } else {
        snprintf(
            text,
            sizeof(text),
            "Wallet Coinbase read-only: non connesso"
        );
    }

    gtk_label_set_text(GTK_LABEL(widgets->remote_wallet_label), text);
}

static void clear_trade_list(GtkWidget *trade_list) {
    GtkWidget *child;

    while ((child = gtk_widget_get_first_child(trade_list)) != NULL) {
        gtk_list_box_remove(GTK_LIST_BOX(trade_list), child);
    }
}

static void refresh_trade_history(AppWidgets *widgets) {
    TradeRecord trades[TRADE_HISTORY_LIMIT];
    int count = db_get_recent_trades(trades, TRADE_HISTORY_LIMIT);

    clear_trade_list(widgets->trade_list);

    if (count == 0) {
        GtkWidget *row_label = gtk_label_new("Nessuna operazione registrata");
        gtk_widget_set_halign(row_label, GTK_ALIGN_START);
        gtk_list_box_append(GTK_LIST_BOX(widgets->trade_list), row_label);
        return;
    }

    for (int i = 0; i < count; i++) {
        char row_text[256];

        snprintf(
            row_text,
            sizeof(row_text),
            "#%d | %s | %.2f EUR | %.2f EUR | %.8f BTC | %s",
            trades[i].id,
            trades[i].type,
            trades[i].price,
            trades[i].eur_amount,
            trades[i].btc_amount,
            trades[i].created_at
        );

        GtkWidget *row_label = gtk_label_new(row_text);
        gtk_widget_set_halign(row_label, GTK_ALIGN_START);
        gtk_widget_set_margin_top(row_label, 4);
        gtk_widget_set_margin_bottom(row_label, 4);
        gtk_widget_set_margin_start(row_label, 6);
        gtk_widget_set_margin_end(row_label, 6);

        gtk_list_box_append(GTK_LIST_BOX(widgets->trade_list), row_label);
    }
}

static void refresh_engine_audit(AppWidgets *widgets) {
    EngineAuditRecord records[ENGINE_AUDIT_LIMIT];
    int count = db_get_recent_engine_audits(records, ENGINE_AUDIT_LIMIT);

    clear_trade_list(widgets->audit_list);

    if (count == 0) {
        GtkWidget *row_label = gtk_label_new("Nessuna decisione motore registrata");
        gtk_widget_set_halign(row_label, GTK_ALIGN_START);
        gtk_list_box_append(GTK_LIST_BOX(widgets->audit_list), row_label);
        return;
    }

    for (int i = 0; i < count; i++) {
        char row_text[512];

        snprintf(
            row_text,
            sizeof(row_text),
            "#%d | %s | %s | prezzo %.2f | BTC %.8f | EUR %.2f | fee %.2f | profit %.2f | %s | %s",
            records[i].id,
            records[i].event_type,
            records[i].decision,
            records[i].price,
            records[i].btc_amount,
            records[i].eur_amount,
            records[i].estimated_fee,
            records[i].net_profit,
            records[i].created_at,
            records[i].reason
        );

        GtkWidget *row_label = gtk_label_new(row_text);
        gtk_widget_set_halign(row_label, GTK_ALIGN_START);
        gtk_label_set_wrap(GTK_LABEL(row_label), TRUE);
        gtk_widget_set_margin_top(row_label, 4);
        gtk_widget_set_margin_bottom(row_label, 4);
        gtk_widget_set_margin_start(row_label, 6);
        gtk_widget_set_margin_end(row_label, 6);

        gtk_list_box_append(GTK_LIST_BOX(widgets->audit_list), row_label);
    }
}

static void refresh_dashboard(AppWidgets *widgets) {
    if (widgets == NULL || widgets->shutting_down) {
        return;
    }

    StrategySettings settings = settings_load();
    LiveReadinessReport readiness = live_readiness_check(widgets->state, &settings);
    ApiHealthReport api_health = api_health_check_light(widgets->state, &settings);

    char price_text[100];
    char eur_text[100];
    char btc_text[100];
    char slots_text[100];
    char mode_text[100];
    char runtime_mode_text[100];
    char live_readiness_text[768];
    char api_health_text[768];
    char last_trade_text[256];
    char settings_text[768];

    snprintf(price_text, sizeof(price_text), "BTC-EUR: %.2f €", widgets->state->current_price);
    snprintf(eur_text, sizeof(eur_text), "EUR disponibili: %.2f", widgets->state->eur_balance);
    snprintf(btc_text, sizeof(btc_text), "BTC detenuti: %.8f", widgets->state->btc_balance);
    snprintf(slots_text, sizeof(slots_text), "Slot usati: %d / %d", widgets->state->used_slots, widgets->state->max_slots);
    snprintf(mode_text, sizeof(mode_text), "Modalità motore: %s", bot_mode_to_string(widgets->state->mode));
    snprintf(runtime_mode_text, sizeof(runtime_mode_text), "Modalità operativa: %s", runtime_mode_to_string(settings.runtime_mode));
    snprintf(
        live_readiness_text,
        sizeof(live_readiness_text),
        "Stato pre-live: %s | blocchi %d | warning %d | %s",
        readiness.status,
        readiness.blocking_count,
        readiness.warning_count,
        readiness.reason
    );

    snprintf(
        api_health_text,
        sizeof(api_health_text),
        "API health: %s | blocchi %d | warning %d | %s",
        api_health.status,
        api_health.blocking_count,
        api_health.warning_count,
        api_health.reason
    );
    snprintf(last_trade_text, sizeof(last_trade_text), "Ultima operazione: %s", widgets->state->last_trade);
    snprintf(
        settings_text,
        sizeof(settings_text),
        "Strategia: slot %.2f € | buy drop %.2f%% | sell %.2f%% | fee stimata %.2f%% | min profit %.2f € / %.2f%% | liquidità min %.2f%% | riserva %.2f%% | slot riserva sbloccati %d | max slot %d | audit %d giorni | max ordini/giorno %d | cooldown %d sec | max loss %.2f € | max drawdown %.2f%% | kill-switch %s | live arm %s",
        settings.slot_amount_eur,
        settings.buy_drop_percent,
        settings.sell_profit_percent,
        settings.estimated_fee_percent,
        settings.min_profit_eur,
        settings.min_profit_percent,
        settings.min_liquidity_percent,
        settings.liquidity_reserve_percent,
        settings.reserve_released_slots,
        settings.max_slots,
        settings.audit_retention_days,
        settings.max_orders_per_day,
        settings.order_cooldown_seconds,
        settings.max_daily_loss_eur,
        settings.max_drawdown_percent,
        settings.emergency_stop_enabled ? "ATTIVO" : "disattivato",
        settings.live_trading_armed ? "ATTIVO" : "disattivato"
    );

    gtk_label_set_text(GTK_LABEL(widgets->price_label), price_text);
    gtk_label_set_text(GTK_LABEL(widgets->eur_label), eur_text);
    gtk_label_set_text(GTK_LABEL(widgets->btc_label), btc_text);
    gtk_label_set_text(GTK_LABEL(widgets->slots_label), slots_text);
    gtk_label_set_text(GTK_LABEL(widgets->mode_label), mode_text);
    gtk_label_set_text(GTK_LABEL(widgets->runtime_mode_label), runtime_mode_text);
    gtk_label_set_text(GTK_LABEL(widgets->live_readiness_label), live_readiness_text);
    gtk_label_set_text(GTK_LABEL(widgets->api_health_label), api_health_text);
    gtk_label_set_text(GTK_LABEL(widgets->last_trade_label), last_trade_text);
    gtk_label_set_text(GTK_LABEL(widgets->settings_label), settings_text);

    refresh_status(widgets);
    refresh_coinbase_credentials_status(widgets);
    refresh_remote_wallet_status(widgets);
    refresh_trade_history(widgets);
    refresh_engine_audit(widgets);
}

static GtkWidget *create_setting_row(const char *label_text, GtkWidget *entry) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *label = gtk_label_new(label_text);

    gtk_widget_set_size_request(label, 170, -1);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(entry, TRUE);

    gtk_box_append(GTK_BOX(row), label);
    gtk_box_append(GTK_BOX(row), entry);

    return row;
}

static GtkWidget *create_main_menu_bar(void) {
    GMenu *menu_bar_model = g_menu_new();

    GMenu *file_menu = g_menu_new();
    g_menu_append(file_menu, "Start bot", "win.start-bot");
    g_menu_append(file_menu, "Stop bot", "win.stop-bot");
    g_menu_append(file_menu, "Esci", "win.quit");
    g_menu_append_submenu(menu_bar_model, "File", G_MENU_MODEL(file_menu));
    g_object_unref(file_menu);

    GMenu *preferences_menu = g_menu_new();
    g_menu_append(preferences_menu, "Impostazioni strategia", "win.show-strategy-settings");
    g_menu_append(preferences_menu, "Coinbase API", "win.show-coinbase-api");
    g_menu_append_submenu(menu_bar_model, "Preferenze", G_MENU_MODEL(preferences_menu));
    g_object_unref(preferences_menu);

    GMenu *view_menu = g_menu_new();
    g_menu_append(view_menu, "Storico operazioni", "win.show-trade-history");
    g_menu_append(view_menu, "Audit decisioni", "win.show-engine-audit");
    g_menu_append_submenu(menu_bar_model, "Visualizza", G_MENU_MODEL(view_menu));
    g_object_unref(view_menu);

    GMenu *help_menu = g_menu_new();
    g_menu_append(help_menu, "Guida", "win.show-help");
    g_menu_append(help_menu, "Informazioni su...", "win.show-about");
    g_menu_append_submenu(menu_bar_model, "?", G_MENU_MODEL(help_menu));
    g_object_unref(help_menu);

    GtkWidget *menu_bar = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(menu_bar_model));
    g_object_unref(menu_bar_model);

    return menu_bar;
}

static gboolean on_hide_window_close_request(GtkWindow *window, gpointer user_data) {
    (void)user_data;

    gtk_widget_set_visible(GTK_WIDGET(window), FALSE);

    return TRUE;
}

static void present_utility_window(GtkWidget *window) {
    if (window == NULL) {
        return;
    }

    gtk_window_present(GTK_WINDOW(window));
}

static void show_text_dialog(AppWidgets *widgets, const char *title, const char *message) {
    GtkWidget *dialog;
    GtkWidget *box;
    GtkWidget *label;
    GtkWidget *button;

    if (widgets == NULL || widgets->window == NULL) {
        return;
    }

    dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(widgets->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 520, 260);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);

    label = gtk_label_new(message);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_valign(label, GTK_ALIGN_START);
    gtk_widget_set_vexpand(label, TRUE);

    button = gtk_button_new_with_label("Chiudi");
    gtk_widget_set_halign(button, GTK_ALIGN_END);

    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), button);

    gtk_window_set_child(GTK_WINDOW(dialog), box);

    g_signal_connect_swapped(button, "clicked", G_CALLBACK(gtk_window_close), dialog);

    gtk_window_present(GTK_WINDOW(dialog));
}

static void fill_settings_entries(AppWidgets *widgets) {
    StrategySettings settings = settings_load();

    char buffer[64];

    snprintf(buffer, sizeof(buffer), "%.2f", settings.slot_amount_eur);
    gtk_editable_set_text(GTK_EDITABLE(widgets->slot_amount_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", settings.buy_drop_percent);
    gtk_editable_set_text(GTK_EDITABLE(widgets->buy_drop_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", settings.sell_profit_percent);
    gtk_editable_set_text(GTK_EDITABLE(widgets->sell_profit_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", settings.estimated_fee_percent);
    gtk_editable_set_text(GTK_EDITABLE(widgets->estimated_fee_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", settings.min_profit_eur);
    gtk_editable_set_text(GTK_EDITABLE(widgets->min_profit_eur_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", settings.min_profit_percent);
    gtk_editable_set_text(GTK_EDITABLE(widgets->min_profit_percent_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", settings.min_liquidity_percent);
    gtk_editable_set_text(GTK_EDITABLE(widgets->min_liquidity_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", settings.liquidity_reserve_percent);
    gtk_editable_set_text(GTK_EDITABLE(widgets->liquidity_reserve_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%d", settings.reserve_released_slots);
    gtk_editable_set_text(GTK_EDITABLE(widgets->reserve_released_slots_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%d", settings.max_slots);
    gtk_editable_set_text(GTK_EDITABLE(widgets->max_slots_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%d", settings.audit_retention_days);
    gtk_editable_set_text(GTK_EDITABLE(widgets->audit_retention_days_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%d", settings.volatility_window_seconds);
    gtk_editable_set_text(GTK_EDITABLE(widgets->volatility_window_seconds_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", settings.volatility_max_move_percent);
    gtk_editable_set_text(GTK_EDITABLE(widgets->volatility_max_move_percent_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%d", settings.max_orders_per_day);
    gtk_editable_set_text(GTK_EDITABLE(widgets->max_orders_per_day_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%d", settings.order_cooldown_seconds);
    gtk_editable_set_text(GTK_EDITABLE(widgets->order_cooldown_seconds_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", settings.max_daily_loss_eur);
    gtk_editable_set_text(GTK_EDITABLE(widgets->max_daily_loss_eur_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", settings.max_drawdown_percent);
    gtk_editable_set_text(GTK_EDITABLE(widgets->max_drawdown_percent_entry), buffer);

    if (widgets->emergency_stop_label != NULL) {
        gtk_label_set_text(
            GTK_LABEL(widgets->emergency_stop_label),
            settings.emergency_stop_enabled ?
                "Kill-switch: ATTIVO - operazioni bloccate" :
                "Kill-switch: disattivato"
        );
    }

    if (widgets->live_trading_arm_label != NULL) {
        gtk_label_set_text(
            GTK_LABEL(widgets->live_trading_arm_label),
            settings.live_trading_armed ?
                "LIVE_TRADING arm: ATTIVO - invio reale ancora bloccato dal codice" :
                "LIVE_TRADING arm: disattivato"
        );
    }

    gtk_drop_down_set_selected(
        GTK_DROP_DOWN(widgets->runtime_mode_dropdown),
        (guint)settings.runtime_mode
    );
}

static void fill_coinbase_entries(AppWidgets *widgets) {
    CoinbaseCredentials credentials = env_load_coinbase_credentials();

    gtk_editable_set_text(
        GTK_EDITABLE(widgets->coinbase_api_key_entry),
        credentials.api_key
    );

    gtk_editable_set_text(
        GTK_EDITABLE(widgets->coinbase_api_secret_entry),
        credentials.api_secret
    );
}

static RuntimeMode get_selected_runtime_mode(GtkWidget *dropdown) {
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(dropdown));

    if (selected == 1) {
        return RUNTIME_MODE_LIVE_READONLY;
    }

    if (selected == 2) {
        return RUNTIME_MODE_LIVE_TRADING;
    }

    return RUNTIME_MODE_SIMULATION;
}

static void on_save_coinbase_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;

    const char *api_key =
        gtk_editable_get_text(GTK_EDITABLE(widgets->coinbase_api_key_entry));

    const char *api_secret =
        gtk_editable_get_text(GTK_EDITABLE(widgets->coinbase_api_secret_entry));

    if (
        api_key == NULL ||
        api_secret == NULL ||
        strlen(api_key) == 0 ||
        strlen(api_secret) == 0
    ) {
        gtk_label_set_text(
            GTK_LABEL(widgets->status_label),
            "Errore: API Key e API Secret sono obbligatorie"
        );
        return;
    }

    if (!env_save_coinbase_credentials(api_key, api_secret)) {
        gtk_label_set_text(
            GTK_LABEL(widgets->status_label),
            "Errore: impossibile salvare .env"
        );
        return;
    }

    gtk_label_set_text(
        GTK_LABEL(widgets->status_label),
        "Credenziali Coinbase salvate in .env"
    );

    refresh_dashboard(widgets);
}

static void on_release_reserve_slot_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;
    StrategySettings settings = settings_load();
    char reason[256];

    if (widgets == NULL || widgets->state == NULL) {
        return;
    }

    if (settings.reserve_released_slots >= settings.max_slots) {
        gtk_label_set_text(
            GTK_LABEL(widgets->status_label),
            "Riserva: tutti gli slot risultano già sbloccati"
        );
        return;
    }

    settings.reserve_released_slots++;
    settings_save(&settings);

    snprintf(
        reason,
        sizeof(reason),
        "Slot riserva sbloccato manualmente: %d/%d",
        settings.reserve_released_slots,
        settings.max_slots
    );

    db_log_engine_audit(
        "LIQUIDITY_RESERVE",
        "RELEASE_SLOT",
        reason,
        widgets->state->current_price,
        widgets->state->btc_balance,
        widgets->state->eur_balance,
        0.0,
        0.0
    );

    gtk_label_set_text(GTK_LABEL(widgets->status_label), reason);
    fill_settings_entries(widgets);
    refresh_dashboard(widgets);
}

static void on_lock_reserve_slot_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;
    StrategySettings settings = settings_load();
    char reason[256];

    if (widgets == NULL || widgets->state == NULL) {
        return;
    }

    if (settings.reserve_released_slots <= 0) {
        gtk_label_set_text(
            GTK_LABEL(widgets->status_label),
            "Riserva: nessuno slot riserva da ribloccare"
        );
        return;
    }

    settings.reserve_released_slots--;
    settings_save(&settings);

    snprintf(
        reason,
        sizeof(reason),
        "Slot riserva ribloccato manualmente: %d/%d",
        settings.reserve_released_slots,
        settings.max_slots
    );

    db_log_engine_audit(
        "LIQUIDITY_RESERVE",
        "LOCK_SLOT",
        reason,
        widgets->state->current_price,
        widgets->state->btc_balance,
        widgets->state->eur_balance,
        0.0,
        0.0
    );

    gtk_label_set_text(GTK_LABEL(widgets->status_label), reason);
    fill_settings_entries(widgets);
    refresh_dashboard(widgets);
}

static void on_activate_emergency_stop_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->state == NULL) {
        return;
    }

    emergency_stop_activate();
    widgets->state->running = false;
    widgets->state->mode = BOT_MODE_PAUSED;
    snprintf(
        widgets->state->last_trade,
        sizeof(widgets->state->last_trade),
        "Emergency stop attivato manualmente"
    );
    db_save_state(widgets->state);

    db_log_engine_audit(
        "EMERGENCY_STOP",
        "ACTIVATED",
        "Kill-switch attivato manualmente dall'utente",
        widgets->state->current_price,
        widgets->state->btc_balance,
        widgets->state->eur_balance,
        0.0,
        0.0
    );

    gtk_label_set_text(
        GTK_LABEL(widgets->status_label),
        "Emergency stop attivato: bot fermo e operazioni bloccate"
    );

    fill_settings_entries(widgets);
    refresh_dashboard(widgets);
}

static void on_reset_emergency_stop_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->state == NULL) {
        return;
    }

    emergency_stop_reset();

    db_log_engine_audit(
        "EMERGENCY_STOP",
        "RESET",
        "Kill-switch resettato manualmente dall'utente",
        widgets->state->current_price,
        widgets->state->btc_balance,
        widgets->state->eur_balance,
        0.0,
        0.0
    );

    gtk_label_set_text(
        GTK_LABEL(widgets->status_label),
        "Emergency stop resettato: il bot resta fermo finché non premi Start"
    );

    fill_settings_entries(widgets);
    refresh_dashboard(widgets);
}

static void on_arm_live_trading_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->state == NULL) {
        return;
    }

    StrategySettings settings = settings_load();

    if (settings.emergency_stop_enabled) {
        gtk_label_set_text(
            GTK_LABEL(widgets->status_label),
            "LIVE_TRADING arm bloccato: kill-switch attivo"
        );
        return;
    }

    settings.live_trading_armed = 1;
    settings_save(&settings);

    db_log_engine_audit(
        "LIVE_TRADING_ARM",
        "ARMED",
        "LIVE_TRADING armato manualmente: invio ordini reali ancora bloccato dal codice",
        widgets->state->current_price,
        widgets->state->btc_balance,
        widgets->state->eur_balance,
        0.0,
        0.0
    );

    gtk_label_set_text(
        GTK_LABEL(widgets->status_label),
        "LIVE_TRADING armato: invio ordini reali ancora bloccato dal codice"
    );

    fill_settings_entries(widgets);
    refresh_dashboard(widgets);
}

static void on_disarm_live_trading_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->state == NULL) {
        return;
    }

    StrategySettings settings = settings_load();
    settings.live_trading_armed = 0;
    settings_save(&settings);

    db_log_engine_audit(
        "LIVE_TRADING_ARM",
        "DISARMED",
        "LIVE_TRADING disarmato manualmente dall'utente",
        widgets->state->current_price,
        widgets->state->btc_balance,
        widgets->state->eur_balance,
        0.0,
        0.0
    );

    gtk_label_set_text(
        GTK_LABEL(widgets->status_label),
        "LIVE_TRADING disarmato"
    );

    fill_settings_entries(widgets);
    refresh_dashboard(widgets);
}

static void on_save_settings_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;

    StrategySettings settings = settings_load();

    settings.slot_amount_eur =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(widgets->slot_amount_entry)));

    settings.buy_drop_percent =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(widgets->buy_drop_entry)));

    settings.sell_profit_percent =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(widgets->sell_profit_entry)));

    settings.estimated_fee_percent =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(widgets->estimated_fee_entry)));

    settings.min_profit_eur =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(widgets->min_profit_eur_entry)));

    settings.min_profit_percent =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(widgets->min_profit_percent_entry)));

    settings.min_liquidity_percent =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(widgets->min_liquidity_entry)));

    settings.liquidity_reserve_percent =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(widgets->liquidity_reserve_entry)));

    settings.reserve_released_slots =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->reserve_released_slots_entry)));

    settings.max_slots =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->max_slots_entry)));

    settings.audit_retention_days =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->audit_retention_days_entry)));

    settings.volatility_window_seconds =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->volatility_window_seconds_entry)));

    settings.volatility_max_move_percent =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(widgets->volatility_max_move_percent_entry)));

    settings.max_orders_per_day =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->max_orders_per_day_entry)));

    settings.order_cooldown_seconds =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->order_cooldown_seconds_entry)));

    settings.max_daily_loss_eur =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(widgets->max_daily_loss_eur_entry)));

    settings.max_drawdown_percent =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(widgets->max_drawdown_percent_entry)));

    settings.runtime_mode =
        get_selected_runtime_mode(widgets->runtime_mode_dropdown);

    if (
        settings.slot_amount_eur <= 0.0 ||
        settings.buy_drop_percent <= 0.0 ||
        settings.sell_profit_percent <= 0.0 ||
        settings.estimated_fee_percent < 0.0 ||
        settings.estimated_fee_percent >= 100.0 ||
        settings.min_profit_eur < 0.0 ||
        settings.min_profit_percent < 0.0 ||
        settings.min_liquidity_percent < 0.0 ||
        settings.min_liquidity_percent >= 100.0 ||
        settings.liquidity_reserve_percent < 0.0 ||
        settings.liquidity_reserve_percent > 95.0 ||
        settings.reserve_released_slots < 0 ||
        settings.reserve_released_slots > settings.max_slots ||
        settings.max_slots <= 0 ||
        settings.audit_retention_days <= 0 ||
        settings.volatility_window_seconds < 10 ||
        settings.volatility_max_move_percent <= 0.0 ||
        settings.max_orders_per_day <= 0 ||
        settings.order_cooldown_seconds < 0 ||
        settings.max_daily_loss_eur < 0.0 ||
        settings.max_drawdown_percent < 0.0
    ) {
        gtk_label_set_text(
            GTK_LABEL(widgets->status_label),
            "Errore: impostazioni non valide"
        );
        return;
    }

    settings_save(&settings);

    widgets->state->max_slots = settings.max_slots;
    db_save_state(widgets->state);

    if (settings.runtime_mode == RUNTIME_MODE_LIVE_TRADING) {
        gtk_label_set_text(
            GTK_LABEL(widgets->status_label),
            "LIVE_TRADING salvato ma bloccato dai safety checks"
        );
    } else {
        gtk_label_set_text(
            GTK_LABEL(widgets->status_label),
            "Impostazioni salvate"
        );
    }

    refresh_dashboard(widgets);
}

static void on_start_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;

    widgets->state->running = true;
    db_save_state(widgets->state);

    refresh_dashboard(widgets);
}

static void on_stop_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;

    widgets->state->running = false;
    widgets->state->mode = BOT_MODE_PAUSED;
    db_save_state(widgets->state);

    refresh_dashboard(widgets);
}

static void on_menu_start_bot_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    on_start_clicked(NULL, user_data);
}

static void on_menu_stop_bot_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    on_stop_clicked(NULL, user_data);
}

static void on_menu_quit_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;

    if (widgets != NULL && widgets->window != NULL) {
        gtk_window_close(GTK_WINDOW(widgets->window));
    }
}

static void on_menu_show_strategy_settings_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->settings_expander == NULL) {
        return;
    }

    fill_settings_entries(widgets);
    present_utility_window(widgets->settings_expander);
    gtk_label_set_text(GTK_LABEL(widgets->status_label), "Preferenze: impostazioni strategia aperte");
}

static void on_menu_show_coinbase_api_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->coinbase_expander == NULL) {
        return;
    }

    fill_coinbase_entries(widgets);
    present_utility_window(widgets->coinbase_expander);
    gtk_label_set_text(GTK_LABEL(widgets->status_label), "Preferenze: Coinbase API aperte");
}

static void on_menu_show_trade_history_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->history_scrolled_window == NULL) {
        return;
    }

    refresh_trade_history(widgets);
    present_utility_window(widgets->history_scrolled_window);
    gtk_label_set_text(GTK_LABEL(widgets->status_label), "Visualizza: storico operazioni aperto");
}

static void on_menu_show_engine_audit_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->audit_scrolled_window == NULL) {
        return;
    }

    refresh_engine_audit(widgets);
    present_utility_window(widgets->audit_scrolled_window);
    gtk_label_set_text(GTK_LABEL(widgets->status_label), "Visualizza: audit decisioni aperto");
}

static void on_menu_show_help_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;

    show_text_dialog(
        widgets,
        "Guida Helix",
        "File: avvia, ferma o chiude Helix.\n\n"
        "Preferenze: apre le impostazioni strategia e Coinbase API in finestre dedicate.\n\n"
        "Visualizza: apre storico operazioni e audit decisioni in finestre dedicate.\n\n"
        "LIVE_TRADING resta bloccato dai safety gate e dal build normale."
    );
}

static void on_menu_show_about_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;

    show_text_dialog(
        widgets,
        "Informazioni su Helix",
        "Helix\n\n"
        "C/GTK4 BTC-EUR trading engine pre-live.\n\n"
        "Supporta simulazione, LIVE_READONLY, preview Coinbase, safety gate, dry-run executor e journal.\n\n"
        "L'esecuzione reale degli ordini resta intenzionalmente bloccata."
    );
}

static gboolean on_engine_timer(gpointer user_data) {
    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->shutting_down || widgets->state == NULL) {
        return G_SOURCE_REMOVE;
    }

    helix_engine_tick(widgets->state);
    db_save_state(widgets->state);
    refresh_dashboard(widgets);

    return G_SOURCE_CONTINUE;
}

void on_app_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    BotState *state = g_malloc(sizeof(BotState));
    *state = bot_state_default();

    db_init();
    settings_save_defaults_if_missing();

    if (!db_load_state(state)) {
        db_save_state(state);
    }

    StrategySettings settings = settings_load();
    state->max_slots = settings.max_slots;

    GtkWidget *window;
    GtkWidget *root_box;
    GtkWidget *main_box;
    GtkWidget *menu_bar;
    GtkWidget *page_scrolled_window;
    GtkWidget *top_box;
    GtkWidget *price_label;
    GtkWidget *eur_label;
    GtkWidget *btc_label;
    GtkWidget *slots_label;
    GtkWidget *mode_label;
    GtkWidget *runtime_mode_label;
    GtkWidget *live_readiness_label;
    GtkWidget *api_health_label;
    GtkWidget *coinbase_credentials_label;
    GtkWidget *remote_wallet_label;
    GtkWidget *last_trade_label;
    GtkWidget *settings_label;
    GtkWidget *status_label;
    GtkWidget *settings_expander;
    GtkWidget *settings_box;
    GtkWidget *settings_dialog_scrolled_window;
    GtkWidget *slot_amount_entry;
    GtkWidget *buy_drop_entry;
    GtkWidget *sell_profit_entry;
    GtkWidget *estimated_fee_entry;
    GtkWidget *min_profit_eur_entry;
    GtkWidget *min_profit_percent_entry;
    GtkWidget *min_liquidity_entry;
    GtkWidget *liquidity_reserve_entry;
    GtkWidget *reserve_released_slots_entry;
    GtkWidget *max_slots_entry;
    GtkWidget *audit_retention_days_entry;
    GtkWidget *volatility_window_seconds_entry;
    GtkWidget *volatility_max_move_percent_entry;
    GtkWidget *max_orders_per_day_entry;
    GtkWidget *order_cooldown_seconds_entry;
    GtkWidget *max_daily_loss_eur_entry;
    GtkWidget *max_drawdown_percent_entry;
    GtkWidget *emergency_stop_label;
    GtkWidget *live_trading_arm_label;
    GtkWidget *live_trading_arm_buttons_box;
    GtkWidget *arm_live_trading_button;
    GtkWidget *disarm_live_trading_button;
    GtkWidget *runtime_mode_dropdown;
    GtkWidget *save_settings_button;
    GtkWidget *emergency_buttons_box;
    GtkWidget *activate_emergency_stop_button;
    GtkWidget *reset_emergency_stop_button;
    GtkWidget *reserve_buttons_box;
    GtkWidget *release_reserve_slot_button;
    GtkWidget *lock_reserve_slot_button;

    GtkWidget *coinbase_expander;
    GtkWidget *coinbase_box;
    GtkWidget *coinbase_api_key_entry;
    GtkWidget *coinbase_api_secret_entry;
    GtkWidget *save_coinbase_button;

    GtkWidget *history_title;
    GtkWidget *history_window;
    GtkWidget *history_box;
    GtkWidget *trade_list;
    GtkWidget *audit_title;
    GtkWidget *audit_window;
    GtkWidget *audit_box;
    GtkWidget *audit_list;
    GtkWidget *scrolled_window;
    GtkWidget *audit_scrolled_window;

    char price_text[100];
    char eur_text[100];
    char btc_text[100];
    char slots_text[100];

    snprintf(price_text, sizeof(price_text), "BTC-EUR: %.2f €", state->current_price);
    snprintf(eur_text, sizeof(eur_text), "EUR disponibili: %.2f", state->eur_balance);
    snprintf(btc_text, sizeof(btc_text), "BTC detenuti: %.8f", state->btc_balance);
    snprintf(slots_text, sizeof(slots_text), "Slot usati: %d / %d", state->used_slots, state->max_slots);

    load_app_css();

    window = gtk_application_window_new(app);

    gtk_window_set_title(GTK_WINDOW(window), "Helix");
    gtk_window_set_default_size(GTK_WINDOW(window), 980, 800);

    menu_bar = create_main_menu_bar();

    root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    page_scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(page_scrolled_window),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC
    );
    gtk_widget_set_vexpand(page_scrolled_window, TRUE);

    main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(main_box, 16);
    gtk_widget_set_margin_bottom(main_box, 30);
    gtk_widget_set_margin_start(main_box, 30);
    gtk_widget_set_margin_end(main_box, 30);

    top_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    price_label = gtk_label_new(price_text);
    eur_label = gtk_label_new(eur_text);
    btc_label = gtk_label_new(btc_text);
    slots_label = gtk_label_new(slots_text);
    mode_label = gtk_label_new("");
    runtime_mode_label = gtk_label_new("");
    live_readiness_label = gtk_label_new("");
    api_health_label = gtk_label_new("");
    coinbase_credentials_label = gtk_label_new("");
    remote_wallet_label = gtk_label_new("");
    last_trade_label = gtk_label_new("");
    settings_label = gtk_label_new("");
    status_label = gtk_label_new("");

    gtk_widget_set_halign(price_label, GTK_ALIGN_START);
    gtk_widget_set_halign(eur_label, GTK_ALIGN_START);
    gtk_widget_set_halign(btc_label, GTK_ALIGN_START);
    gtk_widget_set_halign(slots_label, GTK_ALIGN_START);
    gtk_widget_set_halign(mode_label, GTK_ALIGN_START);
    gtk_widget_set_halign(runtime_mode_label, GTK_ALIGN_START);
    gtk_widget_set_halign(live_readiness_label, GTK_ALIGN_START);
    gtk_label_set_wrap(GTK_LABEL(live_readiness_label), TRUE);
    gtk_widget_set_halign(api_health_label, GTK_ALIGN_START);
    gtk_label_set_wrap(GTK_LABEL(api_health_label), TRUE);
    gtk_widget_set_halign(coinbase_credentials_label, GTK_ALIGN_START);
    gtk_widget_set_halign(remote_wallet_label, GTK_ALIGN_START);
    gtk_widget_set_halign(last_trade_label, GTK_ALIGN_START);
    gtk_widget_set_halign(settings_label, GTK_ALIGN_START);
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);

    settings_expander = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(settings_expander), "Impostazioni strategia");
    gtk_window_set_transient_for(GTK_WINDOW(settings_expander), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(settings_expander), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(settings_expander), 760, 760);
    g_signal_connect(settings_expander, "close-request", G_CALLBACK(on_hide_window_close_request), NULL);

    settings_dialog_scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(settings_dialog_scrolled_window),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC
    );

    settings_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(settings_box, 10);
    gtk_widget_set_margin_bottom(settings_box, 10);
    gtk_widget_set_margin_start(settings_box, 10);
    gtk_widget_set_margin_end(settings_box, 10);

    slot_amount_entry = gtk_entry_new();
    buy_drop_entry = gtk_entry_new();
    sell_profit_entry = gtk_entry_new();
    estimated_fee_entry = gtk_entry_new();
    min_profit_eur_entry = gtk_entry_new();
    min_profit_percent_entry = gtk_entry_new();
    min_liquidity_entry = gtk_entry_new();
    liquidity_reserve_entry = gtk_entry_new();
    reserve_released_slots_entry = gtk_entry_new();
    max_slots_entry = gtk_entry_new();
    audit_retention_days_entry = gtk_entry_new();
    volatility_window_seconds_entry = gtk_entry_new();
    volatility_max_move_percent_entry = gtk_entry_new();
    max_orders_per_day_entry = gtk_entry_new();
    order_cooldown_seconds_entry = gtk_entry_new();
    max_daily_loss_eur_entry = gtk_entry_new();
    max_drawdown_percent_entry = gtk_entry_new();

    const char *runtime_modes[] = {
        "SIMULATION",
        "LIVE_READONLY",
        "LIVE_TRADING",
        NULL
    };

    runtime_mode_dropdown = gtk_drop_down_new_from_strings(runtime_modes);

    save_settings_button = gtk_button_new_with_label("Salva impostazioni");

    emergency_stop_label = gtk_label_new("");
    gtk_widget_set_halign(emergency_stop_label, GTK_ALIGN_START);

    emergency_buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    activate_emergency_stop_button = gtk_button_new_with_label("Attiva kill-switch");
    reset_emergency_stop_button = gtk_button_new_with_label("Reset kill-switch");
    gtk_box_append(GTK_BOX(emergency_buttons_box), activate_emergency_stop_button);
    gtk_box_append(GTK_BOX(emergency_buttons_box), reset_emergency_stop_button);

    live_trading_arm_label = gtk_label_new("");
    gtk_widget_set_halign(live_trading_arm_label, GTK_ALIGN_START);

    live_trading_arm_buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    arm_live_trading_button = gtk_button_new_with_label("Arma LIVE_TRADING");
    disarm_live_trading_button = gtk_button_new_with_label("Disarma LIVE_TRADING");
    gtk_box_append(GTK_BOX(live_trading_arm_buttons_box), arm_live_trading_button);
    gtk_box_append(GTK_BOX(live_trading_arm_buttons_box), disarm_live_trading_button);

    reserve_buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    release_reserve_slot_button = gtk_button_new_with_label("Sblocca 1 slot riserva");
    lock_reserve_slot_button = gtk_button_new_with_label("Riblocca 1 slot riserva");
    gtk_box_append(GTK_BOX(reserve_buttons_box), release_reserve_slot_button);
    gtk_box_append(GTK_BOX(reserve_buttons_box), lock_reserve_slot_button);

    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Slot EUR", slot_amount_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Buy drop %", buy_drop_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Sell profit lordo %", sell_profit_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Fee stimata %", estimated_fee_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Profitto minimo EUR", min_profit_eur_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Profitto minimo %", min_profit_percent_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Liquidità min %", min_liquidity_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Riserva protetta %", liquidity_reserve_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Slot riserva sbloccati", reserve_released_slots_entry));
    gtk_box_append(GTK_BOX(settings_box), reserve_buttons_box);
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Max slot", max_slots_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Conserva audit giorni", audit_retention_days_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Volatilità finestra sec", volatility_window_seconds_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Volatilità max movimento %", volatility_max_move_percent_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Max ordini al giorno", max_orders_per_day_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Cooldown ordini sec", order_cooldown_seconds_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Max perdita giornaliera EUR", max_daily_loss_eur_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Max drawdown %", max_drawdown_percent_entry));
    gtk_box_append(GTK_BOX(settings_box), emergency_stop_label);
    gtk_box_append(GTK_BOX(settings_box), emergency_buttons_box);
    gtk_box_append(GTK_BOX(settings_box), live_trading_arm_label);
    gtk_box_append(GTK_BOX(settings_box), live_trading_arm_buttons_box);
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Modalità operativa", runtime_mode_dropdown));
    gtk_box_append(GTK_BOX(settings_box), save_settings_button);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(settings_dialog_scrolled_window), settings_box);
    gtk_window_set_child(GTK_WINDOW(settings_expander), settings_dialog_scrolled_window);

    coinbase_expander = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(coinbase_expander), "Coinbase API");
    gtk_window_set_transient_for(GTK_WINDOW(coinbase_expander), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(coinbase_expander), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(coinbase_expander), 760, 260);
    g_signal_connect(coinbase_expander, "close-request", G_CALLBACK(on_hide_window_close_request), NULL);

    coinbase_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(coinbase_box, 10);
    gtk_widget_set_margin_bottom(coinbase_box, 10);
    gtk_widget_set_margin_start(coinbase_box, 10);
    gtk_widget_set_margin_end(coinbase_box, 10);

    coinbase_api_key_entry = gtk_entry_new();
    coinbase_api_secret_entry = gtk_password_entry_new();

    save_coinbase_button = gtk_button_new_with_label("Salva credenziali Coinbase");

    gtk_box_append(GTK_BOX(coinbase_box), create_setting_row("API Key", coinbase_api_key_entry));
    gtk_box_append(GTK_BOX(coinbase_box), create_setting_row("API Secret", coinbase_api_secret_entry));
    gtk_box_append(GTK_BOX(coinbase_box), save_coinbase_button);

    gtk_window_set_child(GTK_WINDOW(coinbase_expander), coinbase_box);

    history_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(history_window), "Storico operazioni");
    gtk_window_set_transient_for(GTK_WINDOW(history_window), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(history_window), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(history_window), 900, 520);
    g_signal_connect(history_window, "close-request", G_CALLBACK(on_hide_window_close_request), NULL);

    history_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(history_box, 12);
    gtk_widget_set_margin_bottom(history_box, 12);
    gtk_widget_set_margin_start(history_box, 12);
    gtk_widget_set_margin_end(history_box, 12);

    history_title = gtk_label_new("Storico operazioni - più recenti in alto");
    gtk_widget_add_css_class(history_title, "title-3");
    gtk_widget_set_halign(history_title, GTK_ALIGN_START);

    trade_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(trade_list), GTK_SELECTION_NONE);

    scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), trade_list);

    gtk_box_append(GTK_BOX(history_box), history_title);
    gtk_box_append(GTK_BOX(history_box), scrolled_window);
    gtk_window_set_child(GTK_WINDOW(history_window), history_box);

    audit_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(audit_window), "Audit decisioni motore");
    gtk_window_set_transient_for(GTK_WINDOW(audit_window), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(audit_window), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(audit_window), 1000, 560);
    g_signal_connect(audit_window, "close-request", G_CALLBACK(on_hide_window_close_request), NULL);

    audit_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(audit_box, 12);
    gtk_widget_set_margin_bottom(audit_box, 12);
    gtk_widget_set_margin_start(audit_box, 12);
    gtk_widget_set_margin_end(audit_box, 12);

    audit_title = gtk_label_new("Audit decisioni motore - più recenti in alto");
    gtk_widget_add_css_class(audit_title, "title-3");
    gtk_widget_set_halign(audit_title, GTK_ALIGN_START);

    audit_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(audit_list), GTK_SELECTION_NONE);

    audit_scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(audit_scrolled_window, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(audit_scrolled_window), audit_list);

    gtk_box_append(GTK_BOX(audit_box), audit_title);
    gtk_box_append(GTK_BOX(audit_box), audit_scrolled_window);
    gtk_window_set_child(GTK_WINDOW(audit_window), audit_box);


    gtk_box_append(GTK_BOX(top_box), price_label);
    gtk_box_append(GTK_BOX(top_box), eur_label);
    gtk_box_append(GTK_BOX(top_box), btc_label);
    gtk_box_append(GTK_BOX(top_box), slots_label);
    gtk_box_append(GTK_BOX(top_box), mode_label);
    gtk_box_append(GTK_BOX(top_box), runtime_mode_label);
    gtk_box_append(GTK_BOX(top_box), live_readiness_label);
    gtk_box_append(GTK_BOX(top_box), api_health_label);
    gtk_box_append(GTK_BOX(top_box), coinbase_credentials_label);
    gtk_box_append(GTK_BOX(top_box), remote_wallet_label);
    gtk_box_append(GTK_BOX(top_box), last_trade_label);
    gtk_box_append(GTK_BOX(top_box), settings_label);
    gtk_box_append(GTK_BOX(top_box), status_label);

    gtk_box_append(GTK_BOX(main_box), top_box);

    AppWidgets *widgets = g_malloc0(sizeof(AppWidgets));
    widgets->state = state;
    widgets->window = window;
    widgets->timer_id = 0;
    widgets->shutting_down = FALSE;
    widgets->price_label = price_label;
    widgets->eur_label = eur_label;
    widgets->btc_label = btc_label;
    widgets->slots_label = slots_label;
    widgets->mode_label = mode_label;
    widgets->runtime_mode_label = runtime_mode_label;
    widgets->live_readiness_label = live_readiness_label;
    widgets->api_health_label = api_health_label;
    widgets->coinbase_credentials_label = coinbase_credentials_label;
    widgets->remote_wallet_label = remote_wallet_label;
    widgets->last_trade_label = last_trade_label;
    widgets->settings_label = settings_label;
    widgets->status_label = status_label;
    widgets->settings_expander = settings_expander;
    widgets->coinbase_expander = coinbase_expander;
    widgets->history_title = history_title;
    widgets->history_scrolled_window = history_window;
    widgets->audit_title = audit_title;
    widgets->audit_scrolled_window = audit_window;
    widgets->trade_list = trade_list;
    widgets->audit_list = audit_list;
    widgets->slot_amount_entry = slot_amount_entry;
    widgets->buy_drop_entry = buy_drop_entry;
    widgets->sell_profit_entry = sell_profit_entry;
    widgets->estimated_fee_entry = estimated_fee_entry;
    widgets->min_profit_eur_entry = min_profit_eur_entry;
    widgets->min_profit_percent_entry = min_profit_percent_entry;
    widgets->min_liquidity_entry = min_liquidity_entry;
    widgets->liquidity_reserve_entry = liquidity_reserve_entry;
    widgets->reserve_released_slots_entry = reserve_released_slots_entry;
    widgets->max_slots_entry = max_slots_entry;
    widgets->audit_retention_days_entry = audit_retention_days_entry;
    widgets->volatility_window_seconds_entry = volatility_window_seconds_entry;
    widgets->volatility_max_move_percent_entry = volatility_max_move_percent_entry;
    widgets->max_orders_per_day_entry = max_orders_per_day_entry;
    widgets->order_cooldown_seconds_entry = order_cooldown_seconds_entry;
    widgets->max_daily_loss_eur_entry = max_daily_loss_eur_entry;
    widgets->max_drawdown_percent_entry = max_drawdown_percent_entry;
    widgets->emergency_stop_label = emergency_stop_label;
    widgets->live_trading_arm_label = live_trading_arm_label;
    widgets->live_trading_arm_buttons_box = live_trading_arm_buttons_box;
    widgets->arm_live_trading_button = arm_live_trading_button;
    widgets->disarm_live_trading_button = disarm_live_trading_button;
    widgets->runtime_mode_dropdown = runtime_mode_dropdown;
    widgets->coinbase_api_key_entry = coinbase_api_key_entry;
    widgets->coinbase_api_secret_entry = coinbase_api_secret_entry;

    const GActionEntry window_actions[] = {
        {
            .name = "start-bot",
            .activate = on_menu_start_bot_action
        },
        {
            .name = "stop-bot",
            .activate = on_menu_stop_bot_action
        },
        {
            .name = "quit",
            .activate = on_menu_quit_action
        },
        {
            .name = "show-strategy-settings",
            .activate = on_menu_show_strategy_settings_action
        },
        {
            .name = "show-coinbase-api",
            .activate = on_menu_show_coinbase_api_action
        },
        {
            .name = "show-trade-history",
            .activate = on_menu_show_trade_history_action
        },
        {
            .name = "show-engine-audit",
            .activate = on_menu_show_engine_audit_action
        },
        {
            .name = "show-help",
            .activate = on_menu_show_help_action
        },
        {
            .name = "show-about",
            .activate = on_menu_show_about_action
        }
    };

    g_action_map_add_action_entries(
        G_ACTION_MAP(window),
        window_actions,
        G_N_ELEMENTS(window_actions),
        widgets
    );

    fill_settings_entries(widgets);
    fill_coinbase_entries(widgets);
    refresh_dashboard(widgets);

    g_signal_connect(save_settings_button, "clicked", G_CALLBACK(on_save_settings_clicked), widgets);
    g_signal_connect(activate_emergency_stop_button, "clicked", G_CALLBACK(on_activate_emergency_stop_clicked), widgets);
    g_signal_connect(reset_emergency_stop_button, "clicked", G_CALLBACK(on_reset_emergency_stop_clicked), widgets);
    g_signal_connect(arm_live_trading_button, "clicked", G_CALLBACK(on_arm_live_trading_clicked), widgets);
    g_signal_connect(disarm_live_trading_button, "clicked", G_CALLBACK(on_disarm_live_trading_clicked), widgets);
    g_signal_connect(release_reserve_slot_button, "clicked", G_CALLBACK(on_release_reserve_slot_clicked), widgets);
    g_signal_connect(lock_reserve_slot_button, "clicked", G_CALLBACK(on_lock_reserve_slot_clicked), widgets);
    g_signal_connect(save_coinbase_button, "clicked", G_CALLBACK(on_save_coinbase_clicked), widgets);
    g_signal_connect(window, "close-request", G_CALLBACK(on_window_close_request), widgets);

    g_object_set_data_full(
        G_OBJECT(window),
        "helix-app-widgets",
        widgets,
        free_app_widgets
    );

    widgets->timer_id = g_timeout_add_seconds(2, on_engine_timer, widgets);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(page_scrolled_window), main_box);
    gtk_box_append(GTK_BOX(root_box), menu_bar);
    gtk_box_append(GTK_BOX(root_box), page_scrolled_window);
    gtk_window_set_child(GTK_WINDOW(window), root_box);
    gtk_window_present(GTK_WINDOW(window));
}
