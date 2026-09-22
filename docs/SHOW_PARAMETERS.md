# Show Parameter Configuration Guide

## Overview

ledz now supports configurable parameters for shows. Parameters are passed as JSON objects and automatically parsed by the ShowFactory.

**✅ Web UI Implemented**: The control interface includes parameter controls that automatically appear based on the selected show:
- **Solid Show**: Dynamic color inputs with optional gradients and flag presets (Warm White, Gradient, Ukraine 🇺🇦, Italy 🇮🇹, France 🇫🇷), plus optional custom `ranges`
- **Mandelbrot Show**: Input fields for Cre0, Cim0, Cim1 (complex plane coordinates), scale, max_iterations, and color_scale
- **Chaos Show**: Input fields for Rmin, Rmax, and Rdelta (logistic map parameters)
- **Fire Show**: Inputs for cooling, spread, ignition, spark_amount, start_offset, and spark_range
- **Starlight Show**: Probability, star length/fade, and star color
- **Stroboscope Show**: Flash color and on/off cycle counts
- **TheaterChase Show**: `num_steps_per_cycle`
- **MorseCode Show**: Message text and timing (speed, dot/dash lengths, spacing)
- **Rainbow Show**: `time_step` and `pixel_step` sliders

Each parameterized show also exposes a **preset selector** populated from
`scripts/show_variants.json` (see `docs/SHOW_PREVIEWS.md`).

## Architecture

```
Web UI → API → ShowController → Queue → ShowFactory → Show Instance
         (JSON)                (JSON)    (parse)      (constructed)
```

### Flow:
1. User configures parameters in web UI
2. UI sends JSON to `/api/show` endpoint
3. ShowController queues the command (thread-safe)
4. LED task processes queue
5. ShowFactory merges the user's JSON over the show's canonical defaults and creates the show
6. Parameters are saved to NVS for persistence

### Canonical defaults

Each show's default parameters live in `scripts/show_variants.json`. A build
script (`scripts/gen_show_variants.py`) turns that file into
`src/generated/show_variants.h`, which `ShowFactory` deserialises at runtime
as the base document. User-supplied keys are overlaid on top, so omitted keys
fall back to the canonical default. The `|` fallbacks inside each factory
lambda remain as defense-in-depth but are normally unreachable.

## API Usage

### Change Show with Parameters

**Endpoint**: `POST /api/show`

**Request Body**:
```json
{
  "name": "Solid",
  "params": {
    "r": 255,
    "g": 0,
    "b": 0
  }
}
```

**Example: Set Solid to Red**
```bash
curl -X POST http://192.168.1.100/api/show \
  -H "Content-Type: application/json" \
  -d '{"name":"Solid","params":{"r":255,"g":0,"b":0}}'
```

**Example: Set Solid to Blue**
```bash
curl -X POST http://192.168.1.100/api/show \
  -H "Content-Type: application/json" \
  -d '{"name":"Solid","params":{"r":0,"g":0,"b":255}}'
```

**Example: Set Mandelbrot Parameters**
```bash
curl -X POST http://192.168.1.100/api/show \
  -H "Content-Type: application/json" \
  -d '{"name":"Mandelbrot","params":{"Cre0":-0.5,"Cim0":0,"Cim1":-0.5,"scale":5,"max_iterations":50,"color_scale":10}}'
```

**Example: Set Chaos Parameters**
```bash
curl -X POST http://192.168.1.100/api/show \
  -H "Content-Type: application/json" \
  -d '{"name":"Chaos","params":{"Rmin":3.5,"Rmax":4.0,"Rdelta":0.001}}'
```

**Example: Set Solid with Linear Gradient (replaces TwoColorBlend)**
```bash
curl -X POST http://192.168.1.100/api/show \
  -H "Content-Type: application/json" \
  -d '{"name":"Solid","params":{"colors":[[255,0,0],[0,0,255]],"gradient":true}}'
```

