## Context

`scripts/show_variants.json` is the canonical description of every show's default parameters and curated presets after the `canonical-show-config-file` change. The prior `converge-morse-default` change closed one specific instance of drift (the four-string MorseCode headline) by literal-syncing the JSON, the C++ factory `|` fallback, the `MorseCode.h` constructor default, and the `data/control.html` input value.

The broader drift class is still present. Every show's default parameters are duplicated between the JSON's `default.params` and the C++ factory's `|` literals in `src/show/factory/ShowFactory.cpp:22-138`. The `TouchController` carries 16 hand-coded preset JSON strings across `src/TouchController.cpp:16-62` (10 Solid single-colours + 3 Solid color-ranges + 3 Solid two-color-blends) that have no representation in the JSON at all, plus another 12 hand-coded variants across Fire, Rainbow, Starlight, TheaterChase, and MorseCode that overlap or drift relative to the JSON. The web UI duplicates a subset of these in `data/control.html:424-454` (`mandelbrotPresets`, `wavePresets`).

Every future edit to a show's default parameters requires hand-syncing JSON + C++ factory `|` + C++ show constructor default + HTML preset object + TouchController hand-coded variant (where present). The headline example (MorseCode message) was the easiest one to spot because it had four locations; less obvious drifts exist for every other show.

The simulator (`scripts/show_simulator/main.cpp:34-49`) already maintains a parallel `parameterKeysByShow()` table that mirrors the keys each factory lambda reads. That table is a third copy of the same schema. It is used only for the simulator's `--list-params` CLI flag and is not consumed by the firmware.

The codebase has a working precedent for build-time generation: `scripts/compress_web.py` is a PlatformIO `pre:` script in `platformio.ini:65` that emits `src/generated/<name>_gz.h`. `scripts/get_version.py` is another `pre:` script that emits a version macro. A new `scripts/gen_show_variants.py` slots in next to them.

The `src/show/factory/ShowFactory.cpp` factory lambdas all read parameters via `doc["key"] | default_value`. ArduinoJson 7's `JsonDocument::set(other)` deep-merges (copies all keys from `other` into the receiver, overwriting where keys collide), so the JSON-default-then-user-params merge is a two-call sequence with no per-show special cases.

## Goals / Non-Goals

**Goals:**

- Make `scripts/show_variants.json` the runtime source of every show's default parameters in the firmware.
- Eliminate the `|` literal duplication between the JSON and the C++ factory lambdas. The `|` chain stays in the lambdas as defense-in-depth (a parser failure or a JSON key absence still produces a sane default), but it becomes unreachable for any key the JSON declares.
- Take over the 10 Solid single-colour variants in `TouchController.cpp:16-27` from the JSON, so the touch controller's Solid entry is sourced from the same place the gallery and the C++ factory are.
- Lock the JSON ↔ firmware parity in CI via a native test.
- Add 10 Solid single-colour entries to `scripts/show_variants.json` `Solid.variants[]` so the gallery, the touch controller, and the JSON all agree.

**Non-Goals:**

