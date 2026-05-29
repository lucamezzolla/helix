#include "window.h"
#include "../engine/bot_state.h"
#include "../engine/settings.h"
#include "../db/database.h"
#include "../engine/engine.h"
#include "../engine/emergency_stop.h"
#include "../engine/live_readiness.h"
#include "../engine/api_health.h"
#include "../engine/order_journal.h"
#include "../engine/email_delivery.h"
#include "../engine/daily_report.h"
#include "../engine/trade_preview.h"
#include "../exchange/coinbase_client.h"
#include "../wallet/wallet_info.h"
#include "../config/env_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

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
    guint status_message_timeout_id;
    gboolean has_temporary_status_message;
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
    GtkWidget *email_expander;
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
    GtkWidget *micro_live_enabled_entry;
    GtkWidget *micro_live_max_order_eur_entry;
    GtkWidget *micro_live_stop_after_real_order_entry;
    GtkWidget *micro_live_allow_accumulation_entry;
    GtkWidget *email_daily_enabled_entry;
    GtkWidget *email_recipient_entry;
    GtkWidget *email_report_hour_entry;
    GtkWidget *email_report_minute_entry;
    GtkWidget *email_sendmail_command_entry;
    GtkWidget *save_email_button;
    GtkWidget *test_email_button;
    GtkWidget *emergency_stop_label;
    GtkWidget *live_trading_arm_label;
    GtkWidget *live_trading_arm_buttons_box;
    GtkWidget *arm_live_trading_button;
    GtkWidget *disarm_live_trading_button;
    GtkWidget *acknowledge_real_order_button;
    GtkWidget *seed_paper_slots_button;
    GtkWidget *clear_paper_slots_button;
    GtkWidget *run_paper_best_profit_button;
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

static const char *real_executor_build_status_text(void) {
#ifdef HELIX_ENABLE_REAL_COINBASE_ORDERS
    return "compilato nella build live; esecuzione reale ancora soggetta a .env e safety gate";
#else
    return "presente ma bloccato dalla build normale";
#endif
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

    if (widgets->status_message_timeout_id != 0) {
        g_source_remove(widgets->status_message_timeout_id);
        widgets->status_message_timeout_id = 0;
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
    if (widgets == NULL || widgets->has_temporary_status_message) {
        return;
    }

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

static gboolean clear_temporary_status_message(gpointer user_data) {
    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->shutting_down) {
        return G_SOURCE_REMOVE;
    }

    widgets->has_temporary_status_message = FALSE;
    widgets->status_message_timeout_id = 0;

    refresh_status(widgets);

    return G_SOURCE_REMOVE;
}

static void set_temporary_status_message(AppWidgets *widgets, const char *message) {
    if (widgets == NULL || widgets->status_label == NULL || message == NULL) {
        return;
    }

    gtk_widget_remove_css_class(widgets->status_label, "status-running");
    gtk_widget_remove_css_class(widgets->status_label, "status-stopped");
    gtk_label_set_text(GTK_LABEL(widgets->status_label), message);

    widgets->has_temporary_status_message = TRUE;

    if (widgets->status_message_timeout_id != 0) {
        g_source_remove(widgets->status_message_timeout_id);
        widgets->status_message_timeout_id = 0;
    }

    widgets->status_message_timeout_id =
        g_timeout_add_seconds(5, clear_temporary_status_message, widgets);
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
        "Strategia: slot %.2f € | buy drop %.2f%% | sell %.2f%% | fee stimata %.2f%% | min profit %.2f € / %.2f%% | liquidità min %.2f%% | riserva %.2f%% | slot riserva sbloccati %d | max slot %d | audit %d giorni | max ordini/giorno %d | cooldown %d sec | max loss %.2f € | max drawdown %.2f%% | micro-live %s | max micro ordine %.2f € | stop dopo ordine %s | accumulo %s | kill-switch %s | live arm %s",
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
        settings.micro_live_enabled ? "ATTIVO" : "disattivato",
        settings.micro_live_max_order_eur,
        settings.micro_live_stop_after_real_order ? "ATTIVO" : "disattivato",
        settings.micro_live_allow_accumulation ? "ATTIVO" : "disattivato",
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
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 24);
    gtk_widget_set_hexpand(entry, TRUE);

    gtk_box_append(GTK_BOX(row), label);
    gtk_box_append(GTK_BOX(row), entry);

    return row;
}

static GtkWidget *create_section_title(const char *title) {
    GtkWidget *label = gtk_label_new(title);

    gtk_widget_add_css_class(label, "title-4");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);

    return label;
}

