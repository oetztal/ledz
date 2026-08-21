## Context

`data/settings.html` is a single 686-line page with five sections (Device Name, Hardware Configuration, Touch Control, Firmware Updates, WiFi Configuration) plus Factory Reset. It has no framework and no shared state: each section has its own `loadXxx()` function that fetches an endpoint and writes into DOM nodes, and its own `updateXxx()` function that reads DOM nodes and POSTs.

Two population styles coexist, and the split is by input type rather than by intent:

```
  ┌──────────────────┬─────────┬──────────────────────────────────────┐
  │ input            │ filled? │ source                               │
  ├──────────────────┼─────────┼──────────────────────────────────────┤
  │ deviceName  text │    ✗    │ status.device_name      (available)  │
  │ numPixels    num │    ✗    │ status.num_pixels       (available)  │
  │ ledPin       num │    ✗    │ status.led_pin          (available)  │
  │ gammaMode    sel │    ✓    │ status.gamma_mode                    │
  │ cycleTime    sel │    ✓    │ status.cycle_time                    │
  │ touchEnabled chk │    ✓    │ /api/touch .enabled                  │
  │ touchThresh  num │    ✓    │ /api/touch .threshold                │
  │ wifiSSID    text │    ✗    │ status.wifi_ssid — STA-connected only │
  │ wifiPassword pwd │    ✗    │ not exposed, and must not be         │
  └──────────────────┴─────────┴──────────────────────────────────────┘
```

The `<select>` elements are prefilled because a select has no empty state to fall back to — the pattern arrived by necessity, not decision. The text and number inputs grew read-only `info-box` echoes instead (`settings.html:30-32`, `:75-80`).

The Touch section is already fully correct (`loadTouchConfig()`, `settings.html:615-632`) and is the model the rest of the page should follow.

## Goals / Non-Goals

**Goals:**
- Every editable control on the settings page shows the device's current value when the page loads
- The user can still tell, after editing, what the saved value was
- Submitting sends only what changed, and the confirmation dialog says what changed
- Prefilling the WiFi SSID does not make it easy to destroy the stored password

**Non-Goals:**
- Restyling the settings page, or splitting it into read/edit views
- Touching the AP-mode first-run setup form (`data/config.html` → `POST /api/wifi`) — it correctly starts empty
- Live/optimistic saving, or removing the explicit Update buttons
- A generic form-binding helper or refactor of the page's per-section structure — this change follows the existing hand-written pattern
- Exposing the stored WiFi password through any endpoint, in any form, including masked or length-hinted
- Prefilling the OTA section (it has no editable settings inputs; `forceUpdateCheckbox` is a per-action flag, correctly defaulting to off)

## Decisions

### 1. Prefill every editable input, including free-text

**Decision:** `deviceName`, `numPixels`, `ledPin` and `wifiSSID` get `.value` set from the loaded state, exactly as `touchThreshold` already does.

**Why:** It is what the user asked for, and it is the behavior four of the nine controls already have. The remaining `placeholder` attributes stay as-is — they only surface if a value is genuinely missing, which is the correct fallback.

**Consequence:** the post-save `element.value = ''` resets (`settings.html:268`, `:337-338`, `:372-373`) become wrong and are replaced by re-reading the snapshot. For the hardware and WiFi sections the device restarts anyway, so the reset is cosmetic; for `deviceName` it is not, and the field must be repopulated with the newly saved name.

### 2. Diff against a loaded snapshot, not against emptiness

**Decision:** `loadHardwareSettings()` (and the device-name / WiFi loaders) store what they fetched into a module-level `savedSettings` object. Submit handlers build the POST body by comparing each control's current value against `savedSettings`, and include only differing keys.

**Why:** Prefill removes the signal the current code depends on. `updateHardwareSettings()` (`settings.html:281-322`) uses `isNaN(parseInt(field.value))` as "user did not want to change this"; with prefill, every `parseInt` succeeds and every field would be sent on every save. Nothing breaks on the device — the handler applies each key independently (`WebServerManager.cpp:687-743`) — but the confirmation dialog would read *"Update hardware settings to 300 LEDs and pin 35 and 10ms cycle time and Default gamma?"* on a save where only the pin changed, and the `changes.length === 0` guard would become unreachable.

The snapshot is also what powers the modified indicator (decision 3), so it pays for itself twice.

```
  load ──▶ savedSettings = {num_pixels:300, led_pin:35, …}
             │                         │
             ▼                         ▼
        input.value              diff on submit ──▶ POST {led_pin:39}
                                       │
                                       └──▶ modified indicator
```

### 3. Info-boxes become modified indicators, not deleted

**Decision:** Remove the value-echo spans (`currentDeviceName`, `currentNumPixels`, `currentLedPin`, `currentCycleTime`, `currentGammaMode`) and render, in the same `info-box`, a line that appears only when the section is dirty: *"Modified — saved: 300 LEDs on pin 35, cycle 10 ms, default gamma."* The static warnings in those boxes ("The device will restart after updating these settings.") are kept verbatim.

