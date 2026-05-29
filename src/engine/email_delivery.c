#include "email_delivery.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>

static int has_basic_email_shape(const char *value) {
    const char *at;
    const char *dot;

    if (value == NULL || value[0] == '\0') {
        return 0;
    }

    at = strchr(value, '@');
    if (at == NULL || at == value || at[1] == '\0') {
        return 0;
    }

    dot = strchr(at + 1, '.');
    return dot != NULL && dot[1] != '\0';
}

static void set_message(char *message, int message_size, const char *text) {
    if (message == NULL || message_size <= 0) {
        return;
    }

    snprintf(message, (size_t)message_size, "%s", text ? text : "");
}

int email_delivery_send_test(
    const StrategySettings *settings,
    char *message,
    int message_size
) {
    FILE *pipe;
    int rc;
    time_t now;
    struct tm *local_now;
    char date_buffer[64];
    const char *command;
    char command_with_stderr[512];
    char error_buffer[192];
    const char *error_path = "/tmp/helix_email_delivery_last_error.log";

    if (settings == NULL) {
        set_message(message, message_size, "Email test: settings non disponibili");
        return 0;
    }

    if (!settings->email_daily_enabled) {
        set_message(message, message_size, "Email test: invio email disabilitato nelle impostazioni");
        return 0;
    }

    if (!has_basic_email_shape(settings->email_recipient)) {
        set_message(message, message_size, "Email test: destinatario non valido o mancante");
        return 0;
    }

    command = settings->email_sendmail_command[0] != '\0' ?
        settings->email_sendmail_command :
        "sendmail -t";

    remove(error_path);

    snprintf(
        command_with_stderr,
        sizeof(command_with_stderr),
        "%s 2> %s",
        command,
        error_path
    );

    pipe = popen(command_with_stderr, "w");
    if (pipe == NULL) {
        set_message(message, message_size, "Email test: impossibile avviare comando sendmail/msmtp");
        return 0;
    }

    now = time(NULL);
    local_now = localtime(&now);
    if (local_now != NULL) {
        strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d %H:%M:%S", local_now);
    } else {
        snprintf(date_buffer, sizeof(date_buffer), "%s", "data non disponibile");
    }

    fprintf(pipe, "To: %s\n", settings->email_recipient);
    fprintf(pipe, "Subject: Helix - test consegna email\n");
    fprintf(pipe, "Content-Type: text/plain; charset=UTF-8\n");
    fprintf(pipe, "\n");
    fprintf(pipe, "Helix test email\n");
    fprintf(pipe, "================\n\n");
    fprintf(pipe, "Se stai leggendo questa email, il test di consegna funziona.\n\n");
    fprintf(pipe, "Data locale: %s\n", date_buffer);
    fprintf(pipe, "Destinatario configurato: %s\n", settings->email_recipient);
    fprintf(pipe, "Comando invio: %s\n", command);
    fprintf(pipe, "\nQuesta è solo una email di test. Nessun ordine è stato eseguito.\n");

    rc = pclose(pipe);

    if (rc == -1) {
        set_message(message, message_size, "Email test: errore chiusura comando invio");
        return 0;
    }

    if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
        snprintf(
            message,
            (size_t)message_size,
            "Email test inviata a %s",
            settings->email_recipient
        );
        return 1;
    }

    error_buffer[0] = '\0';

    {
        FILE *error_file = fopen(error_path, "r");

        if (error_file != NULL) {
            size_t bytes_read = fread(error_buffer, 1, sizeof(error_buffer) - 1, error_file);
            error_buffer[bytes_read] = '\0';
            fclose(error_file);
        }
    }

    if (error_buffer[0] != '\0') {
        snprintf(
            message,
            (size_t)message_size,
            "Email test fallita: codice %d | %.150s",
            WIFEXITED(rc) ? WEXITSTATUS(rc) : rc,
            error_buffer
        );
    } else {
        snprintf(
            message,
            (size_t)message_size,
            "Email test fallita: comando invio terminato con codice %d",
            WIFEXITED(rc) ? WEXITSTATUS(rc) : rc
        );
    }

    return 0;
}
