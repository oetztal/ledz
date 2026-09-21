## Context

The project already has a working native-build pipeline (`[env:native]` in `platformio.ini`) that compiles `src/show/`, `src/support/`, `src/strip/`, `src/color.cpp`, and `src/Timer.cpp` for the host, links them against `MockStrip`, and runs Unity tests against every show. The `Show::Show` virtual interface (`execute(Strip::Strip&, Iteration)`) is what the LED task calls every tick on the device; replaying it on the host produces pixels that are byte-for-byte identical to what the strip would receive, because the WS2812B driver is the only step that converts the `Strip::Color` value into wire format, and `MockStrip` records the value before that step. So the "real" show already runs natively for free — the simulator is essentially a stripped-down test binary that writes its state to stdout instead of asserting on it.

Today, `scripts/wave_show.py` does not use any of that. It is a 318-line hand-written Python port of `Wave.cpp` (the docstring is explicit about this), and has drifted once already (see `archive/2026-09-21-wave-revert-wavelength-and-fix-wheel/` — the fix was needed because the Python reference had moved on while the C++ had not). That drift is the symptom; the disease is duplication. Replacing the Python simulation with a thin renderer around the host-built simulator eliminates the duplication and removes the drift permanently. Because the simulator binary needs `pio run` to build, the renderer script gains a build step — a one-time ~10-second cost on first run, free on subsequent runs thanks to PlatformIO's incremental compile.

The GitHub Pages side is straightforward: the same `pio run` produces the simulator binary, the simulator binary produces raw RGB, the renderer wraps it into PNGs, and `actions/deploy-pages@v4` ships `docs/` to Pages. No bespoke deployment tooling needed; everything composes from pieces the project already uses.

## Constraints

- **No firmware changes.** The `[env:adafruit_qtpy_esp32s3_nopsram]` build must be byte-identical before and after this change. Anything that goes into `src/show/`, `src/strip/`, `src/support/`, `src/color.cpp`, or `src/Timer.cpp` is firmware code.
- **No PSRAM, ~200 KB RAM, 100 Hz LED task** on device. The simulator is host-only and has no such budget; we use full `std::random_device`, dynamic allocation, and `std::cout` freely in `scripts/show_simulator/main.cpp`.
- **Deterministic renders.** Random shows (Fire, Starlight, ColorRun) must produce the same PNG on every CI run for a given `--seed`. Deterministic shows (Wave, Rainbow, Jump, ColorRanges, TheaterChase, Chaos, Mandelbrot, MorseCode, Stroboscope) ignore the seed and remain deterministic regardless.
- **Python is still required** for the renderer because the script's only host-side dependency beyond `pio` is the PNG writer, which stays stdlib-only. Pulling in numpy or Pillow would be a regression.
- **CI does not change.** `pio test -e native` stays the source of truth for show correctness; the simulator is a downstream artefact and runs in a separate workflow.

## Goals / Non-Goals

**Goals:**

- One source of truth: the simulator drives `src/show/*` exactly as the device does. There is no second implementation to keep in sync.
- Adding a new show to `ShowFactory` automatically gives it a preview on the GitHub Pages gallery with zero extra work.
- Deterministic renders for every show (seedable RNG).
- `scripts/wave_show.py` keeps its current CLI surface (`-n`, `-W`, `-o`, `--wave`, `--decay-rate`, `--brightness-frequency`, `--mode`, `-v`, `--list`) so existing local invocations keep working.
- GitHub Pages gallery is regenerated on every push to `main` and is also runnable on `workflow_dispatch` with a custom seed.

**Non-Goals:**

- Changing the show factory, the JSON parameter contract, or the web UI.
- Changing `MockStrip` (the test mock at `test/MockStrip.h` already does what we need). The simulator hosts its own copy because `test/` is filtered out by PlatformIO's test harness and we don't want to depend on it from a non-test binary.
- Animating the previews or making them clickable. Static PNGs are enough; the strip is a 1-D medium.
- Generating interactive demos, GIFs, or per-parameter-sweep galleries. One default-parameters PNG per show is sufficient as a living gallery.
- Removing or renaming `scripts/wave_show.py`. It stays; it just loses its hand-rolled simulation.
- Self-hosting the simulator binary. The script depends on `pio` for the build; the GitHub Pages workflow does too. This is fine — `pio` is already a CI dependency.

## Decisions

### Decision 1: A new PlatformIO env, not a test binary

