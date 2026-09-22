## Context

`scripts/show_variants.json` carries `MorseCode.default.params.message = "HELLO WORLD"` (set by the prior `canonical-show-config-file` change). Three other locations still carry drifted strings:

| Location | Current literal | Reference |
|---|---|---|
| `src/show/factory/ShowFactory.cpp:105` | `"HELLO"` | C++ `\|` fallback for `{}` |
| `src/show/MorseCode.h:42` | `"HELLO WORLD!"` | Constructor default argument |
| `data/control.html:277` | `"HELLO WORLD!"` | `<input>` `value` attribute |

`ShowFactory` is the only caller of `MorseCode`'s constructor and always passes the message explicitly (from `doc["message"]`). The constructor default in `MorseCode.h:42` is never reached at runtime. The `<input>` value in `control.html` is reached when the user opens the control page, is reset to defaults, or reloads.

`scripts/compress_web.py` is registered as a PlatformIO `pre:` script in `platformio.ini:65`. It reads `data/*.html`, minifies + gzips, and writes `src/generated/<name>_gz.h`. So `data/control.html:277` reaches the firmware via the generated `control_gz.h`, regenerated automatically on every build.

## Goals / Non-Goals

**Goals:**

- Make all four locations declare the identical string `"HELLO WORLD"`.
- Keep the change to literal substitutions — no shared constant, no helper, no refactor.
- Keep the runtime contract: every `POST /api/show {name, params}` that produced a particular strip state before this change produces the same state after, with the single exception of `{"name":"MorseCode","params":{}}` which now scrolls `"HELLO WORLD"` instead of `"HELLO"`.

**Non-Goals:**

- Hoisting `"HELLO WORLD"` into a shared constant (e.g. `src/show/MorseCodeDefaults.h`). The string is plain ASCII, three call sites, and a shared constant would couple `ShowFactory.cpp`, `MorseCode.h`, and `control.html` to a new header — `control.html` is HTML, not C++, so it could not consume the constant anyway. Three independent literals are simpler.
- Removing the constructor default argument from `MorseCode.h:42`. It is dead code today, but removing it is a separate refactor; this change only corrects the literal.
- Removing `ShowFactory.cpp`'s `|` fallback chain. That is the next major step (follow-up change "consume canonical config in C++") which will replace the JSON-parsing path with a generated-header lookup; the `|` literals come out then.
- Generating `data/control.html`'s Morse `value` attribute from `scripts/show_variants.json` via `build_pages.py`. Also a follow-up change.
- Touching `src/TouchController.cpp`'s Morse variants (`{"message":"foo bar baz"}`, `{"message":"gutes neues"}`). They are touch-specific curated messages, out of scope per the user's call.

## Decisions

### Three independent literal edits, no shared constant

The simplest path is the right path here. The four locations are:

- one C++ source file (`ShowFactory.cpp`)
- one C++ header (`MorseCode.h`)
- one HTML file (`control.html`)
- one JSON file (`show_variants.json`, already correct)

A shared constant in `MorseCode.h` would be reachable by the C++ source and header, but not by the HTML. The HTML would still be a hand-maintained literal that drifts whenever someone edits the JSON. Three independent edits, each manually verified against the JSON, is the same maintenance burden as one shared constant for the firmware, and avoids the new header dependency entirely.

**Alternatives considered:**

- *Shared C++ constant + JSON+HTML manual sync*: rejected — see above; does not reduce maintenance.
- *A `morse_default_message` constant in `MorseCode.h` consumed only by `ShowFactory.cpp`, leaving `MorseCode.h` and `control.html` independent*: rejected — `MorseCode.h:42` would still be a literal, so the "two literals in the same file" anti-pattern remains. The whole point is to remove the literal from `MorseCode.h`.
- *A small Python emitter that writes all four from a single source*: rejected — overengineering for one string. The single source is the JSON, which is already correct.

### Spec amendment, not spec addition

The existing requirement `Default.params for MorseCode uses "HELLO WORLD"` in `openspec/specs/canonical-show-config/spec.md` already names the three drifted locations and promises "SHALL be reconciled to `"HELLO WORLD"` in a follow-up change." The amendment edits that requirement in place: it replaces the "out of scope" wording with a positive requirement covering all four locations, and updates the scenario accordingly. No new requirement is introduced; no new capability is added.

**Alternatives considered:**

- *A new requirement `MorseCode default message is consistent across all sources`*: rejected — duplicates the existing requirement rather than updating it, and creates two places in the spec that govern the same field.
- *A new capability `cross-source-default-consistency`*: rejected — too broad for one string; would invite future expansions.

### `src/generated/control_gz.h` regeneration is implicit

`compress_web.py` is a PlatformIO `pre:` script (`platformio.ini:65`). Any edit to `data/control.html` triggers regeneration of `control_gz.h` on the next `pio run`. No manual regeneration step is needed; the generated header is a build artefact.

**Risk acknowledged:** if a developer edits `data/control.html` and then inspects `src/generated/control_gz.h` without rebuilding, the two will diverge visually. This is the same risk the codebase already accepts for every other HTML/CSS/SVG file under `data/`.

### Empty-params path is the only behaviour change

The change is intentionally narrow: only `POST /api/show {"name":"MorseCode","params":{}}` produces a different strip state. Any request that carries a `"message"` key in `params` is unaffected — the JSON parsing path already honours whatever the user supplied.

The `data/control.html:277` `value="HELLO WORLD"` change affects what shows up in the input box on page load. The user can edit the input and POST any string they want; the change only affects the initial value shown to them.

The `src/show/MorseCode.h:42` constructor default is dead code; the change is documentation-only.

## Risks / Trade-offs

- **[Risk]** A future maintainer changes one of the four literals and forgets the other three, recreating the drift → **Mitigation**: the spec requirement now binds all four locations; a future CI check (grep across the four files for `HELLO`) could enforce it but is out of scope here. For now, the change adds a code comment on the `|` literal in `ShowFactory.cpp` pointing at the JSON, mirroring the existing comment style on `Wave`'s `wave_speed` / `wavelength` line in `ShowFactory.cpp:82-83`.
- **[Risk]** `POST /api/show {"name":"MorseCode","params":{}}` behaviour change surprises a user who relied on `"HELLO"` as the empty-params default → **Mitigation**: the change is intentional and was telegraphed by the prior change's proposal (`Why` section). The current requirement in `canonical-show-config/spec.md` already calls out `"HELLO WORLD"` as the canonical default. No NVS migration is needed: existing saved params are honoured as-is.
- **[Risk]** The `<input value>` change shifts the on-screen default for the control page → **Mitigation**: same as above; intentional, telegraphed.
- **[Risk]** `MorseCode.h:42` constructor default is dead code today and could be removed entirely → **Acknowledged**: deferred to a follow-up refactor; the change here only corrects the literal.
- **[Risk]** A test could regress silently because the test asserts the wrong string → **Mitigation**: the existing native test suite does not exercise `MorseCode`'s message literal (verified by reading `test/`); the change has no test impact.

## Migration Plan

None. The change is three literal edits plus a spec amendment. The firmware build regenerates `control_gz.h` automatically; the gzipped on-the-wire HTML is the only thing the user observes, and it changes from `"HELLO WORLD!"` to `"HELLO WORLD"` on next page load. No NVS migration. No user-visible rollback path is needed; the change is reversible by reverting the three literal edits.

## Open Questions

None.
