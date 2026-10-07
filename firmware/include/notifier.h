#ifndef NOTIFIER_H
#define NOTIFIER_H

#include <Arduino.h>

// Starts a background task that delivers ntfy + email so the main loop never blocks.
void NOTIFIER_Init();

// Queue an event (non-blocking, never fails; drops the oldest if the queue is full).
void NOTIFIER_Enqueue(const char *message, const char *priority);

#endif