Add `[env:native_show_sim]` to `platformio.ini` whose `src_filter` includes only `scripts/show_simulator/**`, `src/show/**`, `src/show/factory/**`, `src/strip/**`, `src/support/**`, `src/color.cpp`, and `src/Timer.cpp`. The existing `[env:native]` stays unchanged and continues to drive Unity tests; the new env produces a CLI binary at `.pio/build/native_show_sim/program`.

**Alternatives considered:**

- *Reuse `[env:native]` with a custom `main()`*: would force the test harness to coexist with a CLI in the same binary. Test entry points (`main()` for Unity) and the CLI entry point can't both be `main`; would need a `BUILD_MODE` macro and a runtime switch. Rejected — too much build-system surgery for a clean separation.
- *Make `scripts/show_simulator/` a test directory*: PlatformIO's test runner expects `unity_config.h` and `test_*.cpp`; it isn't built as a standalone program. Rejected — fights the test harness.
- *Use SCons outside PlatformIO*: possible but loses the existing incremental build, the ArduinoJson dependency declaration, the C++17 flags, and the `--coverage` plumbing. Rejected — far more friction than it saves.

### Decision 2: stdout is the wire format

The simulator writes `width * iterations * 3` bytes of raw RGB to stdout (one row per iteration, RGB triplets left-to-right, row-major). No framing, no header, no length prefix. The renderer script reads exactly that many bytes from stdout and feeds it straight into the PNG writer. This is the simplest possible inter-process contract — no JSON, no CBOR, no length-prefixed protocol — and the failure modes are obvious: short read → truncated PNG, long read → extra bytes logged and ignored.

**Alternatives considered:**

- *JSON per pixel*: 5–10x bloat for zero benefit. The PNG writer already takes raw RGB.
- *PNM (`.ppm` stream)*: would let us skip the PNG writer entirely, but PNM isn't browser-native and would need a second pass to convert. Rejected.
- *Length-prefixed binary framing*: defensible if we needed partial reads, but the renderer always wants the whole grid. Rejected — YAGNI.

### Decision 3: Seed override via `Support::Random`

The simulator adds a tiny, well-defined seeding hook to `Support::Random.h`:

```cpp
namespace Support {
    void setRandomSeedOverride(Random::result_type seed);   // -1 / unset by default
    Random::result_type randomSeed();                       // returns override if set, else platform entropy
}
```

`setRandomSeedOverride(N)` is called from `scripts/show_simulator/main.cpp` immediately before constructing the show. Fire and ColorRun both call `Support::randomSeed()` from their constructors and pick up the override; Starlight uses C `rand()` and is handled separately with `std::srand(seed)`. The override is cleared after construction so subsequent calls (none today, but future shows may use it lazily) get fresh entropy on the device. Cost on ESP32: one `int32_t` static, one branch. Negligible.

**Alternatives considered:**

- *Thread-local override*: cleaner isolation, but the LED task is single-threaded and the override is set once at boot. `static` is fine.
- *Constructor parameter to every show*: invasive. Would touch 12 show headers and break the existing JSON contract.
- *Seeding only the `rand()` path and accepting non-determinism for Fire/ColorRun*: rejected — those are the visually most-random shows and the ones the gallery will showcase. They have to be reproducible.

### Decision 4: Render `ColorRanges` variants as separate gallery entries

`Solid` (and the broader `ColorRanges` family) is parameterised: one set of colors produces one look, a different set produces another. The gallery entry for `Solid` uses a fixed, document-recommended default (the French-flag preset that already exists as a `loadFrenchFlag()` JS function in `data/control.html`). One PNG per show is sufficient — adding "show me every preset" is out of scope per the goals.

### Decision 5: GitHub Pages via `actions/deploy-pages@v4` on `docs/`

The workflow:

1. `actions/checkout@v7` with `fetch-depth: 0` (Pages cares about git history for diff URLs).
2. `actions/setup-python@v7` + `actions/cache@v6` + `pip install platformio` (cached across runs by `platformio.ini` hash).
3. `pio run -e native_show_sim` (cached compile).
4. `python3 scripts/render_gallery.py --out docs/show_previews --seed 42` — a thin wrapper that runs the renderer per show and writes `docs/show_previews/<show>.png` plus a generated `docs/show_previews/index.html`.
5. `actions/configure-pages@v5` + `actions/upload-pages-artifact@v3` + `actions/deploy-pages@v4`.