static void configure_dashboard_label(GtkWidget *label) {
    if (label == NULL) {
        return;
    }

    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(label, TRUE);

    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 90);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_NONE);
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
    g_menu_append(preferences_menu, "Email report", "win.show-email-report");
    g_menu_append(preferences_menu, "Coinbase API", "win.show-coinbase-api");
    g_menu_append_submenu(menu_bar_model, "Preferenze", G_MENU_MODEL(preferences_menu));
    g_object_unref(preferences_menu);

    GMenu *view_menu = g_menu_new();
    g_menu_append(view_menu, "Storico operazioni", "win.show-trade-history");
    g_menu_append(view_menu, "Audit decisioni", "win.show-engine-audit");
    g_menu_append(view_menu, "Tabella slot reali", "win.show-position-slots-table");
    g_menu_append(view_menu, "Tabella trades", "win.show-trades-table");
    g_menu_append(view_menu, "Tabella order journal", "win.show-order-journal-table");
    g_menu_append(view_menu, "Tabella engine audit", "win.show-engine-audit-table");
    g_menu_append(view_menu, "Report pre-live", "win.show-prelive-report");
    g_menu_append(view_menu, "Stato protezioni", "win.show-safety-status");
    g_menu_append(view_menu, "Simula scenario dry-run", "win.show-dryrun-scenario");
    g_menu_append(view_menu, "Esporta report pre-live", "win.export-prelive-report");
    g_menu_append(view_menu, "Esporta snapshot stato", "win.export-status-snapshot");
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
    GtkWidget *content_widget;
    GtkWidget *scrolled_window = NULL;
    size_t message_length;

    if (widgets == NULL || widgets->window == NULL) {
        return;
    }

    if (message == NULL) {
        message = "";
    }

    message_length = strlen(message);

    dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(widgets->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 620, -1);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);

    label = gtk_label_new(message);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_valign(label, GTK_ALIGN_START);
    gtk_widget_set_vexpand(label, FALSE);

    if (message_length > 1200) {
        scrolled_window = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(
            GTK_SCROLLED_WINDOW(scrolled_window),
            GTK_POLICY_AUTOMATIC,
            GTK_POLICY_AUTOMATIC
        );
        gtk_widget_set_size_request(scrolled_window, 620, 420);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), label);
        content_widget = scrolled_window;
    } else {
        content_widget = label;
    }

    button = gtk_button_new_with_label("Chiudi");
    gtk_widget_set_halign(button, GTK_ALIGN_END);

    gtk_box_append(GTK_BOX(box), content_widget);
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

    snprintf(buffer, sizeof(buffer), "%d", settings.micro_live_enabled ? 1 : 0);
    gtk_editable_set_text(GTK_EDITABLE(widgets->micro_live_enabled_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", settings.micro_live_max_order_eur);
    gtk_editable_set_text(GTK_EDITABLE(widgets->micro_live_max_order_eur_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%d", settings.micro_live_stop_after_real_order ? 1 : 0);
    gtk_editable_set_text(GTK_EDITABLE(widgets->micro_live_stop_after_real_order_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%d", settings.micro_live_allow_accumulation ? 1 : 0);
    gtk_editable_set_text(GTK_EDITABLE(widgets->micro_live_allow_accumulation_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%d", settings.email_daily_enabled ? 1 : 0);
    gtk_editable_set_text(GTK_EDITABLE(widgets->email_daily_enabled_entry), buffer);

    gtk_editable_set_text(GTK_EDITABLE(widgets->email_recipient_entry), settings.email_recipient);

    snprintf(buffer, sizeof(buffer), "%d", settings.email_report_hour);
    gtk_editable_set_text(GTK_EDITABLE(widgets->email_report_hour_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%d", settings.email_report_minute);
    gtk_editable_set_text(GTK_EDITABLE(widgets->email_report_minute_entry), buffer);

    gtk_editable_set_text(GTK_EDITABLE(widgets->email_sendmail_command_entry), settings.email_sendmail_command);

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


static void on_acknowledge_real_order_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->state == NULL) {
        return;
    }

    char latest_order_id[128];
    latest_order_id[0] = '\0';

    if (!order_journal_get_latest_real_sent_order_id(latest_order_id, sizeof(latest_order_id))) {
        gtk_label_set_text(
            GTK_LABEL(widgets->status_label),
            "Nessun ordine reale REAL_SENT da riconoscere"
        );
        return;
    }

    StrategySettings settings = settings_load();

    snprintf(
        settings.micro_live_last_real_order_acknowledged,
        sizeof(settings.micro_live_last_real_order_acknowledged),
        "%s",
        latest_order_id
    );

    settings_save(&settings);

    char reason[256];
    snprintf(
        reason,
        sizeof(reason),
        "Ultimo ordine reale riconosciuto manualmente: %s",
        latest_order_id
    );

    db_log_engine_audit(
        "REAL_ORDER_ACKNOWLEDGED",
        "ACKNOWLEDGED",
        reason,
        widgets->state->current_price,
        widgets->state->btc_balance,
        widgets->state->eur_balance,
        0.0,
        0.0
    );

    gtk_label_set_text(
        GTK_LABEL(widgets->status_label),
        "Ultimo ordine reale riconosciuto: Helix può essere riarmato dopo revisione"
    );

    fill_settings_entries(widgets);
    refresh_dashboard(widgets);
}


static void on_seed_paper_slots_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->state == NULL) {
        return;
    }

    db_clear_paper_position_slots();
    int created = db_seed_demo_paper_position_slots();

    char message[256];
    snprintf(
        message,
        sizeof(message),
        "Paper/SIM: seed demo ricreato, slot OPEN: %d",
        created
    );

    db_log_engine_audit(
        "PAPER_SIM",
        "SEED_DEMO_SLOTS",
        message,
        widgets->state->current_price,
        widgets->state->btc_balance,
        widgets->state->eur_balance,
        0.0,
        0.0
    );

    gtk_label_set_text(GTK_LABEL(widgets->status_label), message);

    refresh_dashboard(widgets);
}

static void on_clear_paper_slots_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->state == NULL) {
        return;
    }

    int ok = db_clear_paper_position_slots();

    const char *message = ok ?
        "Paper/SIM: slot paper eliminati" :
        "Paper/SIM: errore eliminazione slot paper";

    db_log_engine_audit(
        "PAPER_SIM",
        ok ? "CLEAR_SLOTS" : "CLEAR_SLOTS_FAILED",
        message,
        widgets->state->current_price,
        widgets->state->btc_balance,
        widgets->state->eur_balance,
        0.0,
        0.0
    );

    gtk_label_set_text(GTK_LABEL(widgets->status_label), message);

    refresh_dashboard(widgets);
}


static void on_run_paper_best_profit_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->state == NULL) {
        return;
    }

    StrategySettings settings = settings_load();

    if (widgets->state->current_price <= 0.0) {
        gtk_label_set_text(
            GTK_LABEL(widgets->status_label),
            "Paper/SIM BEST_PROFIT bloccato: prezzo corrente non valido"
        );

        db_log_engine_audit(
            "PAPER_SIM",
            "BEST_PROFIT_BLOCKED_NO_PRICE",
            "Paper BEST_PROFIT bloccato: prezzo corrente non valido",
            widgets->state->current_price,
            widgets->state->btc_balance,
            widgets->state->eur_balance,
            0.0,
            0.0
        );

        return;
    }

    PaperPositionSlotRecord slots[64];
    memset(slots, 0, sizeof(slots));

    int slot_count = db_get_open_paper_position_slots(
        slots,
        64
    );

    if (slot_count <= 0) {
        gtk_label_set_text(
            GTK_LABEL(widgets->status_label),
            "Paper/SIM BEST_PROFIT: nessuno slot paper OPEN"
        );

        db_log_engine_audit(
            "PAPER_SIM",
            "BEST_PROFIT_NO_OPEN_SLOTS",
            "Paper BEST_PROFIT: nessuno slot paper OPEN",
            widgets->state->current_price,
            widgets->state->btc_balance,
            widgets->state->eur_balance,
            0.0,
            0.0
        );

        return;
    }

    double fee_rate = settings.estimated_fee_percent / 100.0;
    if (fee_rate < 0.0) {
        fee_rate = 0.0;
    }

    int best_index = -1;
    double best_gross = 0.0;
    double best_fee = 0.0;
    double best_net = 0.0;
    double best_profit = 0.0;
    double best_profit_percent = 0.0;

    for (int i = 0; i < slot_count; i++) {
        double gross = slots[i].base_size_btc * widgets->state->current_price;
        double fee = gross * fee_rate;
        double net = gross - fee;
        double profit = net - slots[i].cost_eur;
        double profit_percent = slots[i].cost_eur > 0.0 ?
            (profit / slots[i].cost_eur) * 100.0 :
            0.0;

        char reason[320];
        snprintf(
            reason,
            sizeof(reason),
            "Paper slot %s valutato | gross %.2f | fee %.2f | net %.2f | cost %.2f | profit %.2f EUR %.2f%%",
            slots[i].label,
            gross,
            fee,
            net,
            slots[i].cost_eur,
            profit,
            profit_percent
        );

        db_log_engine_audit(
            "PAPER_SIM",
            "BEST_PROFIT_SLOT_EVALUATED",
            reason,
            widgets->state->current_price,
            slots[i].base_size_btc,
            net,
            fee,
            profit
        );

        if (best_index < 0 || profit > best_profit) {
            best_index = i;
            best_gross = gross;
            best_fee = fee;
            best_net = net;
            best_profit = profit;
            best_profit_percent = profit_percent;
        }
    }

    if (best_index < 0) {
        gtk_label_set_text(
            GTK_LABEL(widgets->status_label),
            "Paper/SIM BEST_PROFIT: nessuno slot selezionabile"
        );
        return;
    }

    PaperPositionSlotRecord *best = &slots[best_index];

    int profitable =
        best_profit >= settings.min_profit_eur &&
        best_profit_percent >= settings.min_profit_percent;

    char message[360];

    if (profitable) {
        int closed = db_close_paper_position_slot(
            best->id,
            best_net,
            best_profit
        );

        snprintf(
            message,
            sizeof(message),
            "Paper BEST_PROFIT: slot %s scelto e %s | gross %.2f | fee %.2f | net %.2f | profit %.2f EUR %.2f%%",
            best->label,
            closed ? "chiuso" : "NON chiuso",
            best_gross,
            best_fee,
            best_net,
            best_profit,
            best_profit_percent
        );

        db_log_engine_audit(
            "PAPER_SIM",
            closed ? "BEST_PROFIT_SLOT_CLOSED" : "BEST_PROFIT_CLOSE_FAILED",
            message,
            widgets->state->current_price,
            best->base_size_btc,
            best_net,
            best_fee,
            best_profit
        );
    } else {
        snprintf(
            message,
            sizeof(message),
            "Paper BEST_PROFIT: slot %s migliore ma non profittevole | gross %.2f | fee %.2f | net %.2f | profit %.2f EUR %.2f%% | soglie %.2f EUR %.2f%%",
            best->label,
            best_gross,
            best_fee,
            best_net,
            best_profit,
            best_profit_percent,
            settings.min_profit_eur,
            settings.min_profit_percent
        );

        db_log_engine_audit(
            "PAPER_SIM",
            "BEST_PROFIT_NOT_PROFITABLE",
            message,
            widgets->state->current_price,
            best->base_size_btc,
            best_net,
            best_fee,
            best_profit
        );
    }

    gtk_label_set_text(GTK_LABEL(widgets->status_label), message);

    refresh_dashboard(widgets);
}

static void read_email_settings_from_entries(AppWidgets *widgets, StrategySettings *settings) {
    if (widgets == NULL || settings == NULL) {
        return;
    }

    settings->email_daily_enabled =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->email_daily_enabled_entry))) ? 1 : 0;

    snprintf(
        settings->email_recipient,
        sizeof(settings->email_recipient),
        "%s",
        gtk_editable_get_text(GTK_EDITABLE(widgets->email_recipient_entry))
    );

    settings->email_report_hour =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->email_report_hour_entry)));

    settings->email_report_minute =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->email_report_minute_entry)));

    snprintf(
        settings->email_sendmail_command,
        sizeof(settings->email_sendmail_command),
        "%s",
        gtk_editable_get_text(GTK_EDITABLE(widgets->email_sendmail_command_entry))
    );
}

