## 1. Converge the three drifted literals

- [x] 1.1 In `src/show/factory/ShowFactory.cpp:105`, change `doc["message"] | "HELLO"` to `doc["message"] | "HELLO WORLD"`. The surrounding lambda body and the rest of the file are unchanged. Verify the file still compiles with `pio run -e adafruit_qtpy_esp32s3_nopsram` and `pio run -e native`.
- [x] 1.2 In `src/show/MorseCode.h:42`, change the constructor default `MorseCode(const std::string &message = "HELLO WORLD!",` to `MorseCode(const std::string &message = "HELLO WORLD",`. No other edits to the header.
- [x] 1.3 In `data/control.html:277`, change the input element's `value="HELLO WORLD!"` to `value="HELLO WORLD"`. No other edits to the file.

## 2. Regenerate the gzipped web header

- [x] 2.1 Confirm `src/generated/control_gz.h` is regenerated on the next `pio run -e adafruit_qtpy_esp32s3_nopsram` (the `compress_web.py` PlatformIO `pre:` hook in `platformio.ini:65` does this automatically). The new file should differ from `git HEAD` only in the gzipped bytes; the source HTML is byte-different only on the one input line.

## 3. Verify

- [x] 3.1 Run `python3 -c "import json; json.load(open('scripts/show_variants.json'))"` to confirm the JSON still parses (no edit to this file, but guards against an accidental cascade).
- [x] 3.2 Run `grep -n 'HELLO WORLD\|"HELLO"' src/show/factory/ShowFactory.cpp src/show/MorseCode.h data/control.html scripts/show_variants.json` and confirm:
  - `scripts/show_variants.json` declares `"message": "HELLO WORLD"` exactly once.
  - `src/show/factory/ShowFactory.cpp` has `| "HELLO WORLD"` exactly once and no `| "HELLO"` anywhere.
  - `src/show/MorseCode.h` has `"HELLO WORLD"` exactly once (the constructor default) and no `"HELLO WORLD!"`.
  - `data/control.html` has `value="HELLO WORLD"` exactly once and no `value="HELLO WORLD!"`.
- [x] 3.3 Run `pio run -e adafruit_qtpy_esp32s3_nopsram` and confirm the firmware builds cleanly. `control_gz.h` regenerates as part of this build.
- [x] 3.4 Run `pio test -e native` and confirm all existing tests still pass. No new tests required for this change.
- [x] 3.5 Run `pio run -e native_show_sim` to confirm the simulator binary still builds (no edit to it, but guards against an accidental dependency).

## 4. Archive

- [x] 4.1 After all tasks above are complete and `pio run -e adafruit_qtpy_esp32s3_nopsram` succeeds, archive the change via `openspec archive converge-morse-default`. The archive step will merge the delta spec from `openspec/changes/converge-morse-default/specs/canonical-show-config/spec.md` into `openspec/specs/canonical-show-config/spec.md`, replacing the `Default.params for MorseCode uses "HELLO WORLD"` requirement with the new `MorseCode default message is declared identically across all four sources` requirement.
