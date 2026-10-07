#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

// Runtime settings, stored in NVS (flash). Nothing secret is compiled into a release binary.
// On the first boot, values found in include/secrets.h (if that file exists) are copied into NVS,
// so a USB-flashed build keeps working after it is replaced by a generic OTA build.
struct Settings
{
    char     mqttHost[64];
    uint16_t mqttPort;
    char     mqttUser[48];
    char     mqttPass[64];
    char     ntfyTopic[64];
    char     mailUser[64];
    char     mailPass[40];
    char     mailTo[64];
};

extern Settings cfg;
extern const char *const SETUP_AP_NAME;
extern const char *const SETUP_AP_PASSWORD;

void SETTINGS_Load();
void SETTINGS_Save();
bool SETTINGS_Complete();     // broker host + user are known

#endif
