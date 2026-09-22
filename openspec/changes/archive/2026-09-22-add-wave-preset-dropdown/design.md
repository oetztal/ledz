## Context

`scripts/show_variants.json:221-252` already names three Wave variants:

```json
"Wave": {
  "variants": [
    { "name": "default", "params": { "mode": "bounce", "decay_rate": 2.0, "brightness_frequency": 0.1 } },
    { "name": "tight",   "params": { "mode": "bounce", "decay_rate": 4.0, "brightness_frequency": 0.4 } },
    { "name": "calm",    "params": { "mode": "bounce", "decay_rate": 1.0, "brightness_frequency": 0.05 } }
  ]
}
```

The file is consumed only by `scripts/build_pages.py:108-155` to render preview PNGs. The device firmware knows nothing about these variants. `src/TouchController.cpp:45` carries a one-element `WAVE_VARIANTS[] = {"{}"}` so the touch button only offers the factory defaults.

`src/show/factory/ShowFactory.cpp:81-93` already parses `decay_rate` and `brightness_frequency` from JSON and forwards them to `Wave(decay_rate, brightness_frequency, mode)`. `src/show/Wave.{h,cpp}` accept both parameters via the constructor. No C++ work is needed for this change — the wiring already exists.

The control page's existing parameter UI for Wave (`data/control.html:242-262`) has a Mode dropdown, two numeric inputs, and an Apply button. Other shows with curated parameter sets already have a preset affordance. The Mandelbrot section (`data/control.html:78-118`) is the closest analogue:

- A `<select id="mandelbrotPreset" onchange="loadMandelbrotPreset(this.value)">` at the top of the params block.
- A JS `mandelbrotPresets` object whose keys are the variant names and whose values are the params object.
- A `loadMandelbrotPreset(value)` function that writes each field into the corresponding numeric input.
- A reset line `document.getElementById('mandelbrotPreset').value = '';` inside the `populateShowParams` `case 'Mandelbrot':` branch (`control.html:961`) so the dropdown doesn't keep claiming "Deep Zoom" after the user has edited the numeric inputs.

`data/control.html:1034-1036` lists the shows whose dropdown change does NOT auto-apply on selection; Wave is already in that list, so selecting Wave from the show dropdown correctly waits for Apply today.

## Goals / Non-Goals

**Goals:**
- Make the three named variants from `scripts/show_variants.json` selectable from the control page with one click into the dropdown plus an Apply click.
- Follow the Mandelbrot preset pattern line-for-line so the two affordances are symmetric and easy to reason about together.
- Behave correctly after Apply: the numeric inputs reflect the applied values, the preset dropdown is reset so it doesn't lie about the user's intent while they edit.
- Reset `Mode` to `bounce` when a preset is selected, so the dropdown is honest about what it sets.

**Non-Goals:**
- Touch button cycling.
- Single source of truth across `show_variants.json` and the JS `wavePresets` object.
- Spec work beyond extending the existing `Web UI exposes the Wave parameters` requirement.

## Decisions

### Dropdown lives at the top of `#waveParams`, above Mode

```html
<div class="param-row">
    <label class="param-label" for="wavePreset">Preset</label>
    <select id="wavePreset" onchange="loadWavePreset(this.value)">
        <option value="">-- select a preset --</option>
        <option value="default">Default bounce (no change)</option>
        <option value="tight">⚡ Tight fast</option>
        <option value="calm">🌊 Calm broad</option>
    </select>
    <small style="display:block; margin-top:4px; color:#666;">
        Sets decay rate and bounce frequency. Click "Apply Parameters" to use.
    </small>
</div>
```

Mirrors Mandelbrot's preset row at `data/control.html:79-92`. The existing Mode, Decay Rate, Brightness Frequency rows and Apply button stay in their current order below it.

**Alternatives considered:**
- Place the preset row below the numeric inputs: rejected — the preset is the higher-level concept; it should dominate the layout.
- A button row instead of a `<select>` (the ColorRanges pattern): rejected — the `show_variants.json` schema is `{name, label, params}`, which maps naturally onto `<select>` options; three buttons also consume more vertical space on a phone than a dropdown.

### `default` entry is labelled "(no change)" instead of dropped

`default` has `decay_rate: 2.0, brightness_frequency: 0.1`, which is byte-identical to the input element defaults at `data/control.html:253, 258`. Picking it from the dropdown fills the inputs with values they already have — a visible no-op. The user might mistake that for a bug.

Two alternatives:
- **Drop `default` and start the dropdown with `-- select a preset -- / Tight fast / Calm broad`**: every selectable row does something visible. But the dropdown then stops being a faithful 1:1 mirror of `show_variants.json`, breaking the implicit "what the gallery shows is what the dropdown offers" contract.
- **Keep `default` and label it `Default bounce (no change)`**: the row is honest about its no-op status. The dropdown still mirrors the file. A user reading it understands why nothing changes.

