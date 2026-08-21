## 1. Expose the configured WiFi SSID

- [x] 1.1 In `GET /api/status` (`src/WebServerManager.cpp:161-204`), load the WiFi config and add `doc["wifi_configured_ssid"]` unconditionally, alongside the existing `wifi_connected` / `wifi_ssid` block — do not alter `wifi_ssid`
- [x] 1.2 Raise the `GET /api/status` document from `Config::JSON_DOC_LARGE` to `Config::JSON_DOC_XLARGE` (design decision 10 — the new key spends most of the remaining margin and ArduinoJson overflows silently)

## 2. Preserve the stored WiFi password

- [x] 2.1 Add `src/support/WiFiCredentials.h/.cpp` with `WiFiCredentialUpdate` and `mergeWiFiCredentials(existing, update)`, preserving `existing.password` when `update.password == nullptr` and overwriting it otherwise (including with `""`); set `configured = true` (design decision 9)
- [x] 2.2 In `POST /api/settings/wifi` (`src/WebServerManager.cpp:582-628`), build a `WiFiCredentialUpdate` from the parsed body — passing `nullptr` for `password` when `!doc.containsKey("password")` — and persist `mergeWiFiCredentials(config.loadWiFiConfig(), update)`, replacing the fresh `Config::WiFiConfig` and its `else { password[0] = '\0'; }` branch
- [x] 2.3 Keep the existing empty/missing-SSID `400` guard unchanged
- [x] 2.4 Verify `handleWiFiConfig` (`POST /api/wifi`, `src/WebServerManager.cpp:1381+`) is untouched

## 3. Prefill the Device Name section

- [x] 3.1 Introduce a module-level `savedSettings` object in `data/settings.html` holding the values loaded from `/api/status`
- [x] 3.2 Set `deviceName.value` from `device_name` (falling back to `device_id`) and record it in `savedSettings`. The three `/api/status`-backed loaders were consolidated into a single `loadSettings()` — `loadDeviceName()` and `loadHardwareSettings()` each fetched `/api/status` separately, so adding a third would have meant three requests for one payload
- [x] 3.3 Replace the `currentDeviceName` echo span in the info box with a modified indicator that renders only when `deviceName.value !== savedSettings.device_name`, naming the saved value
- [x] 3.4 In `updateDeviceName()`, reject an empty name with a validation message, skip the request when unchanged, and on success update `savedSettings` and repopulate the input instead of clearing it (`settings.html:268`)

## 4. Prefill and diff the Hardware Configuration section

- [x] 4.1 In `loadSettings()`/`populateFromSnapshot()`, set `numPixels.value` and `ledPin.value` in addition to the two selects, and record all four values in `savedSettings`
- [x] 4.2 Rewrite `updateHardwareSettings()` to build the POST body by diffing each control against `savedSettings` rather than testing `isNaN` (`settings.html:281-322`); keep the existing range validation for values that are present
- [x] 4.3 Reject an empty `numPixels` or `ledPin` with a validation message
- [x] 4.4 Report "nothing changed" and send no request when the diff is empty, replacing the "Please enter at least one value to update" path
- [x] 4.5 Build the confirmation dialog text from the diff so it names only changed fields
- [x] 4.6 Replace the `currentNumPixels` / `currentLedPin` / `currentCycleTime` / `currentGammaMode` echoes with a single modified indicator that names the saved values; keep the "device will restart" notice verbatim; keep `getGammaModeName()` for rendering the saved gamma
- [x] 4.7 Remove the post-save `value = ''` resets (`settings.html:337-338`)

## 5. Prefill and guard the WiFi section

- [x] 5.1 Set `wifiSSID.value` from `wifi_configured_ssid` in `loadSettings()` and record it in `savedSettings`; call `loadSettings()` from the page-load block
- [x] 5.2 Add an "Open network (no password)" checkbox (`wifiOpenNetwork`) to the WiFi form group, with an explanatory `small` note; disable and clear the password input while it is checked
- [x] 5.3 In `updateWiFi()`, omit `password` from the body when the field is blank and the open-network box is unchecked; send `password: ""` when it is checked; send the typed value otherwise
- [x] 5.4 Keep the empty-SSID validation, and remove the post-save `value = ''` resets (`settings.html:372-373`)
- [x] 5.5 Add a modified indicator to the WiFi info box for the SSID, keeping the existing restart warning

## 6. Leave the Touch and OTA sections alone

- [x] 6.1 Confirm `loadTouchConfig()`, `updateTouchEnabled()` and `updateTouchThreshold()` are unchanged
- [x] 6.2 Confirm the OTA section and `forceUpdateCheckbox` (default off) are unchanged

## 7. Tests

- [x] 7.1 Add `test/test_wifi_credentials/` covering `mergeWiFiCredentials`: absent password preserves the stored one, present-and-empty clears it, present-and-non-empty replaces it, SSID is always replaced, `configured` becomes true, and long SSID/password inputs are truncated without overrunning the buffers
- [x] 7.2 Confirm the native env builds the new suite — `src/support/` is already in `build_src_filter`, so no `platformio.ini` change should be needed
- [x] 7.3 Run `pio test -e native` and confirm all tests pass
- [x] 7.4 `GET /api/status` and the settings page remain manually verified only (section 8) — the native env cannot compile `WebServerManager.cpp` or run the HTML

## 8. Build and manual verification

- [x] 8.1 Run `pio run -e adafruit_qtpy_esp32s3_nopsram` — the pre-build step regenerates `src/generated/settings_gz.h` via `scripts/compress_web.py`
- [ ] 8.2 Flash and open the settings page: confirm all nine controls show current values on load
- [ ] 8.3 Change only the LED pin, confirm the dialog names only that change and the device applies it after restart
- [ ] 8.4 Edit a field and revert it, confirm the modified indicator appears and clears
- [ ] 8.5 Change the SSID with a blank password, confirm the device rejoins using the preserved password
- [ ] 8.6 Verify in AP mode (no station connection) that `wifiSSID` is still prefilled from the stored configuration
- [ ] 8.7 Verify `about.html` and `control.html` still display the connected SSID correctly
