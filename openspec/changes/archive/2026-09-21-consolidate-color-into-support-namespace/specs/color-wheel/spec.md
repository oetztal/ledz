## ADDED Requirements

### Requirement: wheel() lives in the Support::Color namespace

`wheel()` SHALL be reachable as `Support::Color::wheel(float)`. The header declaring it SHALL be `src/support/color.h`. The unqualified name `wheel()` SHALL NOT be reachable outside the `Support::Color` namespace — i.e. no `using namespace Support::Color;` or equivalent is introduced at any call site.

#### Scenario: Header location
- **WHEN** the ledz source tree is searched for the declaration of `wheel()`
- **THEN** it appears in `src/support/color.h`, not `src/color.h`

#### Scenario: Call sites use the qualified name
- **WHEN** every call to `wheel()` is searched across `src/` and `test/`
- **THEN** every match is prefixed with `Support::Color::`

## MODIFIED Requirements

### Requirement: wheel() accepts a float hue index

`Support::Color::wheel()` SHALL accept a `float` hue index `h` and return a `Strip::Color`. Finite inputs SHALL be wrapped continuously into `[0, 255)` via `fmodf` so callers can pass any finite float (including values outside `[0, 254]` and negative values) without their own modulo arithmetic. The existing six-sector cube-walk body SHALL be generalized to use `pos = h * 6 / 255`, `section = (int)pos`, `frac = pos - section`, with the channel computed as `255 * frac`, `255 * (1 - frac)`, or `0` depending on which two of the three channels are transitioning in that sector.

#### Scenario: Pure red at h=0
- **WHEN** `Support::Color::wheel(0.0f)` is called
- **THEN** it returns `0xFF0000`

#### Scenario: Pure green at h=85
- **WHEN** `Support::Color::wheel(85.0f)` is called
- **THEN** it returns `0x00FF00`

#### Scenario: Pure blue at h=170
- **WHEN** `Support::Color::wheel(170.0f)` is called
- **THEN** it returns `0x0000FF`

### Requirement: wheel() reaches the pure primary and secondary colours

At the centre of each of the six cube-walk sectors (`h ≈ 42.5`, `85.0`, `127.5`, `170.0`, `212.5`) `Support::Color::wheel()` SHALL produce the corresponding pure colour. These six mid-sector hues were unreachable with the prior `unsigned char` input because every integer index sat one channel slightly off full intensity.

#### Scenario: Pure yellow at the red-yellow boundary
- **WHEN** `Support::Color::wheel(42.5f)` is called
- **THEN** it returns `0xFFFF00` (red and green at full, blue at zero)

#### Scenario: Pure cyan at the green-cyan boundary
- **WHEN** `Support::Color::wheel(127.5f)` is called
- **THEN** it returns `0x00FFFF` (green and blue at full, red at zero)

#### Scenario: Pure magenta at the blue-magenta boundary
- **WHEN** `Support::Color::wheel(212.5f)` is called
- **THEN** it returns `0xFF00FF` (red and blue at full, green at zero)

### Requirement: wheel() is byte-identical to the prior integer input at every integer in [0, 254]

For every integer hue `h` in `[0, 254]`, `Support::Color::wheel((float)h)` SHALL return the same `Strip::Color` value that the prior `wheel((unsigned char)h)` returned. This guarantees that callers passing integer hues (Chaos, Mandelbrot, MorseCode, and the non-default-parameter paths in Rainbow, Wave, TheaterChase) observe byte-identical rendered output before and after the signature change.

#### Scenario: All 255 integer positions match the prior implementation
- **WHEN** `Support::Color::wheel((float)h)` is evaluated for every integer `h` in `[0, 254]`
- **THEN** the resulting `Strip::Color` matches the corresponding prior integer-input output at every position

#### Scenario: Sector-boundary integer inputs match exactly
- **WHEN** `Support::Color::wheel((float)42)`, `Support::Color::wheel((float)43)`, `Support::Color::wheel((float)84)`, `Support::Color::wheel((float)85)`, `Support::Color::wheel((float)127)`, `Support::Color::wheel((float)128)`, `Support::Color::wheel((float)169)`, `Support::Color::wheel((float)170)`, `Support::Color::wheel((float)212)`, and `Support::Color::wheel((float)213)` are evaluated
- **THEN** each result matches the prior integer-input output exactly

### Requirement: wheel() wraps continuously outside [0, 254]

For any finite float input `h` outside `[0, 254]`, `Support::Color::wheel(h)` SHALL produce the same colour as `Support::Color::wheel(fmodf(fmodf(h, 255.0f) + 255.0f, 255.0f))`. Inputs that resolve to a hue in `[254, 255)` after wrapping SHALL be clamped to 254 so the result matches `Support::Color::wheel(254)`. This lets callers pass unbounded float hues (e.g. accumulating iteration counts) and get well-defined continuous-colring output.

#### Scenario: One full revolution returns the starting hue
- **WHEN** `Support::Color::wheel(255.0f)` is called
- **THEN** it returns `0xFF0000` (same as `Support::Color::wheel(0.0f)`)

#### Scenario: Multiple revolutions continue to wrap
- **WHEN** `Support::Color::wheel(510.0f)` is called
- **THEN** it returns `0xFF0000` (same as `Support::Color::wheel(0.0f)`)

#### Scenario: Negative input wraps from the top
- **WHEN** `Support::Color::wheel(-1.0f)` is called
- **THEN** it returns the same colour as `Support::Color::wheel(254.0f)`

#### Scenario: Large negative input wraps and returns to red
- **WHEN** `Support::Color::wheel(-255.0f)` is called
- **THEN** it returns `0xFF0000` (same as `Support::Color::wheel(0.0f)`)

#### Scenario: Sub-degree hue wraps correctly
- **WHEN** `Support::Color::wheel(254.7f)` is called
- **THEN** it returns `0xFF0006` (the same as `Support::Color::wheel(254)`, the clamp case)

### Requirement: wheel() returns black for non-finite input

If `h` is `NaN` or `±Inf`, `Support::Color::wheel()` SHALL return `0x000000` (all pixels off) rather than propagating garbage through the cube-walk and producing an undefined `Strip::Color`. This is a fail-safe: any caller passing a non-finite hue is making an upstream mistake, and producing a visibly wrong-but-not-crashing colour is worse than producing black.

#### Scenario: NaN input returns black
- **WHEN** `Support::Color::wheel(NaN)` is called
- **THEN** it returns `0x000000`

#### Scenario: Positive infinity input returns black
- **WHEN** `Support::Color::wheel(+Inf)` is called
- **THEN** it returns `0x000000`

#### Scenario: Negative infinity input returns black
- **WHEN** `Support::Color::wheel(-Inf)` is called
- **THEN** it returns `0x000000`
