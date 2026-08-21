## Context

The web UI is five hand-written pages (`data/*.html`) sharing one 466-line `data/common.css`, served gzipped from flash via generated headers (`src/generated/*_gz.h`, built by `scripts/compress_web.py`). There is no framework, no build-time CSS tooling and no component system — `common.css` is organised by loose section comments (`/* Form elements */`, `/* Control elements */`, `/* Misc */`).

There are exactly four checkboxes in the entire UI:

```
  ┌──────────────────────────┬──────────────┬───────────────────┬──────────────────────┐
  │ control                  │ page         │ container         │ effect on change     │
  ├──────────────────────────┼──────────────┼───────────────────┼──────────────────────┤
  │ #touchEnabled            │ settings     │ .form-group>label │ POSTs /api/touch now │
  │ #wifiOpenNetwork         │ settings     │ .form-group>label │ local form mutation  │
  │ #forceUpdateCheckbox     │ settings     │ .form-group>label │ read at Install time │
  │ #colorRangesLinearBlend  │ control      │ .param-row>label  │ read at Apply time   │
  └──────────────────────────┴──────────────┴───────────────────┴──────────────────────┘
```

Three of the four are inside a `.form-group`, and that is what breaks them. `common.css:71` sets `width: 100%; padding: 12px; border: 2px solid` on `.form-group input` with no type guard. The type-specific rules at `:242-296` cover `range`, `number`, `text`, `password` and `color`, but never `checkbox`, so the generic rule is the last word for a checkbox and it stretches to fill the container. `#colorRangesLinearBlend` is unaffected only by the accident of living in a `.param-row`, where it carries an inline `style="margin-right: 6px"` instead.

`common.css` also contains two complete, entirely unused components:

```
  .toggle-switch   common.css:298-342   60×34 pill, animated knob, #667eea when checked
  .auto-cycle-row  common.css:357-361   display:flex; justify-content:space-between; align-items:center
```

They are a matched pair — `.auto-cycle-row` is precisely the row layout a labelled toggle needs — and neither is referenced by any HTML. This change is largely a matter of finishing that work rather than inventing new components.

## Goals / Non-Goals

**Goals:**
- No boolean control renders as a full-width stretched box
- The same kind of control looks the same on every page
- A control that changes the device immediately looks different from one that stages a change for a later button
- Boolean controls are keyboard-focusable with a visible focus indicator, and comfortably tappable on a phone
- The two dead components are either used or removed — not left as decoration

**Non-Goals:**
- Any C++, HTTP API or persisted-state change
- Restyling text inputs, selects, sliders, buttons or info boxes
- Converting all 34 inline `<small>` hints (see proposal non-goals)
- A design-token / CSS-variable pass, dark mode, or splitting `common.css`
- Adding `aria-*` attributes; the fix is to make the native `<label for>` association correct, which is strictly better than annotating a broken one
- Changing which values the controls hold or when they are read — every `.checked` access stays valid

## Decisions

### 1. Narrow `.form-group input` by exclusion; do not delete it

**Decision:**

```css
.form-group input:not([type="checkbox"]):not([type="radio"]) { … }
```

**Why:** The obvious simplification is to delete the generic rule outright, on the reasoning that `:242-296` already covers every input type the UI uses. That is wrong. `data/timers.html:110` is `<input type="time" id="dailyAlarmTime">` inside a `.form-group`, and no type-specific rule matches it — `.form-group input` is the only thing giving it padding, border and border-radius. Deleting the rule would silently regress the Daily Alarm time field on a page this change otherwise never touches.

`radio` is excluded alongside `checkbox` even though the UI has no radios today, because the failure mode is invisible: a future radio dropped into a `.form-group` would stretch exactly the same way with nothing to indicate why.

**Alternatives considered:**
- *Delete `.form-group input`.* Regresses `input[type="time"]`, as above.
- *Add a reset rule after it* (`.form-group input[type="checkbox"] { width: auto; padding: 0; border: none; }`). Works, but leaves the misleading rule in place and requires every future component to remember to undo it. The exclusion states the intent once.
- *Add `checkbox` to the type-specific list.* Wrong shape — checkboxes want the opposite of the text-input treatment, not a variant of it.

### 2. Split boolean controls by immediacy

**Decision:** A boolean control is a **toggle switch** when changing it acts on the device immediately, and a **checkbox** when it only stages state that a later explicit button applies.

```
                                  live device effect?
                                          │
              ┌───────────────────────────┴───────────────────────────┐
             YES                                                      NO
              │                                                        │
    ┌─────────────────────┐              ┌────────────────────────────────────────────┐
    │  #touchEnabled      │              │ #wifiOpenNetwork      → armed by "Update"  │
    │  POSTs on change,   │              │ #forceUpdateCheckbox  → armed by "Install" │
    │  reverts on error   │              │ #colorRangesLinear…   → armed by "Apply"   │
    └─────────────────────┘              └────────────────────────────────────────────┘
          TOGGLE                                       CHECKBOX
```

