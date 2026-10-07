# Zeus gateway v1.3.0 - OTA updates

## One-time steps (USB)
1. Optional: copy `include/secrets.h.example` to `include/secrets.h` and fill it in. Without it, the device opens the
   "ZeusSetup" Wi-Fi portal on first boot and asks for the MQTT / ntfy / Gmail details.
2. `pio run -t upload` over USB. The new partition table (two app slots) is written; Wi-Fi settings are kept.
   Do NOT run "erase flash".
3. Watch the serial monitor: it prints the flash size (expect 4194304) and the running slot (app0).

## Publishing an update
1. In your GitHub repo `kosthala/zeus_cloud`, put this project in a folder named `firmware/` (the repo must be PUBLIC,
   the device downloads without a login). Do NOT upload `include/secrets.h` (it is git-ignored).
2. Add the build file `.github/workflows/firmware.yml` (content in `repo-files/firmware.yml`).
3. Change `FW_VERSION` in `include/config.h`, commit, then create a tag such as `fw-v1.3.1` (Releases > Draft a new release >
   choose a new tag `fw-v1.3.1` > Publish). GitHub builds the firmware and attaches `firmware.bin` to that release.
4. In the Zeus app: Settings > Gateway firmware > Update firmware. The gateway downloads
   `releases/latest/download/firmware.bin`, reports progress on `zeus/update_status`, then reboots.

## Safety
- The update is refused while the panel reports an alarm, if the clock is not synced, or if memory is low.
- An `install` message that arrives within 3 s of connecting (a retained message) is ignored.
- The download is verified against 20 public root CAs. If GitHub changes its certificate root, updates fail with a TLS
  error until `include/ota_roots.h` is refreshed.
- Anyone who can publish to `zeus/update` on your broker can trigger an update (of YOUR firmware only, from your repo).
  Keep the HiveMQ credentials private.
- No rollback: a firmware that boots but cannot reach Wi-Fi needs a USB re-flash. Test on the bench first.