**Why:** Once the input shows the current value, the echo says the same thing twice — but deleting it outright loses something real: after you type `240` over `300`, nothing on the page still knows what `300` was. The dirty-state form is the only version of that box that earns its space, and the snapshot from decision 2 makes it nearly free.

**Alternatives considered:**
- *Delete the boxes.* Simplest, but you cannot see the stored value once you have edited past it, and there is no cue that unsaved edits exist.
- *Keep the echoes verbatim.* Zero work, permanently redundant, and still gives no dirty cue.

**Note:** `getGammaModeName()` (`settings.html:605-612`) survives — it is needed to render the saved gamma in the indicator text.

### 4. `/api/status` gains `wifi_configured_ssid` as a new key

**Decision:** Add `doc["wifi_configured_ssid"] = config.loadWiFiConfig().ssid;` unconditionally in the `GET /api/status` handler. Leave `wifi_ssid` exactly as it is.

**Why:** `wifi_ssid` is `WiFi.SSID()` behind an `if (WiFiClass::status() == WL_CONNECTED)` guard (`WebServerManager.cpp:195-199`). Two things make it wrong as a prefill source: it is the network the device is *attached to*, which is not necessarily the network it is *configured for* (a fallback, a rejoin to a different AP with saved credentials); and in AP mode it is absent entirely, which is precisely the situation where a user is most likely to be fixing their WiFi settings.

Adding a key rather than changing `wifi_ssid`'s meaning keeps `about.html:97-100` ("SSID" under connection info) and `control.html:757-759` ("WiFi: …") correct — both genuinely want the connected network.

### 5. Blank password means "keep the stored one"

**Decision:** The WiFi form omits the `password` key from the POST body when the field is blank. `POST /api/settings/wifi` loads the existing `Config::WiFiConfig` and only overwrites `password` when the key is present in the request.

**Why:** This is the hazard prefill introduces, and it is the only one that can lock a user out of the device. Today the form is empty, so pressing "Update WiFi" without typing is obviously a no-op the user would not attempt. With the SSID prefilled the form looks complete and correct at a glance:

```
  ┌──────────────────────────────────────────────┐
  │ SSID      [ HomeNet         ]  ← prefilled   │
  │ Password  [                 ]  ← can't be    │
  │            [ Update WiFi ]                   │
  └──────────────────────────────────────────────┘
              │
              ▼  POST {ssid:"HomeNet", password:""} → restart → cannot join
```

The current handler constructs a fresh `Config::WiFiConfig` and, when `password` is null, writes `wifiConfig.password[0] = '\0'` (`WebServerManager.cpp:604-614`) — so the wipe happens on both the empty-string and the absent-key path. Loading the existing config first fixes both.

**API behavior change:** a client POSTing `{"ssid":"x"}` alone previously cleared the password and will now preserve it. This is an intentional, non-backward-compatible change to that endpoint, judged safe because the prior behavior is not a plausible intent. The AP-setup endpoint `POST /api/wifi` (`handleWiFiConfig`, `WebServerManager.cpp:1381+`) keeps the old semantics — it configures a device that has no stored credentials, so there is nothing to preserve.

### 6. An explicit checkbox for clearing the password

**Decision:** Add `<input type="checkbox" id="wifiOpenNetwork">` labelled "Open network (no password)" to the WiFi section. When checked, the form sends `password: ""` explicitly and disables the password field.

**Why:** Decision 5 makes "blank" unable to express "no password", which would otherwise make open networks unconfigurable from the settings page — a silent regression. One checkbox restores it and makes clearing a stated intent rather than an omission.

### 7. Blank required field is an error

**Decision:** With prefill in place, an empty `numPixels`, `ledPin`, `deviceName` or `wifiSSID` on submit is rejected with a validation message. Only `wifiPassword` retains a meaningful blank.

**Why:** "Blank means unchanged" was a workaround for fields that started blank. Once a field starts populated, the user emptying it is a mistake or an abandoned edit, not an instruction. Silently ignoring it would mean the confirmation dialog and the outcome disagree.

### 8. Touch section unchanged

**Decision:** No changes to `loadTouchConfig()`, `updateTouchEnabled()` or `updateTouchThreshold()`.

**Why:** It already prefills both controls correctly. It does not get the diff/modified treatment because `touchEnabled` saves immediately on change and the threshold has its own inline Update button — a per-section dirty indicator would have almost no lifetime. Consistency here would cost more than it returns.

### 9. The credential merge rule lives in a pure helper

**Decision:** Add `src/support/WiFiCredentials.h/.cpp` exposing

```cpp
struct WiFiCredentialUpdate {
    const char *ssid;
    const char *password;  // nullptr == key absent from the request
};

Config::WiFiConfig mergeWiFiCredentials(const Config::WiFiConfig &existing,
                                        const WiFiCredentialUpdate &update);
```

`POST /api/settings/wifi` calls it; the native test suite tests it directly.

**Why:** Discovered during implementation — the native environment cannot compile `WebServerManager.cpp` at all. `platformio.ini`'s native `build_src_filter` is `-<*> +<show/> +<Timer.cpp> +<color.cpp> +<strip/> +<support/>`, and ArduinoJson is commented out of the native `lib_deps`. The existing `test_ota` suite works around this by mirroring the OTA state machine in pure C++ (`test/test_ota/test_ota.cpp:8-10`) and asserting against the copy — which cannot catch a divergence between the mirror and the shipped handler.