**Why:** A switch reads as a thing you are operating; a checkbox reads as a choice you are recording. `#touchEnabled` is the only control here that operates something — `updateTouchEnabled()` (`settings.html:770-788`) POSTs on `onchange` and reverts `.checked` on failure (`:786`). The other three are inert until a button is pressed, and dressing them as switches would promise an immediacy they do not have.

**Consequence:** the split is 1 toggle / 3 checkboxes, which means the toggle component is introduced for a single site. That is accepted: the component already exists and is already paid for, and the alternative is deleting it.

### 3. `#wifiOpenNetwork` stays a checkbox

**Decision:** Despite reading like a switch ("Open network (no password)"), `#wifiOpenNetwork` is rendered as a checkbox.

**Why:** This is the one genuinely ambiguous control, and it was worth resolving explicitly. `updateOpenNetworkState()` (`settings.html:466-`) only clears and disables the password *input*; nothing reaches the device until "Update WiFi" is pressed, and the `settings-page` spec is explicit that checking it causes the form to send `password: ""` on submit. A switch would encode a false claim of immediacy into the widget — and this is the control where a false claim is most expensive, because the user is editing the credentials that keep them able to reach the device at all.

The general rule in decision 2 already produces this answer; it is recorded separately because the surface reading disagrees with it.

### 4. Tint the native checkbox with `accent-color`; do not custom-draw it

**Decision:** `.checkbox-row input[type="checkbox"]` gets `accent-color: #667eea` and an explicit `20px` square. No `appearance: none`, no pseudo-element checkmark.

**Why:** The tempting symmetry is "custom checkbox to match the custom toggle". It is not worth it:

```
                        │ appearance:none + custom mark │ accent-color
  ──────────────────────┼───────────────────────────────┼──────────────
   theme match          │ exact                         │ exact
   CSS required         │ ~25 lines + mark geometry     │ 1 line
   indeterminate state  │ must be drawn                 │ free
   forced-colors mode   │ must be handled               │ free
   print                │ frequently renders blank      │ correct
```

`accent-color` is supported by every browser that can run this UI. The toggle genuinely has to be custom-drawn because there is no native switch element; the checkbox does not, so the effort is deliberately asymmetric. This also keeps the accessibility story native, which is the argument for not adding `aria-*`.

**Alternatives considered:**
- *Custom-drawn checkbox.* Rejected per the table — it buys nothing visible and costs three correctness edge cases.
- *Leave the checkbox entirely unstyled after decision 1.* This is the minimal fix and it is genuinely defensible; rejected because it leaves the control grey while every other interactive element on the page is `#667eea`, and it does nothing about touch targets.

### 5. Finish `.toggle-switch` before adopting it

**Decision:** Adopt `common.css:298-342` as-is in structure, adding three rules:

```css
.toggle-switch input:focus-visible + .slider { box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.4); }
.toggle-switch input:disabled + .slider     { opacity: 0.5; cursor: not-allowed; }
@media (prefers-reduced-motion: reduce) {
    .toggle-switch .slider, .toggle-switch .slider:before { transition: none; }
}
```

**Why:** The component as written is not shippable. `.toggle-switch input { opacity: 0; width: 0; height: 0 }` (`:306-310`) hides the real input to let the `.slider` span stand in for it, but no rule ever renders the focus state on the slider — so a keyboard user tabbing through the settings page has **no visible indication at all** of where focus is. That is the most serious defect in this change, and it exists in the code today only because nothing uses the component.

The disabled state and the reduced-motion guard are cheap and are the other two things a shared component needs before a second caller appears.

**Note:** decision 1 is a prerequisite for this, not merely adjacent. `.toggle-switch input` zeroes `opacity`, `width` and `height` but not `padding: 12px` or `border: 2px`, both inherited from `.form-group input` at equal specificity. Inside a `.form-group`, today's hidden input would still occupy roughly 28×28 of padding and border inside a 60×34 pill. The exclusion in decision 1 removes that.

### 6. The toggle's text label is a sibling `<label for>`, not a wrapper

**Decision:**

```html
<div class="form-group setting-row">
  <label for="touchEnabled">Enable touch control</label>
  <span class="toggle-switch">
    <input type="checkbox" id="touchEnabled" onchange="updateTouchEnabled()">
    <span class="slider"></span>
  </span>
</div>
```

**Why:** `.toggle-switch` is a fixed 60×34 box with no room for text, so it cannot also be the label. Splitting them gives the text an explicit `for` association — better than today's implicit wrapping — and lets `.setting-row` put the text left and the switch right. `.toggle-switch` becomes a `<span>`; it is only a positioning context for the absolutely-positioned `.slider`.

