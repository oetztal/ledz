## Context

Today the gallery pipeline runs every variant of every registered show at
exactly `iterations=1000` — the only knob is the `--iterations` CLI flag,
which is global. `scripts/show_variants.json` is purely a parameter-set
manifest: per-variant `{name, label, params}` plus a per-show `description`.
Six of the registered shows (`Solid`, `Fire`, `Stroboscope`, `Rainbow`,
`TheaterChase`, `MorseCode`) don't need 1000 rows to communicate what they
do, and the gallery currently wastes render time and PNG bytes on them.

The simulator binary already accepts a per-call `--iterations` count, so the
renderer can drive each variant with its own row count without any change to
the host-side `MockStrip` or the C++ show code. The change is scoped to the
renderer and the JSON manifest: extend the schema, extend the dataclass,
thread the resolved value through to the existing `--iterations` flag, and
stamp `iterations: 300` on the six shows.

## Goals / Non-Goals

**Goals:**

- Add an optional `iterations` override at both the show body level and the
  variant entry level of `scripts/show_variants.json`.
- Resolution order: per-variant → per-show → `--iterations` CLI default.
- Render the six mentioned shows at 300 rows on the next `--all` run.
- Keep existing JSON files that omit the field behaving exactly as today.

**Non-Goals:**

- Per-show or per-variant `width` override (different concern, out of scope).
- Changing the simulator binary's CLI or wire format.
- Changing the firmware (`src/show/**`, `src/strip/**`).
- Pixel-level verification of the regenerated PNGs in CI.

## Decisions

### Decision 1: Field name is `iterations`

Mirrors the existing `--iterations` CLI flag, the `RenderParams.iterations`
dataclass field, and the simulator's `--iterations` option. Aligns the JSON
manifest vocabulary with the Python dataclass vocabulary, which is what the
field actually controls.

*Alternative considered*: `height` (matches the PNG dimension and casual
phrasing) — rejected because it would create vocabulary drift between the
JSON, the dataclass, and the CLI flag for the same concept.

### Decision 2: Per-show level is the primary home; per-variant overrides

For the six shows in scope, every variant shares the same value (300), so
putting `iterations` at the show body is the least repetitive shape.
Per-variant is supported as an escape hatch for future use — but no current
variant exercises it. Resolution: variant field → show field → CLI default.

*Alternative considered*: per-variant only — rejected because it forces the
same integer to be repeated once per variant (10–12 redundant entries) for
no current benefit.

### Decision 3: `Variant.iterations` is `int | None`, never inherited at construction time

`load_variants()` returns plain `Variant` objects with `iterations=None` when
neither level sets the field. Resolution happens at the call site in
`render_all()` (single line: `variant.iterations if variant.iterations is not None else params.iterations`).
This keeps the dataclass dumb and avoids a second lookup table inside
`load_variants()`.

*Alternative considered*: resolve during `load_variants()` and store the
final int — rejected because it makes the override invisible to callers and
would prevent future logging/diagnostics that want to distinguish "set in
JSON" from "fell back to default".

### Decision 4: No validation beyond what `_validate` already enforces

`_validate` already rejects `--iterations < 1`. Reusing the same check via
the existing `RenderParams` pipeline is enough; we don't add a JSON-level
schema check for the new field. The cost of a typo (e.g. `"iterations": "300"`)
is a `TypeError` at `RenderParams` construction, which surfaces immediately
on the first `--all` run.

*Alternative considered*: introduce a JSON schema for `show_variants.json`
— rejected; the current file is intentionally schema-free and a separate
enhancement.

### Decision 5: Simulator binary stays untouched

`scripts/show_simulator/main.cpp` already accepts `--iterations N` per
invocation (`Options::iterations`, default `kDefaultIterations = 1000`). The
renderer just passes `variant.iterations` (or the global fallback) through
the existing argv slot. No rebuild of the binary is required for the
renderer change to take effect; the binary only needs to exist (and it does
in CI).

## Risks / Trade-offs

- **[Risk]** Gallery PNGs become heterogeneous in height: six shows render
  at 300×300, the rest stay at 300×1000. → **Mitigation**: the gallery CSS
  already uses `height: auto; image-rendering: pixelated`, so the responsive
  grid absorbs the variation without a layout change. A one-line note in
  `docs/SHOW_PREVIEWS.md` flags it for future readers.

- **[Risk]** A future contributor omits the new field on a variant that
  *should* have it (e.g. someone adds a new show and forgets they could
  opt into a shorter preview). → **Mitigation**: the field is optional and
  defaults to the global `iterations`, so the omission is harmless — every
  show still gets a preview, just at the default height.

- **[Risk]** A typo in the JSON (e.g. `"iterations": "300"` as a string)
  blows up at `RenderParams` construction rather than with a helpful
  message. → **Mitigation**: the failure mode is a stack trace from the
  renderer pointing at the offending variant; this is consistent with how
  every other malformed variant param already surfaces today.

- **[Risk]** MorseCode's `HELLO WORLD` message takes longer than 3 s to fully
  scroll, so the 300-row preview shows only the first part. → **Mitigation**:
  acknowledged in `docs/SHOW_PREVIEWS.md` so the truncation isn't mistaken
  for a bug. The SOS variant (3 letters) fits comfortably.

## Migration Plan

No schema migration is needed; the new field is optional and additive.

1. Land the renderer change (`scripts/build_pages.py`) and the JSON change
   (`scripts/show_variants.json`) in one commit.
2. Regenerate the gallery locally:
   `python3 scripts/build_pages.py --all docs/show_previews --seed 42 --landing`.
3. Commit the regenerated PNGs and updated `docs/show_previews/index.html`.
4. Push; `pages.yml` will re-run the renderer on `main` regardless.
5. Rollback is a single revert: the JSON entries and the renderer change are
   co-located in two files, and the regenerated PNGs are reproducible from
   any state of those files.

## Open Questions

None at submit time.
