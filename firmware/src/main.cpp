#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>
#include "config.h"
#include "settings.h"
#include "ota.h"
#include "main.h"
#include "mqtt.h"
#include "notifier.h"
#include "time_helper.h"

static WiFiManager wm;
static bool relayOn = false;
static bool pulsing = false;
static uint32_t pulseEnd = 0;
static bool timeStarted = false;
static char bootMessage[112] = "";
static bool bootPublished = false;


// ---------------------------------------------------------------------------
// Panel inputs: table-driven, debounced, with rate-limited external alerts
// ---------------------------------------------------------------------------
struct PanelInput
{
    uint8_t     pin;
    const char *topic;
    const char *msgLow;      // message when pin reads LOW
    const char *msgHigh;     // message when pin reads HIGH
    const char *statusLow;   // zeus/system_status text for LOW
    const char *statusHigh;
    const char *prioLow;     // ntfy priority for LOW
    const char *prioHigh;

    int      stable;         // debounced level
    int      candidate;      // level currently being counted
    uint8_t  count;          // consecutive samples at 'candidate'
    int      notified;       // level last announced via ntfy/email
    bool     pending;        // a change was rate-limited and still needs announcing
    uint8_t  tokens;
    uint32_t lastRefill;
};

static PanelInput inputs[] =
{
    // ARM: LOW = armed
    { (uint8_t)PIN_ARM_STATE,   TOPIC_ARM,
      "ZEUS panel - ARM event",           "ZEUS panel - DISARM event",
      "arm",      "disarm",    "default", "default" },

    // ALARM: HIGH = alarm
    { (uint8_t)PIN_ALARM_STATE, TOPIC_ALARM,
      "ZEUS panel - RESTORE ALARM event", "ZEUS panel - ALARM event",
      "restore",  "alarm",     "default", "urgent" },

    // POWER: LOW = mains OK
    { (uint8_t)PIN_POWER_STATE, TOPIC_POWER,
      "ZEUS panel - POWER ON event",      "ZEUS panel - POWER OFF event",
      "power_on", "power_off", "default", "high" },
};

static const int NUM_INPUTS = sizeof(inputs) / sizeof(inputs[0]);


static void updateLeds()
{
    digitalWrite(PIN_ARM_LED, inputs[0].stable == LOW ? HIGH : LOW);
}


static bool takeToken(PanelInput &in)
{
    uint32_t now = millis();
    uint32_t elapsed = now - in.lastRefill;

    if (elapsed >= NOTIFY_REFILL_MS)
    {
        uint32_t add = elapsed / NOTIFY_REFILL_MS;
        uint32_t t = in.tokens + add;

        in.tokens = (t > NOTIFY_BURST) ? NOTIFY_BURST : (uint8_t)t;
        in.lastRefill += add * NOTIFY_REFILL_MS;
    }

    if (in.tokens == 0)
        return false;

    in.tokens--;
    return true;
}


static void announce(PanelInput &in)
{
    const char *msg  = (in.stable == LOW) ? in.msgLow  : in.msgHigh;
    const char *prio = (in.stable == LOW) ? in.prioLow : in.prioHigh;

    NOTIFIER_Enqueue(msg, prio);

    in.notified = in.stable;
    in.pending = false;
}


static void onInputChange(PanelInput &in)
{
    const char *msg = (in.stable == LOW) ? in.msgLow : in.msgHigh;

    Serial.println(msg);

    // MQTT is immediate and never rate-limited.
    MQTT_Publish(in.topic, msg, MQTT_RETAIN_EVENTS != 0);

    updateLeds();

    if (takeToken(in))
    {
        announce(in);
    }
    else
    {
        in.pending = true;
        Serial.println("[notify] rate-limited, will announce final state later");
    }
}


static void pollInputs()
{
    static uint32_t last = 0;

    uint32_t now = millis();

    if ((now - last) < INPUT_POLL_MS)
        return;

    last = now;

    for (int i = 0; i < NUM_INPUTS; i++)
    {
        PanelInput &in = inputs[i];
        int raw = digitalRead(in.pin);

        if (raw == in.candidate)
        {
            if (in.count < INPUT_DEBOUNCE_SAMPLES)
                in.count++;
        }
        else
        {
            in.candidate = raw;
            in.count = 1;
        }

        if (in.count >= INPUT_DEBOUNCE_SAMPLES && in.candidate != in.stable)
        {
            in.stable = in.candidate;
            onInputChange(in);
        }

        // If a change was rate-limited, make sure the *final* state still gets announced.
        if (in.pending)
        {
            if (in.notified == in.stable)
                in.pending = false;
            else if (takeToken(in))
                announce(in);
        }
    }
}


