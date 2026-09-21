## 1. Renderer changes

- [x] 1.1 Add `iterations: int | None = None` field to the `Variant` dataclass in `scripts/build_pages.py`
- [x] 1.2 Update `load_variants()` to read the show body's `iterations` and pass it down; for each variant, the resolved value is `variant["iterations"] if present else body["iterations"] else None`
- [x] 1.3 Update `render_all()` to construct each per-variant `RenderParams` with `iterations = variant.iterations if variant.iterations is not None else params.iterations`
- [x] 1.4 Verify `_validate()` still rejects `iterations < 1` for both the CLI path and the per-variant path (no change needed if `RenderParams` is the single entry point)

## 2. Variants manifest

- [x] 2.1 Add `"iterations": 300` to the `Solid` show body in `scripts/show_variants.json`
- [x] 2.2 Add `"iterations": 300` to the `Fire` show body
- [x] 2.3 Add `"iterations": 300` to the `Stroboscope` show body
- [x] 2.4 Add `"iterations": 300` to the `Rainbow` show body
- [x] 2.5 Add `"iterations": 300` to the `TheaterChase` show body
- [x] 2.6 Add `"iterations": 300` to the `MorseCode` show body

## 3. Documentation

- [x] 3.1 Update `docs/SHOW_PREVIEWS.md` "Adding a variant" section with a short note that `iterations` may be set at the show body or variant entry level, defaulting to the CLI value
- [x] 3.2 Add a sentence in `docs/SHOW_PREVIEWS.md` acknowledging that affected shows now render at 300×300 and that `MorseCode`'s long-message variants are intentionally truncated to a 3 s slice
- [x] 3.3 Update the `_comment` line at the top of `scripts/show_variants.json` to mention the new optional `iterations` field

## 4. Regenerate and verify

- [x] 4.1 Run `python3 scripts/build_pages.py --all docs/show_previews --seed 42 --landing` from the repository root
- [x] 4.2 Confirm `docs/show_previews/Solid_*.png`, `Fire_*.png`, `Stroboscope_*.png`, `Rainbow_*.png`, `TheaterChase_*.png`, `MorseCode_*.png` are all `300 x 300` (`file docs/show_previews/<show>_*.png`)
- [x] 4.3 Confirm every other preview PNG remains `300 x 1000`
- [x] 4.4 Spot-check `docs/show_previews/index.html` for the six affected shows — labels and dimensions should match expectations, layout still adapts via the existing CSS

## 5. Wrap-up

- [x] 5.1 Commit the renderer change, the JSON change, the doc change, and the regenerated gallery assets in one PR
- [x] 5.2 Push and let `pages.yml` re-run the renderer on `main` as a smoke test