static int email_settings_are_valid(const StrategySettings *settings, char *message, size_t message_size) {
    if (settings == NULL) {
        snprintf(message, message_size, "Email report: settings non disponibili");
        return 0;
    }

    if (settings->email_report_hour < 0 || settings->email_report_hour > 23) {
        snprintf(message, message_size, "Email report: ora non valida, usare 0-23");
        return 0;
    }

    if (settings->email_report_minute < 0 || settings->email_report_minute > 59) {
        snprintf(message, message_size, "Email report: minuto non valido, usare 0-59");
        return 0;
    }

    if (settings->email_recipient[0] == '\0') {
        snprintf(message, message_size, "Email report: destinatario mancante");
        return 0;
    }

    if (settings->email_sendmail_command[0] == '\0') {
        snprintf(message, message_size, "Email report: comando invio mancante");
        return 0;
    }

    snprintf(message, message_size, "Email report: impostazioni valide");
    return 1;
}

static void on_save_email_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;
    StrategySettings settings = settings_load();
    char message[256];

    if (widgets == NULL) {
        return;
    }

    read_email_settings_from_entries(widgets, &settings);

    if (!email_settings_are_valid(&settings, message, sizeof(message))) {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), message);
        return;
    }

    settings_save(&settings);
    gtk_label_set_text(GTK_LABEL(widgets->status_label), "Preferenze email salvate");
}

static void on_test_email_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;
    StrategySettings settings = settings_load();
    char message[256];

    if (widgets == NULL) {
        return;
    }

    settings_save(&settings);

    if (email_delivery_send_test(&settings, message, sizeof(message))) {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), message);
    } else {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), message);
    }
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

    settings.micro_live_enabled =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->micro_live_enabled_entry))) ? 1 : 0;

    settings.micro_live_max_order_eur =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(widgets->micro_live_max_order_eur_entry)));

    settings.micro_live_stop_after_real_order =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->micro_live_stop_after_real_order_entry))) ? 1 : 0;

    settings.micro_live_allow_accumulation =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->micro_live_allow_accumulation_entry))) ? 1 : 0;

    settings.email_daily_enabled =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->email_daily_enabled_entry))) ? 1 : 0;

    snprintf(
        settings.email_recipient,
        sizeof(settings.email_recipient),
        "%s",
        gtk_editable_get_text(GTK_EDITABLE(widgets->email_recipient_entry))
    );

    settings.email_report_hour =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->email_report_hour_entry)));

    settings.email_report_minute =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->email_report_minute_entry)));

    snprintf(
        settings.email_sendmail_command,
        sizeof(settings.email_sendmail_command),
        "%s",
        gtk_editable_get_text(GTK_EDITABLE(widgets->email_sendmail_command_entry))
    );

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
        settings.max_drawdown_percent < 0.0 ||
        settings.micro_live_max_order_eur <= 0.0 ||
        settings.micro_live_max_order_eur > 50.0
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


typedef struct {
    AppWidgets *app_widgets;
    GtkWidget *eur_entry;
    GtkWidget *btc_entry;
    GtkWidget *price_entry;
    GtkWidget *used_slots_entry;
    GtkWidget *result_label;
} DryRunScenarioWidgets;

static void set_entry_double(GtkWidget *entry, double value, int decimals) {
    char buffer[64];

    if (entry == NULL) {
        return;
    }

    if (decimals <= 2) {
        snprintf(buffer, sizeof(buffer), "%.2f", value);
    } else {
        snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    }

    gtk_editable_set_text(GTK_EDITABLE(entry), buffer);
}

static void set_entry_int(GtkWidget *entry, int value) {
    char buffer[32];

    if (entry == NULL) {
        return;
    }

    snprintf(buffer, sizeof(buffer), "%d", value);
    gtk_editable_set_text(GTK_EDITABLE(entry), buffer);
}

static void calculate_dryrun_scenario(DryRunScenarioWidgets *scenario) {
    if (
        scenario == NULL ||
        scenario->app_widgets == NULL ||
        scenario->app_widgets->state == NULL ||
        scenario->result_label == NULL
    ) {
        return;
    }

    StrategySettings settings = settings_load();

    double eur_balance =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(scenario->eur_entry)));
    double btc_balance =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(scenario->btc_entry)));
    double current_price =
        parse_decimal_input(gtk_editable_get_text(GTK_EDITABLE(scenario->price_entry)));
    int used_slots =
        atoi(gtk_editable_get_text(GTK_EDITABLE(scenario->used_slots_entry)));

    if (used_slots < 0) {
        used_slots = 0;
    }

    double fee_rate = settings.estimated_fee_percent / 100.0;
    double slot_amount = settings.slot_amount_eur;
    double protected_eur = eur_balance * (settings.liquidity_reserve_percent / 100.0);
    double released_eur = settings.reserve_released_slots * slot_amount;
    double operational_eur = eur_balance - protected_eur + released_eur;

    if (operational_eur < 0.0) {
        operational_eur = 0.0;
    }

    if (operational_eur > eur_balance) {
        operational_eur = eur_balance;
    }

    int slots_available = settings.max_slots - used_slots;
    if (slots_available < 0) {
        slots_available = 0;
    }

    int buy_possible =
        current_price > 0.0 &&
        slot_amount > 0.0 &&
        operational_eur >= slot_amount &&
        slots_available > 0 &&
        settings.emergency_stop_enabled == 0;

    TradePreview buy_preview = trade_preview_buy(
        slot_amount,
        current_price,
        fee_rate
    );

    double avg_buy_price = scenario->app_widgets->state->avg_buy_price;
    if (avg_buy_price <= 0.0 && current_price > 0.0) {
        avg_buy_price = current_price;
    }

    double cost_basis = btc_balance * avg_buy_price;

    TradePreview sell_preview = trade_preview_sell(
        btc_balance,
        current_price,
        cost_basis,
        fee_rate,
        settings.min_profit_eur,
        settings.min_profit_percent
    );

    const char *buy_decision = "BLOCK";
    const char *buy_reason = "condizioni BUY non sufficienti";

    if (settings.emergency_stop_enabled) {
        buy_reason = "kill-switch attivo";
    } else if (current_price <= 0.0) {
        buy_reason = "prezzo non valido";
    } else if (slots_available <= 0) {
        buy_reason = "nessuno slot libero";
    } else if (operational_eur < slot_amount) {
        buy_reason = "liquidità operativa insufficiente";
    } else if (buy_possible && buy_preview.net_value > 0.0) {
        buy_decision = "ALLOW";
        buy_reason = "BUY simulato consentito dai vincoli locali";
    }

    const char *sell_decision = "BLOCK";
    const char *sell_reason = "condizioni SELL non sufficienti";

    if (settings.emergency_stop_enabled) {
        sell_reason = "kill-switch attivo";
    } else if (current_price <= 0.0) {
        sell_reason = "prezzo non valido";
    } else if (btc_balance <= 0.0) {
        sell_reason = "nessun BTC disponibile";
    } else if (sell_preview.allowed) {
        sell_decision = "ALLOW";
        sell_reason = "SELL simulato consentito: profitto netto sufficiente";
    } else {
        sell_reason = "SELL simulato bloccato: profitto netto insufficiente";
    }

    char result[4096];

    snprintf(
        result,
        sizeof(result),
        "=== SCENARIO INSERITO ===\n"
        "EUR simulati: %.2f\n"
        "BTC simulati: %.8f\n"
        "Prezzo BTC-EUR simulato: %.2f\n"
        "Slot usati simulati: %d / %d\n\n"

        "=== LIQUIDITÀ ===\n"
        "Riserva protetta: %.2f%% = %.2f EUR\n"
        "Slot riserva sbloccati: %d\n"
        "Liquidità rilasciata: %.2f EUR\n"
        "Liquidità operativa stimata: %.2f EUR\n"
        "Slot EUR: %.2f\n"
        "Slot liberi: %d\n\n"

        "=== BUY DRY-RUN ===\n"
        "Decisione: %s\n"
        "Motivo: %s\n"
        "EUR impegnati: %.2f\n"
        "Fee stimata: %.2f\n"
        "BTC stimati: %.8f\n"
        "Prezzo medio effettivo stimato: %.2f\n\n"

        "=== SELL DRY-RUN ===\n"
        "Decisione: %s\n"
        "Motivo: %s\n"
        "Valore lordo: %.2f\n"
        "Fee stimata: %.2f\n"
        "Netto vendita: %.2f\n"
        "Cost basis stimato: %.2f\n"
        "Profitto netto stimato: %.2f EUR\n"
        "Profitto netto stimato: %.2f%%\n\n"

        "Nota: questa simulazione non legge Coinbase e non invia ordini reali.",
        eur_balance,
        btc_balance,
        current_price,
        used_slots,
        settings.max_slots,

        settings.liquidity_reserve_percent,
        protected_eur,
        settings.reserve_released_slots,
        released_eur,
        operational_eur,
        slot_amount,
        slots_available,

        buy_decision,
        buy_reason,
        slot_amount,
        buy_preview.estimated_fee,
        buy_preview.net_value,
        buy_preview.net_value > 0.0 ? slot_amount / buy_preview.net_value : 0.0,

        sell_decision,
        sell_reason,
        sell_preview.gross_value,
        sell_preview.estimated_fee,
        sell_preview.net_value,
        sell_preview.cost_basis,
        sell_preview.net_profit,
        sell_preview.net_profit_percent
    );

    gtk_label_set_text(GTK_LABEL(scenario->result_label), result);
}

