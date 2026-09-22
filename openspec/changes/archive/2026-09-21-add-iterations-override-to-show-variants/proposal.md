## Why

Every preview in `docs/show_previews/` is rendered at 1000 iterations (≈10 s of
animation) regardless of how much vertical resolution the show actually needs.
For shows whose pattern repeats inside a short window — Solid, Fire,
Stroboscope, Rainbow, TheaterChase, MorseCode — the resulting 300×1000 PNGs
are pointlessly tall: slower to render, slower to load in the gallery, and
visually no more informative than a 300×300 slice. Letting the variants
manifest cap the row count per show keeps the gallery lightweight without
forcing a separate configuration file or touching the simulator binary.

## What Changes

- Add an optional `iterations` field at both the show body level and the
  variant entry level in `scripts/show_variants.json`. Resolution order is
  per-variant → per-show → CLI `--iterations` default (1000).
- Apply `iterations: 300` to the show bodies of Solid, Fire, Stroboscope,
  Rainbow, TheaterChase, and MorseCode. All of their variants inherit the
  value; no per-variant override is needed today.
- Extend the `Variant` dataclass and `load_variants()` in
  `scripts/build_pages.py` to read the new field; update `render_all()` to
  pass the resolved row count into each per-variant `RenderParams`.
- Document the new field in `docs/SHOW_PREVIEWS.md` (the "Adding a variant"
  section and the flags table).
- Refresh the existing PNGs in `docs/show_previews/` to their new dimensions
  by re-running the renderer. The simulator binary itself is unchanged.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `show-simulator`: The variants-manifest requirement gains support for an
  optional `iterations` override at both the show body and the variant entry
  level, with per-variant taking precedence over per-show.

## Impact

- `scripts/show_variants.json` — six new lines, no structural change.
- `scripts/build_pages.py` — additive change to `Variant`, `load_variants()`,
  and the per-variant `RenderParams` construction in `render_all()`. No
  public CLI flag changes; `--iterations` keeps its current meaning.
- `scripts/show_simulator/main.cpp` — unchanged (already accepts per-call
  `--iterations`).
- `docs/SHOW_PREVIEWS.md` — one paragraph in "Adding a variant" plus a row
  in the flags table.
- `openspec/specs/show-simulator/spec.md` — add a scenario under the
  "Variants manifest drives the gallery" requirement covering the override.
- `docs/show_previews/*.png` — six shows' previews shrink from 300×1000 to
  300×300, and the affected `index.html` is regenerated. The gallery page
  uses `height: auto` + `image-rendering: pixelated` so layout adapts
  automatically; intrinsic `<img>` heights just become heterogeneous across
  sections.
