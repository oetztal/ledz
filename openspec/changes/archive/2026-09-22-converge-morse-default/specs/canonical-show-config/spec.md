## MODIFIED Requirements

### Requirement: Default.params for MorseCode uses "HELLO WORLD"

The MorseCode default message SHALL be the string `"HELLO WORLD"` and SHALL be declared identically in all four locations:

1. `scripts/show_variants.json` `MorseCode.default.params.message`
2. `src/show/factory/ShowFactory.cpp` factory fallback (the literal on the right-hand side of `|` for the `"message"` key in `MorseCode`'s lambda)
3. `src/show/MorseCode.h` `MorseCode` constructor default argument for the `message` parameter
4. `data/control.html` `<input type="text" id="morseMessage">` element's `value` attribute

A drift between any pair of these locations is a defect in `canonical-show-config`. The four locations form a single contract: there is one canonical default message, and every surface that names it names the same string.

#### Scenario: All four locations declare "HELLO WORLD"
- **WHEN** the manifest is parsed
- **THEN** `MorseCode.default.params["message"]` equals `"HELLO WORLD"`
- **WHEN** `src/show/factory/ShowFactory.cpp` parses `{"name":"MorseCode","params":{}}`
- **THEN** the factory fallback for the `"message"` key returns the string `"HELLO WORLD"`
- **WHEN** `src/show/MorseCode.h` `MorseCode` constructor is inspected
- **THEN** the default value of the `message` parameter equals `"HELLO WORLD"`
- **WHEN** `data/control.html` is rendered
- **THEN** the `<input type="text" id="morseMessage">` element has `value="HELLO WORLD"`

#### Scenario: Empty-params API call scrolls "HELLO WORLD"
- **WHEN** the firmware receives `POST /api/show {"name":"MorseCode","params":{}}`
- **THEN** the strip scrolls the message `"HELLO WORLD"`
- **THEN** no other parameter is overridden (the other six keys still come from their C++ `|` fallbacks, which match the JSON `default.params` exactly)

#### Scenario: User-supplied message is honoured
- **WHEN** the firmware receives `POST /api/show {"name":"MorseCode","params":{"message":"SOS"}}`
- **THEN** the strip scrolls the message `"SOS"` regardless of any of the four default locations
- **THEN** the empty-params path is unaffected by this rule