static void on_dryrun_scenario_calculate_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    calculate_dryrun_scenario((DryRunScenarioWidgets *)user_data);
}

static void on_dryrun_scenario_use_current_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    DryRunScenarioWidgets *scenario = user_data;

    if (scenario == NULL || scenario->app_widgets == NULL || scenario->app_widgets->state == NULL) {
        return;
    }

    BotState *state = scenario->app_widgets->state;

    set_entry_double(scenario->eur_entry, state->eur_balance, 2);
    set_entry_double(scenario->btc_entry, state->btc_balance, 8);
    set_entry_double(scenario->price_entry, state->current_price, 2);
    set_entry_int(scenario->used_slots_entry, state->used_slots);

    calculate_dryrun_scenario(scenario);
}

static void on_dryrun_scenario_close_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    GtkWidget *dialog = user_data;

    if (dialog != NULL) {
        gtk_window_close(GTK_WINDOW(dialog));
    }
}

static void show_dryrun_scenario_dialog(AppWidgets *widgets) {
    if (widgets == NULL || widgets->window == NULL || widgets->state == NULL) {
        return;
    }

    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Simula scenario dry-run");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(widgets->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 760, -1);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 18);
    gtk_widget_set_margin_bottom(box, 18);
    gtk_widget_set_margin_start(box, 18);
    gtk_widget_set_margin_end(box, 18);

    GtkWidget *description = gtk_label_new(
        "Inserisci valori ipotetici per vedere cosa farebbe Helix in dry-run. "
        "La simulazione non modifica wallet, DB, Coinbase o ordini."
    );
    gtk_label_set_wrap(GTK_LABEL(description), TRUE);
    gtk_widget_set_halign(description, GTK_ALIGN_START);

    GtkWidget *eur_entry = gtk_entry_new();
    GtkWidget *btc_entry = gtk_entry_new();
    GtkWidget *price_entry = gtk_entry_new();
    GtkWidget *used_slots_entry = gtk_entry_new();

    set_entry_double(eur_entry, widgets->state->eur_balance, 2);
    set_entry_double(btc_entry, widgets->state->btc_balance, 8);
    set_entry_double(price_entry, widgets->state->current_price, 2);
    set_entry_int(used_slots_entry, widgets->state->used_slots);

    GtkWidget *result_label = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(result_label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(result_label), TRUE);
    gtk_widget_set_halign(result_label, GTK_ALIGN_START);

    GtkWidget *result_scrolled = gtk_scrolled_window_new();
    gtk_widget_set_size_request(result_scrolled, -1, 320);
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(result_scrolled),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC
    );
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(result_scrolled), result_label);

    GtkWidget *buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *calculate_button = gtk_button_new_with_label("Calcola scenario");
    GtkWidget *use_current_button = gtk_button_new_with_label("Usa valori correnti");
    GtkWidget *close_button = gtk_button_new_with_label("Chiudi");

    gtk_box_append(GTK_BOX(buttons_box), calculate_button);
    gtk_box_append(GTK_BOX(buttons_box), use_current_button);
    gtk_box_append(GTK_BOX(buttons_box), close_button);

    gtk_box_append(GTK_BOX(box), description);
    gtk_box_append(GTK_BOX(box), create_setting_row("EUR simulati", eur_entry));
    gtk_box_append(GTK_BOX(box), create_setting_row("BTC simulati", btc_entry));
    gtk_box_append(GTK_BOX(box), create_setting_row("Prezzo BTC-EUR simulato", price_entry));
    gtk_box_append(GTK_BOX(box), create_setting_row("Slot usati simulati", used_slots_entry));
    gtk_box_append(GTK_BOX(box), buttons_box);
    gtk_box_append(GTK_BOX(box), result_scrolled);

    DryRunScenarioWidgets *scenario = g_malloc0(sizeof(DryRunScenarioWidgets));
    scenario->app_widgets = widgets;
    scenario->eur_entry = eur_entry;
    scenario->btc_entry = btc_entry;
    scenario->price_entry = price_entry;
    scenario->used_slots_entry = used_slots_entry;
    scenario->result_label = result_label;

    g_object_set_data_full(
        G_OBJECT(dialog),
        "helix-dryrun-scenario",
        scenario,
        g_free
    );

    g_signal_connect(calculate_button, "clicked", G_CALLBACK(on_dryrun_scenario_calculate_clicked), scenario);
    g_signal_connect(use_current_button, "clicked", G_CALLBACK(on_dryrun_scenario_use_current_clicked), scenario);
    g_signal_connect(close_button, "clicked", G_CALLBACK(on_dryrun_scenario_close_clicked), dialog);
    g_signal_connect(dialog, "close-request", G_CALLBACK(on_hide_window_close_request), NULL);

    gtk_window_set_child(GTK_WINDOW(dialog), box);

    calculate_dryrun_scenario(scenario);

    gtk_window_present(GTK_WINDOW(dialog));
}



static GtkWidget *create_table_cell_label(const char *text, gboolean header) {
    GtkWidget *label = gtk_label_new(text ? text : "");

    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_yalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(label), header ? 28 : 44);

    gtk_widget_set_margin_top(label, 4);
    gtk_widget_set_margin_bottom(label, 4);
    gtk_widget_set_margin_start(label, 6);
    gtk_widget_set_margin_end(label, 6);

    if (header) {
        gtk_widget_add_css_class(label, "heading");
    }

    return label;
}

static void show_sql_table_window(
    AppWidgets *widgets,
    const char *title,
    const char *sql,
    int default_width,
    int default_height
) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    GtkWidget *window;
    GtkWidget *root_box;
    GtkWidget *scrolled_window;
    GtkWidget *grid;
    GtkWidget *footer_box;
    GtkWidget *close_button;
    GtkWidget *status_label;
    int column_count;
    int row = 1;
    int sqlite_rc;

    if (widgets == NULL || widgets->window == NULL || sql == NULL) {
        return;
    }

    if (sqlite3_open("data/helix.db", &db) != SQLITE_OK) {
        show_text_dialog(widgets, title, "Impossibile aprire data/helix.db");
        return;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        char error_message[512];

        snprintf(
            error_message,
            sizeof(error_message),
            "Errore SQL: %.420s",
            sqlite3_errmsg(db)
        );

        sqlite3_close(db);
        show_text_dialog(widgets, title, error_message);
        return;
    }

    column_count = sqlite3_column_count(stmt);

    window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window), title ? title : "Tabella Helix");
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(widgets->window));
    gtk_window_set_default_size(GTK_WINDOW(window), default_width, default_height);

    root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(root_box, 12);
    gtk_widget_set_margin_bottom(root_box, 12);
    gtk_widget_set_margin_start(root_box, 12);
    gtk_widget_set_margin_end(root_box, 12);

    scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scrolled_window),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC
    );
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_widget_set_hexpand(scrolled_window, TRUE);

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);

    for (int col = 0; col < column_count; col++) {
        GtkWidget *cell = create_table_cell_label(sqlite3_column_name(stmt, col), TRUE);
        gtk_grid_attach(GTK_GRID(grid), cell, col, 0, 1, 1);
    }

    while ((sqlite_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        for (int col = 0; col < column_count; col++) {
            const unsigned char *value = sqlite3_column_text(stmt, col);
            GtkWidget *cell = create_table_cell_label(value ? (const char *)value : "", FALSE);
            gtk_grid_attach(GTK_GRID(grid), cell, col, row, 1, 1);
        }

        row++;
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), grid);

    footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(footer_box, GTK_ALIGN_END);

    {
        char status_text[128];

        if (sqlite_rc == SQLITE_DONE) {
            snprintf(status_text, sizeof(status_text), "Righe visualizzate: %d", row - 1);
        } else {
            snprintf(status_text, sizeof(status_text), "Righe visualizzate: %d | errore lettura", row - 1);
        }

        status_label = gtk_label_new(status_text);
    }

    close_button = gtk_button_new_with_label("Chiudi");

    gtk_box_append(GTK_BOX(footer_box), status_label);
    gtk_box_append(GTK_BOX(footer_box), close_button);

    gtk_box_append(GTK_BOX(root_box), scrolled_window);
    gtk_box_append(GTK_BOX(root_box), footer_box);

    gtk_window_set_child(GTK_WINDOW(window), root_box);

    g_signal_connect_swapped(close_button, "clicked", G_CALLBACK(gtk_window_close), window);
    g_signal_connect(window, "close-request", G_CALLBACK(on_hide_window_close_request), NULL);

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    gtk_window_present(GTK_WINDOW(window));
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

