## ADDED Requirements

### Requirement: A preset selector is rendered for every show with curated variants

The control page SHALL render a preset selector in the currently selected show's parameter section whenever that show has at least one curated variant in `SHOW_VARIANTS_BY_SHOW`, except for the Solid show. The selector SHALL be populated from `SHOW_VARIANTS_BY_SHOW[<ShowName>]` and SHALL be inserted as the first element of the show's parameter section. Shows with no curated variants SHALL render no selector. The Solid show SHALL render no generic preset selector; its bespoke preset buttons SHALL remain.

#### Scenario: A show with variants gets a selector
- **WHEN** the user selects `Fire` (which has one curated variant) on the control page
- **THEN** the Fire parameter section shows a preset selector
- **THEN** the selector's options are `default` and `high`

#### Scenario: A show without variants gets no selector
- **WHEN** the user selects `Jump` (which has no curated variants)
- **THEN** no preset selector is rendered in its parameter section

#### Scenario: Solid is excluded
- **WHEN** the user selects `Solid`
- **THEN** no generic preset selector is rendered
- **THEN** the flag, warm-white and gradient preset buttons remain available

### Requirement: Selector options carry the manifest's labels and include the factory default

The selector SHALL offer the show's factory `default` first, followed by each curated variant in manifest order. Each option's display text SHALL be the entry's `label` from `scripts/show_variants.json`. The factory default SHALL be labelled `Default`. If a variant's manifest entry has no `label`, the option text SHALL fall back to the variant's `name`.

#### Scenario: Mandelbrot labels come from the manifest
- **WHEN** the user opens the Mandelbrot preset selector
- **THEN** the options are `Default`, `Deep zoom on cardioid cusp`, `Wide overview of the set`, and the remaining curated labels declared in `scripts/show_variants.json`
- **THEN** no option text is the raw variant key (`deep-zoom`, `wide-overview`, …)

#### Scenario: Wave labels come from the manifest
- **WHEN** the user opens the Wave preset selector
- **THEN** the options are `Default`, `Tight fast` and `Calm broad`

### Requirement: Selecting a preset applies it immediately

Choosing an option in the selector SHALL immediately send a `POST /api/show` request naming the selected show and carrying that entry's `params` object. No separate Apply action SHALL be required to apply a preset. On completion the page SHALL refresh the show's parameter fields from device state.

#### Scenario: Applying a variant
- **WHEN** the user selects `SOS` in the MorseCode preset selector
- **THEN** the page sends `POST /api/show` with `{"name":"MorseCode","params":{"message":"SOS", …}}`
- **THEN** the strip scrolls `SOS` without any further button press

#### Scenario: Applying the factory default
- **WHEN** the user selects `Default` in the Starlight preset selector
- **THEN** the page sends `POST /api/show` with the show's `default.params`

### Requirement: A manually edited parameter and a selected preset coexist

The show's individual parameter fields and their Apply buttons SHALL remain available after a preset is applied, so the user can fine-tune a preset. The preset selector SHALL be cleared when the page refreshes show state, so it does not imply the device is still showing the last-selected preset.

#### Scenario: Fine-tuning after a preset
- **WHEN** the user applies a preset and then edits a parameter field and presses Apply
- **THEN** the edited value is sent to the device

#### Scenario: Selector clears on status refresh
- **WHEN** the periodic status refresh runs
- **THEN** every preset selector on the page is reset to its placeholder option
