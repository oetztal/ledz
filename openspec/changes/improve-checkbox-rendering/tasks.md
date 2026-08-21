## 1. Fix the cascade in `common.css`

- [x] 1.1 Narrow `.form-group input` (`data/common.css:71-77`) to `.form-group input:not([type="checkbox"]):not([type="radio"])` — do **not** delete the rule; `timers.html:110` is an `input[type="time"]` whose only styling comes from it (design decision 1)
- [x] 1.2 Narrow `.form-group input:focus` (`:79-82`) the same way, so the focus border does not target checkboxes either
- [x] 1.3 Confirm no other container-scoped `input` rule exists that would still catch a checkbox

## 2. Add the `.checkbox-row` component

- [x] 2.1 Add `.checkbox-row` to `data/common.css`: `display:flex; align-items:flex-start; gap:10px; min-height:44px; cursor:pointer` (design decision 8 — `flex-start` because labels wrap on phone widths). Also added `padding: 12px 0` and `margin-bottom: 0`: with the global `box-sizing: border-box`, `12 + 20 + 12` makes the 44px target exact rather than leaving a single-line label top-aligned in a 44px box with 24px of dead space below, and the inherited `margin-bottom: 5px` from `.form-group label` would otherwise stack with the hint's `margin-top`
- [x] 2.2 Add `.checkbox-row input[type="checkbox"]`: `width:20px; height:20px; margin:0; flex:none; accent-color:#667eea` — native control tinted, no `appearance:none` (design decision 4)
- [x] 2.3 Ensure the label text inside `.checkbox-row` is not forced to `display:block` by `.form-group label` (`:63-69`) in a way that breaks the flex row. `label.checkbox-row` is (0,1,1) — an exact *tie* with `.form-group label`, so it won only by source order, and any reordering of `common.css` would have silently reverted the rows to `display:block`. The rule is therefore written as a two-selector list including `.form-group label.checkbox-row` (0,2,1), which outranks it on specificity
- [x] 2.4 Add `.field-hint` (`display:block; margin-top:4px; color:#666`) matching the existing inline hint style. **`font-size: inherit` was deliberately not included** — the elements are `<small>`, and the 31 unconverted hints keep the browser's ~0.8em default; adding `font-size: inherit` would have made the three converted hints visibly larger than the rest

## 3. Finish the `.toggle-switch` component

- [x] 3.1 Add `.toggle-switch input:focus-visible + .slider { box-shadow: 0 0 0 3px rgba(102,126,234,0.4); }` — the component currently has **no visible keyboard focus at all**, because the real input is `opacity:0` (design decision 5)
- [x] 3.2 Add `.toggle-switch input:disabled + .slider { opacity:0.5; cursor:not-allowed; }`
- [x] 3.3 Add a `@media (prefers-reduced-motion: reduce)` block suppressing the `.4s` transitions on `.slider` and `.slider:before`
- [x] 3.4 Rename `.auto-cycle-row` (`:357-361`) to `.setting-row` — the rule has zero HTML references, so there is no caller to update (design decision 7). Declarations are *not* quite unchanged: `min-height: 44px` was added to meet the touch-target requirement (the pill itself is only 34px tall, so the row has to supply the rest), `gap: 12px` to keep a long label off the switch, and a `.setting-row label { margin-bottom: 0; cursor: pointer }` rule because the inherited `margin-bottom: 5px` from `.form-group label` pushes the text off-centre against the switch
- [x] 3.5 Verify the `.slider` stays the immediate next sibling of the input in all new markup — every state rule is an adjacent-sibling selector (`:336`, `:340`)
- [x] 3.6 Use no `calc()` containing `+` or `-` in any rule added by this change; `scripts/compress_web.py:38` strips the required whitespace and produces invalid CSS silently (design decision 9)

## 4. Convert `#touchEnabled` to a toggle

- [x] 4.1 Restructure `data/settings.html:86-91` into a `.form-group.setting-row` with a sibling `<label for="touchEnabled">Enable touch control</label>` and a `<span class="toggle-switch">` wrapping the input plus `<span class="slider"></span>` (design decision 6)
- [x] 4.2 Keep `id="touchEnabled"` and `onchange="updateTouchEnabled()"` exactly as they are
- [x] 4.3 Confirm `loadTouchConfig()` (`:757`), `updateTouchEnabled()` (`:772`, `:786`) and `updateTouchThreshold()` (`:791`) need no changes — all four `.checked` accesses go through `getElementById`

## 5. Convert the three checkboxes

- [x] 5.1 `#wifiOpenNetwork` (`data/settings.html:176-183`) → `.checkbox-row`, label text in a `<span>`, keeping `id` and `onchange="updateOpenNetworkState()"`; its `<small>` becomes `class="field-hint"`
- [x] 5.2 `#forceUpdateCheckbox` (`data/settings.html:131-138`) → `.checkbox-row`, keeping `id`; its `<small>` becomes `class="field-hint"` (note this one currently omits `margin-top`, so it will gain 4px — intended)
- [x] 5.3 `#colorRangesLinearBlend` (`data/control.html:140-148`) → `.checkbox-row`, removing the inline `style="margin-right: 6px"`; its `<small>` becomes `class="field-hint"`
- [x] 5.4 Leave the other 31 inline `<small>` hints untouched (proposal non-goal)
- [x] 5.5 Confirm the seven `.checked` accesses for `#colorRangesLinearBlend` (`control.html:476,485,494,504,514,547,843`) and the three for `#wifiOpenNetwork`/`#forceUpdateCheckbox` (`settings.html:467,479,705`) still resolve

## 6. Build

- [x] 6.1 Record the current sizes of `src/generated/common_gz.h`, `settings_gz.h` and `control_gz.h`
- [x] 6.2 Run `pio run -e adafruit_qtpy_esp32s3_nopsram` — the pre-build step regenerates all three via `scripts/compress_web.py`
- [x] 6.3 Compare the regenerated sizes; confirm the net growth is small (design: ~40 lines of CSS added, offset by removed inline styles and redundant declarations)
- [x] 6.4 Inspect the minified CSS in `common_gz.h` (or run `compress_web.py` standalone) and confirm the new selectors survived — particularly `input:checked+.slider`, `:not([type="checkbox"])` and `(prefers-reduced-motion:reduce)`

## 7. Manual verification

There is no browser test harness and the native environment cannot render the web assets, so every check below is manual.

- [ ] 7.1 Flash and open the settings page: confirm no checkbox is stretched to full width and every label sits on the same line as its box
- [ ] 7.2 Confirm "Enable touch control" renders as a switch, toggles the device immediately, and snaps back if the device is unreachable
- [ ] 7.3 Confirm "Open network (no password)" is a checkbox and still disables and clears the password field
- [ ] 7.4 Trigger the OTA force path (device on the latest version) and confirm `#forceGroup` reveals a correctly rendered checkbox
- [ ] 7.5 On the control page, select the colour-ranges show and confirm "Gradient mode" matches the settings-page checkboxes, and that Apply still sends the gradient flag
- [ ] 7.6 Keyboard-only pass: tab through both pages and confirm every boolean control shows a visible focus indicator — especially the toggle — and responds to Space
- [ ] 7.7 Click each label's text and confirm the control toggles
- [ ] 7.8 Check both pages at a phone viewport width: rows at least 44px high, wrapped labels aligned to the first line
- [ ] 7.9 Enable the OS reduced-motion setting and confirm the toggle changes state without animating
- [ ] 7.10 Confirm the Daily Alarm time field on `timers.html` is unchanged — this is the regression guarded against in task 1.1
