#ifndef HELIX_EMAIL_DELIVERY_H
#define HELIX_EMAIL_DELIVERY_H

#include "settings.h"

int email_delivery_send_message(
    const StrategySettings *settings,
    const char *subject,
    const char *body,
    char *message,
    int message_size
);

int email_delivery_send_test(
    const StrategySettings *settings,
    char *message,
    int message_size
);

#endif
