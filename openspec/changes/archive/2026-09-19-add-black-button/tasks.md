## 1. UI markup

- [x] 1.1 In `data/control.html`, add a third `.status-item` to the `.status-bar` (after the Brightness item, before the timers status item) containing an `<button id="offButton" type="button" class="btn btn-danger">Off</button>`.

## 2. Behaviour

- [x] 2.1 In the script block of `data/control.html`, add an async function `setStripOff()` that `POST`s `/api/show` with body `{"name":"Solid","params":{"colors":[[0,0,0]]}}`, disables the button for the duration of the request, and on success calls `updateStatus()`.
- [x] 2.2 Wire the button to `setStripOff` via a click listener that prevents double-submits while a request is in flight and re-enables the button in `finally`.
- [x] 2.3 Verify that clicking Off causes the status bar, show dropdown, and colour-ranges parameter panel to update via the existing `updateStatus()` poll path.

## 3. Build assets

- [x] 3.1 Run `python3 scripts/compress_web.py` to regenerate `src/generated/control_gz.h` from the updated `data/control.html`.
- [x] 3.2 Confirm `src/generated/control_gz.h` has changed and the other `*_gz.h` files are untouched.

## 4. Verification

- [x] 4.1 Run `pio run -e native` to ensure the native build (which compiles show logic and tests) is unaffected by the HTML change.
- [x] 4.2 Run `pio test -e native` to confirm existing tests still pass.
- [x] 4.3 Run `pio run -e adafruit_qtpy_esp32s3_nopsram` to confirm the firmware still builds with the regenerated gzipped HTML.
- [x] 4.4 Smoke-test on hardware (or via a future manual step): load `/`, click Off, confirm strip goes black, confirm dropdown reflects `Solid`, power-cycle, confirm strip is still black.