The `.slider` must remain the immediate next sibling of the input, because every state rule is an adjacent-sibling selector (`input:checked + .slider`, `:336`).

### 7. Rename `.auto-cycle-row` to `.setting-row`

**Decision:** Rename in place; the declarations are unchanged.

**Why:** The rule is dead (zero HTML references) and its name describes a use site that never existed. It is exactly the row this change needs. Renaming costs nothing — there is no caller to update — and leaving a second misleadingly-named dead class next to the one being revived would defeat the point.

### 8. Checkbox rows are flex with a 44px minimum height

**Decision:**

```css
.checkbox-row {
    display: flex;
    align-items: flex-start;
    gap: 10px;
    min-height: 44px;
    cursor: pointer;
}
.checkbox-row input[type="checkbox"] {
    width: 20px;
    height: 20px;
    margin: 0;
    flex: none;
    accent-color: #667eea;
}
```

**Why:** `align-items: flex-start` rather than `center` because three of the four labels wrap to two lines on a phone, and centring puts the box against the middle of the text block. `flex: none` stops the checkbox being squashed by a long label. `min-height: 44px` is the standard touch target for a UI that is used mostly from a phone browser; a native checkbox is ~13-16px on its own. `cursor: pointer` is currently missing from every checkbox label in the UI.

### 9. Avoid `calc()` with `+` or `-` in any new CSS

**Decision:** Recorded as a constraint on the implementation, not a code change.

**Why:** `scripts/compress_web.py:38` minifies CSS with

```python
css = re.sub(r'\s*([{};:,>~+])\s*', r'\1', css)
```

which strips whitespace around `+` unconditionally, with no awareness of context. `calc(100% + 4px)` is minified to `calc(100%+4px)`, which is **invalid** — CSS `calc()` requires whitespace around `+` and `-`. The rule would be dropped silently at parse time, on the device only, with the unminified source looking perfectly correct.

The selectors this change adds are safe under the same regex: `input:checked + .slider` becomes `input:checked+.slider`, `:not([type="checkbox"])` has no adjacent whitespace to lose, and `@media (prefers-reduced-motion: reduce)` becomes `(prefers-reduced-motion:reduce)` — all still valid.

## Risks / Trade-offs

- **No automated test can catch a CSS regression here** → The project has no browser test harness, and the native PlatformIO environment cannot compile or render the web assets. Mitigation: the change is confined to presentation with no behavioural surface, and the tasks list explicit per-control manual verification including a keyboard-only pass.
- **The minifier is a silent failure mode** → Beyond `calc()` (decision 9), any future CSS whose meaning depends on whitespace around `{};:,>~+` will be corrupted with no build error. Mitigation: decision 9 documents it; verifying the rendered page on-device rather than in the browser against `data/` is the only real check, and is in the tasks.
- **A toggle component with one caller** → `.toggle-switch` is introduced for `#touchEnabled` alone, which is thin justification for maintaining a component. Mitigation: it already exists and is already carried in flash-adjacent source; this change makes it correct and used rather than incorrect and dead. Deleting it instead was considered and rejected because the immediacy distinction in decision 2 is real and will recur.
- **Flash cost** → Assets are gzipped into flash. Adding focus/disabled/reduced-motion rules and `.checkbox-row` grows `common.css` by roughly 40 lines, partly offset by removing four inline `style` attributes and the redundant declarations in decision 1. Mitigation: net change expected to be small after gzip; the tasks compare `src/generated/common_gz.h` size before and after.
- **`accent-color` is not styleable further** → If a future design wants a differently-shaped checkbox, decision 4 has to be revisited wholesale. Mitigation: accepted; that is a design change, not a defect, and `.checkbox-row` is the seam where it would happen.
- **`prefers-reduced-motion` cannot be verified on the device browser easily** → Mitigation: verify in a desktop browser against `data/` directly, where the OS setting is togglable.

## Migration Plan

None. No persisted state, no API surface, no stored user data. The change takes effect when the regenerated `*_gz.h` headers are flashed.

A browser holding a cached `common.css` against new HTML would render the new class names unstyled — checkbox rows would fall back to browser defaults and the toggle would show a bare checkbox with no slider. This resolves on reload. It is the same cache-staleness behaviour the UI already has for every asset, and the failure is cosmetic rather than functional, since all four controls remain real focusable inputs whose `.checked` state the JavaScript reads correctly either way.

## Open Questions

- **Should the remaining 31 inline `<small>` hints be swept in a follow-up, or left alone?** The class is introduced here either way. A sweep is mechanical and would shrink both HTML files, but it touches nearly every parameter row in `control.html` for no behavioural gain.
- **Should `.param-row` and `.form-group` be reconciled?** They are near-duplicates that differ mainly in which one accidentally breaks checkboxes. This change sidesteps the question by making `.checkbox-row` work identically inside either, but the underlying duplication remains and is the root cause of the two-renderings-of-one-control problem.