static void on_menu_show_email_report_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;

    if (widgets == NULL || widgets->email_expander == NULL) {
        return;
    }

    fill_settings_entries(widgets);
    present_utility_window(widgets->email_expander);
    gtk_label_set_text(GTK_LABEL(widgets->status_label), "Preferenze: email report aperte");
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


static void on_menu_show_position_slots_table_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;

    show_sql_table_window(
        widgets,
        "Tabella slot reali",
        "SELECT id, buy_order_id, buy_client_order_id, base_size_btc, cost_eur, "
        "buy_fee_eur, cost_eur + buy_fee_eur AS allocated_cost, avg_buy_price, "
        "status, opened_at, closed_at, sell_order_id, sell_net_eur, realized_profit_eur "
        "FROM position_slots "
        "ORDER BY opened_at ASC, id ASC "
        "LIMIT 200;",
        1180,
        560
    );

    if (widgets != NULL && widgets->status_label != NULL) {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), "Visualizza: tabella slot reali aperta");
    }
}

static void on_menu_show_trades_table_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;

    show_sql_table_window(
        widgets,
        "Tabella trades",
        "SELECT id, type, price, eur_amount, btc_amount, fee_amount, net_total, "
        "reference, source, created_at "
        "FROM trades "
        "ORDER BY datetime(created_at) DESC, id DESC "
        "LIMIT 200;",
        1060,
        520
    );

    if (widgets != NULL && widgets->status_label != NULL) {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), "Visualizza: tabella trades aperta");
    }
}

static void on_menu_show_order_journal_table_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;

    show_sql_table_window(
        widgets,
        "Tabella order journal",
        "SELECT id, client_order_id, side, dry_run, status, phase, decision, "
        "requested_quote_size, requested_base_size, preview_total_eur, preview_fee_eur, "
        "preview_base_size, preview_avg_price, coinbase_order_id, execution_decision, "
        "substr(reason, 1, 160) AS reason, created_at "
        "FROM order_journal "
        "ORDER BY id DESC "
        "LIMIT 200;",
        1280,
        620
    );

    if (widgets != NULL && widgets->status_label != NULL) {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), "Visualizza: tabella order journal aperta");
    }
}

static void on_menu_show_engine_audit_table_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;

    show_sql_table_window(
        widgets,
        "Tabella engine audit",
        "SELECT id, created_at, event_type, decision, price, btc_amount, eur_amount, "
        "estimated_fee, net_profit, substr(reason, 1, 180) AS reason "
        "FROM engine_audit "
        "ORDER BY id DESC "
        "LIMIT 200;",
        1180,
        620
    );

    if (widgets != NULL && widgets->status_label != NULL) {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), "Visualizza: tabella engine audit aperta");
    }
}


static void build_prelive_report_message(AppWidgets *widgets, char *message, size_t message_size) {
    if (message == NULL || message_size == 0) {
        return;
    }

    message[0] = '\0';

    if (widgets == NULL || widgets->state == NULL) {
        snprintf(message, message_size, "Report pre-live non disponibile: stato applicazione non valido.");
        return;
    }

    StrategySettings settings = settings_load();
    LiveReadinessReport readiness = live_readiness_check(widgets->state, &settings);
    ApiHealthReport api_health = api_health_check_light(widgets->state, &settings);
    PreliveReport prelive_report;
    EngineAuditSummary audit_summary;

    memset(&prelive_report, 0, sizeof(prelive_report));
    memset(&audit_summary, 0, sizeof(audit_summary));

    order_journal_get_prelive_report(&prelive_report);
    db_get_engine_audit_summary_last_days(&audit_summary, 7);

    int dry_run_target = 20;
    int dry_run_missing = dry_run_target - prelive_report.dry_run_ready_last_7_days;
    if (dry_run_missing < 0) {
        dry_run_missing = 0;
    }

    int safety_blocks_total =
        audit_summary.reconciliation_blocks +
        audit_summary.order_recovery_blocks +
        audit_summary.volatility_blocks +
        audit_summary.operational_limits_blocks +
        audit_summary.risk_guard_blocks +
        audit_summary.final_live_gate_blocks +
        audit_summary.anti_duplicate_blocks +
        audit_summary.prelive_validation_blocks +
        audit_summary.real_executor_blocks +
        audit_summary.post_order_reconciliation_blocks;

    const char *runtime_label = runtime_mode_to_string(settings.runtime_mode);
    const char *bot_running_label = widgets->state->running ? "avviato" : "fermo";
    const char *kill_switch_label = settings.emergency_stop_enabled ? "ATTIVO" : "disattivato";
    const char *live_arm_label = settings.live_trading_armed ? "ATTIVO" : "disattivato";
    const char *reserve_label = settings.reserve_released_slots > 0 ? "parzialmente sbloccata" : "protetta";

    const char *prelive_status = "NON PRONTO";
    if (
        readiness.blocking_count == 0 &&
        api_health.blocking_count == 0 &&
        prelive_report.dry_run_ready_last_7_days >= dry_run_target &&
        prelive_report.final_gate_ok_last_7_days > 0 &&
        prelive_report.post_order_recon_blocked_last_7_days == 0 &&
        prelive_report.real_sent_last_7_days == 0
    ) {
        prelive_status = "QUASI PRONTO - serve comunque revisione manuale";
    }

    snprintf(
        message,
        message_size,
        "=== STATO GENERALE ===\n"
        "Pre-live: %s\n"
        "Bot: %s\n"
        "Runtime: %s\n"
        "Kill-switch: %s\n"
        "LIVE_TRADING arm: %s\n"
        "Riserva liquidità: %.2f%% (%s, slot sbloccati %d/%d)\nMicro-live accumulo con posizione aperta: %s\n\n"

        "=== READINESS ===\n"
        "Stato: %s\n"
        "Blocchi: %d\n"
        "Warning: %d\n"
        "Motivo: %.500s\n\n"

        "=== API HEALTH ===\n"
        "Stato: %s\n"
        "Blocchi: %d\n"
        "Warning: %d\n"
        "Motivo: %.500s\n\n"

        "=== DRY-RUN E JOURNAL ===\n"
        "Dry-run totali ultimi 7 giorni: %d\n"
        "Dry-run validi: %d / %d\n"
        "Dry-run bloccati correttamente: %d\n"
        "  BUY totali/validi/bloccati: %d / %d / %d\n"
        "  SELL totali/validi/bloccati: %d / %d / %d\n"
        "  SELL bloccati perché non profittevoli: %d\n"
        "Dry-run validi mancanti alla soglia minima: %d\n"
        "Journal ultime 24h: %d\n"
        "Ultimo client_order_id: %s\n"
        "Ultimo evento journal: %s\n"
        "Ordini reali inviati ultimi 7 giorni: %d\n\n"

        "=== FINAL GATE / EXECUTOR ===\n"
        "Final gate OK: %d\n"
        "Final gate bloccati: %d\n"
        "Executor reale bloccato: %d\n"
        "Post-order reconciliation OK: %d\n"
        "Post-order reconciliation bloccata: %d\n\n"

        "=== SAFETY BLOCKS ULTIMI 7 GIORNI ===\n"
        "Totale audit ultimi 7 giorni: %d\n"
        "Totale blocchi safety: %d\n"
        "RECONCILIATION: %d\n"
        "ORDER_RECOVERY: %d\n"
        "VOLATILITY_PROTECTION: %d\n"
        "OPERATIONAL_LIMITS: %d\n"
        "RISK_GUARD: %d\n"
        "FINAL_LIVE_GATE: %d\n"
        "ANTI_DUPLICATE_ORDER: %d\n"
        "PRELIVE_VALIDATION: %d\n"
        "REAL_EXECUTOR: %d\n"
        "POST_ORDER_RECONCILIATION: %d\n"
        "EMERGENCY_STOP eventi: %d\n"
        "LIVE_TRADING_ARM eventi: %d\n\n"

        "=== ULTIMO BLOCCO JOURNAL ===\n"
        "%s%s%s\n\n"

        "=== PROSSIME AZIONI CONSIGLIATE ===\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s\n\n"

        "Nota: questo report è diagnostico. Non abilita ordini reali.",
        prelive_status,
        bot_running_label,
        runtime_label,
        kill_switch_label,
        live_arm_label,
        settings.liquidity_reserve_percent,
        reserve_label,
        settings.reserve_released_slots,
        settings.max_slots,
        settings.micro_live_allow_accumulation ? "ATTIVO" : "disattivato",

        readiness.status,
        readiness.blocking_count,
        readiness.warning_count,
        readiness.reason,

        api_health.status,
        api_health.blocking_count,
        api_health.warning_count,
        api_health.reason,

        prelive_report.dry_run_last_7_days,
        prelive_report.dry_run_ready_last_7_days,
        dry_run_target,
        prelive_report.dry_run_blocked_last_7_days,
        prelive_report.buy_dry_run_last_7_days,
        prelive_report.buy_dry_run_ready_last_7_days,
        prelive_report.buy_dry_run_blocked_last_7_days,
        prelive_report.sell_dry_run_last_7_days,
        prelive_report.sell_dry_run_ready_last_7_days,
        prelive_report.sell_dry_run_blocked_last_7_days,
        prelive_report.sell_blocked_not_profitable_last_7_days,
        dry_run_missing,
        prelive_report.journal_entries_last_24h,
        prelive_report.last_client_order_id[0] ? prelive_report.last_client_order_id : "nessuno",
        prelive_report.last_journal_at[0] ? prelive_report.last_journal_at : "nessuno",
        prelive_report.real_sent_last_7_days,

        prelive_report.final_gate_ok_last_7_days,
        prelive_report.final_gate_blocked_last_7_days,
        prelive_report.real_executor_blocked_last_7_days,
        prelive_report.post_order_recon_ok_last_7_days,
        prelive_report.post_order_recon_blocked_last_7_days,

        audit_summary.total_last_days,
        safety_blocks_total,
        audit_summary.reconciliation_blocks,
        audit_summary.order_recovery_blocks,
        audit_summary.volatility_blocks,
        audit_summary.operational_limits_blocks,
        audit_summary.risk_guard_blocks,
        audit_summary.final_live_gate_blocks,
        audit_summary.anti_duplicate_blocks,
        audit_summary.prelive_validation_blocks,
        audit_summary.real_executor_blocks,
        audit_summary.post_order_reconciliation_blocks,
        audit_summary.emergency_stop_events,
        audit_summary.live_trading_arm_events,

        prelive_report.last_blocked_at[0] ? prelive_report.last_blocked_at : "Nessun blocco registrato",
        prelive_report.last_block_reason[0] ? " | " : "",
        prelive_report.last_block_reason,

        dry_run_missing > 0 ?
            "- Continua a far girare Helix in LIVE_READONLY + dry-run: servono ancora dry-run validi." :
            "- Soglia dry-run valida raggiunta: revisiona audit e journal prima di qualunque live.",
        prelive_report.dry_run_blocked_last_7_days > 0 ?
            "- I dry-run bloccati correttamente sono utili: indicano che i gate stanno evitando operazioni non convenienti o non sicure." :
            "- Nessun dry-run bloccato registrato negli ultimi 7 giorni.",
        prelive_report.real_sent_last_7_days > 0 ?
            "- ATTENZIONE: risultano ordini reali nel journal. Verifica subito Coinbase e DB." :
            "- Nessun ordine reale risulta inviato negli ultimi 7 giorni.",
        safety_blocks_total > 0 ?
            "- Analizza le categorie di blocco più frequenti prima di cambiare strategia." :
            "- Nessun blocco safety frequente negli ultimi 7 giorni.",
        readiness.blocking_count > 0 ?
            "- Risolvi i blocchi readiness prima di considerare LIVE_TRADING." :
            "- Readiness senza blocchi critici.",
        api_health.blocking_count > 0 ?
            "- Risolvi i blocchi API health prima di qualunque test live." :
            "- API health senza blocchi critici."
    );
}

