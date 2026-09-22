## 1. HTML markup

- [x] 1.1 In `data/control.html`, inside `#waveParams` (`control.html:242`), insert a new `<div class="param-row">` immediately above the existing `Mode` row (`control.html:243-250`). It SHALL contain a `<label class="param-label" for="wavePreset">Preset</label>`, a `<select id="wavePreset" onchange="loadWavePreset(this.value)">` with four `<option>` elements:
  - `value=""` text `-- select a preset --` (default selected)
  - `value="default"` text `Default bounce (no change)`
  - `value="tight"` text `⚡ Tight fast`
  - `value="calm"` text `🌊 Calm broad`
  - and a `<small>` help line reading `Sets decay rate and bounce frequency. Click "Apply Parameters" to use.`

- [x] 1.2 Confirm the existing Mode, Decay Rate, Brightness Frequency rows and Apply Parameters button below the new preset row are unchanged.

## 2. JavaScript — preset table

- [x] 2.1 In `data/control.html`, after the `mandelbrotPresets` block (`control.html:422`), add a `wavePresets` object with the exact three keys and values from `scripts/show_variants.json` lines 222-250:

  ```javascript
  const wavePresets = {
      default: { decay_rate: 2.0, brightness_frequency: 0.1 },
      tight:   { decay_rate: 4.0, brightness_frequency: 0.4 },
      calm:    { decay_rate: 1.0, brightness_frequency: 0.05 },
  };
  ```

- [x] 2.2 In `data/control.html`, after the `loadMandelbrotPreset` function (`control.html:434`), add `loadWavePreset(value)`:

  ```javascript
  function loadWavePreset(value) {
      if (!value) return;
      const p = wavePresets[value];
      if (!p) return;
      document.getElementById('waveDecay').value = p.decay_rate;
      document.getElementById('waveBrightnessFreq').value = p.brightness_frequency;
      document.getElementById('waveMode').value = 'bounce';
      updateWaveBrightnessFreqHint();
  }
  ```

## 3. JavaScript — populateShowParams reset

- [x] 3.1 In `data/control.html`, inside the `case 'Wave':` branch of `populateShowParams` (`control.html:937-944`), add one line at the top: `document.getElementById('wavePreset').value = '';` so the dropdown does not stay on a stale preset name after the server echoes back the applied params.

## 4. Spec extension

- [x] 4.1 In `openspec/specs/wave-show/spec.md`, extend the `Web UI exposes the Wave parameters` requirement (lines 139-155) by appending to its normative paragraph: "The parameter section SHALL also include a `Preset` dropdown listing the variants `default`, `tight`, and `calm` from `scripts/show_variants.json`. Selecting a preset SHALL set the `Decay Rate` and `Brightness Frequency` inputs to the variant's `decay_rate` and `brightness_frequency` values and SHALL set the `Mode` selector to `Bounce`. Selecting a preset SHALL NOT send a request to the device."

- [x] 4.2 In `openspec/specs/wave-show/spec.md`, add a scenario to the `Web UI exposes the Wave parameters` requirement:

  ```
  #### Scenario: Selecting a preset fills decay and frequency and resets Mode
  - **WHEN** the user picks `Tight fast` from the Preset dropdown
  - **THEN** the `Decay Rate` input shows `4.0` and the `Brightness Frequency` input shows `0.4`
  - **THEN** the `Mode` selector reads `Bounce`
  - **THEN** no request is sent to the device
  ```

- [x] 4.3 In `openspec/specs/wave-show/spec.md`, add a second scenario:

  ```
  #### Scenario: Preset dropdown resets after the server echoes applied params
  - **WHEN** the user picks `Tight fast`, clicks Apply Parameters, and the next status poll populates the inputs from `show_params`
  - **THEN** the Preset dropdown reads `-- select a preset --` while the numeric inputs continue to show the applied values
  ```

## 5. Build assets

- [x] 5.1 Run `python3 scripts/compress_web.py` (or `pio run -e adafruit_qtpy_esp32s3_nopsram`, which invokes it as a pre-build hook) and confirm `src/generated/control_gz.h` is regenerated.
- [x] 5.2 Confirm only `control_gz.h` changed; `config_gz.h`, `settings_gz.h`, `timers_gz.h`, `about_gz.h`, `common_gz.h`, `favicon_gz.h` are untouched.

## 6. Verification

- [x] 6.1 Run `pio test -e native` and confirm existing tests still pass. No test source change.
- [x] 6.2 Run `pio run -e adafruit_qtpy_esp32s3_nopsram` and confirm firmware builds cleanly.
- [x] 6.3 Manual: open `/`, pick `Wave` from the show dropdown. Confirm the Preset row appears at the top of the params block with four options, default selected. Confirm Mode / Decay Rate / Brightness Frequency rows and Apply Parameters button are unchanged below it.
- [x] 6.4 Manual: pick `Tight fast` from the Preset dropdown. Confirm Decay Rate = `4.0`, Brightness Frequency = `0.4`, Mode = `Bounce`. Confirm no request is sent to the device yet (network panel quiet).
- [x] 6.5 Manual: click `Apply Parameters`. Confirm `POST /api/show` body is `{"name":"Wave","params":{"mode":"bounce","decay_rate":4.0,"brightness_frequency":0.4}}` (decay_rate and brightness_frequency flipped to the new values; mode forced to bounce).
- [x] 6.6 Manual: wait for one status poll (≤ 10 s). Confirm the Preset dropdown has reset to `-- select a preset --` while Decay Rate and Brightness Frequency still show `4.0` and `0.4`.
- [x] 6.7 Manual: pick `Calm broad`. Confirm Decay Rate = `1.0`, Brightness Frequency = `0.05`, Mode = `Bounce`. Apply. Verify the strip behaviour visually matches `Calm broad` (slow broad oscillation).
- [x] 6.8 Manual: pick `Default bounce (no change)`. Confirm the inputs are unchanged visually (still show whatever values they had), Mode is forced to `Bounce` if it was previously `Traveling`. Apply. Confirm the device responds normally.
- [x] 6.9 Manual: pick Mode = `Traveling` from the Mode dropdown, then pick `Tight fast`. Confirm Mode snaps back to `Bounce`. This proves the implicit mode reset works.
- [x] 6.10 Manual: with `Tight fast` applied and applied across a reboot, confirm Wave restarts with `decay_rate=4.0, brightness_frequency=0.4` (NVS persistence untouched by this change but worth verifying).
