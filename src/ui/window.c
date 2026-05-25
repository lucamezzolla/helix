#include "window.h"
#include "../engine/bot_state.h"
#include "../engine/settings.h"
#include "../db/database.h"
#include "../engine/engine.h"

#include <stdio.h>
#include <stdlib.h>

#define TRADE_HISTORY_LIMIT 8

typedef struct {
    BotState *state;
    GtkWidget *price_label;
    GtkWidget *eur_label;
    GtkWidget *btc_label;
    GtkWidget *slots_label;
    GtkWidget *mode_label;
    GtkWidget *runtime_mode_label;
    GtkWidget *last_trade_label;
    GtkWidget *settings_label;
    GtkWidget *status_label;
    GtkWidget *trade_list;

    GtkWidget *slot_amount_entry;
    GtkWidget *buy_drop_entry;
    GtkWidget *sell_profit_entry;
    GtkWidget *min_liquidity_entry;
    GtkWidget *max_slots_entry;
    GtkWidget *runtime_mode_dropdown;
} AppWidgets;

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

static void refresh_status(AppWidgets *widgets) {
    gtk_label_set_text(
        GTK_LABEL(widgets->status_label),
        widgets->state->running ? "Stato: bot avviato" : "Stato: bot fermo"
    );
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

        /*
         * db_get_recent_trades legge ORDER BY id DESC.
         * append mantiene quindi il più recente in alto.
         */
        gtk_list_box_append(GTK_LIST_BOX(widgets->trade_list), row_label);
    }
}