Decision 5 is the one place in this change where a regression locks the user out of the device, so a test that proves nothing about the real code is not good enough. `src/support/` is already in the native build filter, and `Config::WiFiConfig` is native-safe (`src/Config.h` guards only `ConfigManager` behind `#ifdef ARDUINO`, line 179), so the merge rule can move into genuinely tested code with no mocking.

**Alternatives considered:**
- *Mirror-style test, following `test_ota`.* Consistent with precedent, but for a five-line branch the mirror is nearly a tautology.
- *No automated test, manual verification only.* Rejected for the lockout reason above.

### 10. `/api/status` moves to `JSON_DOC_XLARGE`

**Decision:** Change the `GET /api/status` document from `StaticJsonDocument<Config::JSON_DOC_LARGE>` (1024) to `Config::JSON_DOC_XLARGE` (2048).

**Why:** Also discovered during implementation. ArduinoJson v6 overflows silently — a full `StaticJsonDocument` serializes truncated JSON with no error, which would break the entire settings page rather than just the new field. Worst-case accounting for the response on a 32-bit target:

```
  15 member slots × 16 B                       240
  device_id 16 + device_name 32 + show ~16      64
  ip_address ~15 + wifi_ssid ~32                47
  wifi_configured_ssid (char[64])           →   64   ← added here
  show_params deep-copy (parsed via MEDIUM)   ≤ 512
                                              ─────
                                               ~927 / 1024
```

`show_params` is deep-copied from a `JSON_DOC_MEDIUM` parse buffer (`WebServerManager.cpp:186-192`), so a show with rich `params_json` plus a long SSID lands close to the ceiling, and `wifi_configured_ssid` spends most of the remaining margin. The extra ~1 KB of stack in the async handler has precedent — `WebServerManager.cpp:355` already uses `JSON_DOC_XLARGE` in a request handler.

**Note:** `Config::WiFiConfig::ssid` is `char[64]` while the 802.11 SSID limit is 32 bytes, so the realistic worst case is ~32 B smaller than accounted above. The buffer size is what bounds the copy, so the conservative figure is used.

## Risks / Trade-offs

- **Silent behavior change for existing `/api/settings/wifi` clients** → Anyone scripting a password clear via `{"ssid":"x"}` loses that. Mitigation: documented in the proposal and the spec; the new checkbox path sends an explicit `""`, which still works.
- **Five sections, five hand-written diff paths** → The page has no shared form abstraction, so decisions 2 and 3 are implemented three times (device name, hardware, WiFi). Mitigation: accepted — a generic binder is a larger refactor than the change warrants, and the sections have genuinely different submit semantics (restart vs. no restart, single field vs. multi).
- **Prefill hides that a field has a device-side default** → With `numPixels` showing `300`, it is less obvious that `300` came from a default rather than a deliberate setting. Mitigation: the `small` hints under the fields already state defaults; unchanged.
- **Modified indicator can go stale** → The page does not poll `/api/status`, so if the device is changed from another browser tab the snapshot is out of date and the indicator lies. Mitigation: pre-existing condition (the current info-boxes have the same staleness), and the sections that matter trigger a device restart.
- **`wifi_configured_ssid` grows `/api/status`** → The response uses `StaticJsonDocument<Config::JSON_DOC_LARGE>`; one more short string. Mitigation: verify against the existing budget during implementation; `about.html`'s `/api/system` response already carries similar fields.
- **Testing is manual for the HTML** → There is no browser test harness in this project, and the native environment cannot compile `WebServerManager.cpp` either (see decision 9), so neither the page nor the handlers are directly testable. Mitigation: the one dangerous rule is extracted into a tested pure helper; everything else relies on the explicit manual verification steps in tasks, covering both STA and AP mode.
- **The merge helper can be bypassed** → Nothing forces `POST /api/settings/wifi` to keep using `mergeWiFiCredentials`; a future edit could inline the logic again and silently lose the coverage. Mitigation: a comment at the call site; accepted as a normal maintenance risk.

## Migration Plan

No stored-data migration. `/api/status` gains a key, which is additive. The changed `POST /api/settings/wifi` semantics take effect on the OTA update with no persisted state involved.

Users on an older cached copy of `settings.html` against new firmware would post `{"ssid":..., "password":""}` — an explicit empty string, which the new handler honors as a clear, matching today's behavior. The failure mode is unchanged, not worsened.

## Open Questions

- **Should the WiFi section be in scope at all?** Decisions 4-6 are the bulk of the work and the only backend changes; leaving the WiFi section untouched would deliver the prefill for Device Name and Hardware Configuration at roughly a third of the cost. Prefilling SSID is genuinely useful (it is the field users most often need to confirm), but deferring it is defensible if this should stay a pure frontend change.
- **Should the modified indicator also offer a Revert control?** The saved value is in the snapshot, so a "Revert" link next to the indicator is a few lines. Left out of scope for now.
