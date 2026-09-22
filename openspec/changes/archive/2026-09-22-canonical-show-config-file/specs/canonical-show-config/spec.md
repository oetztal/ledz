# canonical-show-config Specification

## Purpose

Defines `scripts/show_variants.json` as the single canonical source of every show's factory default parameters and curated preset parameters, with a schema that makes both roles explicit at the data-model level. The file is consumed today by the gallery renderer (`scripts/build_pages.py`) and is intended as the runtime source of defaults for the firmware in a follow-up change. The schema invariants in this specification are the contract every consumer reads against.

## ADDED Requirements

### Requirement: Variants manifest is the canonical source for show defaults and presets

`scripts/show_variants.json` SHALL be the canonical source for two roles, both expressed as a single JSON object keyed by show name:

1. **Factory default** — the parameter set the runtime uses when a show is requested without explicit parameters.
2. **Curated preset** — a parameter set the user can pick from a dropdown or that the gallery can preview.

Both roles SHALL be present in the file. A show that has only one interesting parameter set (its default) SHALL still declare the default explicitly.

#### Scenario: File declares both roles per show
- **WHEN** the manifest is parsed
- **THEN** every show entry has a top-level `default` object with a `params` field
- **THEN** every show entry has a `variants` array (possibly empty)

#### Scenario: A show with no curated presets
- **WHEN** a show has no interesting variants beyond its default (e.g. `ColorRun`, `Jump`)
- **THEN** the show's `variants` array is empty (`[]`)
- **THEN** the show's `default` is still present and materialised

### Requirement: Default is structurally distinct from variants

The factory default SHALL live in a top-level `default` object on the show entry, separate from the `variants` array. The default SHALL NOT be a member of `variants`. There SHALL be at most one `default` per show.

```jsonc
{
  "ShowName": {
    "description": "...",
    "iterations": 300,                  // optional, show-level render-row override
    "default": {
      "params": {<fully materialised>}
    },
    "variants": [
      { "name": "...", "label": "...", "params": {...}, "iterations": 50 },
      ...
    ]
  }
}
```

#### Scenario: Default is a sibling of variants, not an entry
- **WHEN** a consumer parses the manifest
- **THEN** `body["default"]` returns the default's `params` and any default-level overrides
- **THEN** `body["variants"]` returns the curated presets only; the default does not appear in this list

#### Scenario: No duplicate default-in-variants
- **WHEN** a variant in `variants` has `params` byte-identical to `default.params`
- **THEN** the variant is removed from `variants` and the default is the sole source for that parameter set

### Requirement: Every variant's params is complete (no implicit C++ fallback)

The `params` object of `default` and every entry in `variants` SHALL list every key the on-device factory reads for that show. A consumer reading the manifest SHALL NOT need to consult any C++ source to know what params a variant uses. The empty-object shorthand (`{}`) SHALL NOT appear on any variant's `params` in the manifest after this capability is in force.

#### Scenario: Fully materialised default for a multi-field show
- **WHEN** the manifest declares `Fire`'s default
- **THEN** `default.params` carries all six keys: `cooling`, `spread`, `ignition`, `spark_amount`, `start_offset`, `spark_range`
- **THEN** every key has the same value the on-device `ShowFactory` `|` fallback produces for `{}` today

#### Scenario: Fully materialised non-default variant
- **WHEN** the manifest declares `Fire.high`
- **THEN** its `params` carries `start_offset` and `spark_range` explicitly, even though those fields are also the C++ defaults

#### Scenario: Show with no params keeps an empty default.params
- **WHEN** a show takes no parameters at runtime (e.g. `ColorRun`, `Jump`)
- **THEN** `default.params` is `{}` and `variants` is `[]`
- **THEN** `{}` here means "the show has no parameters," not "rely on a C++ fallback"

### Requirement: Default.params for MorseCode uses "HELLO WORLD"

The manifest SHALL declare `"message": "HELLO WORLD"` in `MorseCode`'s `default.params`. This is the canonical default message for `MorseCode`. The C++ factory's `| "HELLO"` fallback and the `MorseCode.h:42` constructor default and the `data/control.html` input value are out of scope for this capability and SHALL be reconciled to `"HELLO WORLD"` in a follow-up change.

#### Scenario: MorseCode default message in the manifest
- **WHEN** the manifest declares `MorseCode`'s default
- **THEN** `default.params["message"]` equals `"HELLO WORLD"`
- **THEN** the other six `MorseCode` keys (`speed`, `dot_length`, `dash_length`, `symbol_space`, `letter_space`, `word_space`) carry the C++ factory's `|` fallback values

### Requirement: Schema is consumable by Python and by future C++ / JS consumers

The manifest SHALL be parseable by Python's stdlib `json.load()` with no preprocessing. It SHALL NOT contain JSON5 features, comments other than the top-of-file `_comment`, trailing commas, or non-string keys. A future C++ consumer SHALL be able to generate a C++ source file from the manifest via a Python build script that emits a `std::unordered_map<std::string, std::string>` of `show_name → default_params_json` and a parallel map of `show_name → list<variant>`.

#### Scenario: Manifest parses as stdlib JSON
- **WHEN** the file is read with `python3 -c "import json; json.load(open('scripts/show_variants.json'))"`
- **THEN** no exception is raised

#### Scenario: `_comment` is the only non-data field
- **WHEN** the manifest is parsed
- **THEN** the only top-level keys are `_comment` and the show names
- **THEN** show entries carry only `description`, optional `iterations`, `default`, and `variants`
- **THEN** `default` carries only `params` and optional `iterations`
- **THEN** `variants[]` entries carry only `name`, `label`, `params`, and optional `iterations`
- **THEN** no other keys appear (consumers may ignore unknown keys, but they SHALL NOT be relied on)

### Requirement: Show-level iterations applies to the default

The show entry's optional `iterations` field SHALL apply to `default` and to every entry in `variants` that does not carry its own `iterations`. A per-variant `iterations` override SHALL continue to take precedence for that variant. A future C++ consumer SHALL NOT need to honour `iterations` (it is a renderer concern only); the field is preserved as-is.

#### Scenario: Mandelbrot iterations lifted to show level
- **WHEN** the manifest declares `Mandelbrot` with `"iterations": 1500` at the show body
- **THEN** the default and every variant render at 1500 rows in the gallery
- **THEN** no per-variant `iterations` override is needed

### Requirement: Adding a new show to the manifest is enough to preview it

A maintainer SHALL be able to add a new show to the gallery by adding an entry to `scripts/show_variants.json` with at least a `description` and a `default` object. No source-code change to `ShowFactory`, the simulator binary, or the firmware is required for the gallery to preview the new show. The renderer SHALL fall back to a single empty-params variant for shows not present in the manifest, preserving the existing behaviour for backwards compatibility during incremental rollout.

#### Scenario: New show appears in the gallery without source change
- **WHEN** a new entry is added to the manifest under a show name registered in `ShowFactory`
- **THEN** the next `python3 scripts/build_pages.py --all docs/show_previews` invocation produces a `{ShowName}_default.png`
- **THEN** `docs/show_previews/index.html` lists the new show alongside the existing shows

#### Scenario: Manifest-omitted show still previews
- **WHEN** `ShowFactory` registers a show that is absent from the manifest
- **THEN** the renderer produces a `{ShowName}_default.png` rendered with `--params "{}"`
- **THEN** the gallery still lists the show with a "Default" label