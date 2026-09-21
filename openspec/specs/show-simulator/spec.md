# show-simulator Specification

## Purpose

The show simulator is a host-side CLI binary that drives any registered show against a `MockStrip` for `N` iterations and emits the resulting strip state as a raw RGB stream on stdout. It exists so that the same C++ source that runs on the device can be used to preview show parameter sets and to populate the GitHub Pages gallery of show previews, without maintaining a parallel Python port.

## Requirements

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

`scripts/build_pages.py` SHALL replace its hand-written Python simulation with a call to the simulator binary. The script SHALL auto-build the binary via `pio run -e native_show_sim` if `.pio/build/native_show_sim/program` is missing. The script SHALL keep its existing CLI surface (`-n`/`--iterations`, `-W`/`--width`, `-o`/`--output`, `--wave`, `--decay-rate`, `--brightness-frequency`, `--mode`, `-v`, `--list`) and SHALL add `--seed N`, `--show <name>`, `--params <json>`, `--simulator-binary <path>`, and `--all <output-dir>`.

#### Scenario: Default invocation builds and runs

- **WHEN** the user runs `python3 scripts/build_pages.py` with no prior build
- **THEN** the script invokes `pio run -e native_show_sim`
- **THEN** on successful build, it invokes `.pio/build/native_show_sim/program` with the matching args
- **THEN** on successful run, it writes `show_preview.png` of the expected dimensions

#### Scenario: Existing CLI flags keep working

- **WHEN** the user runs `python3 scripts/build_pages.py --decay-rate 1.5 --brightness-frequency 0.2 -o preview.png`
- **THEN** the output PNG matches the Wave show with those parameters, byte-for-byte across runs

#### Scenario: --all renders every show

- **WHEN** the user runs `python3 scripts/build_pages.py --all ./previews --seed 42`
- **THEN** the directory `./previews/` contains one PNG per show currently registered in `ShowFactory`
- **THEN** every PNG is byte-for-byte identical to a fresh render with the same args

### Requirement: Variants manifest drives the gallery

`scripts/build_pages.py --all` SHALL consult a variants manifest at `scripts/show_variants.json` to decide which parameter sets to render for each registered show. The manifest SHALL be a JSON object keyed by show name, where each value carries a `description`, an optional `iterations` override (positive integer), and a `variants` array. Each variant entry SHALL have a unique `name`, a human-readable `label`, an optional `iterations` override (positive integer) that takes precedence over the show-level value, and a `params` object that is passed verbatim to the simulator via `--params`. When neither the variant entry nor the show body specifies `iterations`, the renderer SHALL fall back to the `--iterations` CLI flag's value (default 1000). A show that is omitted from the manifest SHALL still render a single default variant with empty parameters so the gallery stays complete.

#### Scenario: Variant filename and image

- **WHEN** the renderer encounters variant `{name: "tight", label: "Tight fast", params: {...}}` for show `Wave`
- **THEN** it writes `Wave_tight.png` next to the other previews
- **THEN** the rendered pixels match a `--show Wave --params <params>` simulator invocation

#### Scenario: Show missing from the manifest

- **WHEN** the renderer is asked for a show that is not in `scripts/show_variants.json`
- **THEN** it writes `{show}_default.png` produced by `--params "{}"`
- **THEN** the gallery still lists that show with a "Default" label

#### Scenario: Adding a variant changes only the gallery output

- **WHEN** a new variant entry is added to `scripts/show_variants.json`
- **THEN** the next `--all` run produces one additional PNG and an updated `index.html`
- **THEN** no source-code change to the simulator, the firmware, or the workflow is required

#### Scenario: Show-level iterations overrides the CLI default

- **WHEN** the manifest defines `Solid` with `"iterations": 300` at the show body
- **THEN** every `Solid_*` preview is written as a 300×300 PNG
- **THEN** the simulator is invoked with `--iterations 300` for each of those variants
- **THEN** the global `--iterations` CLI flag's value is ignored for those variants

#### Scenario: Variant-level iterations overrides the show body

- **WHEN** a variant entry of show `Solid` carries its own `"iterations": 50`
- **THEN** that single variant renders at 50 rows
- **THEN** other variants of `Solid` (without the field) still inherit the show body's `iterations` value

#### Scenario: Absent iterations falls through to the CLI default

- **WHEN** neither the variant entry nor the show body specifies `iterations`
- **THEN** the renderer uses the value passed to `--iterations` on the CLI (default 1000)
- **THEN** behavior is identical to a manifest without the new field

### Requirement: Gallery page embeds every preview inline

`docs/show_previews/index.html` SHALL embed every preview directly in the page (via `<img>` tags pointing at sibling PNGs), grouped into one section per show. Each variant block SHALL carry the variant label as a heading, the preview image, and the exact JSON parameters that produced it. The page SHALL include a table-of-contents anchored to each show section.

#### Scenario: Gallery groups by show

- **WHEN** the manifest defines two variants for `Wave` and one default for `ColorRun`
- **THEN** `docs/show_previews/index.html` contains one section per registered show
- **THEN** each section lists every variant of that show underneath, in the order they appear in the manifest

#### Scenario: Gallery parameters are reproducible

- **WHEN** a reader expands the "Parameters" disclosure on a variant
- **THEN** they see a JSON object that, when passed to the simulator, reproduces the pixels of the preview byte-for-byte

### Requirement: Site landing page rendered from README.md

When invoked with `--landing`, `scripts/build_pages.py` SHALL also render `docs/index.html` from `README.md` using the `markdown` Python package (with `tables`, `fenced_code`, `sane_lists`, and `nl2br` extensions). The landing page SHALL include navigation cards that link to the gallery and to every other `docs/*.md` page, and SHALL append the converted README content underneath those cards.

#### Scenario: Landing page renders README content

- **WHEN** the script is invoked with `--all docs/show_previews --landing --readme README.md`
- **THEN** `docs/index.html` is written
- **THEN** its `<article>` contains the headings, lists, tables, and code blocks from `README.md`

#### Scenario: Doc pages rendered alongside the landing

- **WHEN** `--landing` is set
- **THEN** the script also writes `{stem}.html` next to every `docs/*.md` file other than `README.md`
- **THEN** each generated HTML page renders the corresponding Markdown with the same template as the landing page

### Requirement: GitHub Pages gallery regeneration

`.github/workflows/pages.yml` SHALL run on push to `main` and on `workflow_dispatch`. The workflow SHALL build the simulator, invoke the renderer in `--all` mode (with `--landing`) into `docs/`, and deploy `docs/` via `actions/deploy-pages@v4`. The workflow SHALL install the `markdown` Python package alongside PlatformIO. The workflow SHALL NOT run on pull requests.

#### Scenario: Push to main deploys the gallery

- **WHEN** a commit is pushed to `main`
- **THEN** `.github/workflows/pages.yml` runs
- **THEN** on success, `docs/show_previews/` contains one PNG per (show, variant) pair from the manifest
- **THEN** on success, `docs/index.html` and a `{stem}.html` next to each `docs/*.md` are present
- **THEN** on success, the Pages site at the repository's GitHub Pages URL is updated

#### Scenario: Manual re-render with a different seed

- **WHEN** a maintainer triggers `workflow_dispatch` with input `seed=99`
- **THEN** the workflow builds, renders with `--seed 99`, and deploys
- **THEN** the resulting PNGs differ from the previous seed-42 render in any random-show entries only

#### Scenario: Pull request does not deploy

- **WHEN** a pull request is opened against `main`
- **THEN** `.github/workflows/pages.yml` does not run (no `pull_request` trigger)