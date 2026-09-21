# show-simulator Specification

## Purpose

The show simulator is a host-side CLI binary that drives any registered show against a `MockStrip` for `N` iterations and emits the resulting strip state as a raw RGB stream on stdout. It exists so that the same C++ source that runs on the device can be used to preview show parameter sets and to populate the GitHub Pages gallery of show previews, without maintaining a parallel Python port.

## ADDED Requirements

### Requirement: Host CLI drives every registered show

The show simulator binary SHALL be built by `[env:native_show_sim]` from `scripts/show_simulator/main.cpp`, `src/show/**`, `src/show/factory/**`, `src/strip/**`, `src/support/**`, `src/color.cpp`, and `src/Timer.cpp`. Given a show name on argv, it SHALL construct that show through `Show::Factory::ShowFactory::createShow(name, params_json)`, drive it against a `MockStrip` of the requested length for the requested number of iterations, and write the resulting pixels to stdout. On a malformed or unknown show name the binary SHALL exit non-zero with a message naming the offending argument.

#### Scenario: Rendering Wave with default parameters

- **WHEN** the binary is invoked with `--show Wave --width 60 --iterations 1000 --params "{}"`
- **THEN** it constructs `Show::Wave` with default parameters and writes `60 * 1000 * 3 = 180000` bytes to stdout
- **THEN** every triplet is the `(r, g, b)` value that `MockStrip` records for one LED at one iteration
- **THEN** the byte sequence is byte-for-byte identical to the corresponding sequence produced by running the same C++ on the device

#### Scenario: Unknown show name exits non-zero

- **WHEN** the binary is invoked with `--show NoSuchShow`
- **THEN** the binary exits with a non-zero status and prints a message naming `NoSuchShow` and listing the available show names

#### Scenario: Malformed JSON falls back to defaults

- **WHEN** the binary is invoked with `--show Wave --params "not json"`
- **THEN** `Wave` is constructed with default parameters and the binary exits zero

### Requirement: RGB stream wire format

The bytes written to stdout SHALL be exactly `width * iterations * 3` raw RGB bytes, arranged row-major with one row per iteration and RGB triplets left-to-right within a row. There SHALL be no header, no per-row framing, no length prefix, and no trailing bytes.

#### Scenario: Deterministic byte count

- **WHEN** the binary is invoked with `--width 300 --iterations 1000`
- **THEN** it writes exactly `900000` bytes to stdout before exiting
- **THEN** `stdout` is closed cleanly

#### Scenario: Row-major ordering

- **WHEN** the binary is invoked with `--width 5 --iterations 3 --show Wave --params "{}"`
- **THEN** bytes 0–14 are iteration 0, bytes 15–29 are iteration 1, bytes 30–44 are iteration 2
- **THEN** within each row, bytes 0–2 are LED 0, bytes 3–5 are LED 1, and so on

### Requirement: RNG seed override for deterministic renders

The binary SHALL accept a `--seed N` flag that, when set, makes every random source used by the show deterministic for the duration of the run. Fire and ColorRun SHALL pick up the seed via `Support::randomSeed()`. Starlight (which uses C `rand()` directly) SHALL pick up the seed via `std::srand(seed)`. The seed SHALL be applied before any show construction and SHALL NOT be applied to deterministic shows that do not consume it.

#### Scenario: Seeded Fire produces identical bytes across runs

- **WHEN** the binary is invoked twice with identical args plus `--seed 42 --show Fire --width 60 --iterations 200`
- **THEN** both invocations write the exact same byte sequence to stdout

#### Scenario: Unseeded Fire is non-deterministic

- **WHEN** the binary is invoked twice with `--show Fire --width 60 --iterations 200` (no `--seed`)
- **THEN** the two byte sequences MAY differ (no contract on equality)

#### Scenario: Deterministic show ignores the seed

- **WHEN** the binary is invoked twice with `--show Wave --width 60 --iterations 100` (no `--seed` either time)
- **THEN** both invocations write the exact same byte sequence to stdout

### Requirement: List shows query

