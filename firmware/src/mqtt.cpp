#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "settings.h"
#include "ota.h"
#include "certs.h"
#include "mqtt.h"
#include "main.h"

static WiFiClientSecure wifiClient;
static PubSubClient mqttClient(wifiClient);

static char     clientId[32];
static uint32_t nextAttemptMs = 0;
static uint32_t backoffMs = MQTT_RETRY_MIN_MS;
static bool     wasConnected = false;
static uint32_t connectedAt = 0;


static void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    char buf[24];

    if (length >= sizeof(buf))
    {
        Serial.println("[MQTT] Payload too long, ignored");
        return;
    }

    memcpy(buf, payload, length);
    buf[length] = '\0';

    String message(buf);
    message.trim();
    message.toLowerCase();

    Serial.printf("[MQTT] %s -> %s\n", topic, message.c_str());

    if (strcmp(topic, TOPIC_REMOTE) == 0)
    {
        if (message == "on")
            setRelayState(true);
        else if (message == "off")
            setRelayState(false);
        else if (message == "pulse")
            pulseRelay();
    }
    else if (strcmp(topic, TOPIC_REQUEST_STATUS) == 0)
    {
        if (message == "status")
            sendStatus();
    }
    else if (strcmp(topic, TOPIC_UPDATE) == 0)
    {
        // Anything delivered right after (re)connecting may be a retained message. Ignoring it
        // prevents an update/reboot loop if someone publishes "install" with the retain flag.
        if (message == "install" && (millis() - connectedAt) > 3000)
            OTA_Request();
    }
}


void MQTT_Init()
{
    wifiClient.setCACert(ISRG_ROOT_X1);
    wifiClient.setHandshakeTimeout(10);       // seconds; default is far longer than our watchdog

    // Stable per-device client id (random ids leave ghost sessions on the broker).
    snprintf(clientId, sizeof(clientId), "ZeusClient-%012llX", (unsigned long long)ESP.getEfuseMac());

    mqttClient.setServer(cfg.mqttHost, cfg.mqttPort);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(30);
    mqttClient.setSocketTimeout(10);
}


bool MQTT_IsConnected()
{
    return mqttClient.connected();
}


bool MQTT_Publish(const char *topic, const char *message, bool retain)
{
    if (!mqttClient.connected())
        return false;

    return mqttClient.publish(topic, message, retain);
}


// Non-blocking: one connection attempt at a time, with exponential backoff.
// Never restarts the ESP - ntfy and email must keep working when the broker is down.
void MQTT_Loop()
{
    if (WiFi.status() != WL_CONNECTED || cfg.mqttHost[0] == 0)
        return;

    if (mqttClient.connected())
    {
        mqttClient.loop();
        return;
    }

    if (wasConnected)
    {
        wasConnected = false;
        Serial.println("[MQTT] Connection lost");
    }

    uint32_t now = millis();

    if ((int32_t)(now - nextAttemptMs) < 0)
        return;

    esp_task_wdt_reset();

    Serial.println("[MQTT] Connecting (TLS)...");

    // Last Will: broker publishes "offline" (retained) if we vanish.
    if (mqttClient.connect(clientId, cfg.mqttUser[0] ? cfg.mqttUser : NULL, cfg.mqttUser[0] ? cfg.mqttPass : NULL, TOPIC_AVAILABILITY, 0, true, "offline"))
    {
        Serial.println("[MQTT] Connected");

        backoffMs = MQTT_RETRY_MIN_MS;
        wasConnected = true;

        mqttClient.subscribe(TOPIC_REMOTE);
        mqttClient.subscribe(TOPIC_REQUEST_STATUS);
        mqttClient.subscribe(TOPIC_UPDATE);
        connectedAt = millis();
        mqttClient.publish(TOPIC_AVAILABILITY, "online", true);

        sendStatus();   // fresh state for subscribers after every (re)connect
    }
    else
    {
        Serial.printf("[MQTT] Failed (rc=%d), retry in %lu s\n", mqttClient.state(), backoffMs / 1000UL);

        nextAttemptMs = now + backoffMs;
        backoffMs = (backoffMs * 2 > MQTT_RETRY_MAX_MS) ? MQTT_RETRY_MAX_MS : backoffMs * 2;
    }
}