**Example: Set Solid Parameters (Ukraine Flag)**
```bash
curl -X POST http://192.168.1.100/api/show \
  -H "Content-Type: application/json" \
  -d '{"name":"Solid","params":{"colors":[[0,87,183],[255,215,0]]}}'
```

**Example: Set Solid Parameters (Italian Flag)**
```bash
curl -X POST http://192.168.1.100/api/show \
  -H "Content-Type: application/json" \
  -d '{"name":"Solid","params":{"colors":[[0,140,69],[255,255,255],[205,33,42]]}}'
```

**Example: Set Solid with Custom Ranges**
```bash
curl -X POST http://192.168.1.100/api/show \
  -H "Content-Type: application/json" \
  -d '{"name":"Solid","params":{"colors":[[255,0,0],[255,255,255],[0,0,255]],"ranges":[25,75]}}'
```

**Example: Set MorseCode Message**
```bash
curl -X POST http://192.168.1.100/api/show \
  -H "Content-Type: application/json" \
  -d '{"name":"MorseCode","params":{"message":"HELLO WORLD","speed":0.5}}'
```

## Supported Shows & Parameters

### Solid
**Parameters**:
- `r` (uint8_t, 0-255): Red component (default: 255)
- `g` (uint8_t, 0-255): Green component (default: 255)
- `b` (uint8_t, 0-255): Blue component (default: 255)

**Example JSON**:
```json
{"r": 255, "g": 100, "b": 0}  // Orange
{"r": 128, "g": 0, "b": 128}  // Purple
{"r": 0, "g": 255, "b": 0}    // Green
```

### Mandelbrot
**Parameters**:
- `Cre0` (float): Real component minimum (default: -1.05)
- `Cim0` (float): Imaginary component minimum (default: -0.3616)
- `Cim1` (float): Imaginary component maximum (default: -0.3156)
- `scale` (unsigned int): Scale factor for scanning (default: 5, range: 1-20)
- `max_iterations` (unsigned int): Maximum iterations for convergence test (default: 50, range: 10-200)
- `color_scale` (unsigned int): Color scaling factor (default: 10, range: 1-50)

**Example JSON**:
```json
{"Cre0": -0.5, "Cim0": 0, "Cim1": -0.5, "scale": 5, "max_iterations": 50, "color_scale": 10}  // Classic view
{"Cre0": -1.05, "Cim0": -0.3616, "Cim1": -0.3156, "scale": 5, "max_iterations": 50, "color_scale": 10}  // Default spiral
{"Cre0": -0.8, "Cim0": -0.2, "Cim1": 0.2, "scale": 10, "max_iterations": 100, "color_scale": 5}  // High detail view
```

### Chaos
**Parameters**:
- `Rmin` (float): Starting R value for logistic map (default: 2.95)
- `Rmax` (float): Maximum R value (default: 4.0)
- `Rdelta` (float): R increment per iteration (default: 0.0002)

**Example JSON**:
```json
{"Rmin": 2.95, "Rmax": 4.0, "Rdelta": 0.0002}  // Default - slow evolution
{"Rmin": 3.5, "Rmax": 4.0, "Rdelta": 0.001}    // Faster evolution, starting in chaotic region
{"Rmin": 2.8, "Rmax": 3.6, "Rdelta": 0.0001}   // Slower, exploring period-doubling
```

### Solid
**Parameters**:
- `colors` (array of RGB arrays, optional): List of colors as `[r, g, b]` arrays. Defaults to a single warm white (`[255, 250, 230]`) when omitted.
- `ranges` (array of floats, optional): Boundary percentages (0-100) where colors transition. **Must have exactly N-1 values for N colors.** If omitted, colors distribute equally. Note: ignored in gradient mode.
- `gradient` (boolean, optional): If `true`, colors act as waypoints with smooth interpolation. If `false` (default), colors fill sections with sharp boundaries.

**Description**: Displays color sections across the LED strip. Perfect for flags, banners, multi-color patterns, and gradients. Colors are smoothly blended from the current state using SmoothBlend.

**Note**: This show replaces the former `TwoColorBlend` show. To create a gradient, use `gradient: true`.