The binary SHALL accept a `--list` flag that prints every registered show name (one per line, in display order, matching `ShowFactory::listShows()`) to stdout and exits zero. No pixel data is emitted in this mode.

#### Scenario: List mode emits no pixels

- **WHEN** the binary is invoked with `--list`
- **THEN** stdout contains one show name per line followed by a trailing newline
- **THEN** the byte count equals `len(names) * (len(name) + 1)` exactly
- **THEN** the binary exits zero

#### Scenario: List mode reflects current factory registration

- **WHEN** a new show is added to `ShowFactory`'s constructor
- **THEN** the next `--list` invocation includes the new show's name in its output

### Requirement: List parameters query

The binary SHALL accept a `--list-params <show>` flag that prints the JSON keys accepted by that show's factory lambda, one per line, to stdout, and exits zero. The list SHALL be sourced from the `JsonDocument` the factory receives at construction time, by snapshotting the keys of a representative empty document.

#### Scenario: Param list for Wave

- **WHEN** the binary is invoked with `--list-params Wave`
- **THEN** stdout contains `mode`, `decay_rate`, `brightness_frequency` (one per line) — the same keys `Show::Factory::ShowFactory`'s Wave lambda reads from the document

### Requirement: Renderer script wraps the binary

`scripts/wave_show.py` SHALL replace its hand-written Python simulation with a call to the simulator binary. The script SHALL auto-build the binary via `pio run -e native_show_sim` if `.pio/build/native_show_sim/program` is missing. The script SHALL keep its existing CLI surface (`-n`/`--iterations`, `-W`/`--width`, `-o`/`--output`, `--wave`, `--decay-rate`, `--brightness-frequency`, `--mode`, `-v`, `--list`) and SHALL add `--seed N`, `--show <name>`, `--params <json>`, `--simulator-binary <path>`, and `--all <output-dir>`.

#### Scenario: Default invocation builds and runs

- **WHEN** the user runs `python3 scripts/wave_show.py` with no prior build
- **THEN** the script invokes `pio run -e native_show_sim`
- **THEN** on successful build, it invokes `.pio/build/native_show_sim/program` with the matching args
- **THEN** on successful run, it writes `wave_show.png` of the expected dimensions

#### Scenario: Existing CLI flags keep working

- **WHEN** the user runs `python3 scripts/wave_show.py --decay-rate 1.5 --brightness-frequency 0.2 -o preview.png`
- **THEN** the output PNG matches the Wave show with those parameters, byte-for-byte across runs

#### Scenario: --all renders every show

- **WHEN** the user runs `python3 scripts/wave_show.py --all ./previews --seed 42`
- **THEN** the directory `./previews/` contains one PNG per show currently registered in `ShowFactory`
- **THEN** every PNG is byte-for-byte identical to a fresh render with the same args

### Requirement: GitHub Pages gallery regeneration

`.github/workflows/pages.yml` SHALL run on push to `main` and on `workflow_dispatch`. The workflow SHALL build the simulator, invoke the renderer in `--all` mode into `docs/show_previews/`, generate `docs/show_previews/index.html` listing each preview, and deploy `docs/` via `actions/deploy-pages@v4`. The workflow SHALL NOT run on pull requests.

#### Scenario: Push to main deploys the gallery

- **WHEN** a commit is pushed to `main`
- **THEN** `.github/workflows/pages.yml` runs
- **THEN** on success, `docs/show_previews/` contains one PNG per registered show
- **THEN** on success, the Pages site at the repository's GitHub Pages URL is updated

#### Scenario: Manual re-render with a different seed

- **WHEN** a maintainer triggers `workflow_dispatch` with input `seed=99`
- **THEN** the workflow builds, renders with `--seed 99`, and deploys
- **THEN** the resulting PNGs differ from the previous seed-42 render in any random-show entries only

#### Scenario: Pull request does not deploy

- **WHEN** a pull request is opened against `main`
- **THEN** `.github/workflows/pages.yml` does not run (no `pull_request` trigger)