static void initInputs()
{
    // No event at boot: start from the current state. The boot message reports it.
    for (int i = 0; i < NUM_INPUTS; i++)
    {
        PanelInput &in = inputs[i];

        in.stable = in.candidate = in.notified = digitalRead(in.pin);
        in.count = INPUT_DEBOUNCE_SAMPLES;
        in.pending = false;
        in.tokens = NOTIFY_BURST;
        in.lastRefill = millis();
    }

    updateLeds();
}


// ---------------------------------------------------------------------------
// Public helpers used by mqtt.cpp
// ---------------------------------------------------------------------------
static void publishWifi()
{
    if (WiFi.status() != WL_CONNECTED)
        return;
    String ssid = WiFi.SSID();
    ssid.replace("\\", "\\\\");
    ssid.replace("\"", "\\\"");
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"ssid\":\"%s\",\"rssi\":%d}", ssid.c_str(), (int)WiFi.RSSI());
    MQTT_Publish(TOPIC_WIFI, buf, true);
}

void sendStatus()
{
    for (int i = 0; i < NUM_INPUTS; i++)
        MQTT_Publish(TOPIC_SYSTEM_STATUS, inputs[i].stable == LOW ? inputs[i].statusLow : inputs[i].statusHigh);

    MQTT_Publish(TOPIC_RELAY_STATE, relayOn ? "on" : "off", true);
    MQTT_Publish(TOPIC_VERSION, FW_VERSION, true);
    publishWifi();
}


void setRelayState(bool turnOn)
{
    pulsing = false;   // any explicit on/off cancels a running pulse
    relayOn = turnOn;
    digitalWrite(PIN_RELAY, turnOn ? HIGH : LOW);

    MQTT_Publish(TOPIC_RELAY_STATE, turnOn ? "on" : "off", true);   // acknowledge
}


void pulseRelay()
{
    if (pulsing)
        return;   // ignore repeats while a pulse is running

    setRelayState(true);
    pulsing = true;
    pulseEnd = millis() + PULSE_MS;
}


static void servicePulse()
{
    if (pulsing && (int32_t)(millis() - pulseEnd) >= 0)
        setRelayState(false);   // also clears 'pulsing' and publishes relay_state
}


bool alarmActive()
{
    return inputs[1].stable == HIGH;
}


bool getRelayState()
{
    return relayOn;
}


// ---------------------------------------------------------------------------
// WiFi / time / watchdog
// ---------------------------------------------------------------------------
static void initWatchdog()
{
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    esp_task_wdt_config_t cfg = {};
    cfg.timeout_ms = WDT_TIMEOUT_S * 1000;
    cfg.idle_core_mask = 0;
    cfg.trigger_panic = true;
    esp_task_wdt_reconfigure(&cfg);
#else
    esp_task_wdt_init(WDT_TIMEOUT_S, true);   // panic -> restart
#endif
    esp_task_wdt_add(NULL);
}


static void startTimeIfNeeded()
{
    if (!timeStarted && WiFi.status() == WL_CONNECTED)
    {
        TIME_HELPER_Init();
        timeStarted = true;
        OTA_OnWiFiUp();

        Serial.println("Local IP: " + WiFi.localIP().toString());
        Serial.printf("RSSI = %d\n", WiFi.RSSI());
    }
}


static WiFiManagerParameter *pHost, *pPort, *pUser, *pPass, *pNtfy, *pMailUser, *pMailPass, *pMailTo;


// Copy what was typed in the setup portal into cfg and save it. Empty password boxes keep the stored value.
static void applyPortalParams()
{
    if (!pHost)
        return;

    Settings before = cfg;
    auto text = [](WiFiManagerParameter *p, char *dst, size_t n) { strlcpy(dst, p->getValue(), n); };
    auto secret = [](WiFiManagerParameter *p, char *dst, size_t n) { if (p->getValue()[0]) strlcpy(dst, p->getValue(), n); };

    text(pHost, cfg.mqttHost, sizeof(cfg.mqttHost));
    text(pUser, cfg.mqttUser, sizeof(cfg.mqttUser));
    text(pNtfy, cfg.ntfyTopic, sizeof(cfg.ntfyTopic));
    text(pMailUser, cfg.mailUser, sizeof(cfg.mailUser));
    text(pMailTo, cfg.mailTo, sizeof(cfg.mailTo));
    secret(pPass, cfg.mqttPass, sizeof(cfg.mqttPass));
    secret(pMailPass, cfg.mailPass, sizeof(cfg.mailPass));

    int port = atoi(pPort->getValue());
    if (port > 0 && port < 65536)
        cfg.mqttPort = (uint16_t)port;

    if (memcmp(&before, &cfg, sizeof(cfg)) != 0)
    {
        SETTINGS_Save();
        Serial.println("Settings saved");
    }
}


