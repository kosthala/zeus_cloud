#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"
#include "ota.h"
#include "ota_roots.h"
#include "mqtt.h"
#include "main.h"
#include "notifier.h"
#include "time_helper.h"

#if OTA_ARDUINO_ENABLE
#include <ArduinoOTA.h>
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef OTA_LAN_PASSWORD
#error "Define OTA_LAN_PASSWORD in include/secrets.h when OTA_ARDUINO_ENABLE is 1"
#endif
static bool lanStarted = false;
#endif

// The download runs in its own task (TLS needs a big stack). The task only writes these
// variables; MQTT is published from OTA_Loop() on the main loop so PubSubClient is never
// used from two tasks.
static volatile bool    requested = false;
static volatile bool    finished  = false;
static volatile bool    success   = false;
static volatile int     progress  = -1;
static char             finalMsg[96] = "";
static int              reported  = -1;
static uint32_t         waitSince = 0;
static uint32_t         rebootAt  = 0;
static uint32_t         rebootDeadline = 0;

enum OtaState { OTA_IDLE, OTA_WAIT_ALERTS, OTA_DOWNLOAD, OTA_REBOOT };
static OtaState state = OTA_IDLE;


static void status(const char *msg)
{
    Serial.printf("[OTA] %s\n", msg);
    MQTT_Publish(TOPIC_UPDATE_STATUS, msg);
}


static void onProgress(int current, int total)
{
    if (total > 0)
        progress = (int)((int64_t)current * 100 / total);
}


static void otaTask(void *)
{
    WiFiClientSecure client;
    client.setCACert(OTA_ROOTS);
    client.setHandshakeTimeout(20);

    httpUpdate.rebootOnUpdate(false);                      // we report success first, then restart
    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);   // GitHub redirects to another host
    httpUpdate.onProgress(onProgress);

    t_httpUpdate_return r = httpUpdate.update(client, OTA_URL, FW_VERSION);

    if (r == HTTP_UPDATE_OK)
    {
        success = true;
        strlcpy(finalMsg, "success, rebooting", sizeof(finalMsg));
    }
    else
    {
        success = false;

        if (r == HTTP_UPDATE_NO_UPDATES)
            strlcpy(finalMsg, "failed: server has no update", sizeof(finalMsg));
        else
            snprintf(finalMsg, sizeof(finalMsg), "failed: %.80s", httpUpdate.getLastErrorString().c_str());
    }

    finished = true;
    vTaskDelete(nullptr);
}


void OTA_Request()
{
    requested = true;
}


void OTA_OnWiFiUp()
{
#if OTA_ARDUINO_ENABLE
    if (lanStarted)
        return;

    ArduinoOTA.setHostname("zeus-gateway");
    ArduinoOTA.setPassword(OTA_LAN_PASSWORD);
    ArduinoOTA.onProgress([](unsigned int, unsigned int) { esp_task_wdt_reset(); });
    ArduinoOTA.begin();
    lanStarted = true;
#endif
}


void OTA_Loop()
{
#if OTA_ARDUINO_ENABLE
    if (lanStarted)
        ArduinoOTA.handle();
#endif

    switch (state)
    {
    case OTA_IDLE:
        if (!requested)
            return;

        requested = false;

        if (WiFi.status() != WL_CONNECTED)       { status("failed: no WiFi");               return; }
        if (alarmActive())                       { status("refused: alarm is active");      return; }
        if (!TIME_HELPER_IsSynced())             { status("failed: clock not synced yet");  return; }

        status("starting");

        // Two TLS sessions at once do not fit in RAM: stop alerts and let the one in progress finish.
        NOTIFIER_Pause(true);
        waitSince = millis();
        state = OTA_WAIT_ALERTS;
        return;

    case OTA_WAIT_ALERTS:
        requested = false;

        if (!NOTIFIER_Idle() && (millis() - waitSince) < 90000UL)
            return;

        Serial.printf("[OTA] free heap %u, largest block %u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

        if (ESP.getFreeHeap() < OTA_MIN_FREE_HEAP)
        {
            status("failed: not enough memory");
            NOTIFIER_Pause(false);
            state = OTA_IDLE;
            return;
        }

        progress = -1;
        reported = -1;
        finished = false;
        xTaskCreatePinnedToCore(otaTask, "ota", 16384, nullptr, 1, nullptr, 0);
        state = OTA_DOWNLOAD;
        return;

    case OTA_DOWNLOAD:
    {
        requested = false;   // a request that arrives while one is running is dropped

        int p = progress;

        if (p >= 0 && p / 10 != reported / 10 && p != reported)
        {
            char msg[24];
            snprintf(msg, sizeof(msg), "progress %d", (p / 10) * 10);
            status(msg);
            reported = p;
        }

        if (!finished)
            return;

        NOTIFIER_Pause(false);

        if (success)
        {
            status("progress 100");
            status(finalMsg);
            NOTIFIER_Enqueue("ZEUS - firmware updated, rebooting", "default");
            rebootAt = millis() + 2000;
            rebootDeadline = millis() + 40000UL;   // let the alert go out first, but never wait forever
            state = OTA_REBOOT;
        }
        else
        {
            status(finalMsg);
            NOTIFIER_Enqueue("ZEUS - firmware update FAILED", "high");
            state = OTA_IDLE;
        }
        return;
    }

    case OTA_REBOOT:
        MQTT_Loop();

        if ((int32_t)(millis() - rebootAt) >= 0 && (NOTIFIER_Idle() || (int32_t)(millis() - rebootDeadline) >= 0))
            ESP.restart();
        return;
    }
}
