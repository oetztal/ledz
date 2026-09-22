## 1. HTML markup

- [x] 1.1 In `data/control.html`, remove the existing `<div class="status-item" id="timerStatusItem">…</div>` and the `<div class="status-item"><button id="offButton" …>Off</button></div>` cells from `.status-bar` so the bar contains only Current Show and Brightness.
- [x] 1.2 In `data/control.html`, add a new `<button id="offButton" type="button" class="off-button" aria-label="Turn off LEDs" title="Turn off LEDs">` containing only the inline power-symbol SVG to the `.content` element as a direct child (sibling of `.status-bar`).
- [x] 1.3 In `data/control.html`, add a new `<div class="section" id="timerBanner" hidden>` block between `.status-bar` and the Brightness `.control-group`, containing the four inner nodes: `#timerBannerBadge`, `#timerBannerTime`, `#timerBannerAction`, `#timerBannerDays`, plus a right-aligned `<a href="/timers" id="timerBannerManage">Manage →</a>` link.
- [x] 1.4 In `data/control.html`, give `.content` `position: relative` in an inline style or scoped rule so the Off button has a positioning context.

## 2. CSS

- [x] 2.1 In `data/common.css`, add a `.off-button` rule: `position: absolute; top: 24px; right: 24px; z-index: 5; width: 48px; height: 48px; border-radius: 50%; background-color: #d9534f; color: white; border: none; display: flex; align-items: center; justify-content: center; cursor: pointer; box-shadow: 0 4px 12px rgba(0, 0, 0, 0.18); transition: transform 0.15s, box-shadow 0.15s;`.
- [x] 2.2 In `data/common.css`, add `.off-button:hover` (lift), `.off-button:active` (reset), `.off-button:disabled` (dim + not-allowed cursor), and a `prefers-reduced-motion` rule that suppresses the transition.
- [x] 2.3 In `data/control.html` (or `data/common.css`), add gradient + padding + border-radius styles for `#timerBanner` matching the timezone band, plus chip, mono-time, day-dot and day-letter rules for the four inner nodes.
- [x] 2.4 In `data/common.css`, add `margin-right: 80px` to `.status-bar` so the floating Off button no longer overlaps the bar's right edge.

## 3. JavaScript refactor

- [x] 3.1 In `data/control.html`, add a local `renderDayDots(mask)` helper inside the script block (same algorithm as `data/timers.html:225-230`).
- [x] 3.2 In `data/control.html`, rewrite `updateTimerDisplay()` to write into the new banner DOM nodes: hide the banner when no timers, otherwise update `#timerBannerBadge`, `#timerBannerTime`, `#timerBannerAction`, and `#timerBannerDays` from the head of the sorted list. Sort by soonest-to-fire across countdown and schedule timers, considering the current local time so a passed-today time-of-day doesn't shadow a later-time-of-day that actually fires next. Paused schedules sort last.
- [x] 3.3 In `data/control.html`, ensure the 1 s `setInterval(updateTimerDisplay, 1000)` at `control.html:780` still drives the banner; remove the now-dead references to `#timerStatus` and `#timerStatusItem`.

## 4. Off-button behaviour

- [x] 4.1 Confirm `setStripOff(button)` (`control.html:1059-1080`) still disables the button while its request is in flight and re-enables on completion; no signature change.
- [x] 4.2 Confirm the click handler at `control.html:1082-1084` still wires the button to `setStripOff`.

## 5. Build assets

- [x] 5.1 Run `python3 scripts/compress_web.py` (or trigger a `pio run`) to regenerate `src/generated/control_gz.h` and any other affected `*_gz.h` files from the updated `data/control.html` and `data/common.css`.
- [x] 5.2 Confirm only `control_gz.h` and `common_gz.h` changed; timers_gz.h, settings_gz.h, config_gz.h, about_gz.h are untouched.

## 6. Verification

- [x] 6.1 Run `pio test -e native` and confirm existing tests still pass.
- [x] 6.2 Run `pio run -e adafruit_qtpy_esp32s3_nopsram` and confirm firmware builds cleanly.
- [ ] 6.3 Manual: open `/`, confirm the Off button is a 48×48 red circle with a power icon anchored top-right and the status bar contains only Current Show and Brightness (no overlap thanks to `.status-bar { margin-right: 80px }`).
- [ ] 6.4 Manual: with no timers, confirm the banner is not rendered. Set a 1-minute countdown via `/timers`, return to `/`, and confirm the banner appears with the correct time, badge, action description and Manage link.
- [ ] 6.5 Manual: pause and resume a schedule via `/timers`, return to `/`, confirm the banner shows `PAUSED` (or `SCHEDULE`) and the day dots correctly reflect the weekday mask.
- [ ] 6.6 Manual: press the Off button, confirm the strip goes black, the request payload is unchanged (`Solid`/`[[0,0,0]]`), the banner hides when no timers are configured, and a power cycle preserves the off state.
- [ ] 6.7 Accessibility: tab to the Off button (no visible keyboard ring required beyond what the existing focus styles provide), activate with Space/Enter, and confirm the screen reader announces "Turn off LEDs" via `aria-label`.
