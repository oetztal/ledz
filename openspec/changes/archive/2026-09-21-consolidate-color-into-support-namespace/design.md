## Context

`src/color.{h,cpp}` declares and defines `wheel(float)`, `color(r,g,b)`, `red(color)`, `green(color)`, `blue(color)` in the global namespace. `src/support/color.{h,cpp}` declares and defines `Support::Color::black_body_color(float)`. Both files exist simultaneously and share the same base filename.

The codebase's established pattern is:

```
src/support/
├── Gamma.{h,cpp}              — non-trivial utility
├── LocalTime.{h,cpp}          — non-trivial utility
├── Palette.{h,cpp}            — non-trivial utility
├── SmoothBlend.{h,cpp}        — non-trivial utility
├── WiFiCredentials.{h,cpp}    — non-trivial utility
└── color.{h,cpp}               — Support::Color::black_body_color
```

Every non-trivial utility lives under `src/support/`. The root `src/color.h` is the lone exception — it sits in the global namespace despite housing a 30-line cube-walk that is functionally more complex than `SmoothBlend` or `Palette`. The trivial packers/extractors (`color`, `red`, `green`, `blue`) are global because they pre-date the `Support::Color` namespace convention, but they share the same "color utility" purpose.

Sixteen files currently `#include "color.h"`. After consolidation they will `#include "support/color.h"` and prefix every call with `Support::Color::`. The function `color()` is renamed to `from_rgb()` because `Support::Color::color()` reads as a constructor call and is misleading.

## Goals / Non-Goals

**Goals:**
- One home for color utilities: `src/support/color.{h,cpp}` declaring and defining all six functions (`wheel`, `from_rgb`, `red`, `green`, `blue`, `black_body_color`).
- All color helpers reachable via a single qualified namespace prefix, matching the convention used by every other non-trivial utility.
- Behaviour byte-identical to before the refactor — every pixel on every show renders the same RGB.

**Non-Goals:**
- Changing the behaviour of any color helper. `wheel()` keeps its float signature, NaN→black, wrap, clamp, byte-identical-at-integer-inputs contract. `black_body_color` keeps its existing algorithm.
- Adding new color helpers.
- Touching the existing `color-wheel` spec beyond what the namespace move requires (the five behavioural requirements are MODIFIED only to update `wheel()` → `Support::Color::wheel()`; no behavioural change).
- Renaming the `color-wheel` spec. It remains focused on `wheel`; the new namespace requirement lives alongside, not as a replacement.

## Decisions

### Decision 1: Move everything, don't keep the trivial helpers at the root

**Rationale:** Splitting trivial helpers (`from_rgb`, `red`, `green`, `blue`) into the root and the non-trivial ones (`wheel`, `black_body_color`) into `Support::Color` perpetuates the two-files-with-the-same-name confusion. The trivial helpers are 1–4 line functions; pulling them into `Support::Color` alongside the others costs nothing at call sites (every call already gets a `Support::Color::` prefix from being in the namespace) and the result is "one place to find color utilities."

**Alternatives considered:**
- Move only `wheel()`; leave trivial helpers in `src/color.{h,cpp}`. Rejected: leaves the dual-`color.cpp` confusion in place for the trivial helpers.
- Move the trivial helpers and `black_body_color` *up* to `src/color.cpp`, deleting `src/support/color.{h,cpp}`. Rejected: inverts the established pattern where advanced helpers live in `support/`.

### Decision 2: Rename `color` → `from_rgb`, keep `red`/`green`/`blue`/`wheel` as-is

**Rationale:** `Support::Color::color(r, g, b)` reads as "the `color` function in the `Color` namespace" — which is misleading because it sounds like a constructor for a `Color` type (we have a `Strip::Color` typedef elsewhere). `from_rgb` is the conventional name in image/color libraries (Godot's `Color8::from_rgba8`, Skia's `SkColorSetRGB`, etc.) and reads cleanly: "construct a color from RGB components."

`red`, `green`, `blue` extractors are fine as-is — `Support::Color::red(c)` reads as "the red component of color c," no ambiguity. `wheel` is fine as-is — the cube-walk name is the established term in NeoPixel-land and reads cleanly with the namespace prefix.

**Alternatives considered:**
- Rename `color` → `rgb`. Rejected: ambiguous (could be a getter or a setter).
- Rename `color` → `pack`. Rejected: less idiomatic; "from_rgb" reads as constructing, "pack" reads as low-level bit packing.
- Keep `color` as-is. Rejected: `Support::Color::color(...)` reads as a constructor.
- Delete `color()` entirely, inline the bit shifts at every call site. Rejected: ~15 call sites; the function adds clarity.

