# Zeus MQTT gateway - v1.1 changes

## Setup (required)
1. Copy `include/secrets.h.example` to `include/secrets.h` and fill it in.
2. **Rotate** the old Gmail app password, HiveMQ credential and ntfy topic (they were in v1.0's source).
3. `pio run -t upload`. The build is untested on hardware - test with the panel disconnected first.

## Fixes
| v1.0 problem | v1.1 |
|---|---|
| Credentials in source | `secrets.h` (git-ignored); unused ssid/pass removed |
| NTP wait could hang forever while feeding the WDT | Non-blocking; 10 s wait at boot only; events carry their own timestamp |
| Broker down -> reboot loop, no ntfy/email | MQTT is non-blocking with backoff, never reboots the device |
| Spurious ARM/ALARM/POWER events on every boot | Flags initialised from pin state; one boot message with reset reason + panel state |
| ntfy/SMTP/MQTT sent serially in loop() | Background task + queue (16 events, retries, holds events while offline) |
| ntfy hung on keep-alive | `Connection: close`, bounded wait, TLS verification (`NTFY_VERIFY_TLS`) |
| Flapping inputs flooded push/email | 300 ms debounce + per-input token bucket (burst 5, +1/min); final state always announced |
| WiFi drop blocked loop / opened portal | Non-blocking reconnect; reboot after 15 min offline so portal can open |
| Single short press wiped WiFi | Hold button 3 s |
| Manual DST maths | POSIX TZ rule `EET-2EEST,M3.5.0/3,M10.5.0/4` |
| 1 Hz ping, no offline detection | Ping every 5 s + retained `zeus/availability` with Last Will |
| Relay changes unacknowledged | Retained `zeus/relay_state`; commands case-insensitive |
| Random MQTT client id | Stable id from chip MAC |
| Unpinned platform, duplicate WiFiManager libs, SimpleTimer | Pinned `espressif32@6.9.0`; one WiFiManager; SimpleTimer removed |

## Behaviour changes to be aware of
- `zeus/ping` interval 1 s -> 5 s (`PING_INTERVAL_MS`). Adjust any dashboard timeout.
- Env renamed `featheresp32` -> `esp32dev`.
- ntfy priority: only ALARM = urgent, POWER OFF = high, everything else default.
- Topics are unchanged; two new ones added (`zeus/availability`, `zeus/relay_state`).

## Not changed (hardware / deployment decisions for you)
- GPIO2/5/15 are boot-strapping pins; inputs have no pull resistors; confirm PGM outputs are 3.3 V safe.
- Partition table untouched (no `ota_1`, 2 MB used). Changing it would move NVS and erase saved WiFi.

## v1.2 (pulse relay)
- New MQTT command `pulse` on `zeus/zone_remote`: relay ON for `PULSE_MS` (5 s), then OFF. Timed in the firmware,
  so closing the browser mid-pulse cannot leave the relay stuck on. Repeats during a pulse are ignored.
  `on` / `off` still work and cancel a running pulse. Do not publish `pulse` with the retain flag.
- `PING_INTERVAL_MS` is 1000 again (the web app expects a heartbeat every second).

## v1.3.0 (OTA)
- OTA update over HTTPS: publish `install` (not retained) to `zeus/update`, or press the button in the app. The gateway
  downloads `OTA_URL` (GitHub release asset), writes the second app slot, reports `zeus/update_status`, reboots.
  `zeus/version` (retained) shows the installed version.
- Partition table: two 1.875 MB app slots (`app0`/`app1`). `nvs` and `otadata` keep their offsets. Needs one USB flash.
- Settings (MQTT, ntfy, Gmail) now live in NVS and are edited in the setup portal, so release binaries contain no secrets.
  `include/secrets.h` is optional and only seeds NVS on first boot.
- Boot log prints flash size and the running partition.
- Optional ArduinoOTA over the LAN (`OTA_ARDUINO_ENABLE`, off).

- v1.3.0 (cert fix): added ISRG Root YR and ISRG Root YE to the OTA trust list; GitHub download hosts now use Let's Encrypt Gen Y certificates.

- v1.3.0 (memory fix): the update waits for running alerts and pauses alerts while downloading (two TLS sessions did not fit in RAM); trust list trimmed to 9 roots.

## 2.1.0
- Gateway publishes retained zeus/wifi {"ssid","rssi"} on connect and every 30 s (shown in the app).
- PULSE_MS 5000 -> 3000 (matches the app: hold 1 s, then 3 s lock).
