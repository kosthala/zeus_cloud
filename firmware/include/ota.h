#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

void OTA_OnWiFiUp();    // call once when WiFi first connects (starts optional LAN OTA)
void OTA_Request();     // ask for a firmware update; runs from OTA_Loop(), never inside the MQTT callback
void OTA_Loop();        // call every loop(): starts the download task, reports progress, reboots when done

#endif