### Decision 3: Update test calls to use the qualified prefix; tests pin the qualified contract

**Rationale:** The `test_color` suite already has eight `wheel` tests. After the move, each becomes `Support::Color::wheel(...)`. The `test_wheel_byte_identical_at_integer_inputs` loop becomes `Support::Color::wheel((float)h)` — same behaviour, same coverage, new prefix. Tests stay machine-checked; the prefix becomes part of the contract pinned by them.

The spec's new ADDED scenario "Call sites use the qualified name" codifies that the unqualified `wheel()` SHALL NOT appear at call sites — this prevents a future PR from introducing `using namespace Support::Color;` and quietly undoing the namespace hygiene.

### Decision 4: Delete `src/color.{h,cpp}` rather than leaving them as forwarding shims

**Rationale:** Keeping the root `color.h` as `#include "support/color.h" using Support::Color::wheel;` etc. would preserve source compatibility for any external code that imports ledz headers (none today, but hypothetically). Cost: an extra 5 lines per file, two files to keep in sync, and `using` declarations that pollute the global namespace. Benefit: zero — no external consumer exists.

**Alternatives considered:**
- Keep `src/color.h` as a shim with `using Support::Color::wheel;` etc. Rejected: pure indirection cost for no current consumer.
- Keep `src/color.h` as `#include "support/color.h"` only (no using declarations). Rejected: every call site still needs `Support::Color::` prefix, so the shim adds no value.

### Decision 5: Apply order — header first, then callers, then delete root files

**Rationale:** Once `support/color.h` declares the new functions, every caller can be updated independently and the build will fail only at the final delete. In practice the cleanest order is:

1. Add the five new declarations to `src/support/color.h` and definitions to `src/support/color.cpp`.
2. Update every `#include "color.h"` to `#include "support/color.h"` and prefix every call.
3. Delete `src/color.{h,cpp}` once no file references them.

After step 1, the build is temporarily broken (caller includes `color.h` but uses unqualified names; new declarations don't conflict because the names live in a namespace). After step 2, the build is green. After step 3, the tree has one fewer file.

## Risks / Trade-offs

- **[16 files get touched]** → A trivial mechanical refactor; risk is typos in the prefix substitution. *Mitigation: full native test suite (134 cases) must remain green; the byte-identical-at-integer-inputs test catches any silent body change; visual diff with the prior commit for show output is optional belt-and-braces.*
- **[`wheel()` no longer reachable as unqualified]** → Any future code that expected `wheel()` in the global namespace will fail to compile. There is no such code today. *Mitigation: the spec's ADDED "Call sites use the qualified name" scenario; grep for `using namespace Support::Color` in PR review.*
- **[More verbose call sites]** → `Support::Color::wheel(hue)` is 12 characters longer than `wheel(hue)`; over ~30 call sites that's ~360 characters of source noise. *Mitigation: this is the established convention for every other non-trivial utility in the codebase; brevity is not a goal.*
- **[Risk of misnaming during refactor]** → `red`, `green`, `blue` extractors and the new `from_rgb` are short and similar to colour-pseudocode; a global search-replace might catch `color` in unrelated places. *Mitigation: prefer per-file targeted edits over a global sed; the test suite catches any false-positive substitution.*
- **[One commit touches 16 files + deletes 2]** → Larger diff than the average ledz change. *Mitigation: this is the natural shape of a a refactor that consolidates; the commit is well-bounded (one concern: namespace consolidation).*
- **[Old `src/color.h` may have been consumed by other libraries/tools]** → None known. *Mitigation: a grep over the whole tree (not just `src/` and `test/`) confirms there are no other consumers.*

## Migration Plan

This is a single-step change: rename, move, delete, update. No phased rollout.

- **Apply order**: support header → support impl → 16 callers → delete root files → run tests.
- **Rollback**: `git revert` the commit. The pre-refactor files are preserved in git history.
- **Verification**:
  - `pio test -e native` — 134/134 must remain green.
  - `git grep "include \"color.h\""` returns zero matches outside `openspec/`.
  - `git grep "Support::Color::wheel\|Support::Color::from_rgb\|Support::Color::red\|Support::Color::green\|Support::Color::blue\|Support::Color::black_body_color"` returns matches only in `src/` and `test/` (no matches in archive or openspec).
  - `grep -r "using namespace Support::Color" src/ test/` returns zero matches.

## Open Questions

None. All design choices (move everything, rename `color` → `from_rgb`, qualified-prefix call sites, delete root files, apply order) are settled. The user's selection (`Move everything` + `from_rgb`) is encoded directly in the design above.
