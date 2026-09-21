## Why

`scripts/wave_show.py` exists to preview LED show parameter sets before any firmware work, and to keep the visual contract documented in code. Today it is a hand-written Python port of `src/show/Wave.cpp` (the module docstring explicitly says so). That port has already drifted once — see `archive/2026-09-21-wave-revert-wavelength-and-fix-wheel/` — and every drift between the Python reference and the C++ source is a future bug. As we add more shows, porting each one to Python doubles the maintenance surface for zero benefit: the `pio run -e native` environment already compiles every show against a host `MockStrip` for unit tests, so the "real" show already runs natively. Driving the simulator from the C++ source removes the duplication and makes the simulator an automatic byproduct of adding a show. While we are at it, rendering the same simulator output on every CI run and publishing the result to GitHub Pages turns the simulator from a manual one-shot tool into a living gallery of every show the firmware supports.

## What Changes

- Add a new native C++ host program at `scripts/show_simulator/main.cpp`, built by a new `[env:native_show_sim]` PlatformIO environment that compiles the show library (`src/show/`, `src/support/`, `src/strip/`, `src/color.cpp`, `src/Timer.cpp`) plus `MockStrip` and links them into a CLI binary. The binary takes `{show name, params JSON, iterations, width}` on argv/stdin, constructs the show through the existing `ShowFactory`, drives it against a `MockStrip` for the requested number of iterations, and writes `width * iterations * 3` bytes of raw RGB to stdout (one pixel triplet per LED, one row per iteration, row-major). Pixel values match what `MockStrip` records from `setPixelColor`, which is exactly what the device would write to the WS2812B strip.
- Replace the Python simulation in `scripts/wave_show.py` with a thin renderer that (a) builds the C++ simulator via `pio run -e native_show_sim`, (b) invokes it as a subprocess and reads its RGB stream from stdout, (c) writes the PNG. The stdlib PNG writer and CLI surface stay; the `WAVES` registry, the `wheel()`, and `wave_show()` Python functions are deleted — they are no longer the source of truth.
- Add a `--all` mode to the script that iterates over every registered show, renders a default-parameters preview for each, and writes `<show>.png` into an output directory. The list of shows comes from the C++ factory (the host binary can dump its registered names on a `--list` query).
- Add a `seeding` knob for non-deterministic shows (Fire, Starlight): the host binary accepts `--seed N`, seeds `random()`/`esp_random()`-equivalent sources before the run, and the script threads `--seed` through. Deterministic shows (Wave, Rainbow, Jump, ColorRun, …) ignore the seed.
- Add `.github/workflows/pages.yml` that runs on push to `main` (and on manual `workflow_dispatch`): builds the simulator, renders the `--all` gallery into `docs/show_previews/`, and deploys `docs/` to GitHub Pages via `actions/deploy-pages@v4`. A short `docs/show_previews/index.html` lists each preview with the show name and the parameters used to render it.
- Add `docs/SHOW_PREVIEWS.md` explaining how the gallery is generated, that previews are produced by the same C++ that runs on the device, and how to regenerate locally.
- No firmware behaviour changes. `src/show/*`, `src/show/factory/ShowFactory.*`, the JSON contract, and the web UI are untouched.

## Capabilities

### New Capabilities

- `show-simulator`: host-side CLI that drives any registered show against a `MockStrip` and emits a raw RGB stream of every iteration's strip state. Covers the simulator binary contract, its argv/JSON interface, deterministic-seeding for random shows, the integration with `wave_show.py`, and the GitHub Pages gallery.

### Modified Capabilities

- `wave-show`: drop the requirement that `scripts/wave_show.py`'s pixel output must match the C++ implementation byte-for-byte (the simulator now drives the C++ directly, so the requirement is vacuous — the requirement collapses to "the C++ implementation is its own reference"). The existing "Computational profile unchanged" requirement stays.

## Impact

- **New code**: `scripts/show_simulator/main.cpp`, `scripts/show_simulator/MockStrip.h` (host copy of the test mock, since the binary lives outside `test/`), `platformio.ini` `[env:native_show_sim]` block, `.github/workflows/pages.yml`, `docs/show_previews/index.html` (generated), `docs/SHOW_PREVIEWS.md`.
- **Modified code**: `scripts/wave_show.py` loses the Python `wheel()`, `wave_show()`, and `WAVES` registry; gains the subprocess invocation, `--all` mode, and `--seed` threading. The PNG writer, `WaveParams`, `build_parser()`, and the module-level CLI scaffolding stay.
- **CI**: `pages.yml` adds a new workflow; `test.yml` is unchanged (the simulator does not need to run on every PR, only on `main` and on demand — running it on every PR would inflate CI time without value since the previews are downstream artefacts).
- **Tooling**: `pio` is now required by `scripts/wave_show.py` (it already is for `pio run -e native`). The script gains a "did you forget to build?" guard that runs `pio run -e native_show_sim` automatically if `.pio/build/native_show_sim/program` is missing.
- **NVS / runtime contract**: unchanged. The JSON parameter shapes for every show stay exactly as they are.
- **Bundle size / RAM**: zero impact on the firmware. The simulator is host-only.
- **GitHub Pages**: requires repository settings → Pages → Source: "GitHub Actions" to be enabled once. Until then the workflow will fail at the deploy step with a clear message; the build itself still succeeds.
- **Existing users of `scripts/wave_show.py`**: the public CLI flags (`-n`, `-W`, `-o`, `--wave`, `--decay-rate`, `--brightness-frequency`, `--mode`, `-v`) stay. `--list` output changes from "waves: wave" to whatever the C++ factory currently registers (Rainbow, Wave, Fire, …). Anyone scripting around the output of `--list` will need to update.