static void refresh_dashboard(AppWidgets *widgets) {
    StrategySettings settings = settings_load();

    char price_text[100];
    char eur_text[100];
    char btc_text[100];
    char slots_text[100];
    char mode_text[100];
    char runtime_mode_text[100];
    char last_trade_text[256];
    char settings_text[320];

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
        "Strategia: slot %.2f € | buy drop %.2f%% | sell profit %.2f%% | liquidità min %.2f%% | max slot %d",
        settings.slot_amount_eur,
        settings.buy_drop_percent,
        settings.sell_profit_percent,
        settings.min_liquidity_percent,
        settings.max_slots
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
    refresh_trade_history(widgets);
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

    snprintf(buffer, sizeof(buffer), "%.2f", settings.min_liquidity_percent);
    gtk_editable_set_text(GTK_EDITABLE(widgets->min_liquidity_entry), buffer);

    snprintf(buffer, sizeof(buffer), "%d", settings.max_slots);
    gtk_editable_set_text(GTK_EDITABLE(widgets->max_slots_entry), buffer);

    gtk_drop_down_set_selected(
        GTK_DROP_DOWN(widgets->runtime_mode_dropdown),
        (guint)settings.runtime_mode
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

static void on_save_settings_clicked(GtkButton *button, gpointer user_data) {
    (void)button;

    AppWidgets *widgets = user_data;

    StrategySettings settings;

    settings.slot_amount_eur =
        atof(gtk_editable_get_text(GTK_EDITABLE(widgets->slot_amount_entry)));

    settings.buy_drop_percent =
        atof(gtk_editable_get_text(GTK_EDITABLE(widgets->buy_drop_entry)));

    settings.sell_profit_percent =
        atof(gtk_editable_get_text(GTK_EDITABLE(widgets->sell_profit_entry)));

    settings.min_liquidity_percent =
        atof(gtk_editable_get_text(GTK_EDITABLE(widgets->min_liquidity_entry)));

    settings.max_slots =
        atoi(gtk_editable_get_text(GTK_EDITABLE(widgets->max_slots_entry)));

    settings.runtime_mode =
        get_selected_runtime_mode(widgets->runtime_mode_dropdown);

    if (
        settings.slot_amount_eur <= 0.0 ||
        settings.buy_drop_percent <= 0.0 ||
        settings.sell_profit_percent <= 0.0 ||
        settings.min_liquidity_percent < 0.0 ||
        settings.min_liquidity_percent >= 100.0 ||
        settings.max_slots <= 0
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
    GtkWidget *last_trade_label;
    GtkWidget *settings_label;
    GtkWidget *status_label;
    GtkWidget *buttons_box;
    GtkWidget *start_button;
    GtkWidget *stop_button;
    GtkWidget *settings_frame;
    GtkWidget *settings_box;
    GtkWidget *settings_title;
    GtkWidget *slot_amount_entry;
    GtkWidget *buy_drop_entry;
    GtkWidget *sell_profit_entry;
    GtkWidget *min_liquidity_entry;
    GtkWidget *max_slots_entry;
    GtkWidget *runtime_mode_dropdown;
    GtkWidget *save_settings_button;
    GtkWidget *history_title;
    GtkWidget *trade_list;
    GtkWidget *scrolled_window;

    char price_text[100];
    char eur_text[100];
    char btc_text[100];
    char slots_text[100];

    snprintf(price_text, sizeof(price_text), "BTC-EUR: %.2f €", state->current_price);
    snprintf(eur_text, sizeof(eur_text), "EUR disponibili: %.2f", state->eur_balance);
    snprintf(btc_text, sizeof(btc_text), "BTC detenuti: %.8f", state->btc_balance);
    snprintf(slots_text, sizeof(slots_text), "Slot usati: %d / %d", state->used_slots, state->max_slots);

    window = gtk_application_window_new(app);

    gtk_window_set_title(GTK_WINDOW(window), "Helix");
    gtk_window_set_default_size(GTK_WINDOW(window), 980, 860);

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
    last_trade_label = gtk_label_new("");
    settings_label = gtk_label_new("");
    status_label = gtk_label_new("");

    gtk_widget_set_halign(price_label, GTK_ALIGN_START);
    gtk_widget_set_halign(eur_label, GTK_ALIGN_START);
    gtk_widget_set_halign(btc_label, GTK_ALIGN_START);
    gtk_widget_set_halign(slots_label, GTK_ALIGN_START);
    gtk_widget_set_halign(mode_label, GTK_ALIGN_START);
    gtk_widget_set_halign(runtime_mode_label, GTK_ALIGN_START);
    gtk_widget_set_halign(last_trade_label, GTK_ALIGN_START);
    gtk_widget_set_halign(settings_label, GTK_ALIGN_START);
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);

    buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

    start_button = gtk_button_new_with_label("Start bot");
    stop_button = gtk_button_new_with_label("Stop bot");

    gtk_box_append(GTK_BOX(buttons_box), start_button);
    gtk_box_append(GTK_BOX(buttons_box), stop_button);

    settings_frame = gtk_frame_new(NULL);
    settings_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(settings_box, 10);
    gtk_widget_set_margin_bottom(settings_box, 10);
    gtk_widget_set_margin_start(settings_box, 10);
    gtk_widget_set_margin_end(settings_box, 10);

    settings_title = gtk_label_new("Impostazioni strategia");
    gtk_widget_add_css_class(settings_title, "title-3");
    gtk_widget_set_halign(settings_title, GTK_ALIGN_START);

    slot_amount_entry = gtk_entry_new();
    buy_drop_entry = gtk_entry_new();
    sell_profit_entry = gtk_entry_new();
    min_liquidity_entry = gtk_entry_new();
    max_slots_entry = gtk_entry_new();

    const char *runtime_modes[] = {
        "SIMULATION",
        "LIVE_READONLY",
        "LIVE_TRADING",
        NULL
    };

    runtime_mode_dropdown = gtk_drop_down_new_from_strings(runtime_modes);

    save_settings_button = gtk_button_new_with_label("Salva impostazioni");

    gtk_box_append(GTK_BOX(settings_box), settings_title);
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Slot EUR", slot_amount_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Buy drop %", buy_drop_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Sell profit %", sell_profit_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Liquidità min %", min_liquidity_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Max slot", max_slots_entry));
    gtk_box_append(GTK_BOX(settings_box), create_setting_row("Modalità operativa", runtime_mode_dropdown));
    gtk_box_append(GTK_BOX(settings_box), save_settings_button);

    gtk_frame_set_child(GTK_FRAME(settings_frame), settings_box);

    history_title = gtk_label_new("Storico operazioni - più recenti in alto");
    gtk_widget_add_css_class(history_title, "title-3");
    gtk_widget_set_halign(history_title, GTK_ALIGN_START);

    trade_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(trade_list), GTK_SELECTION_NONE);

    scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), trade_list);

    gtk_box_append(GTK_BOX(top_box), title);
    gtk_box_append(GTK_BOX(top_box), price_label);
    gtk_box_append(GTK_BOX(top_box), eur_label);
    gtk_box_append(GTK_BOX(top_box), btc_label);
    gtk_box_append(GTK_BOX(top_box), slots_label);
    gtk_box_append(GTK_BOX(top_box), mode_label);
    gtk_box_append(GTK_BOX(top_box), runtime_mode_label);
    gtk_box_append(GTK_BOX(top_box), last_trade_label);
    gtk_box_append(GTK_BOX(top_box), settings_label);
    gtk_box_append(GTK_BOX(top_box), status_label);
    gtk_box_append(GTK_BOX(top_box), buttons_box);

    gtk_box_append(GTK_BOX(main_box), top_box);
    gtk_box_append(GTK_BOX(main_box), settings_frame);
    gtk_box_append(GTK_BOX(main_box), history_title);
    gtk_box_append(GTK_BOX(main_box), scrolled_window);

    AppWidgets *widgets = g_malloc(sizeof(AppWidgets));
    widgets->state = state;
    widgets->price_label = price_label;
    widgets->eur_label = eur_label;
    widgets->btc_label = btc_label;
    widgets->slots_label = slots_label;
    widgets->mode_label = mode_label;
    widgets->runtime_mode_label = runtime_mode_label;
    widgets->last_trade_label = last_trade_label;
    widgets->settings_label = settings_label;
    widgets->status_label = status_label;
    widgets->trade_list = trade_list;
    widgets->slot_amount_entry = slot_amount_entry;
    widgets->buy_drop_entry = buy_drop_entry;
    widgets->sell_profit_entry = sell_profit_entry;
    widgets->min_liquidity_entry = min_liquidity_entry;
    widgets->max_slots_entry = max_slots_entry;
    widgets->runtime_mode_dropdown = runtime_mode_dropdown;

    fill_settings_entries(widgets);
    refresh_dashboard(widgets);

    g_signal_connect(start_button, "clicked", G_CALLBACK(on_start_clicked), widgets);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_clicked), widgets);
    g_signal_connect(save_settings_button, "clicked", G_CALLBACK(on_save_settings_clicked), widgets);

    g_timeout_add_seconds(2, on_engine_timer, widgets);

    gtk_window_set_child(GTK_WINDOW(window), main_box);
    gtk_window_present(GTK_WINDOW(window));
}
