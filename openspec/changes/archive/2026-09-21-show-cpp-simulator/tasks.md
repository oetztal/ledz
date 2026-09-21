## 1. Seed override hook

- [x] 1.1 Add `void setRandomSeedOverride(Random::result_type)` and a static override to `src/support/Random.h`. `Support::randomSeed()` returns the override when set, otherwise the existing platform entropy (`esp_random()` on Arduino, `steady_clock` on host). Default state: override unset. Keep the function `inline` to avoid touching `Random.cpp` (which doesn't exist yet) and to keep the device's flash footprint unchanged.
- [x] 1.2 Verify `pio run -e adafruit_qtpy_esp32s3_nopsram` produces a binary byte-identical (modulo `firmware.bin` size ≤ +8 bytes) to before this change. The override path is one branch on a static `int32_t`.

## 2. Simulator binary

- [x] 2.1 Create `scripts/show_simulator/MockStrip.h`: a copy of `test/MockStrip.h` adapted to the simulator's include layout (no `MockStrip.h` reachability from a non-test binary). Same semantics: vector-backed, no-ops for `show()` / `setBrightness()`.
- [x] 2.2 Create `scripts/show_simulator/main.cpp`: argv parser (uses a small hand-rolled parser, no `getopt` dependency), `--show`, `--width`, `--iterations`, `--params`, `--seed`, `--list`, `--list-params` flags; constructs `ShowFactory`, builds a `MockStrip`, calls `execute()` in a loop, writes `width * iterations * 3` RGB bytes to `stdout`. Uses raw `std::cout.write(...)` after `sync_with_stdio(false)`. Unknown show / unknown flag exits non-zero with a clear message. Respects `--seed` via `setRandomSeedOverride(...)` and `std::srand(seed)` before show construction.
- [x] 2.3 Add `[env:native_show_sim]` to `platformio.ini`: `platform = native`, `build_src_filter = -<*> +<show/> +<show/factory/> +<strip/> +<support/> +<color.cpp> +<Timer.cpp> +<scripts/show_simulator/>`, `build_flags = -std=gnu++17` (no `--coverage`, no `coverage_link.py`), `build_unflags = -std=gnu++11`, `lib_deps = bblanchon/ArduinoJson@^7.4.3`.
- [x] 2.4 Manually verify `pio run -e native_show_sim` builds clean and produces `.pio/build/native_show_sim/program`. Smoke-test `.pio/build/native_show_sim/program --show Wave --width 60 --iterations 5 --seed 1 --params "{}" | wc -c` returns `900`.

## 3. wave_show.py renderer rewrite

- [x] 3.1 Remove `wheel()`, `wave_show()`, `WAVES`, and `MODE_TRAVELING`-as-distinct-mode logic from `scripts/wave_show.py`. Keep `WaveParams` (rename to `RenderParams` and add `seed`, `simulator_binary`, `all_dir` fields), `_validate`, `render` (now: invoke simulator + write PNG), `write_png`, `build_parser` (extended), `params_from_args`, `main`. Drop the docstring paragraphs that describe the Python port and the `wheel()` HSV derivation — replace with a paragraph that names the simulator binary and links to `docs/SHOW_PREVIEWS.md`.
- [x] 3.2 Implement `ensure_simulator_built(params)` that runs `pio run -e native_show_sim` if `.pio/build/native_show_sim/program` is missing. Capture and print `pio`'s stderr on failure. Catch `FileNotFoundError` for `pio` and exit with a message naming the missing tool.
- [x] 3.3 Implement `invoke_simulator(params) -> bytes` that builds the argv (`--show`, `--width`, `--iterations`, `--params`, optional `--seed`) and reads the simulator's stdout into a `bytes` of length `width * iterations * 3`. Use `subprocess.run(..., check=True, capture_output=True)`.
- [x] 3.4 Implement `render_one(params)` that calls `ensure_simulator_built` → `invoke_simulator` → `write_png`. Existing single-show invocation goes through this path.
- [x] 3.5 Implement `--all <dir>` mode: iterates over `subprocess.check_output([binary, "--list"]).decode().splitlines()`, runs `render_one` per show with `--params "{}"` and the user-supplied `--seed`, writes `<show>.png` per show, and emits a minimal `index.html` listing each preview with a hyperlink. Each PNG is rendered in a separate subprocess call (so the simulator's per-process state — including `--seed` — is clean).

## 4. Documentation

- [x] 4.1 Add `docs/SHOW_PREVIEWS.md` describing what the gallery is, that it is generated from the same C++ that runs on the device, how to regenerate locally (`pio run -e native_show_sim && python3 scripts/wave_show.py --all ./out`), and the repo-settings prerequisite for GitHub Pages (`Settings → Pages → Source: GitHub Actions`).
- [x] 4.2 Update `docs/SHOW_PARAMETERS.md` to mention that `--list` on the script now reflects the live `ShowFactory` registration (not a hard-coded Python list).

## 5. GitHub Pages workflow

- [x] 5.1 Create `.github/workflows/pages.yml`: triggers on `push` to `main` and on `workflow_dispatch` (with a `seed` input). Steps: `actions/checkout@v7` (`fetch-depth: 0`), `actions/setup-python@v7` (3.12), `actions/cache@v6` keyed on `platformio.ini`, `pip install platformio`, `pio run -e native_show_sim`, `python3 scripts/wave_show.py --all docs/show_previews --seed $SEED`, `actions/configure-pages@v5`, `actions/upload-pages-artifact@v3` (artifact path: `docs/`), `actions/deploy-pages@v4`. Top-level `permissions: contents: read, pages: write, id-token: write`.
- [x] 5.2 Add a brief comment header to `pages.yml` documenting the `Settings → Pages → Source: GitHub Actions` prerequisite and what to do if `deploy-pages` fails with `403`.

## 6. Verify

- [x] 6.1 `pio run -e adafruit_qtpy_esp32s3_nopsram` builds clean and the binary is within ±8 bytes of pre-change size.
- [x] 6.2 `pio test -e native` runs the full Unity suite green. Existing Wave, Rainbow, Fire, etc. tests pass unchanged.
- [x] 6.3 `pio run -e native_show_sim` builds clean.
- [x] 6.4 `python3 scripts/wave_show.py --list` prints every registered show name.
- [x] 6.5 `python3 scripts/wave_show.py -o /tmp/wave.png` produces a PNG byte-identical to a manual reproduction: `pio run -e native_show_sim && .pio/build/native_show_sim/program --show Wave --width 300 --iterations 1000 --params "{}" | python3 -c 'import sys, struct, zlib; ...' ` produces the same PNG. (Verification only — the Python PNG generator already exists; we just confirm both paths agree.)
- [x] 6.6 `python3 scripts/wave_show.py --all /tmp/previews --seed 42` produces one PNG per show, all of them byte-identical across two consecutive runs (seeded determinism).
- [x] 6.7 `grep -rn "wheel\(" scripts/wave_show.py` returns nothing — the Python `wheel()` is gone, only the simulator binary owns the colour logic now.
- [x] 6.8 Spot-check `docs/show_previews/Wave.png` against the device-side Wave render by eye: bouncing rainbow source, exponential decay envelope, hue trail — all match the reference simulation.