**Range Calculation**:
- **Equal Distribution** (default): If `ranges` is omitted, colors are distributed equally across the strip
  - 2 colors: 50% each (no ranges needed)
  - 3 colors: 33.3% each (no ranges needed)
  - 4 colors: 25% each (no ranges needed)
- **Custom Distribution**: Provide boundary percentages. **For N colors, you need N-1 boundary values.**
  - 2 colors: 1 boundary (e.g., `[60]` = 60% color1, 40% color2)
  - 3 colors: 2 boundaries (e.g., `[25, 75]` = 25% color1, 50% color2, 25% color3)
  - 4 colors: 3 boundaries (e.g., `[20, 50, 80]` = 20% color1, 30% color2, 30% color3, 20% color4)

**Modes**:
- **Solid mode** (default, `gradient: false`): Colors fill sections with sharp boundaries
- **Gradient mode** (`gradient: true`): Colors act as waypoints, smooth interpolation between them

In gradient mode, colors are treated as evenly-spaced waypoints across the strip:
- 2 colors: waypoints at 0% and 100% → full strip gradient
- 3 colors: waypoints at 0%, 50%, 100% → gradient A→B in first half, B→C in second half
- N colors: waypoints evenly distributed from 0% to 100%

**Example JSON**:
```json
// Single warm white color
{
  "colors": [[255, 250, 230]]
}

// Ukraine Flag - Blue and Yellow, sharp boundary (50% each)
{
  "colors": [[0, 87, 183], [255, 215, 0]]
}

// Italian Flag - Green, White, Red with sharp boundaries
{
  "colors": [[0, 140, 69], [255, 255, 255], [205, 33, 42]]
}

// Linear gradient from red to blue (replaces TwoColorBlend)
// Gradient spans the ENTIRE strip
{
  "colors": [[255, 0, 0], [0, 0, 255]],
  "gradient": true
}

// Rainbow gradient (smooth transitions between all colors)
// Colors are waypoints: red at 0%, orange at 20%, yellow at 40%, etc.
{
  "colors": [
    [255, 0, 0],     // Red (0%)
    [255, 127, 0],   // Orange (20%)
    [255, 255, 0],   // Yellow (40%)
    [0, 255, 0],     // Green (60%)
    [0, 0, 255],     // Blue (80%)
    [75, 0, 130]     // Indigo (100%)
  ],
  "gradient": true
}

// 2 colors with custom boundary: 60% red, 40% blue (solid mode)
{
  "colors": [[255, 0, 0], [0, 0, 255]],
  "ranges": [60]
}

// 3 colors with custom ranges: 25% red, 50% white, 25% blue (solid mode)
{
  "colors": [[255, 0, 0], [255, 255, 255], [0, 0, 255]],
  "ranges": [25, 75]
}
```