static void on_menu_show_prelive_report_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;
    char message[8192];

    build_prelive_report_message(widgets, message, sizeof(message));

    show_text_dialog(
        widgets,
        "Report pre-live",
        message
    );

    if (widgets != NULL && widgets->status_label != NULL) {
        gtk_label_set_text(GTK_LABEL(widgets->status_label), "Visualizza: report pre-live aperto");
    }
}

static void on_menu_export_prelive_report_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;
    char report[8192];
    char timestamp[64];
    time_t now;
    struct tm *local_time;
    FILE *file;

    build_prelive_report_message(widgets, report, sizeof(report));

    now = time(NULL);
    local_time = localtime(&now);

    if (local_time != NULL) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", local_time);
    } else {
        snprintf(timestamp, sizeof(timestamp), "timestamp non disponibile");
    }

    file = fopen("data/prelive_report.txt", "w");

    if (file == NULL) {
        if (widgets != NULL && widgets->status_label != NULL) {
            gtk_label_set_text(
                GTK_LABEL(widgets->status_label),
                "Errore: impossibile esportare data/prelive_report.txt"
            );
        }
        return;
    }

    fprintf(file, "Helix - Report pre-live\n");
    fprintf(file, "Generato: %s\n\n", timestamp);
    fprintf(file, "%s\n", report);
    fclose(file);

    set_temporary_status_message(
        widgets,
        "Report pre-live esportato in data/prelive_report.txt"
    );
}

static void build_safety_status_message(AppWidgets *widgets, char *message, size_t message_size) {
    if (message == NULL || message_size == 0) {
        return;
    }

    message[0] = '\0';

    if (widgets == NULL || widgets->state == NULL) {
        snprintf(message, message_size, "Stato protezioni non disponibile: stato applicazione non valido.");
        return;
    }

    StrategySettings settings = settings_load();
    LiveReadinessReport readiness = live_readiness_check(widgets->state, &settings);
    ApiHealthReport api_health = api_health_check_light(widgets->state, &settings);
    EngineAuditSummary audit_summary;

    memset(&audit_summary, 0, sizeof(audit_summary));
    db_get_engine_audit_summary_last_days(&audit_summary, 7);

    double operational_liquidity_percent = 100.0 - settings.liquidity_reserve_percent;
    if (operational_liquidity_percent < 0.0) {
        operational_liquidity_percent = 0.0;
    }

    snprintf(
        message,
        message_size,
        "=== STATO PROTEZIONI ===\n"
        "Bot: %s\n"
        "Runtime: %s\n"
        "Prezzo corrente: %.2f EUR\n"
        "EUR: %.2f\n"
        "BTC: %.8f\n"
        "Slot usati: %d / %d\n\n"

        "=== GATE PRINCIPALI ===\n"
        "Kill-switch: %s\n"
        "LIVE_TRADING arm: %s\n"
        "Readiness: %s (%d blocchi, %d warning)\n"
        "API health: %s (%d blocchi, %d warning)\n"
        "Executor reale: %s\n\n"

        "=== LIQUIDITÀ E SLOT ===\n"
        "Slot EUR: %.2f\n"
        "Riserva protetta: %.2f%%\n"
        "Liquidità operativa teorica: %.2f%%\n"
        "Slot riserva sbloccati manualmente: %d / %d\n"
        "Liquidità minima: %.2f%%\n\n"

        "=== LIMITI OPERATIVI ===\n"
        "Max ordini/giorno: %d\n"
        "Cooldown ordini: %d sec\n"
        "Max perdita giornaliera: %.2f EUR\n"
        "Max drawdown: %.2f%%\n"
        "Micro-live: %s | max ordine %.2f EUR | stop dopo ordine %s | accumulo %s\n"
        "Volatilità: max %.2f%% in %d sec\n\n"

        "=== AUDIT ULTIMI 7 GIORNI ===\n"
        "Totale audit: %d\n"
        "RECONCILIATION: %d\n"
        "ORDER_RECOVERY: %d\n"
        "VOLATILITY_PROTECTION: %d\n"
        "OPERATIONAL_LIMITS: %d\n"
        "RISK_GUARD: %d\n"
        "FINAL_LIVE_GATE: %d\n"
        "ANTI_DUPLICATE_ORDER: %d\n"
        "PRELIVE_VALIDATION: %d\n"
        "REAL_EXECUTOR: %d\n"
        "POST_ORDER_RECONCILIATION: %d\n\n"

        "=== NOTE ===\n"
        "Questo pannello non abilita trading reale. Serve solo a capire quali protezioni sono attive e quali stanno bloccando Helix.",
        widgets->state->running ? "avviato" : "fermo",
        runtime_mode_to_string(settings.runtime_mode),
        widgets->state->current_price,
        widgets->state->eur_balance,
        widgets->state->btc_balance,
        widgets->state->used_slots,
        widgets->state->max_slots,

        settings.emergency_stop_enabled ? "ATTIVO" : "disattivato",
        settings.live_trading_armed ? "ATTIVO" : "disattivato",
        readiness.status,
        readiness.blocking_count,
        readiness.warning_count,
        api_health.status,
        api_health.blocking_count,
        api_health.warning_count,
        real_executor_build_status_text(),

        settings.slot_amount_eur,
        settings.liquidity_reserve_percent,
        operational_liquidity_percent,
        settings.reserve_released_slots,
        settings.max_slots,
        settings.min_liquidity_percent,

        settings.max_orders_per_day,
        settings.order_cooldown_seconds,
        settings.max_daily_loss_eur,
        settings.max_drawdown_percent,
        settings.micro_live_enabled ? "ATTIVO" : "disattivato",
        settings.micro_live_max_order_eur,
        settings.micro_live_stop_after_real_order ? "ATTIVO" : "disattivato",
        settings.micro_live_allow_accumulation ? "ATTIVO" : "disattivato",
        settings.volatility_max_move_percent,
        settings.volatility_window_seconds,

        audit_summary.total_last_days,
        audit_summary.reconciliation_blocks,
        audit_summary.order_recovery_blocks,
        audit_summary.volatility_blocks,
        audit_summary.operational_limits_blocks,
        audit_summary.risk_guard_blocks,
        audit_summary.final_live_gate_blocks,
        audit_summary.anti_duplicate_blocks,
        audit_summary.prelive_validation_blocks,
        audit_summary.real_executor_blocks,
        audit_summary.post_order_reconciliation_blocks
    );
}

