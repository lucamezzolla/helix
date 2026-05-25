#include "window.h"
#include "../engine/bot_state.h"
#include "../db/database.h"
#include "../engine/engine.h"

typedef struct {
    BotState *state;
    GtkWidget *price_label;
    GtkWidget *eur_label;
    GtkWidget *btc_label;
    GtkWidget *slots_label;
    GtkWidget *mode_label;
    GtkWidget *status_label;
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

static void refresh_dashboard(AppWidgets *widgets) {
    char price_text[100];
    char eur_text[100];
    char btc_text[100];
    char slots_text[100];
    char mode_text[100];

    snprintf(price_text, sizeof(price_text), "BTC-EUR: %.2f €", widgets->state->current_price);
    snprintf(eur_text, sizeof(eur_text), "EUR disponibili: %.2f", widgets->state->eur_balance);
    snprintf(btc_text, sizeof(btc_text), "BTC detenuti: %.8f", widgets->state->btc_balance);
    snprintf(slots_text, sizeof(slots_text), "Slot usati: %d / %d", widgets->state->used_slots, widgets->state->max_slots);
    snprintf(mode_text, sizeof(mode_text), "Modalità: %s", bot_mode_to_string(widgets->state->mode));

    gtk_label_set_text(GTK_LABEL(widgets->price_label), price_text);
    gtk_label_set_text(GTK_LABEL(widgets->eur_label), eur_text);
    gtk_label_set_text(GTK_LABEL(widgets->btc_label), btc_text);
    gtk_label_set_text(GTK_LABEL(widgets->slots_label), slots_text);
    gtk_label_set_text(GTK_LABEL(widgets->mode_label), mode_text);

    refresh_status(widgets);
}

static void on_start_clicked(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = user_data;

    widgets->state->running = true;
    db_save_state(widgets->state);

    refresh_dashboard(widgets);
}

static void on_stop_clicked(GtkButton *button, gpointer user_data) {
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
    BotState *state = g_malloc(sizeof(BotState));
    *state = bot_state_default();

    db_init();

    if (!db_load_state(state)) {
        db_save_state(state);
    }

    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *title;
    GtkWidget *price_label;
    GtkWidget *eur_label;
    GtkWidget *btc_label;
    GtkWidget *slots_label;
    GtkWidget *mode_label;
    GtkWidget *status_label;
    GtkWidget *buttons_box;
    GtkWidget *start_button;
    GtkWidget *stop_button;

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
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 500);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 30);
    gtk_widget_set_margin_bottom(box, 30);
    gtk_widget_set_margin_start(box, 30);
    gtk_widget_set_margin_end(box, 30);

    title = gtk_label_new("Helix");
    gtk_widget_add_css_class(title, "title-1");

    price_label = gtk_label_new(price_text);
    eur_label = gtk_label_new(eur_text);
    btc_label = gtk_label_new(btc_text);
    slots_label = gtk_label_new(slots_text);
    mode_label = gtk_label_new("");
    status_label = gtk_label_new("");

    AppWidgets *widgets = g_malloc(sizeof(AppWidgets));
    widgets->state = state;
    widgets->price_label = price_label;
    widgets->eur_label = eur_label;
    widgets->btc_label = btc_label;
    widgets->slots_label = slots_label;
    widgets->mode_label = mode_label;
    widgets->status_label = status_label;

    refresh_dashboard(widgets);

    buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

    start_button = gtk_button_new_with_label("Start bot");
    stop_button = gtk_button_new_with_label("Stop bot");

    g_signal_connect(start_button, "clicked", G_CALLBACK(on_start_clicked), widgets);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_clicked), widgets);

    g_timeout_add_seconds(2, on_engine_timer, widgets);

    gtk_box_append(GTK_BOX(buttons_box), start_button);
    gtk_box_append(GTK_BOX(buttons_box), stop_button);

    gtk_box_append(GTK_BOX(box), title);
    gtk_box_append(GTK_BOX(box), price_label);
    gtk_box_append(GTK_BOX(box), eur_label);
    gtk_box_append(GTK_BOX(box), btc_label);
    gtk_box_append(GTK_BOX(box), slots_label);
    gtk_box_append(GTK_BOX(box), mode_label);
    gtk_box_append(GTK_BOX(box), status_label);
    gtk_box_append(GTK_BOX(box), buttons_box);

    gtk_window_set_child(GTK_WINDOW(window), box);
    gtk_window_present(GTK_WINDOW(window));
}