// Boot-time only: WiFiManager may open its config portal (blocking, max PORTAL_TIMEOUT_S).
static void bootWiFi()
{
    WiFi.mode(WIFI_STA);

    char port[8];
    snprintf(port, sizeof(port), "%u", cfg.mqttPort);

    pHost     = new WiFiManagerParameter("mq_host", "MQTT broker host", cfg.mqttHost, sizeof(cfg.mqttHost) - 1);
    pPort     = new WiFiManagerParameter("mq_port", "MQTT port (8883)", port, 5);
    pUser     = new WiFiManagerParameter("mq_user", "MQTT username", cfg.mqttUser, sizeof(cfg.mqttUser) - 1);
    pPass     = new WiFiManagerParameter("mq_pass", "MQTT password (blank = keep)", "", sizeof(cfg.mqttPass) - 1, "type=\"password\"");
    pNtfy     = new WiFiManagerParameter("ntfy", "ntfy topic", cfg.ntfyTopic, sizeof(cfg.ntfyTopic) - 1);
    pMailUser = new WiFiManagerParameter("ml_user", "Gmail address (sender)", cfg.mailUser, sizeof(cfg.mailUser) - 1);
    pMailPass = new WiFiManagerParameter("ml_pass", "Gmail app password (blank = keep)", "", sizeof(cfg.mailPass) - 1, "type=\"password\"");
    pMailTo   = new WiFiManagerParameter("ml_to", "Alert recipient email", cfg.mailTo, sizeof(cfg.mailTo) - 1);

    wm.addParameter(pHost);
    wm.addParameter(pPort);
    wm.addParameter(pUser);
    wm.addParameter(pPass);
    wm.addParameter(pNtfy);
    wm.addParameter(pMailUser);
    wm.addParameter(pMailPass);
    wm.addParameter(pMailTo);
    wm.setSaveParamsCallback(applyPortalParams);

    wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
    wm.setConnectTimeout(20);

    esp_task_wdt_delete(NULL);   // portal is long-running by design
    bool ok = wm.autoConnect(SETUP_AP_NAME, SETUP_AP_PASSWORD);
    applyPortalParams();

    if (ok && !SETTINGS_Complete())
    {
        // Connected to WiFi but the broker is not configured yet: open the portal for it.
        Serial.println("MQTT settings missing - opening setup portal");
        wm.startConfigPortal(SETUP_AP_NAME, SETUP_AP_PASSWORD);
        applyPortalParams();
        ok = (WiFi.status() == WL_CONNECTED);
    }

    esp_task_wdt_add(NULL);
    esp_task_wdt_reset();

    if (!ok)
    {
        // Keep running offline: inputs, LED and relay still work, alerts are queued.
        Serial.println("WiFi not connected - running offline, will keep retrying");
        WiFi.mode(WIFI_STA);
        WiFi.begin();
    }
}


// Runtime: never blocks, never opens the portal.
static void serviceWiFi()
{
    static uint32_t lastTry = 0;
    static uint32_t downSince = 0;
    static bool     wasDown = false;

    bool up = (WiFi.status() == WL_CONNECTED);

    digitalWrite(PIN_WIFI_LED, up ? HIGH : LOW);

    if (up)
    {
        wasDown = false;
        startTimeIfNeeded();
        return;
    }

    uint32_t now = millis();

    if (!wasDown)
    {
        wasDown = true;
        downSince = now;
        lastTry = now;
        Serial.println("WiFi lost");
    }

    if ((now - lastTry) >= WIFI_RETRY_MS)
    {
        lastTry = now;
        Serial.println("WiFi: reconnecting...");
        WiFi.reconnect();
    }

    // Long outage (e.g. router changed): reboot so the setup portal can open.
    if ((now - downSince) >= WIFI_OFFLINE_RESTART_MS)
    {
        Serial.println("WiFi down too long, restarting");
        delay(200);
        ESP.restart();
    }
}


