#ifndef TIME_HELPER_H
#define TIME_HELPER_H

#include <Arduino.h>
#include <time.h>

void   TIME_HELPER_Init();                       // start SNTP with Athens TZ rules (DST automatic)
bool   TIME_HELPER_IsSynced();
String TIME_HELPER_Format(time_t ts);            // "time unknown" if ts is invalid
String TIME_HELPER_UpdateTime();                 // formatted current time

#endif