**Flag Presets**:
- **🇺🇦 Ukraine**: Blue (#0057B7) and Yellow (#FFD700)
- **🇮🇹 Italy**: Green (#008C45), White (#FFFFFF), Red (#CD212A)

### Rainbow
**Parameters**:
- `time_step` (float): Hue change per frame (default: `1.0`). Higher values scroll faster; `0` freezes the rainbow in time.
- `pixel_step` (float): Hue change per pixel (default: `1.0`). Higher values compress more rainbow cycles into the strip; `0` makes the whole strip share one hue.

**Behavior with zero values**:
- `time_step=0, pixel_step=0`: strip is solid (all pixels `wheel(0)`).
- `pixel_step=0`: whole strip cycles one color in unison over time.
- `time_step=0`: static rainbow distribution frozen in time.

**UI range**: Sliders clamp to `0.0–5.0` with step `0.05`. The C++ side accepts any finite float, including negative values via the API (which reverses the scroll direction), but the UI does not expose them.

**Example JSON**:
```json
// Default rainbow
{"time_step": 1.0, "pixel_step": 1.0}

// Fast scroll, classic spectrum
{"time_step": 3.0, "pixel_step": 1.0}

// Static rainbow distribution (no time animation)
{"time_step": 0.0, "pixel_step": 1.0}

// All pixels cycle one color together
{"time_step": 1.0, "pixel_step": 0.0}

// Compressed spectrum (multiple cycles visible)
{"time_step": 1.0, "pixel_step": 3.0}
```

### Wave
**Behavior**: A rainbow source that bounces smoothly between the two ends of the strip using a cosine motion. Wavefronts are emitted in the source's current direction; when the source reverses at an end, the wavefronts it previously emitted keep propagating in the other direction, so the strip fills with overlapping wavefronts that self-interfere. Brightness decays exponentially with distance from the (moving) source, so the whole strip is meaningfully lit at all times. Each pixel's hue is determined by the time at which the wavefront currently sitting on it was emitted by the source, producing a rainbow that drifts as wavefronts age.

Two modes are accepted by the JSON contract (`bounce` and `traveling`) but currently produce identical output — the wavelength-based stripe layer they used to differentiate was removed to match the reference implementation in `scripts/build_pages.py`. The mode parameter is kept for future expansion.

**Parameters**:
- `mode` (string, default: `"bounce"`): Phase mode, either `"bounce"` or `"traveling"`. Currently a no-op — both modes render identically. Unknown values fall back to `"bounce"`.
- `decay_rate` (float, default: `2.0`): Exponential decay per unit distance from the oscillating source, measured in units of strip length. Higher values make the bright spot around the source narrower; lower values make the strip more uniformly lit.
- `brightness_frequency` (float, default: `0.1`): Source bounce frequency in cycles per second (higher = source zips along the strip). Also drives the rate at which the rainbow hue cycles.

**`wavelength` and `wave_speed` are no longer accepted and are not exposed in the web UI.** Configurations stored in NVS or sent via the API may still contain either field; both are silently ignored.

**Example JSON**:
```json
// Default: gentle bouncing rainbow with medium decay
{"mode": "bounce", "decay_rate": 2.0, "brightness_frequency": 0.1}

// Tight, fast, dense source
{"mode": "bounce", "decay_rate": 4.0, "brightness_frequency": 0.4}

// Calm, broad bright region that fills the whole strip
{"mode": "bounce", "decay_rate": 1.0, "brightness_frequency": 0.05}

// Traveling (currently identical to bounce)
{"mode": "traveling", "decay_rate": 2.0, "brightness_frequency": 0.1}
```

### Fire
**Behavior**: Simulates a flame. Each frame the strip cools, sparks are injected near one end, and heat spreads upward. Pixels are colored along a black→red→orange→yellow→white ramp by temperature.

**Parameters**:
- `cooling` (float, default: `0.1`): Per-frame temperature decay. Higher values make the flames die out faster and the strip darker.
- `spread` (float, default: `10.0`): How strongly heat diffuses to neighbouring pixels.
- `ignition` (float, default: `0.5`): Temperature added at the base each frame; higher values keep a hotter bed of embers.
- `spark_amount` (float, default: `0.5`): Amount of randomness injected into the base temperature (flicker intensity).
- `start_offset` (int, default: `5`): How far from the strip start the fire bed is anchored.
- `spark_range` (int, default: `5`): Width of the spark region around the start offset.

**Example JSON**:
```json
{"cooling": 0.1, "spread": 10.0, "ignition": 0.5, "spark_amount": 0.5, "start_offset": 5, "spark_range": 5}
```

### Starlight
**Behavior**: Single pixels light up at random, hold at full brightness, then fade out — like stars in a night sky.

**Parameters**:
- `probability` (float, default: `0.1`): Chance per frame of spawning a new star. `0.0–1.0`.
- `length` (unsigned long, default: `5000`): Hold duration at full brightness, in milliseconds.
- `fade` (unsigned long, default: `1000`): Fade-in and fade-out duration each, in milliseconds.
- `r`, `g`, `b` (uint8_t, default: `255`, `180`, `50`): Star colour (default is a warm amber).

**Example JSON**:
```json
// Default warm stars
{"probability": 0.1, "length": 5000, "fade": 1000, "r": 255, "g": 180, "b": 50}

// Dense field of cool white stars with a short hold
{"probability": 0.3, "length": 2000, "fade": 500, "r": 200, "g": 220, "b": 255}
```

### Stroboscope
**Behavior**: Hard on/off flashes of a single colour followed by darkness, repeating on a fixed cycle count.

**Parameters**:
- `r`, `g`, `b` (uint8_t, default: `255`, `255`, `255`): Flash colour.
- `on_cycles` (unsigned int, default: `1`): Number of 10 ms cycles the strip stays on before turning off.
- `off_cycles` (unsigned int, default: `10`): Number of 10 ms cycles the strip stays dark.

**Example JSON**:
```json
// White flash, brief on, long off
{"r": 255, "g": 255, "b": 255, "on_cycles": 1, "off_cycles": 10}

// Red flash, longer on-time (slower strobe)
{"r": 255, "g": 0, "b": 0, "on_cycles": 5, "off_cycles": 20}
```

### TheaterChase
**Behavior**: Evenly spaced rainbow dots march along the strip like a theater marquee, colour-cycling as they go.

**Parameters**:
- `num_steps_per_cycle` (unsigned int, default: `21`): Steps needed for one complete colour rotation. Should be a multiple of `7` so the three dots cover the full hue wheel cleanly. Smaller values rotate colours faster.

**Example JSON**:
```json
{"num_steps_per_cycle": 21}
```

### MorseCode
**Behavior**: Spells out an arbitrary message in International Morse code and scrolls it across the strip as dots and dashes.

**Parameters**:
- `message` (string, default: `"HELLO WORLD"`): Text to display. Converted to uppercase; unknown characters are skipped.
- `speed` (float, default: `0.5`): Scroll speed in LEDs per frame.
- `dot_length` (unsigned int, default: `2`): Length of a dot in LEDs.
- `dash_length` (unsigned int, default: `4`): Length of a dash in LEDs.
- `symbol_space` (unsigned int, default: `2`): Gap between symbols within a letter.
- `letter_space` (unsigned int, default: `3`): Gap between letters.
- `word_space` (unsigned int, default: `5`): Gap between words.

**Example JSON**:
```json
// Default message
{"message": "HELLO WORLD", "speed": 0.5, "dot_length": 2, "dash_length": 4, "symbol_space": 2, "letter_space": 3, "word_space": 5}

// Continuous SOS
{"message": "SOS", "speed": 0.5, "dot_length": 2, "dash_length": 4, "symbol_space": 2, "letter_space": 3, "word_space": 5}
```

### Other Shows
ColorRun and Jump currently don't support parameters and will use their default behavior.

## Adding Parameter Support to New Shows

### Step 1: Define Parameters in Show Class

```cpp
// show/MyShow.h
class MyShow : public Show {
private:
    int speed;
    Color color;
public:
    MyShow(int speed = 50, Color color = color(255,255,255));
    // ...
};
```

### Step 2: Add JSON Parsing in ShowFactory

```cpp
// ShowFactory.cpp constructor: register a lambda that receives the merged
// JsonDocument and returns a unique_ptr.
registerShow("MyShow", "One-line description shown in the UI",
             [](const JsonDocument &doc) {
    // `|` fallbacks are defense-in-depth; the canonical default comes from
    // scripts/show_variants.json -> src/generated/show_variants.h.
    int speed = doc["speed"] | 50;
    uint8_t r = doc["r"] | 255;
    uint8_t g = doc["g"] | 255;
    uint8_t b = doc["b"] | 255;

    ESP_LOGI(TAG, "Creating MyShow speed=%d, RGB(%d,%d,%d)", speed, r, g, b);
    return std::make_unique<MyShow>(speed, color(r, g, b));
});
```

Also add an entry to `scripts/show_variants.json` so the show gets a preview
and a default-parameter source (see `docs/SHOW_PREVIEWS.md`).

### Step 3: Test via API

```bash
curl -X POST http://device-ip/api/show \
  -H "Content-Type: application/json" \
  -d '{"name":"MyShow","params":{"speed":100,"r":255,"g":0,"b":0}}'
```

## Adding Web UI for Parameters

### Simple Example: Color Picker for Solid

Add this to the control HTML page after the show selector:

```html
<div class="control-group" id="solidColorPicker" style="display:none;">
    <label class="control-label">Color</label>
    <input type="color" id="colorInput" value="#ffffff">
    <button onclick="updateSolidColor()">Apply Color</button>
</div>

<script>
// Show/hide color picker based on selected show
document.getElementById('showSelect').addEventListener('change', (e) => {
    const picker = document.getElementById('solidColorPicker');
    picker.style.display = (e.target.value === 'Solid') ? 'block' : 'none';
});

async function updateSolidColor() {
    const hex = document.getElementById('colorInput').value;
    // Convert hex to RGB
    const r = parseInt(hex.substr(1,2), 16);
    const g = parseInt(hex.substr(3,2), 16);
    const b = parseInt(hex.substr(5,2), 16);

    try {
        await fetch('/api/show', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                name: 'Solid',
                params: { r, g, b }
            })
        });
    } catch (error) {
        console.error('Failed to update color:', error);
    }
}
</script>
```

### Advanced Example: Dynamic Parameter Forms

```javascript
// Show parameter schemas
const showParams = {
    'Solid': [
        { name: 'r', label: 'Red', type: 'range', min: 0, max: 255, default: 255 },
        { name: 'g', label: 'Green', type: 'range', min: 0, max: 255, default: 255 },
        { name: 'b', label: 'Blue', type: 'range', min: 0, max: 255, default: 255 }
    ],
    'Mandelbrot': [
        { name: 'Cre0', label: 'Cre0 (Real min)', type: 'number', step: 0.01, default: -1.05 },
        { name: 'Cim0', label: 'Cim0 (Imaginary min)', type: 'number', step: 0.01, default: -0.3616 },
        { name: 'Cim1', label: 'Cim1 (Imaginary max)', type: 'number', step: 0.01, default: -0.3156 },
        { name: 'scale', label: 'Scale', type: 'number', min: 1, max: 20, default: 5 },
        { name: 'max_iterations', label: 'Max Iterations', type: 'number', min: 10, max: 200, default: 50 },
        { name: 'color_scale', label: 'Color Scale', type: 'number', min: 1, max: 50, default: 10 }
    ]
};

// Generate parameter form dynamically
function showParameterForm(showName) {
    const container = document.getElementById('parameterForm');
    container.innerHTML = '';

    const params = showParams[showName];
    if (!params) return;

    params.forEach(param => {
        const input = document.createElement('input');
        input.type = param.type;
        input.id = `param_${param.name}`;
        input.min = param.min;
        input.max = param.max;
        input.step = param.step;
        input.value = param.default;

        const label = document.createElement('label');
        label.textContent = param.label;

        container.appendChild(label);
        container.appendChild(input);
    });
}

// Apply parameters
async function applyShowParameters(showName) {
    const params = {};
    const paramDefs = showParams[showName];

    paramDefs.forEach(param => {
        const input = document.getElementById(`param_${param.name}`);
        params[param.name] = param.type === 'range' ?
            parseInt(input.value) : parseFloat(input.value);
    });

    await fetch('/api/show', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: showName, params })
    });
}
```

## Persistence

Parameters are automatically saved to NVS when a show is changed. On restart:
1. ShowController loads `ShowConfig.params_json`
2. Passes parameters to ShowFactory
3. Show is recreated with saved parameters

## Memory Considerations

- **ShowCommand**: `char *show_name` and `char *params_json` are heap-allocated with `strdup()` when the command is queued and freed by the LED task after it is processed. The struct itself is fixed-size; the JSON length is unbounded.
- **ShowConfig**: `params_json[256]` is the one fixed buffer — the NVS-persisted parameter string is truncated to 255 characters.
- **Heap usage**: ArduinoJson 7 `JsonDocument` grows on demand, so parsing allocates roughly what the payload needs rather than a fixed buffer. There is no compile-time capacity to tune and no silent truncation when a payload outgrows one.

The command queue holds at most `SHOW_COMMAND_QUEUE_SIZE` (5) commands; because
the strings are heap-allocated, queue memory usage scales with the JSON payload
sizes rather than a fixed per-slot budget.

## Testing

### Test Parameter Persistence

```bash
# 1. Set Solid to red
curl -X POST http://device-ip/api/show \
  -d '{"name":"Solid","params":{"r":255,"g":0,"b":0}}'

# 2. Restart device
curl -X POST http://device-ip/api/restart

# 3. Device should boot with red Solid show
```

### Test Parameter Validation

```bash
# Invalid JSON - should use defaults
curl -X POST http://device-ip/api/show \
  -d '{"name":"Solid","params":{"invalid"}}'

# Missing parameters - should use defaults
curl -X POST http://device-ip/api/show \
  -d '{"name":"Solid","params":{}}'

# Partial parameters - should use defaults for missing
curl -X POST http://device-ip/api/show \
  -d '{"name":"Solid","params":{"r":255}}'  # g=255, b=255 (white→red blend)

# Mandelbrot with partial parameters
curl -X POST http://device-ip/api/show \
  -d '{"name":"Mandelbrot","params":{"Cre0":-0.8,"max_iterations":100}}'  # Other params use defaults
```

## Common Color Presets for Solid Show

```javascript
const colorPresets = {
    'Red': {r:255, g:0, b:0},
    'Green': {r:0, g:255, b:0},
    'Blue': {r:0, g:0, b:255},
    'White': {r:255, g:255, b:255},
    'Warm White': {r:255, g:180, b:107},
    'Orange': {r:255, g:100, b:0},
    'Yellow': {r:255, g:255, b:0},
    'Purple': {r:128, g:0, b:128},
    'Pink': {r:255, g:192, b:203},
    'Cyan': {r:0, g:255, b:255},
    'Magenta': {r:255, g:0, b:255}
};

// Apply preset
async function applyPreset(name) {
    const color = colorPresets[name];
    await fetch('/api/show', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: 'Solid', params: color })
    });
}
```

## Debug Output

The ShowFactory and ShowController log parameter information via the
`ESP_LOGx` macros (tags `show` and `ctrl`), e.g.:

```
00001234 I    show Creating Fire cooling=0.10, spread=10.00, ignition=0.50, spark_amount=0.50, start_offset=5, spark_range=5
00001240 I    ctrl Switched to show: Fire with params: {"cooling":0.1,"spread":10}
```

Monitor serial output to verify parameters are being parsed correctly.

## Host-side simulation and preview

`scripts/build_pages.py` produces a PNG preview of any registered show by
driving the same C++ source that runs on the device against a host-only
`MockStrip` (see `docs/SHOW_PREVIEWS.md` for the full story). The script's
`--list` flag reads the show list from the live `ShowFactory` registration,
so the output always reflects whatever shows are currently compiled into
the firmware — adding a new show to `src/show/factory/ShowFactory.cpp` makes
it appear in `--list` and on the GitHub Pages gallery with no further work.

## Next Steps

1. ✅ ~~Add color picker UI to web interface~~ (Completed)
2. ✅ ~~Add parameter forms for Mandelbrot coordinates~~ (Completed)
3. ✅ ~~Add preset buttons for common colors~~ (Completed — flag presets and per-show preset selectors)
4. ✅ ~~Add parameter support to other shows (ColorRun speed, Rainbow rate, etc.)~~ (Rainbow, Fire, Starlight, Stroboscope, TheaterChase and MorseCode done; ColorRun and Jump still parameterless)
5. ✅ ~~Add "favorite" presets that users can save~~ (Completed — user presets via `/api/presets`)
6. Add parameter validation (range checking)
