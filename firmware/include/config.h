#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ---------------- Pins (unchanged from v1.0) ----------------
constexpr int PIN_RELAY       = 21;
constexpr int PIN_WIFI_LED    = 2;
constexpr int PIN_ARM_LED     = 4;
constexpr int PIN_ARM_STATE   = 17;   // LOW = armed
constexpr int PIN_ALARM_STATE = 5;    // HIGH = alarm
constexpr int PIN_POWER_STATE = 18;   // LOW = mains OK
constexpr int PIN_BUTTON      = 15;   // hold to wipe WiFi settings

// ---------------- MQTT topics (unchanged from v1.0) ----------------
#define TOPIC_REMOTE          "zeus/zone_remote"
#define TOPIC_REQUEST_STATUS  "zeus/request_status"
#define TOPIC_ARM             "zeus/arm"
#define TOPIC_ALARM           "zeus/alarm"
#define TOPIC_POWER           "zeus/power"
#define TOPIC_PING            "zeus/ping"
#define TOPIC_RESET           "zeus/reset_reason"
#define TOPIC_SYSTEM_STATUS   "zeus/system_status"
// New in v1.1
#define TOPIC_AVAILABILITY    "zeus/availability"   // retained online/offline (LWT)
#define TOPIC_RELAY_STATE     "zeus/relay_state"    // retained on/off acknowledgement

// New in v1.3 (OTA)
#define TOPIC_UPDATE          "zeus/update"          // publish "install" (NOT retained) to start an update
#define TOPIC_UPDATE_STATUS   "zeus/update_status"   // starting / progress N / success, rebooting / failed: ...
#define TOPIC_VERSION         "zeus/version"         // retained firmware version

#define FW_VERSION            "1.3.2"

// Where the gateway downloads new firmware from (HTTPS). The release asset must be named firmware.bin.
#define OTA_URL               "https://github.com/kosthala/zeus_cloud/releases/latest/download/firmware.bin"
#define OTA_MIN_FREE_HEAP     60000

// Optional LAN upload from PlatformIO (upload_protocol = espota). Off by default.
// If you turn it on, define OTA_LAN_PASSWORD in include/secrets.h.
#define OTA_ARDUINO_ENABLE    0

// ---------------- Behaviour ----------------
#define WDT_TIMEOUT_S            30
#define INPUT_POLL_MS            100      // sample inputs every 100 ms
#define INPUT_DEBOUNCE_SAMPLES   3        // must be stable for 3 samples (~300 ms)
#define PING_INTERVAL_MS         1000     // heartbeat every second (web app expects this)
#define PULSE_MS                 5000     // relay pulse length for the "pulse" command
#define WIFI_RETRY_MS            30000UL
#define WIFI_OFFLINE_RESTART_MS  (15UL * 60UL * 1000UL)
#define PORTAL_TIMEOUT_S         120
#define BUTTON_HOLD_MS           3000     // long-press to reset WiFi settings
#define TIME_WAIT_AT_BOOT_MS     10000

// Push/email rate limit per input (token bucket). MQTT is never limited.
#define NOTIFY_BURST             5
#define NOTIFY_REFILL_MS         60000UL  // one new token per minute
#define NOTIFY_QUEUE_LEN         16
#define NOTIFY_RETRIES           3

#define MQTT_RETRY_MIN_MS        5000UL
#define MQTT_RETRY_MAX_MS        60000UL
#define MQTT_RETAIN_EVENTS       0        // 1 = retain zeus/arm|alarm|power events

#define NTFY_VERIFY_TLS          1        // verify ntfy.sh certificate (needs valid time)

#define TZ_ATHENS "EET-2EEST,M3.5.0/3,M10.5.0/4"

#endif