Chose the labelled keep. The Mandelbrot dropdown does the same thing for its no-op `default` variant (it's just labelled "Default (factory)" there — Wave's variant label is "Default bounce", which carries the file's verbatim `label`).

### `loadWavePreset` also resets Mode to `bounce`

All three variants in `show_variants.json` carry `"mode": "bounce"`. If the user had earlier selected Mode = `traveling` from the Mode dropdown, picking a preset leaves Mode at `traveling` unless the handler resets it. The handler therefore does:

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

Cost: two extra lines. Benefit: the dropdown is honest about what it sets, and the implicit invariant `variants imply mode=bounce` is preserved at the UI layer. Today `mode` is a no-op (`data/control.html:1103-1108`, `src/show/Wave.h:17-21`), so this is invisible. The day `mode` becomes meaningful, no follow-up is needed.

**Alternatives considered:**
- Only touch `decay_rate` and `brightness_frequency`, leave `mode` alone: rejected — silently contradicts the variant's `mode: "bounce"` in the file.
- Disable the Mode dropdown while a preset is selected: rejected — over-engineers a problem the user can't perceive today.

### Dropdown resets to `""` inside `populateShowParams`'s Wave branch

```javascript
case 'Wave':
    document.getElementById('wavePreset').value = '';     // NEW
    if (params.mode !== undefined) { ... existing ... }
    if (params.decay_rate !== undefined) document.getElementById('waveDecay').value = params.decay_rate;
    if (params.brightness_frequency !== undefined) document.getElementById('waveBrightnessFreq').value = params.brightness_frequency;
    break;
```

After Apply, the server stores the params in `params_json` (`src/Config.cpp:117`). On the next 10 s `setInterval(updateStatus, 10000)` poll (`data/control.html:1292`), `updateStatus` calls `populateShowParams` which fills them back into the numeric inputs. Without the reset line, the dropdown would continue to display "⚡ Tight fast" even after the user has typed new numbers into the inputs, and the page would disagree with itself.

Mirrors Mandelbrot's `document.getElementById('mandelbrotPreset').value = '';` at `data/control.html:961`.

### Three sources of truth stay duplicated

The three numbers (`2.0, 0.1`), (`4.0, 0.4`), (`1.0, 0.05`) appear in:
- `scripts/show_variants.json` (consumed by `build_pages.py` for preview PNGs)
- `src/show/factory/ShowFactory.cpp:88-89` (C++ defaults, matches `default` variant exactly)
- `data/control.html` `wavePresets` object (new; consumed by the browser)

`build_pages.py` runs on a developer machine, not the device, so the two device-side copies can't disagree at runtime unless someone hand-edits one and not the other. The Mandelbrot preset dropdown has the exact same shape of duplication today, and consolidating it (or the three Wave sources) would be a separate change with broader scope.

### Wave spec extension

The existing `wave-show` requirement `Web UI exposes the Wave parameters` (`openspec/specs/wave-show/spec.md:139-155`) lists the three controls. The change extends that requirement's normative paragraph to mention the `Preset` dropdown, and adds two scenarios covering preset selection and the post-Apply reset. Implementation updates `specs/wave-show/spec.md`; the proposal references it under Modified Capabilities.

### Scope: web UI only

`WAVE_VARIANTS[]` in `src/TouchController.cpp:45` stays at `{"{}"}`. Expanding it to mirror the same three variants would let the touch button cycle through them, but the user explicitly chose to keep this change web-only. A follow-up could do both surfaces if the touch cycling is ever wanted.

## Risks

- **Duplication drift**: if `scripts/show_variants.json` adds a fourth variant, the device-side `wavePresets` object will silently miss it. Acceptable for now; matches the Mandelbrot precedent.
- **Misleading default**: a user who picks `Default bounce (no change)` may still think it broke. Mitigated by the `(no change)` suffix in the label.
- **Mode-reset surprise**: a user who has set Mode = `traveling` for some reason will find their choice silently overridden by picking a preset. Today `mode` is a no-op so this is invisible. The `updateWaveBrightnessFreqHint` call preserves the same hint text (`Bounces per second`) for `bounce`, so the visible UI doesn't change.
- **Build hook order**: `compress_web.py` is invoked from `platformio.ini` as a pre-build step. Saving `data/control.html` and running `pio run -e adafruit_qtpy_esp32s3_nopsram` regenerates `src/generated/control_gz.h` before C++ compilation. Verified by reading `scripts/compress_web.py:200`. No build-step change needed.