// ---------------------------------------------------------------------------
// Misc
// ---------------------------------------------------------------------------
static const char *resetReasonText(esp_reset_reason_t r)
{
    switch (r)
    {
        case ESP_RST_UNKNOWN:   return "Unknown reset reason";
        case ESP_RST_POWERON:   return "Power-on reset";
        case ESP_RST_EXT:       return "External pin reset";
        case ESP_RST_SW:        return "Software reset";
        case ESP_RST_PANIC:     return "Exception/panic reset";
        case ESP_RST_INT_WDT:   return "Interrupt watchdog reset";
        case ESP_RST_TASK_WDT:  return "Task watchdog reset";
        case ESP_RST_WDT:       return "Other watchdog reset";
        case ESP_RST_DEEPSLEEP: return "Reset after exiting deep sleep";
        case ESP_RST_BROWNOUT:  return "Brownout reset (voltage drop)";
        case ESP_RST_SDIO:      return "Reset over SDIO";
        default:                return "Unrecognized reset reason";
    }
}


static void heartbeat()
{
    static uint32_t last = 0;
    static bool flag = false;

    if ((millis() - last) < PING_INTERVAL_MS)
        return;

    last = millis();
    flag = !flag;

    MQTT_Publish(TOPIC_PING, flag ? "on" : "off");

    static uint32_t lastWifi = 0;
    if (lastWifi == 0 || (millis() - lastWifi) >= WIFI_REPORT_MS)
    {
        lastWifi = millis();
        publishWifi();
    }
}


// Long-press (BUTTON_HOLD_MS) wipes the saved WiFi credentials.
static void handleButton()
{
    static uint32_t pressedAt = 0;

    if (digitalRead(PIN_BUTTON) != LOW)
    {
        pressedAt = 0;
        return;
    }

    if (pressedAt == 0)
    {
        pressedAt = millis() | 1;   // never 0
        return;
    }

    if ((millis() - pressedAt) >= BUTTON_HOLD_MS)
    {
        Serial.println("Button held: resetting WiFi settings");
        wm.resetSettings();
        delay(500);
        ESP.restart();
    }
}


// ---------------------------------------------------------------------------
void setup()
{
    pinMode(PIN_RELAY, OUTPUT);
    pinMode(PIN_WIFI_LED, OUTPUT);
    pinMode(PIN_ARM_LED, OUTPUT);

    pinMode(PIN_ARM_STATE, INPUT);
    pinMode(PIN_ALARM_STATE, INPUT);
    pinMode(PIN_POWER_STATE, INPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    digitalWrite(PIN_RELAY, LOW);
    digitalWrite(PIN_WIFI_LED, LOW);
    digitalWrite(PIN_ARM_LED, LOW);

    Serial.begin(115200);
    Serial.println();
    Serial.printf("Zeus gateway v%s\n", FW_VERSION);
    Serial.printf("Flash: %u bytes, running from %s\n", (unsigned)ESP.getFlashChipSize(), esp_ota_get_running_partition()->label);

    SETTINGS_Load();

    initWatchdog();
    initInputs();

    NOTIFIER_Init();

    bootWiFi();
    MQTT_Init();
    startTimeIfNeeded();

    // Give SNTP a moment so the boot message carries a timestamp (not fatal if it fails).
    uint32_t t0 = millis();

    while (timeStarted && !TIME_HELPER_IsSynced() && (millis() - t0) < TIME_WAIT_AT_BOOT_MS)
    {
        esp_task_wdt_reset();
        delay(250);
    }

    snprintf(bootMessage, sizeof(bootMessage), "ZEUS - Boot: %s | %s, %s, %s",
             resetReasonText(esp_reset_reason()),
             inputs[0].stable == LOW  ? "ARMED"      : "DISARMED",
             inputs[1].stable == HIGH ? "ALARM"      : "no alarm",
             inputs[2].stable == LOW  ? "mains OK"   : "MAINS LOST");

    Serial.println(bootMessage);

    NOTIFIER_Enqueue(bootMessage, "default");
}


void loop()
{
    esp_task_wdt_reset();

    serviceWiFi();
    MQTT_Loop();
    pollInputs();
    handleButton();
    heartbeat();
    servicePulse();
    OTA_Loop();

    // Boot/reset reason goes to MQTT once, as soon as the broker is reachable.
    if (!bootPublished && MQTT_IsConnected())
    {
        MQTT_Publish(TOPIC_RESET, bootMessage);
        bootPublished = true;
    }
}
