## MODIFIED Requirements

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