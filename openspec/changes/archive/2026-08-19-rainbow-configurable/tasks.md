## 1. Update Rainbow show to accept parameters

- [x] 1.1 Add `time_step` and `pixel_step` float fields to `src/show/Rainbow.h` with a constructor accepting both (default `1.0f`)
- [x] 1.2 Replace `(iteration + index) % 255` in `src/show/Rainbow.cpp` with `fmodf(static_cast<float>(iteration) * time_step + static_cast<float>(index) * pixel_step, 255.0f)` cast to `uint8_t`

## 2. Wire parameters through ShowFactory

- [x] 2.1 Update the Rainbow lambda in `src/ShowFactory.cpp` to parse `time_step` and `pixel_step` from the JSON document with `|` defaults of `1.0f`, and pass them to the `Show::Rainbow` constructor
- [x] 2.2 Add a `Serial.printf` log line matching the style used by Wave/Mandelbrot so created parameters are visible in serial output

## 3. Add Rainbow params section to web UI

- [x] 3.1 Add a `<div id="rainbowParams" class="params-section">` to `data/control.html` containing two range sliders (`rainbowTimeStep`, `rainbowPixelStep`), live value displays, and an Apply button — following the styling of other params sections
- [x] 3.2 Add `Rainbow` to the `showsWithParams` array in the `showSelect` change handler so it does not auto-apply on selection
- [x] 3.3 Add a `case 'Rainbow'` in `updateParameterVisibility()` that toggles the new section's `.visible` class
- [x] 3.4 Add an `applyRainbowParams()` function that reads both slider values and POSTs `{name:"Rainbow", params:{time_step, pixel_step}}` to `/api/show`
- [x] 3.5 Add a `case 'Rainbow'` in `populateShowParams()` that sets each slider's `.value` from the corresponding stored param
- [x] 3.6 Regenerate the compressed web assets by running the build (PlatformIO invokes `scripts/compress_web.py` automatically as a pre-build step)

## 4. Update documentation

- [x] 4.1 Replace the "Rainbow, ColorRun, Jump currently don't support parameters" line in `docs/SHOW_PARAMETERS.md` with a new Rainbow parameters section describing `time_step` and `pixel_step`, their defaults, behavior with zero values, and an example JSON payload

## 5. Add native test coverage

- [x] 5.1 Add a `test_rainbow` test suite under `test/test_rainbow/` (or extend `test/test_shows/`) with at least: (a) default constructor produces `time_step == 1.0f` and `pixel_step == 1.0f`, (b) `execute()` runs without crashing on a `MockStrip`, (c) explicit constructor values are stored verbatim
- [x] 5.2 Add the new test suite to `platformio.ini` test config if a new directory was created
- [x] 5.3 Run `pio test -e native` and confirm all tests pass

## 6. Verify the build

- [x] 6.1 Run `pio run -e adafruit_qtpy_esp32s3_nopsram` to confirm the firmware compiles with the new constructor signature
- [x] 6.2 Verify `RAINBOW_VARIANTS[]` in `TouchController.cpp` is unchanged (still `{"{}"}`)