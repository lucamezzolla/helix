#include "window.h"
#include "../engine/bot_state.h"
#include "../engine/settings.h"
#include "../db/database.h"
#include "../engine/engine.h"
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
    GtkWidget *coinbase_credentials_label;
    GtkWidget *remote_wallet_label;
    GtkWidget *last_trade_label;
    GtkWidget *settings_label;
    GtkWidget *status_label;
    GtkWidget *trade_list;
    GtkWidget *audit_list;

    GtkWidget *slot_amount_entry;
    GtkWidget *buy_drop_entry;
    GtkWidget *sell_profit_entry;
    GtkWidget *estimated_fee_entry;
    GtkWidget *min_profit_eur_entry;
    GtkWidget *min_profit_percent_entry;
    GtkWidget *min_liquidity_entry;
    GtkWidget *max_slots_entry;
    GtkWidget *audit_retention_days_entry;
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

    char price_text[100];
    char eur_text[100];
    char btc_text[100];
    char slots_text[100];
    char mode_text[100];
    char runtime_mode_text[100];
    char last_trade_text[256];
    char settings_text[512];

    snprintf(price_text, sizeof(price_text), "BTC-EUR: %.2f €", widgets->state->current_price);
    snprintf(eur_text, sizeof(eur_text), "EUR disponibili: %.2f", widgets->state->eur_balance);
    snprintf(btc_text, sizeof(btc_text), "BTC detenuti: %.8f", widgets->state->btc_balance);
    snprintf(slots_text, sizeof(slots_text), "Slot usati: %d / %d", widgets->state->used_slots, widgets->state->max_slots);
    snprintf(mode_text, sizeof(mode_text), "Modalità motore: %s", bot_mode_to_string(widgets->state->mode));
    snprintf(runtime_mode_text, sizeof(runtime_mode_text), "Modalità operativa: %s", runtime_mode_to_string(settings.runtime_mode));
    snprintf(last_trade_text, sizeof(last_trade_text), "Ultima operazione: %s", widgets->state->last_trade);
    snprintf(
        settings_text,
        sizeof(settings_text),
        "Strategia: slot %.2f € | buy drop %.2f%% | sell %.2f%% | fee stimata %.2f%% | min profit %.2f € / %.2f%% | liquidità %.2f%% | max slot %d | audit %d giorni",
        settings.slot_amount_eur,
        settings.buy_drop_percent,
        settings.sell_profit_percent,
        settings.estimated_fee_percent,
        settings.min_profit_eur,
        settings.min_profit_percent,
        settings.min_liquidity_percent,
        settings.max_slots,
        settings.audit_retention_days
    );

    gtk_label_set_text(GTK_LABEL(widgets->price_label), price_text);
    gtk_label_set_text(GTK_LABEL(widgets->eur_label), eur_text);
    gtk_label_set_text(GTK_LABEL(widgets->btc_label), btc_text);
    gtk_label_set_text(GTK_LABEL(widgets->slots_label), slots_text);
    gtk_label_set_text(GTK_LABEL(widgets->mode_label), mode_text);
    gtk_label_set_text(GTK_LABEL(widgets->runtime_mode_label), runtime_mode_text);
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

    snprintf(buffer, sizeof(buffer), "%d", settings.max_slots);
    gtk_editable_set_text(GTK_EDITABLE(widgets->max_slots_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%d", settings.audit_retention_days);
    gtk_editable_set_text(GTK_EDITABLE(widgets->audit_retention_days_entry), buffer);

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

static void on_save_settings_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;

    StrategySettings settings;

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

    settings.max_slots =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->max_slots_entry)));

    settings.audit_retention_days =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->audit_retention_days_entry)));

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
        settings.max_slots <= 0 ||
        settings.audit_retention_days <= 0
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
    GtkWidget *main_box;
    GtkWidget *top_box;
    GtkWidget *title;
    GtkWidget *price_label;
    GtkWidget *eur_label;
    GtkWidget *btc_label;
    GtkWidget *slots_label;
    GtkWidget *mode_label;
    GtkWidget *runtime_mode_label;
    GtkWidget *coinbase_credentials_label;
    GtkWidget *remote_wallet_label;
    GtkWidget *last_trade_label;
    GtkWidget *settings_label;
    GtkWidget *status_label;
    GtkWidget *buttons_box;
    GtkWidget *start_button;
    GtkWidget *stop_button;

    GtkWidget *settings_expander;
    GtkWidget *settings_box;
    GtkWidget *slot_amount_entry;
    GtkWidget *buy_drop_entry;
    GtkWidget *sell_profit_entry;
    GtkWidget *estimated_fee_entry;
    GtkWidget *min_profit_eur_entry;
    GtkWidget *min_profit_percent_entry;
    GtkWidget *min_liquidity_entry;
    GtkWidget *max_slots_entry;
    GtkWidget *audit_retention_days_entry;
    GtkWidget *runtime_mode_dropdown;
    GtkWidget *save_settings_button;

    GtkWidget *coinbase_expander;
    GtkWidget *coinbase_box;
    GtkWidget *coinbase_api_key_entry;
    GtkWidget *coinbase_api_secret_entry;
    GtkWidget *save_coinbase_button;

    GtkWidget *history_title;
    GtkWidget *trade_list;
    GtkWidget *audit_title;
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
    gtk_window_set_default_size(GTK_WINDOW(window), 980, 900);

    main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(main_box, 30);
    gtk_widget_set_margin_bottom(main_box, 30);
    gtk_widget_set_margin_start(main_box, 30);
    gtk_widget_set_margin_end(main_box, 30);

    top_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    title = gtk_label_new("Helix");
    gtk_widget_add_css_class(title, "title-1");
    gtk_widget_set_halign(title, GTK_ALIGN_START);

    price_label = gtk_label_new(price_text);
    eur_label = gtk_label_new(eur_text);
    btc_label = gtk_label_new(btc_text);
    slots_label = gtk_label_new(slots_text);
    mode_label = gtk_label_new("");
    runtime_mode_label = gtk_label_new("");
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
    gtk_widget_set_halign(coinbase_credentials_label, GTK_ALIGN_START);
    gtk_widget_set_halign(remote_wallet_label, GTK_ALIGN_START);
    gtk_widget_set_halign(last_trade_label, GTK_ALIGN_START);
    gtk_widget_set_halign(settings_label, GTK_ALIGN_START);
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);

    buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

    start_button = gtk_button_new_with_label("Start bot");
    stop_button = gtk_button_new_with_label("Stop bot");

    gtk_box_append(GTK_BOX(buttons_box), start_button);
    gtk_box_append(GTK_BOX(buttons_box), stop_button);

    settings_expander = gtk_expander_new("Impostazioni strategia");
    gtk_expander_set_expanded(GTK_EXPANDER(settings_expander), TRUE);

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
    max_slots_entry = gtk_entry_new();
    audit_retention_days_entry = gtk_entry_new();

    const char *runtime_modes[] = {
        "SIMULATION",
        "LIVE_READONLY",
        "LIVE_TRADING",
        NULL
    };

    runtime_mode_dropdown = gtk_drop_down_new_from_strings(runtime_modes);

    save_settings_button = gtk_button_new_with_label("Salva impostazioni");

    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Slot EUR", slot_amount_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Buy drop %", buy_drop_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Sell profit lordo %", sell_profit_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Fee stimata %", estimated_fee_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Profitto minimo EUR", min_profit_eur_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Profitto minimo %", min_profit_percent_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Liquidità min %", min_liquidity_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Max slot", max_slots_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Conserva audit giorni", audit_retention_days_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Modalità operativa", runtime_mode_dropdown));
    gtk_box_append(GTK_BOX(settings_box), save_settings_button);

    gtk_expander_set_child(GTK_EXPANDER(settings_expander), settings_box);

    coinbase_expander = gtk_expander_new("Coinbase API");
    gtk_expander_set_expanded(GTK_EXPANDER(coinbase_expander), FALSE);

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

    gtk_expander_set_child(GTK_EXPANDER(coinbase_expander), coinbase_box);

    history_title = gtk_label_new("Storico operazioni - più recenti in alto");
    gtk_widget_add_css_class(history_title, "title-3");
    gtk_widget_set_halign(history_title, GTK_ALIGN_START);

    trade_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(trade_list), GTK_SELECTION_NONE);

    scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), trade_list);

    audit_title = gtk_label_new("Audit decisioni motore - più recenti in alto");
    gtk_widget_add_css_class(audit_title, "title-3");
    gtk_widget_set_halign(audit_title, GTK_ALIGN_START);

    audit_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(audit_list), GTK_SELECTION_NONE);

    audit_scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(audit_scrolled_window, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(audit_scrolled_window), audit_list);

    gtk_box_append(GTK_BOX(top_box), title);
    gtk_box_append(GTK_BOX(top_box), price_label);
    gtk_box_append(GTK_BOX(top_box), eur_label);
    gtk_box_append(GTK_BOX(top_box), btc_label);
    gtk_box_append(GTK_BOX(top_box), slots_label);
    gtk_box_append(GTK_BOX(top_box), mode_label);
    gtk_box_append(GTK_BOX(top_box), runtime_mode_label);
    gtk_box_append(GTK_BOX(top_box), coinbase_credentials_label);
    gtk_box_append(GTK_BOX(top_box), remote_wallet_label);
    gtk_box_append(GTK_BOX(top_box), last_trade_label);
    gtk_box_append(GTK_BOX(top_box), settings_label);
    gtk_box_append(GTK_BOX(top_box), status_label);
    gtk_box_append(GTK_BOX(top_box), buttons_box);

    gtk_box_append(GTK_BOX(main_box), top_box);
    gtk_box_append(GTK_BOX(main_box), settings_expander);
    gtk_box_append(GTK_BOX(main_box), coinbase_expander);
    gtk_box_append(GTK_BOX(main_box), history_title);
    gtk_box_append(GTK_BOX(main_box), scrolled_window);
    gtk_box_append(GTK_BOX(main_box), audit_title);
    gtk_box_append(GTK_BOX(main_box), audit_scrolled_window);

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
    widgets->coinbase_credentials_label = coinbase_credentials_label;
    widgets->remote_wallet_label = remote_wallet_label;
    widgets->last_trade_label = last_trade_label;
    widgets->settings_label = settings_label;
    widgets->status_label = status_label;
    widgets->trade_list = trade_list;
    widgets->audit_list = audit_list;
    widgets->slot_amount_entry = slot_amount_entry;
    widgets->buy_drop_entry = buy_drop_entry;
    widgets->sell_profit_entry = sell_profit_entry;
    widgets->estimated_fee_entry = estimated_fee_entry;
    widgets->min_profit_eur_entry = min_profit_eur_entry;
    widgets->min_profit_percent_entry = min_profit_percent_entry;
    widgets->min_liquidity_entry = min_liquidity_entry;
    widgets->max_slots_entry = max_slots_entry;
    widgets->audit_retention_days_entry = audit_retention_days_entry;
    widgets->runtime_mode_dropdown = runtime_mode_dropdown;
    widgets->coinbase_api_key_entry = coinbase_api_key_entry;
    widgets->coinbase_api_secret_entry = coinbase_api_secret_entry;

    fill_settings_entries(widgets);
    fill_coinbase_entries(widgets);
    refresh_dashboard(widgets);

    g_signal_connect(start_button, "clicked", G_CALLBACK(on_start_clicked), widgets);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_clicked), widgets);
    g_signal_connect(save_settings_button, "clicked", G_CALLBACK(on_save_settings_clicked), widgets);
    g_signal_connect(save_coinbase_button, "clicked", G_CALLBACK(on_save_coinbase_clicked), widgets);
    g_signal_connect(window, "close-request", G_CALLBACK(on_window_close_request), widgets);

    g_object_set_data_full(
        G_OBJECT(window),
        "helix-app-widgets",
        widgets,
        free_app_widgets
    );

    widgets->timer_id = g_timeout_add_seconds(2, on_engine_timer, widgets);

    gtk_window_set_child(GTK_WINDOW(window), main_box);
    gtk_window_present(GTK_WINDOW(window));
}
