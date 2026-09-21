# Show Previews Gallery

Every show that the firmware supports has at least one preview rendered as a
PNG. Previews live in `docs/show_previews/` and are published to the
repository's GitHub Pages site.

## What the previews are

Each preview is a width×iterations grid (default 300×1000, configurable via
CLI flags) where:

- the **x-axis** is the LED index (0 … N-1) — what the strip would show
- the **y-axis** is the iteration count in 10 ms steps — the time axis
- each pixel is the `(r, g, b)` value the show would write to that LED at
  that iteration on the real device

## Gallery layout

`docs/show_previews/index.html` embeds every preview directly in the page
(via `<img>` tags pointing at sibling PNG files), grouped into one section
per show. Each variant block carries a short label and the exact JSON
parameters used to render it, so the gallery is both a look-book and a
recipe reference.

Adding a new variant is a single edit to `scripts/show_variants.json` — the
next regeneration picks it up automatically. A show that is omitted from the
variants file still gets a "default" preview rendered with empty
parameters, so the gallery is always complete.

## Where they come from

The previews are produced by the same C++ source that runs on the device:

1. `pio run -e native_show_sim` builds the host-side simulator binary at
   `.pio/build/native_show_sim/program`. It links every show in
   `src/show/*.cpp` and a host-only `MockStrip` from `scripts/show_simulator/`.
2. For every `(show, variant)` pair in `scripts/show_variants.json` (and for
   one default variant per registered show), the renderer invokes the
   simulator with `--show <name> --width … --iterations … --params <json>`.
   The simulator drives the show for the requested number of iterations and
   writes `width * iterations * 3` raw RGB bytes to stdout.
3. `scripts/build_pages.py` reads that stream and wraps it into a PNG named
   `{show}_{variant}.png`.

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
python3 scripts/build_pages.py --all docs/show_previews --seed 42
```

`docs/show_previews/` will contain one PNG per variant per registered show,
plus `index.html` linking them.

To also render the site landing page and HTML versions of every
`docs/*.md`, pass `--landing` (the GitHub Actions workflow does this
automatically):

```bash
python3 scripts/build_pages.py \
    --all docs/show_previews \
    --seed 42 \
    --landing \
    --readme README.md
```

Common flags:

| Flag                  | Default       | Purpose                                |
|-----------------------|---------------|----------------------------------------|
| `--width` / `-W`      | 300           | Image width in pixels (LED count)      |
| `--iterations` / `-n` | 1000          | Number of rows (each row = 10 ms)      |
| `--seed`              | (none)        | RNG seed for random shows              |
| `--show`              | `Wave`        | Single-show name to render             |
| `--output` / `-o`     | `show_preview.png` | Output PNG path                   |
| `--variants-file`     | `scripts/show_variants.json` | Variants manifest |
| `--all`               | (none)        | Render every variant into DIR          |
| `--landing`           | off           | Also write `index.html` and HTML docs  |
| `--readme`            | `README.md`   | Source for `--landing`                 |

## Adding a variant

Open `scripts/show_variants.json` and append a new entry under the show you
want to extend:

```json
"Wave": {
  "description": "Cosine-bouncing rainbow source with exponential brightness decay",
  "variants": [
    {"name": "default", "label": "Default bounce", "params": {"mode": "bounce", "decay_rate": 2.0, "brightness_frequency": 0.1}},
    {"name": "tight",   "label": "Tight fast",     "params": {"mode": "bounce", "decay_rate": 4.0, "brightness_frequency": 0.4}}
  ]
}
```

Re-run `python3 scripts/build_pages.py --all docs/show_previews` and a new
`Wave_tight.png` plus an updated `index.html` appear in the gallery.

## Gallery regeneration via GitHub Actions

`.github/workflows/pages.yml` runs on every push to `main` and on manual
`workflow_dispatch`. It rebuilds the simulator, renders the gallery into
`docs/show_previews/`, renders `docs/index.html` from `README.md`, renders
the other `docs/*.md` files as HTML, and deploys `docs/` to GitHub Pages.

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
