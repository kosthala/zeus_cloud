#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "time_helper.h"

// Uses a POSIX TZ rule so DST (last Sun March -> last Sun October, EU rules)
// is handled by the C library. No manual DST maths needed.

void TIME_HELPER_Init()
{
    configTzTime(TZ_ATHENS, "pool.ntp.org", "time.google.com");
}

bool TIME_HELPER_IsSynced()
{
    return time(nullptr) > 100000;
}

String TIME_HELPER_Format(time_t ts)
{
    if (ts < 100000)
        return "time unknown";

    struct tm tmInfo;
    localtime_r(&ts, &tmInfo);

    char buf[50];
    strftime(buf, sizeof(buf), "%A, %b %d %Y - %H:%M", &tmInfo);

    return String(buf);
}

String TIME_HELPER_UpdateTime()
{
    return TIME_HELPER_Format(time(nullptr));
}