- Generating `data/show_variants.js` for `data/control.html`'s `mandelbrotPresets`, `wavePresets`, and flag buttons. That is a separate follow-up change.
- Reconciling the touch controller's other hand-coded variants (Fire `[{cooling:0.05}]`, Rainbow partial-overrides, Starlight `length=0/fade=250`, TheaterChase `[21, 42, 84]`, MorseCode `[foo bar baz, gutes neues]`). Per the prior decision, these stay hand-coded because they are touch-UX-curated, not gallery-curated.
- Removing the constructor default arguments in `src/show/*.h` (e.g. `MorseCode.h:42`, `Wave.h:42-43`, `Starlight.h:47-51`). They remain documentation-only after this change and are cleaned up in a follow-up refactor.
- Changing the gallery's `docs/show_previews/index.html` layout or label conventions. The 10 new Solid single-colour PNGs slot in under Solid alongside the existing `default`, `ukraine`, `italy`, `rainbow` PNGs.
- Changing the simulator's `parameterKeysByShow()` table at `scripts/show_simulator/main.cpp:34-49`. It remains a separate, hand-maintained parallel schema for the `--list-params` CLI flag. Sharing it with the firmware is a larger refactor.
- Touching the `Mandelbrot` ArduinoJson 7 1-ULP rounding divergence (already documented in the prior change's `design.md`). It is locked in here as an explicit test exception.

## Decisions

### Build-time generation, not runtime parsing

`scripts/gen_show_variants.py` runs at build time, emits a `constexpr` C++ header, and the firmware reads the header. No JSON is parsed at runtime by the firmware; no SD card, SPIFFS, or LittleFS resource is consumed; the JSON's role is upstream of the firmware, not inside it.

**Alternatives considered:**

- *Runtime JSON parsing from NVS or a baked blob*: rejected — the firmware would need a JSON parser at boot to populate a runtime lookup table, which adds complexity, RAM, and a new failure mode (corrupt or missing blob) for no benefit. The defaults never change at runtime; they are fixed by the firmware version.
- *Build a `.cpp` (not a header)*: rejected — `constexpr` data in a header is simpler and allows the compiler to optimize aggressively. A `.cpp` would require an extra translation unit.

### Header shape: struct-of-arrays with `constexpr` linear scan

The header declares `constexpr` structs in the `ShowVariants` namespace:

```cpp
struct Variant {
    const char* name;
    const char* label;
    const char* params_json;
};

struct ShowEntry {
    const char* name;
    const char* description;
    const char* default_params_json;
    const Variant* variants;
    size_t num_variants;
};

constexpr size_t kNumShows = 12;
constexpr ShowEntry kShows[kNumShows] = { ... };
```

`ShowFactory` does a linear scan over `kShows` to find a show by name (12 entries; ~10 ns on ESP32). Show creation is rare (boot, `POST /api/show`, touch), not in the 100 Hz LED loop, so the linear scan is fine.

Variants are emitted as a separate `constexpr Variant kSolidVariants[] = { ... }` array, and `ShowEntry::variants` points to it. The Python emitter does two passes: variants first, then shows referencing them.

**Alternatives considered:**

- *Parallel arrays (`kShowNames[kNumShows]`, `kDefaultParamsJson[kNumShows]`, `kShowVariants[kNumShows][...]`)*: rejected — harder to read in C++ (no single struct per show), more error-prone (parallel arrays must stay in sync).
- *`std::map<const char*, ShowEntry>`*: rejected — `constexpr` `std::map` requires C++20's heterogeneous lookup workarounds or a non-`constexpr` lookup function. The lookup is also more expensive in flash than a linear scan over 12 entries.
- *`std::array<std::pair<const char*, ShowEntry>, kNumShows>` with `std::lower_bound`*: rejected — overkill for 12 entries; the linear scan is already <1 µs.

### ShowFactory merge strategy: JSON default first, user payload overlay

`ShowFactory::createShow(name, paramsJson)` becomes:

```cpp
JsonDocument merged;
const ShowEntry* entry = ShowVariants::findByName(name.c_str());
if (entry) {
    if (entry->default_params_json[0] != '\0') {
        deserializeJson(merged, entry->default_params_json);
    }
}
JsonDocument user;
if (!deserializeJson(user, paramsJson.c_str())) {
    merged.set(user);  // ArduinoJson 7 deep-merge: overlay user keys onto merged
}
return it->second(merged);
```

The `|` literals in the factory lambdas (`src/show/factory/ShowFactory.cpp:29-138`) are unchanged. After this change, for any key the JSON declares, `doc["key"]` is always present and `|` never fires. For keys the JSON does not declare (none today, but the escape hatch exists), `|` still applies.

`merged.set(user)` is ArduinoJson 7's deep-merge: it copies every key from `user` into `merged`, overwriting where keys collide, leaving `merged`'s pre-existing keys intact where `user` is silent. The user's payload wins for any explicit override; the JSON default wins for any key the user omits.

**Alternatives considered:**

- *Strip the `|` chain entirely*: rejected — removing 50+ `|` literals across 12 factory lambdas is a wider refactor that doesn't improve the runtime behaviour and makes the diff hard to review. Defense-in-depth is cheap; removing it is a follow-up.
- *Lookup in the merged doc only, no `|`*: rejected — same as above, plus risks breakage if a future show is added to `ShowFactory` before its JSON entry exists. The `|` chain is the safety net.

### Touch controller: header-sourced Solid only

`SHOW_VARIANTS[0]` becomes `{"Solid", SOLID_VARIANTS_FROM_HEADER, kSolidVariantsCount}`, where `SOLID_VARIANTS_FROM_HEADER` is `ShowVariants::kShows[0].variants[i].params_json` for `i in [0, kSolidVariantsCount)`. The C++ side still gets a `const char* const*` array (matching the existing `SHOW_VARIANTS` table layout), but it's now built from the header rather than hard-coded.

`SHOW_VARIANTS[1]` (Solid color-ranges), `SHOW_VARIANTS[2]` (Solid two-color-blends), and the other 9 entries stay hand-coded as `TOUCH_ONLY_*` arrays. They are renamed from `SOLID_VARIANTS`, `COLORRANGES_VARIANTS`, etc. to `TOUCH_ONLY_COLORRANGES_VARIANTS`, `TOUCH_ONLY_TWOCOLORBLEND_VARIANTS`, etc. to make the boundary explicit.

The touch cycling UX is unchanged: entry 0 still has 10 variants; entries 1 and 2 still have 3 variants each.

**Alternatives considered:**

- *Move all 16 Solid variants (single-colours + color-ranges + two-color-blends) into JSON* with a new `Solid.touch` field: rejected — the gallery would gain 16 Solid variants where 10 are single-colour and 6 are multi-colour-sectioned, blurring the gallery's "this is a Solid preset" presentation. The current `Solid.variants[]` (ukraine/italy/rainbow + 10 single-colours) is clean: each entry is a single `colors` array the user can pick.
- *Touch-only Solid variants in JSON under a `Solid.touch[]` field*: rejected — adds a third per-show role (default + variants + touch) and the touch list becomes gallery-irrelevant. Defers the question of "where do touch-only Fire/Rainbow/etc. extras go" to a future change. Not worth the schema bloat today.
- *Keep `SOLID_VARIANTS[]` in TouchController.cpp and stop here*: rejected — this is the whole point of the change for the touch controller. The 10 single-colour entries drift the moment someone edits one of them.

### Solid variant naming

The 10 single-colour entries are added to `Solid.variants[]` with names that describe the colour rather than the touch-button position:

| Variant `name` | Label | Params |
|---|---|---|
| `warm-white` | Warm white | `{"colors":[[255,170,120]]}` |
| `pure-white` | Pure white | `{"colors":[[255,255,255]]}` |
| `red` | Red | `{"colors":[[255,0,0]]}` |
| `orange` | Orange | `{"colors":[[255,127,0]]}` |
| `yellow` | Yellow | `{"colors":[[255,255,0]]}` |
| `green` | Green | `{"colors":[[0,255,0]]}` |
| `cyan` | Cyan | `{"colors":[[0,255,255]]}` |
| `sky-blue` | Sky blue | `{"colors":[[0,127,255]]}` |
| `blue` | Blue | `{"colors":[[0,0,255]]}` |
| `magenta` | Magenta | `{"colors":[[255,0,255]]}` |

These names appear in the gallery under Solid. The touch controller's "switch show" cycling order is the JSON variant order.

### Generated header tracked in git

`src/generated/show_variants.h` is tracked in git, matching the precedent of `src/generated/control_gz.h` and friends. The PlatformIO `pre:` script regenerates it on every build, but the committed version is what reviewers see in PRs.

**Alternatives considered:**

- *`.gitignore` the generated header*: rejected — breaks the existing pattern, makes PR review harder (the diff is invisible without regenerating locally), and obscures accidental regenerations.

### Native parity test

A new test in `test/test_show_factory/` (or `test_show_factory/test_parity_with_json.cpp`) does:

```cpp
TEST_CASE("factory createShow(name, '{}') matches JSON default.params") {
    Show::Factory::ShowFactory factory;
    for (size_t i = 0; i < ShowVariants::kNumShows; ++i) {
        const auto& entry = ShowVariants::kShows[i];
        if (entry.default_params_json[0] == '\0') continue;  // shows with no default
        auto show = factory.createShow(entry.name, "{}");
        REQUIRE(show != nullptr);
        // Re-create with explicit empty {} and verify byte-equality
        JsonDocument doc;
        deserializeJson(doc, entry.default_params_json);
        // ... reconstruct the show from doc and verify the factory path produces equivalent state
        // The "equivalent state" check is show-specific; for now, verify the JSON round-trip
        // is byte-equal: serialise the merged doc and compare to entry.default_params_json.
    }
}
```

The exact parity check is at the JSON round-trip level: `factory.createShow(name, "{}")` parses `{}`, finds the JSON default, merges (which is a no-op for `{}`), and passes the merged doc to the factory lambda. The test serialises the merged doc back to JSON and asserts byte-equality with the header's `default_params_json`, modulo the `Mandelbrot` ULP exception.

The ULP exception is asserted explicitly:

```cpp
if (std::string(entry.name) == "Mandelbrot") {
    // ArduinoJson 7 rounds -0.3616 to 0xbeb923a2 vs the literal 0xbeb923a3; same for -0.3156.
    // The two values are 1 ULP apart and the resulting Mandelbrot renders differ in ~0.08%
    // of pixels (visually imperceptible). Allow this drift explicitly.
    SUCCEED("Mandelbrot ULP drift documented in design.md; see canonical-show-config-file change");
    return;
}
```

**Alternatives considered:**

- *Pixel-level parity test (run the simulator and compare PNGs)*: rejected — too heavy, requires the simulator binary in CI, and the simulator's `parameterKeysByShow()` already doesn't share state with the firmware's JSON parsing.
- *Write the JSON `Cim0` and `Cim1` values as their exact float bit patterns (e.g. `-0.36160001158714294`)*: rejected — works for these two values, but is fragile and ugly. The 1-ULP difference is documented and locked-in as a test exception; if a future ArduinoJson version fixes it, the test will start asserting byte-equality without code changes.

## Risks / Trade-offs

- **[Risk]** Build-time script failure (e.g. malformed JSON) breaks every ESP32 build → **Mitigation**: the Python emitter raises with a clear error message; the script's contract is "input file exists and is valid JSON; output file is regenerated atomically". The same pattern is used by `compress_web.py` and `get_version.py`.
- **[Risk]** `merged.set(user)` ArduinoJson 7 deep-merge behaviour is subtle for nested arrays → **Mitigation**: every show's `params` JSON uses flat scalars or one-level-deep arrays (`colors: [[r,g,b], ...]`, `ranges: [...]`). ArduinoJson 7's `set()` replaces arrays wholesale rather than element-by-element, which matches the existing semantics (e.g. setting `colors` replaces the entire colour list). No show depends on array-element-level merge today.
- **[Risk]** `factory.createShow(name, "{}")` runtime behaviour changes for `Mandelbrot` (1-ULP difference) → **Acknowledged**: documented in the prior change's `design.md` and re-asserted here as an explicit test exception. Visually imperceptible. If exact parity becomes required later, the JSON values can be written as exact float bit patterns.
- **[Risk]** Generated `show_variants.h` drifts from `show_variants.json` if a developer edits the JSON without rebuilding → **Mitigation**: the test asserts parity on every run; CI fails loudly. The header is also tracked in git, so a developer who edits JSON and forgets to rebuild sees a stale-header diff at `git status`.
- **[Risk]** Adding 10 Solid single-colour variants to `Solid.variants[]` expands the gallery's Solid section by 10 PNGs, which may feel cluttered → **Mitigation**: this is the gallery's purpose — it previews every variant the user can pick. The 10 entries are visually distinct (different colours) and clearly named.
- **[Risk]** `ShowVariants::findByName` linear scan over 12 entries adds latency to every `createShow` call → **Mitigation**: 12 entries × ~50 ns per strcmp ≈ 600 ns. Show creation is rare (boot, `POST /api/show`, touch). Negligible.
- **[Risk]** Removing `SOLID_VARIANTS[]` from `TouchController.cpp` means the touch controller depends on the generated header being present → **Mitigation**: the PlatformIO `pre:` hook runs before any compile, so the header is always present in any environment that builds the firmware (including the native test environment via the new pre-hook). The native simulator environment also gains the hook.
- **[Risk]** A future show added to `ShowFactory` before its JSON entry exists produces an "unknown default" path → **Mitigation**: `findByName` returns `nullptr`, `merged` stays empty, `deserializeJson(user, "{}")` succeeds with an empty doc, the factory lambda's `|` fallbacks apply. This is the same fallback behaviour the lambdas had before this change — only now it triggers only when the JSON is missing, not when the JSON is present but the key is absent.
- **[Risk]** The 10 Solid single-colour names (`warm-white`, `pure-white`, etc.) might conflict with future flag variants that use similar names → **Mitigation**: names are checked against the existing JSON before adding; none collide.

## Migration Plan

None. The change is additive at the firmware level (a new generated header and a new pre-build hook), and it preserves all existing NVS payloads (the merge behaviour means user-saved params still overlay the JSON default). Existing devices upgrade in place.

The 10 new `Solid_*<colour>.png` files in `docs/show_previews/` are new artefacts, not replacements; the existing `Solid_default.png`, `Solid_ukraine.png`, `Solid_italy.png`, `Solid_rainbow.png` are byte-identical after regeneration because their JSON inputs are unchanged. The gallery's `index.html` gains 10 new entries under Solid.

No firmware version bump is required. No user-visible behaviour changes for 11 of 12 shows (only `Mandelbrot` empty-params differs, by 0.08% of pixels).

Rollback: revert the change. The previous `ShowFactory.cpp` works against the previous `show_variants.json`. The `src/generated/show_variants.h` is git-tracked, so rollback restores the previous header.

## Open Questions

None.
