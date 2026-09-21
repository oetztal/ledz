# Show Previews Gallery

Every show that the firmware supports has a preview rendered as a PNG.
Previews live in `docs/show_previews/` and are published to the repository's
GitHub Pages site.

## What the previews are

Each preview is a width×iterations grid (default 300×1000, configurable via
CLI flags) where:

- the **x-axis** is the LED index (0 … N-1) — what the strip would show
- the **y-axis** is the iteration count in 10 ms steps — the time axis
- each pixel is the `(r, g, b)` value the show would write to that LED at
  that iteration on the real device

## Where they come from

The previews are produced by the same C++ source that runs on the device:

1. `pio run -e native_show_sim` builds the host-side simulator binary at
   `.pio/build/native_show_sim/program`. It links every show in
   `src/show/*.cpp` and a host-only `MockStrip` from `scripts/show_simulator/`.
2. For each registered show the renderer invokes the simulator with
   `--show <name> --width … --iterations … --params "{}"`. The simulator
   drives the show for the requested number of iterations and writes
   `width * iterations * 3` raw RGB bytes to stdout.
3. `scripts/wave_show.py` reads that stream and wraps it into a PNG.

There is no parallel Python implementation of any show. Adding a new show to
`ShowFactory` automatically gives it a preview on the next gallery
regeneration with zero extra work in this repository — though the simulator
binary's parameter key list (`scripts/show_simulator/main.cpp`) does need a
new entry so `--list-params` reflects it.

## Determinism

Random shows (Fire, Starlight, ColorRun) take a `--seed N` flag that the
simulator passes to `Support::setRandomSeedOverride()` before constructing
the show. Solid/ColorRanges uses a wall-clock-driven `SmoothBlend`; the
simulator replaces the host `millis()` with a deterministic counter that
advances 10 ms per iteration. As a result, every preview is byte-identical
across runs when the same `--seed` is supplied.

## Regenerating locally

```bash
pio run -e native_show_sim
python3 scripts/wave_show.py --all ./out --seed 42
```

`./out/` will contain one PNG per registered show plus an `index.html`
linking them.

Common flags:

| Flag                  | Default       | Purpose                                |
|-----------------------|---------------|----------------------------------------|
| `--width` / `-W`      | 300           | Image width in pixels (LED count)      |
| `--iterations` / `-n` | 1000          | Number of rows (each row = 10 ms)      |
| `--seed`              | (none)        | RNG seed for random shows              |
| `--show`              | `Wave`        | Single-show name to render             |
| `--output` / `-o`     | `wave_show.png` | Output PNG path                      |

## Gallery regeneration via GitHub Actions

`.github/workflows/pages.yml` runs on every push to `main` and on manual
`workflow_dispatch`. It rebuilds the simulator, renders the gallery into
`docs/show_previews/`, and deploys `docs/` to GitHub Pages.

### Repository setting prerequisite

The workflow uses `actions/deploy-pages@v4`, which requires that GitHub
Pages be configured to deploy from "GitHub Actions" rather than from a
specific branch. If the deploy step fails with:

```
Failed to create deployment (status: 403)
```

… that almost always means the repository setting has not been toggled. Fix:

1. Go to the repository's **Settings** page.
2. Open **Pages** in the sidebar.
3. Under **Source**, pick **GitHub Actions** (not "Deploy from a branch").
4. Save.

The build itself still succeeds even when deploy is disabled, so the
preview files end up committed in the workflow run's artifacts.