static void on_menu_show_safety_status_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;
    char message[4096];

    build_safety_status_message(widgets, message, sizeof(message));

    show_text_dialog(
        widgets,
        "Stato protezioni",
        message
    );

    set_temporary_status_message(widgets, "Visualizza: stato protezioni aperto");
}

static void on_menu_export_status_snapshot_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;
    char safety_status[4096];
    char prelive_report[8192];
    char timestamp[64];
    time_t now;
    struct tm *local_time;
    FILE *file;

    build_safety_status_message(widgets, safety_status, sizeof(safety_status));
    build_prelive_report_message(widgets, prelive_report, sizeof(prelive_report));

    now = time(NULL);
    local_time = localtime(&now);

    if (local_time != NULL) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", local_time);
    } else {
        snprintf(timestamp, sizeof(timestamp), "timestamp non disponibile");
    }

    file = fopen("data/status_snapshot.txt", "w");

    if (file == NULL) {
        set_temporary_status_message(
            widgets,
            "Errore: impossibile esportare data/status_snapshot.txt"
        );
        return;
    }

    fprintf(file, "Helix - Snapshot stato\n");
    fprintf(file, "Generato: %s\n\n", timestamp);
    fprintf(file, "%s\n\n", safety_status);
    fprintf(file, "%s\n", prelive_report);
    fclose(file);

    set_temporary_status_message(
        widgets,
        "Snapshot stato esportato in data/status_snapshot.txt"
    );
}


