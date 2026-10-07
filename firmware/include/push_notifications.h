#ifndef PUSH_NOTIFICATIONS_H
#define PUSH_NOTIFICATIONS_H

#include <Arduino.h>

// Plain text notification to ntfy.sh
bool ntfy_send(const char *topic, const char *message);

// With title and priority ("min", "low", "default", "high", "urgent")
bool ntfy_send_advanced(const char *topic, const char *title, const char *message, const char *priority);

#endif
