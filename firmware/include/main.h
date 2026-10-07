#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>

void sendStatus();
void setRelayState(bool turnOn);
bool alarmActive();             // panel currently reports an alarm
void pulseRelay();               // relay on for PULSE_MS, then off (timed here, not in the browser)
bool getRelayState();

#endif
