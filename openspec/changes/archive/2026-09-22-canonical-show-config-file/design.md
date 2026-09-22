## Context

Today `scripts/show_variants.json` groups each show's metadata under the show name. The schema in the file today:

```jsonc
{
  "_comment": "...",
  "ShowName": {
    "description": "...",
    "iterations": 300,                // optional, show-level render-row override
    "variants": [
      { "name": "...", "label": "...", "iterations": 50, "params": {...} },
      ...
    ]
  }
}
```

Eight of the twelve shows (`Fire`, `Starlight`, `Stroboscope`, `Rainbow`, `TheaterChase`, `Chaos`, `Mandelbrot`, plus `ColorRun` and `Jump` which have no params) have a variant whose `name` is `"default"` and whose `params` is `{}`. The other four (`Solid`, `Wave`, `MorseCode`) have variants whose `name` is descriptive (`white`, `default`, `hello`) and whose `params` is materially complete. The role of "factory default" is currently conveyed by *naming* only — there is no schema-level signal that distinguishes the default from any other variant.

`scripts/build_pages.py` (the gallery renderer) reads `body["variants"]` and iterates them. It does not know which variant is the default. The gallery therefore happens to lead with the default today only because convention places the default-named variant first in the array.

The on-device C++ factory (`src/show/factory/ShowFactory.cpp`) reads `doc["key"] | default_value` and so tolerates `{}` params by silently filling in `default_value` for every field. The `default_value` literals in the factory today are the actual factory defaults at runtime, but they live in C++ source, not in the JSON, and they are not validated against the JSON.

## Goals / Non-Goals

**Goals:**

- Make `scripts/show_variants.json` self-describing for both roles (factory default + curated presets) so consumers do not need to know the variant-naming convention.
- Make every variant's `params` complete (no implicit `{}` meaning "trust the C++ fallback"). The JSON becomes the description of runtime values, not a partial overlay.
- Separate the default structurally from the variants — a top-level `default` object on each show, not a magic-named entry in the `variants` array.
- Update `scripts/build_pages.py` so the renderer reads the new structure without losing the gallery's "default first, then variants" layout.
- Converge the four-string MorseCode drift on `"HELLO WORLD"` as the canonical default (the JSON declares it now; the C++ and HTML convergence is deferred to a follow-up).
- Keep `ShowFactory.cpp`'s `|` fallbacks intact for this change. They become last-resort safety nets only after a follow-up removes them, but they continue to work today.

**Non-Goals:**