Permissions: `contents: read`, `pages: write`, `id-token: write` — the minimum set required by `deploy-pages@v4`.

The workflow only runs on `push` to `main` and on `workflow_dispatch` (so we can re-render the gallery with a different seed without a code change). It does not run on PRs — the simulator itself is exercised by `test.yml` indirectly (every show compiles and unit-tests green on every PR), and the gallery is a downstream artefact.

**Alternatives considered:**

- *Run on every PR*: would slow PR CI for no reviewer benefit (the gallery is a deliverable, not a test signal). Rejected.
- *Use `peaceiris/actions-gh-pages@v4` and push to `gh-pages` branch*: deprecated by GitHub in favour of the official `actions/deploy-pages@v4`. Rejected.
- *Pre-generate and commit PNGs*: defeats the purpose of "regenerate on every push". Rejected.

### Decision 6: One Python wrapper script, two entry points

`scripts/wave_show.py` keeps its current CLI and gains a `--all` mode that takes an output directory. `scripts/render_gallery.py` (a new script) is a 30-line wrapper that calls `scripts/wave_show.py --all` for each show, then writes `index.html`. Keeping the two scripts separate means local users only need `wave_show.py` for interactive use, while CI uses both.

## Risks / Trade-offs

- **[Simulator build is the critical path]** → If `src/show/*` doesn't compile on the host (e.g., someone introduces an `#ifdef ARDUINO` block with no `#else`), the simulator build breaks and the gallery becomes empty. Mitigation: `test.yml` already runs `pio test -e native` on every PR; if a show stops compiling on the host, the test step fails first and the bad code never reaches `main`. The Pages workflow additionally runs `pio run -e native_show_sim` before rendering — if it fails, the previous gallery stays deployed (Pages keeps the prior artefact).
- **[Starlight seed via `srand` is global]** → If anyone in the future adds a `std::random_device` use elsewhere in the simulator binary that runs before `srand`, that code's entropy will be tainted. Mitigation: `main.cpp` does `std::srand(seed)` as its second line, before any show construction.
- **[`ColorRanges` default is opinionated]** → A user who lands on the Pages gallery expecting to see their preferred flag preset will see ours. Acceptable — the gallery is a living reference of "what the defaults look like", not a UI for tuning shows. The web UI is the place for tuning.
- **[Pages workflow requires repo setting]** → "Source: GitHub Actions" must be enabled in repo settings before the workflow can deploy. Until then, the build succeeds and `actions/deploy-pages@v4` fails with a clear `Failed to create deployment (status: 403)` message. Mitigation: document the prerequisite in `docs/SHOW_PREVIEWS.md`.
- **[Python script now requires `pio` to be installed]** → Anyone who used `wave_show.py` on a machine without `pio` will break. Mitigation: the script detects missing `.pio/build/native_show_sim/program` and runs `pio run -e native_show_sim` automatically; if `pio` is not on `$PATH`, the error message names it explicitly.
- **[First-run latency]** → First invocation pays a ~30-second PlatformIO install (cached after that) and a ~10-second native compile. Subsequent runs are ~0.5s. Acceptable; matches the existing `pio test -e native` experience.

## Migration Plan

No migration. The JSON contract, the firmware, the existing CLI flags of `wave_show.py`, and the show API are all untouched. The `WAVES` registry, the `wheel()`, and the `wave_show()` function inside `scripts/wave_show.py` are removed as part of the change — they have no consumers outside the script itself, and their public surfaces (CLI flags, output PNG) are preserved.

Rollback is a single `git revert` of the implementation commits. The archived change captures the old behaviour (Python port) for reference. The Pages deployment is the only thing that becomes inert on rollback — the workflow file is removed, the `docs/show_previews/` directory stops being updated, and Pages keeps the last-deployed version forever (no cleanup needed).

## Open Questions

- **Should `scripts/wave_show.py` keep its module-level `WIDTH = 300` constant as a default, or expose a `width` CLI flag?** Currently it does — kept. Decided.
- **Should the gallery include a per-show metadata block (parameter names + accepted values)?** Nice-to-have but not required by the goals. Left as a follow-up; the generated `index.html` can be extended later without breaking the spec.
- **Should we expose a `--list-params <show>` query to the simulator binary?** Lets the gallery auto-generate "this show accepts these parameters" hints. Cheap to add (~10 lines in `main.cpp`) and aligns with the "automatic previews" goal. **Decided**: yes, include it in the implementation as a follow-up convenience.