static void on_menu_show_dryrun_scenario_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;

    AppWidgets *widgets = user_data;

    show_dryrun_scenario_dialog(widgets);
    set_temporary_status_message(widgets, "Visualizza: simulatore dry-run aperto");
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

    {
        StrategySettings settings = settings_load();

        /*
         * Il report giornaliero è osservativo: deve poter partire anche
         * se Helix è aperto ma il bot non è running.
         */
        daily_report_maybe_send(widgets->state, &settings);
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
    GtkWidget *email_expander;
    GtkWidget *email_box;
    GtkWidget *email_dialog_scrolled_window;
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
    GtkWidget *micro_live_enabled_entry;
    GtkWidget *micro_live_max_order_eur_entry;
    GtkWidget *micro_live_stop_after_real_order_entry;
    GtkWidget *micro_live_allow_accumulation_entry;
    GtkWidget *email_daily_enabled_entry;
    GtkWidget *email_recipient_entry;
    GtkWidget *email_report_hour_entry;
    GtkWidget *email_report_minute_entry;
    GtkWidget *email_sendmail_command_entry;
    GtkWidget *email_buttons_box;
    GtkWidget *save_email_button;
    GtkWidget *test_email_button;
    GtkWidget *emergency_stop_label;
    GtkWidget *live_trading_arm_label;
    GtkWidget *live_trading_arm_buttons_box;
    GtkWidget *arm_live_trading_button;
    GtkWidget *disarm_live_trading_button;
    GtkWidget *acknowledge_real_order_button;
    GtkWidget *paper_slots_buttons_box;
    GtkWidget *seed_paper_slots_button;
    GtkWidget *clear_paper_slots_button;
    GtkWidget *run_paper_best_profit_button;
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
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

    menu_bar = create_main_menu_bar();

    root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(root_box, TRUE);
    gtk_widget_set_vexpand(root_box, TRUE);

    page_scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(page_scrolled_window),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC
    );
    gtk_widget_set_hexpand(page_scrolled_window, TRUE);
    gtk_widget_set_vexpand(page_scrolled_window, TRUE);

    main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_hexpand(main_box, TRUE);
    gtk_widget_set_vexpand(main_box, TRUE);
    gtk_widget_set_margin_top(main_box, 12);
    gtk_widget_set_margin_bottom(main_box, 12);
    gtk_widget_set_margin_start(main_box, 12);
    gtk_widget_set_margin_end(main_box, 12);

    top_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_hexpand(top_box, TRUE);

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

    configure_dashboard_label(price_label);
    configure_dashboard_label(eur_label);
    configure_dashboard_label(btc_label);
    configure_dashboard_label(slots_label);
    configure_dashboard_label(mode_label);
    configure_dashboard_label(runtime_mode_label);
    configure_dashboard_label(live_readiness_label);
    configure_dashboard_label(api_health_label);
    configure_dashboard_label(coinbase_credentials_label);
    configure_dashboard_label(remote_wallet_label);
    configure_dashboard_label(last_trade_label);
    configure_dashboard_label(settings_label);
    configure_dashboard_label(status_label);

    settings_expander = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(settings_expander), "Impostazioni strategia");
    gtk_window_set_transient_for(GTK_WINDOW(settings_expander), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(settings_expander), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(settings_expander), 760, 680);
    gtk_window_set_resizable(GTK_WINDOW(settings_expander), TRUE);
    g_signal_connect(settings_expander, "close-request", G_CALLBACK(on_hide_window_close_request), NULL);

    settings_dialog_scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(settings_dialog_scrolled_window),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC
    );
    gtk_widget_set_size_request(settings_dialog_scrolled_window, -1, 620);
    gtk_widget_set_vexpand(settings_dialog_scrolled_window, TRUE);

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
    micro_live_enabled_entry = gtk_entry_new();
    micro_live_max_order_eur_entry = gtk_entry_new();
    micro_live_stop_after_real_order_entry = gtk_entry_new();
    micro_live_allow_accumulation_entry = gtk_entry_new();
    email_daily_enabled_entry = gtk_entry_new();
    email_recipient_entry = gtk_entry_new();
    email_report_hour_entry = gtk_entry_new();
    email_report_minute_entry = gtk_entry_new();
    email_sendmail_command_entry = gtk_entry_new();

    const char *runtime_modes[] = {
        "SIMULATION",
        "LIVE_READONLY",
        "LIVE_TRADING",
        NULL
    };

    runtime_mode_dropdown = gtk_drop_down_new_from_strings(runtime_modes);

    save_settings_button = gtk_button_new_with_label("Salva impostazioni");

    email_buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    save_email_button = gtk_button_new_with_label("Salva preferenze email");
    test_email_button = gtk_button_new_with_label("Test consegna email");
    gtk_box_append(GTK_BOX(email_buttons_box), save_email_button);
    gtk_box_append(GTK_BOX(email_buttons_box), test_email_button);

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
    acknowledge_real_order_button = gtk_button_new_with_label("Acknowledge ultimo ordine reale");
    gtk_box_append(GTK_BOX(live_trading_arm_buttons_box), arm_live_trading_button);
    gtk_box_append(GTK_BOX(live_trading_arm_buttons_box), disarm_live_trading_button);
    gtk_box_append(GTK_BOX(live_trading_arm_buttons_box), acknowledge_real_order_button);

    paper_slots_buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    seed_paper_slots_button = gtk_button_new_with_label("Seed paper slots demo");
    clear_paper_slots_button = gtk_button_new_with_label("Clear paper slots");
    run_paper_best_profit_button = gtk_button_new_with_label("Run paper BEST_PROFIT");
    gtk_box_append(GTK_BOX(paper_slots_buttons_box), seed_paper_slots_button);
    gtk_box_append(GTK_BOX(paper_slots_buttons_box), clear_paper_slots_button);
    gtk_box_append(GTK_BOX(paper_slots_buttons_box), run_paper_best_profit_button);

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
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Micro-live attivo (0/1)", micro_live_enabled_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Micro-live max ordine EUR", micro_live_max_order_eur_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Stop dopo ordine reale (0/1)", micro_live_stop_after_real_order_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Consenti accumulo micro-live (0/1)", micro_live_allow_accumulation_entry));

    gtk_box_append(GTK_BOX(settings_box), emergency_stop_label);
    gtk_box_append(GTK_BOX(settings_box), emergency_buttons_box);
    gtk_box_append(GTK_BOX(settings_box), live_trading_arm_label);
    gtk_box_append(GTK_BOX(settings_box), live_trading_arm_buttons_box);
    gtk_box_append(GTK_BOX(settings_box), paper_slots_buttons_box);
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Modalità operativa", runtime_mode_dropdown));
    gtk_box_append(GTK_BOX(settings_box), save_settings_button);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(settings_dialog_scrolled_window), settings_box);
    gtk_window_set_child(GTK_WINDOW(settings_expander), settings_dialog_scrolled_window);

    email_expander = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(email_expander), "Email report");
    gtk_window_set_transient_for(GTK_WINDOW(email_expander), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(email_expander), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(email_expander), 760, 360);
    g_signal_connect(email_expander, "close-request", G_CALLBACK(on_hide_window_close_request), NULL);

    email_dialog_scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(email_dialog_scrolled_window, TRUE);

    email_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(email_box, 10);
    gtk_widget_set_margin_bottom(email_box, 10);
    gtk_widget_set_margin_start(email_box, 10);
    gtk_widget_set_margin_end(email_box, 10);

    gtk_box_append(GTK_BOX(email_box), create_section_title("Email report giornaliero"));
    gtk_box_append(GTK_BOX(email_box), create_setting_row("Email giornaliera attiva (0/1)", email_daily_enabled_entry));
    gtk_box_append(GTK_BOX(email_box), create_setting_row("Destinatario email", email_recipient_entry));
    gtk_box_append(GTK_BOX(email_box), create_setting_row("Ora report email (0-23)", email_report_hour_entry));
    gtk_box_append(GTK_BOX(email_box), create_setting_row("Minuto report email (0-59)", email_report_minute_entry));
    gtk_box_append(GTK_BOX(email_box), create_setting_row("Comando invio email", email_sendmail_command_entry));
    gtk_box_append(GTK_BOX(email_box), create_section_title("Test consegna"));
    gtk_box_append(GTK_BOX(email_box), gtk_label_new("Il test usa il comando configurato, ad esempio: sendmail -t. Se msmtp non è configurato, il test fallirà con il dettaglio dell'errore."));
    gtk_box_append(GTK_BOX(email_box), email_buttons_box);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(email_dialog_scrolled_window), email_box);
    gtk_window_set_child(GTK_WINDOW(email_expander), email_dialog_scrolled_window);

    coinbase_expander = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(coinbase_expander), "Coinbase API");
    gtk_window_set_transient_for(GTK_WINDOW(coinbase_expander), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(coinbase_expander), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(coinbase_expander), 760, 220);
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
    gtk_window_set_default_size(GTK_WINDOW(history_window), 900, 420);
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
    gtk_widget_set_vexpand(scrolled_window, FALSE);
    gtk_widget_set_size_request(scrolled_window, -1, 220);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), trade_list);

    gtk_box_append(GTK_BOX(history_box), history_title);
    gtk_box_append(GTK_BOX(history_box), scrolled_window);
    gtk_window_set_child(GTK_WINDOW(history_window), history_box);

    audit_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(audit_window), "Audit decisioni motore");
    gtk_window_set_transient_for(GTK_WINDOW(audit_window), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(audit_window), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(audit_window), 1000, 480);
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
    gtk_widget_set_vexpand(audit_scrolled_window, FALSE);
    gtk_widget_set_size_request(audit_scrolled_window, -1, 260);
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
    widgets->status_message_timeout_id = 0;
    widgets->has_temporary_status_message = FALSE;
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
    widgets->email_expander = email_expander;
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
    widgets->micro_live_enabled_entry = micro_live_enabled_entry;
    widgets->micro_live_max_order_eur_entry = micro_live_max_order_eur_entry;
    widgets->micro_live_stop_after_real_order_entry = micro_live_stop_after_real_order_entry;
    widgets->micro_live_allow_accumulation_entry = micro_live_allow_accumulation_entry;
    widgets->email_daily_enabled_entry = email_daily_enabled_entry;
    widgets->email_recipient_entry = email_recipient_entry;
    widgets->email_report_hour_entry = email_report_hour_entry;
    widgets->email_report_minute_entry = email_report_minute_entry;
    widgets->email_sendmail_command_entry = email_sendmail_command_entry;
    widgets->save_email_button = save_email_button;
    widgets->test_email_button = test_email_button;
    widgets->emergency_stop_label = emergency_stop_label;
    widgets->live_trading_arm_label = live_trading_arm_label;
    widgets->live_trading_arm_buttons_box = live_trading_arm_buttons_box;
    widgets->arm_live_trading_button = arm_live_trading_button;
    widgets->disarm_live_trading_button = disarm_live_trading_button;
    widgets->acknowledge_real_order_button = acknowledge_real_order_button;
    widgets->seed_paper_slots_button = seed_paper_slots_button;
    widgets->clear_paper_slots_button = clear_paper_slots_button;
    widgets->run_paper_best_profit_button = run_paper_best_profit_button;
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
            .name = "show-email-report",
            .activate = on_menu_show_email_report_action
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
            .name = "show-position-slots-table",
            .activate = on_menu_show_position_slots_table_action
        },
        {
            .name = "show-trades-table",
            .activate = on_menu_show_trades_table_action
        },
        {
            .name = "show-order-journal-table",
            .activate = on_menu_show_order_journal_table_action
        },
        {
            .name = "show-engine-audit-table",
            .activate = on_menu_show_engine_audit_table_action
        },
        {
            .name = "show-prelive-report",
            .activate = on_menu_show_prelive_report_action
        },
        {
            .name = "show-safety-status",
            .activate = on_menu_show_safety_status_action
        },
        {
            .name = "show-dryrun-scenario",
            .activate = on_menu_show_dryrun_scenario_action
        },
        {
            .name = "export-prelive-report",
            .activate = on_menu_export_prelive_report_action
        },
        {
            .name = "export-status-snapshot",
            .activate = on_menu_export_status_snapshot_action
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
    g_signal_connect(save_email_button, "clicked", G_CALLBACK(on_save_email_clicked), widgets);
    g_signal_connect(test_email_button, "clicked", G_CALLBACK(on_test_email_clicked), widgets);
    g_signal_connect(activate_emergency_stop_button, "clicked", G_CALLBACK(on_activate_emergency_stop_clicked), widgets);
    g_signal_connect(reset_emergency_stop_button, "clicked", G_CALLBACK(on_reset_emergency_stop_clicked), widgets);
    g_signal_connect(arm_live_trading_button, "clicked", G_CALLBACK(on_arm_live_trading_clicked), widgets);
    g_signal_connect(disarm_live_trading_button, "clicked", G_CALLBACK(on_disarm_live_trading_clicked), widgets);
    g_signal_connect(acknowledge_real_order_button, "clicked", G_CALLBACK(on_acknowledge_real_order_clicked), widgets);
    g_signal_connect(seed_paper_slots_button, "clicked", G_CALLBACK(on_seed_paper_slots_clicked), widgets);
    g_signal_connect(clear_paper_slots_button, "clicked", G_CALLBACK(on_clear_paper_slots_clicked), widgets);
    g_signal_connect(run_paper_best_profit_button, "clicked", G_CALLBACK(on_run_paper_best_profit_clicked), widgets);
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

    widgets->timer_id = g_timeout_add_seconds(10, on_engine_timer, widgets);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(page_scrolled_window), main_box);
    gtk_box_append(GTK_BOX(root_box), menu_bar);
    gtk_box_append(GTK_BOX(root_box), page_scrolled_window);
    gtk_window_set_child(GTK_WINDOW(window), root_box);
    gtk_window_present(GTK_WINDOW(window));
}