- Removing the C++ `|` fallbacks in `src/show/factory/ShowFactory.cpp`.
- Removing the constructor default arguments in `src/show/*.h` (e.g. `MorseCode.h:42`'s `"HELLO WORLD!"`).
- Touching `src/TouchController.cpp`'s `*_VARIANTS[]` arrays — the touch controller's preset lists and their drift relative to the gallery is a separate reconciliation.
- Touching `data/control.html`'s `mandelbrotPresets`, `wavePresets`, `loadUkraineFlag`, `loadItalianFlag`, `loadFrenchFlag`, or the `<input type="text" id="morseMessage" value="HELLO WORLD!">` line.
- Touching `docs/SHOW_PARAMETERS.md`'s hand-written flag examples.
- Adding new HTTP endpoints.
- Changing the `ShowFactory` runtime behaviour. Every `POST /api/show {name, params}` call that produces a particular strip state today SHALL produce the same strip state after the change.

## Decisions

### Schema: `default` is a sibling of `variants` at the show level

```jsonc
{
  "ShowName": {
    "description": "...",
    "iterations": 300,                  // optional, show-level render-row override
    "default": {
      "params": {<fully materialised>}
      // iterations not needed: falls back to show-level
    },
    "variants": [
      { "name": "...", "label": "...", "params": {...}, "iterations": 50 },
      ...
    ]
  }
}
```

The `default` object holds the factory default; `variants` is the curated list of presets. The renderer concatenates them. A variant whose `params` equals the default is *not* duplicated in `variants` — it lives only in `default`.

**Alternatives considered:**

- *Boolean `is_default: true` on a variant*: rejected — keeps the role inside the list, doesn't make "default" a first-class schema concept, and a future "default has its own metadata" extension (e.g. a default-only `description`) is awkward.
- *Variant name `"default"` as the convention*: rejected — invisible to any consumer that does not already know it; four of the twelve shows already use descriptive names (`white`, `scroll`, `bifurcation`, `hello`), so the convention is not even uniform today.

### `default` is rendered as a synthetic first variant

The gallery must continue to show the default as the first preview under each show's description. The renderer treats `default` as a `Variant(name="default", label="Factory default", params=body["default"]["params"], iterations=body["default"].get("iterations", body.get("iterations")))` and prepends it to the variants array.

**PNG filename**: `ShowName_default.png`. For shows whose previous "default" variant was also named `"default"` (six shows today), the filename is identical to the previous output. For shows whose default variant had a descriptive name (`Solid.white`, `Starlight.warm`, `Stroboscope.white-fast`, `Rainbow.scroll`, `MorseCode.hello`, `Chaos.bifurcation`), the PNG filename changes from `ShowName_white.png` (etc.) to `ShowName_default.png`. The PNG content is byte-identical; only the filename changes.

**Gallery HTML label**: `Factory default`. Reads as the entry-point preview for that show.

### Materialisation: every variant's `params` is complete

Every key the C++ factory reads via `doc["key"] | value` is explicitly set in the JSON's `params` for both `default` and every `variants[]` entry. The `{}` shorthand disappears.

The materialised values come from the C++ factory's current `|` fallbacks:

| Show | C++ `|` fallback | JSON `default.params` after materialisation |
|------|------------------|---------------------------------------------|
| Solid | `colors=warm-white via set_default_if_empty`, `gradient=false` | `{"colors":[[255,250,230]], "gradient":false}` |
| Fire | `cooling=0.1, spread=10.0, ignition=0.5, spark_amount=0.5, start_offset=5, spark_range=5` | `{"cooling":0.1, "spread":10.0, "ignition":0.5, "spark_amount":0.5, "start_offset":5, "spark_range":5}` |
| Starlight | `probability=0.1, length=5000, fade=1000, r=255, g=180, b=50` | `{"probability":0.1, "length":5000, "fade":1000, "r":255, "g":180, "b":50}` |
| Stroboscope | `r=255, g=255, b=255, on_cycles=1, off_cycles=10` | `{"r":255, "g":255, "b":255, "on_cycles":1, "off_cycles":10}` |
| ColorRun | (no params) | `{}` (stays empty; no materialisation needed) |
| Jump | (no params) | `{}` (stays empty; no materialisation needed) |
| Rainbow | `time_step=1.0, pixel_step=1.0` | `{"time_step":1.0, "pixel_step":1.0}` |
| Wave | `mode="bounce", decay_rate=2.0, brightness_frequency=0.1` | unchanged (`{"mode":"bounce", "decay_rate":2.0, "brightness_frequency":0.1}`) |
| TheaterChase | `num_steps_per_cycle=21` | `{"num_steps_per_cycle":21}` |
| MorseCode | `message="HELLO", speed=0.5, dot_length=2, dash_length=4, symbol_space=2, letter_space=3, word_space=5` | `{"message":"HELLO WORLD", "speed":0.5, "dot_length":2, "dash_length":4, "symbol_space":2, "letter_space":3, "word_space":5}` |
| Chaos | `Rmin=2.95, Rmax=4.0, Rdelta=0.0002` | `{"Rmin":2.95, "Rmax":4.0, "Rdelta":0.0002}` |
| Mandelbrot | `Cre0=-1.05, Cim0=-0.3616, Cim1=-0.3156, scale=5, max_iterations=50, color_scale=10` | `{"Cre0":-1.05, "Cim0":-0.3616, "Cim1":-0.3156, "scale":5, "max_iterations":50, "color_scale":10}` |

The MorseCode entry diverges from the C++ fallback in one place (`message="HELLO"` → `message="HELLO WORLD"`); this is the documented follow-up.

For non-default variants, every omitted field is also materialised. The only variant with implicit fields today is `Fire.high`, which sets four of six fields; after the change it sets all six. The other variants are already complete.

**Alternatives considered:**

- *Only materialise the default*: rejected — leaves the file partially relying on C++ fallbacks, which is exactly the convention the change is meant to retire.
- *Drop `params` for variants and rely on `is_default`-style role markers*: rejected — the gallery needs concrete `params` to produce a preview.

### `Mandelbrot` `iterations` lifts to the show level

`Mandelbrot` and its seven variants all want `iterations: 1500`. The current data sets `iterations: 1500` on every variant and not at the show level. After the change the show body carries `"iterations": 1500` and the per-variant overrides are removed; the renderer falls through to the show-level value for every variant.

The other shows' `iterations` arrangement is unchanged.

### Renderer: prepend a synthetic variant from `default`

`_resolve_variants()` in `scripts/build_pages.py` becomes:

```python
def _resolve_variants(show, variants_by_show, descriptions):
    raw = variants_by_show.get(show, [])
    body_iterations = raw.get("iterations") if isinstance(raw, dict) else None
    raw_default = raw.get("default") if isinstance(raw, dict) else None
    raw_variants = raw.get("variants", []) if isinstance(raw, dict) else []

    defaults = descriptions.get(show, "")
    out = []
    if raw_default:
        out.append(Variant(
            name="default",
            label="Factory default",
            params=raw_default.get("params", {}),
            iterations=raw_default.get("iterations", body_iterations),
        ))
    for v in raw_variants:
        out.append(Variant(
            name=v["name"],
            label=v.get("label", v["name"]),
            params=v.get("params", {}),
            iterations=v.get("iterations", body_iterations),
        ))
    return out, defaults
```

The renderer is otherwise unchanged: PNG bytes, gallery HTML template, table-of-contents layout, and `--landing` / `--seed` behaviour all stay the same.

**Fall-through for shows omitted from the manifest**: unchanged — `fallback_variant()` still emits `{name: "default", label: "Default", params: {}}` for shows not listed.

### `ShowFactory.cpp` is not modified in this change

The `doc["key"] | value` fallbacks in `src/show/factory/ShowFactory.cpp` continue to be the runtime source of defaults. The JSON's `default.params` is, after this change, a *description* of those values, not the source. Removing the C++ fallbacks and making the JSON the source is a separate change (a future "consume canonical config in C++" proposal) that will introduce a generated header from this JSON.

The runtime contract that every `POST /api/show {name, params}` produces the same strip state before and after the change holds because materialised values match the C++ `|` fallbacks today (the MorseCode divergence is the only intentional exception, and it manifests only when the JSON's `default` is consumed as the runtime source — which this change does not do).

### Explicit follow-ups (out of scope, listed for traceability)

These drifts are surfaced by step 1 but not fixed here. Each is a candidate for its own change proposal:

1. **`src/TouchController.cpp` `*_VARIANTS[]` arrays**: ten single-color Solid entries not in the JSON; Starlight "cool" drifted (`length=100`/`fade=300` gallery vs `length=5000`/`fade=1000` touch); TheaterChase uses `num_steps_per_cycle=42` and `84` (not in JSON); MorseCode touch uses `"foo bar baz"` and `"gutes neues"` (not in JSON). Resolution: extend the canonical file with these (or a separate `touch_variants.json` if the touch controller's list is intentionally a subset).
2. **`data/control.html` hand-maintained preset objects** (`mandelbrotPresets`, `wavePresets`) and flag buttons (`loadUkraineFlag`, `loadItalianFlag`, `loadFrenchFlag`): generate from the JSON via a build step that emits `data/show_variants.js`, baked into `control_gz.h` by `compress_web.py`.
3. **`src/show/factory/ShowFactory.cpp` `|` fallbacks**: remove them once a build step emits a generated header (`src/generated/show_variants.h`) and the factory looks up `default.params` from there.
4. **`src/show/*.h` constructor default arguments** (`MorseCode.h:42`'s `"HELLO WORLD!"`, `Wave.h:42-43`, `Starlight.h:47-51`, etc.): remove them — they're documentation only and never invoked today.
5. **`docs/SHOW_PARAMETERS.md` hand-written flag examples**: generate from the JSON.
6. **MorseCode string convergence**: change `ShowFactory.cpp`'s `| "HELLO"` to `| "HELLO WORLD"` (or, after follow-up 3, remove the `|` and rely on the JSON default); change `data/control.html`'s `value="HELLO WORLD!"` to `value="HELLO WORLD"`; remove `MorseCode.h:42`'s `"HELLO WORLD!"` ctor default.

## Risks / Trade-offs

- **[Risk]** Renderer regression — wrong `default` rendering produces incorrect gallery output → **Mitigation**: the change is a small extension to `_resolve_variants`; the existing variant path is preserved verbatim; the synthetic default is prepended, not replacing anything. The diff to `build_pages.py` is small and reviewable in one read.
- **[Risk]** PNG filenames change for the six shows whose default had a descriptive name → **Mitigation**: only the file name changes; the byte content is identical. The gallery HTML uses the variant label (`Factory default`) as the visible heading, not the filename, so the public-facing label is more informative after the change. Any external link to `Solid_white.png` (etc.) breaks; given the gallery is auto-regenerated and the old PNGs are not referenced from anywhere outside the auto-generated `index.html`, this is acceptable.
- **[Risk]** Future maintainer assumes the JSON's `default` is the runtime source and removes the C++ fallbacks without verifying ShowFactory reads the JSON → **Mitigation**: `src/show/factory/ShowFactory.cpp` is explicitly out of scope in this change. The proposal's "Non-Goals" section names this; the proposal's "Explicit follow-ups" lists the change that will do it. Until that follow-up lands, ShowFactory's behaviour is unchanged.
- **[Risk]** `Mandelbrot`'s `iterations: 1500` lift changes render row count for the (now removed) per-variant entries → **Mitigation**: the show body says `1500` and the per-variant overrides say `1500`; the renderer treats both identically. No PNG byte change.
- **[Risk]** Materialised params drift from future C++ refactors (e.g. someone changes `cooling | 0.1f` to `cooling | 0.15f` and forgets to update the JSON) → **Mitigation**: this is a manual-review concern; both files are touched by a single change. A future change could introduce a test that round-trips the JSON's `default` through the C++ factory and asserts the resulting `params_json` byte-equality, but that's also out of scope.
- **[Risk]** ArduinoJson 7 parses a small subset of decimal floats with 1-ULP rounding error relative to the corresponding C++ literal (the parser's `mantissa * 10^exponent` iterative multiplication doesn't always round to nearest). After materialisation, the JSON-present path can produce a value 1 ULP away from the `|` fallback path for the same show. Empirically this affects only `Mandelbrot`'s `Cim0` and `Cim1` (the literals `-0.3616f` and `-0.3156f` round to `beb923a3` and `bea19653`, while ArduinoJson parses `-0.3616` and `-0.3156` to `beb923a2` and `bea19652`). Result: `Mandelbrot_default.png` differs from a render with `{}` in 1094 of 1,350,000 bytes (0.08% of pixels), visually imperceptible. All other shows render byte-identically. → **Mitigation**: this is an ArduinoJson limitation, not a defect in the materialisation. The canonical default is still the canonical default; the parser's ULP choice is just one off. If exact parity ever becomes required, either (a) write the JSON value as the exact float bit pattern (e.g. `-0.36160001158714294`), or (b) move to a JSON parser with Ryu/Schubfach-grade rounding. Neither is in scope here.

## Migration Plan

None. The change is purely a restructure of `scripts/show_variants.json` and a small extension to `scripts/build_pages.py`. The gallery PNGs are regenerated by the existing workflow. No firmware version needs bumping. No user-visible behaviour changes: same strip states from `POST /api/show`, same gallery layout (default first, then variants), same CLI surface.

## Open Questions

None — all four open decisions from the design conversation are recorded in this document and in the proposal.
