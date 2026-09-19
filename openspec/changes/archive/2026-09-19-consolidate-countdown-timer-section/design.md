## Context

`data/timers.html` currently shows the countdown timers as two adjacent `.section` blocks:

- "Quick Off Timer" — four `btn-primary` buttons (15 min / 30 min / 1 h / 2 h) whose `onclick` calls `setQuickTimer(seconds)`, which POSTs `{duration, action: 1}` to `/api/timers/countdown` and refreshes the timer list on success.
- "Custom Countdown Timer" — a form with a duration input, an action select (off / preset), a conditional preset selector, and a submit button. Submit calls `setCustomCountdown()`, which reads the form fields and POSTs to the same endpoint with the same payload shape.

Both paths hit `POST /api/timers/countdown`; the only difference is where the duration and action come from. The "Custom" qualifier in the second section's heading exists solely to contrast with the first section. After the merge, the qualifier is redundant.

This change touches one HTML file. No backend, no payload, no scheduling semantics, no NVS schema, no test assertions are affected.

## Goals / Non-Goals

**Goals:**
- Reduce the timers page from four `.section` blocks to three by merging the countdown primitives.
- Drop the "Custom" qualifier from the section heading.
- Make the preset buttons a row of *helpers* for the form rather than standalone actions, so the form and the presets share state and there is exactly one submit path.
- Reuse existing CSS classes so the row sits visually subordinate to the primary submit button.

**Non-Goals:**
- No change to the `/api/timers/countdown` endpoint, its payload, or its response.
- No change to the `setCustomCountdown` submit path or its alert / error handling.
- No new CSS. `.btn-secondary` and `.btn-small` already exist in `data/common.css` for the schedule card's Edit button and are reused here.
- No JS-side deduplication of submit logic; with one submit path there is nothing to deduplicate.
- No rename of `setCustomCountdown` — its name still reads correctly even though "Custom" no longer appears in the section heading, and renaming it would expand the diff without a behavioural reason.

## Decisions

### Decision 1: Preset click pre-fills the form rather than submitting immediately

**Chosen:** Clicking "15 min" writes `15` into `countdownMinutes`, sets `countdownAction.value = "off"`, hides the preset selector group, and focuses the duration input. The user must click "Set Countdown Timer" to actually create the timer.

**Considered:** One-click submit (preserves today's Quick Off Timer contract).

**Why pre-fill:** The user explicitly chose pre-fill during exploration. The trade-off is one extra click for the "I'm going to bed, kill the lights in 15" flow, in exchange for the form and the presets sharing state: the user can click "30 min" and then change the action to "Load preset" before submitting, which the one-click design cannot offer. The pre-fill behaviour also keeps the focus in the duration input so the user can tweak the value with the keyboard before submitting, which the one-click design skips.

**Pre-fill state reset.** The preset always sets `action = "off"` and hides the preset selector, regardless of what the form previously held. Presets are off-only shortcuts — if the user wants a preset action with a custom duration, they configure the form manually rather than clicking a preset and then re-picking the action. This avoids a subtle UX trap where a user who had previously configured a preset action loses track of why a "30 min" click changed their form.

### Decision 2: Preset buttons reuse `.btn-secondary.btn-small`

**Chosen:** Reuse `.btn-secondary.btn-small` from `data/common.css`.

**Considered:** Keep the existing `.btn-primary` styling.

**Why secondary/small:** Four primary buttons in a row directly above one primary submit button creates two competing primary actions on the screen. Secondary buttons read as shortcuts — visually subordinate to "Set Countdown Timer" but still clearly clickable. `.btn-secondary.btn-small` is already used by the schedule card's Edit button, so reusing it keeps the visual vocabulary consistent across the page.

### Decision 3: Preset row sits at the top of the section, above the form

**Chosen:** A `.preset-row` strip above the duration input.

**Considered:** Inline with the duration input (like a number pad).

**Why top:** Reading top-to-bottom, the user sees shortcuts → form → commit, which mirrors the choice they are making (fast path vs. custom path). Inline placement muddles the relationship: the buttons look like augmentations of the duration field but they replace it for the duration dimension, which is confusing. The top strip also gives the description paragraph "Pick a preset or set a custom duration and action." a layout that reads naturally — the affordances are listed in the same order as the description.

### Decision 4: Rename `setQuickTimer` to `applyCountdownPreset`

**Chosen:** Rename the JS helper to `applyCountdownPreset(minutes)`.

**Considered:** Keep the existing name.

**Why rename:** Under the new pre-fill semantics, `setQuickTimer` does not "set" a timer — it *fills* the form for one. Keeping the old name would mislead the next reader into expecting a network call that no longer happens. `applyCountdownPreset` matches what the function actually does and matches the section heading ("Countdown timer") and the row label (presets).

### Decision 5: Visual feedback for the preset click is the focus on the duration input

**Chosen:** After filling the form, call `document.getElementById('countdownMinutes').focus()` and `select()`.

**Considered:** Briefly highlight the submit button; scroll the form into view; no extra feedback.

**Why focus:** Because the form is already visible on the same page, scrolling adds nothing. Briefly highlighting the submit button would require new CSS animations. Focusing the duration input is the natural reaction point — the value is visible in the field, the cursor is there, and the user can immediately tweak with the keyboard before submitting. No new CSS, no animation, no extra DOM.

## Risks / Trade-offs

- **One extra click for the simple off flow.** Pre-fill costs a click that one-click submit avoids. Users who set a "lights off in 15 min" timer every night now click the preset and then the submit button. Mitigation: the preset and submit buttons sit close together in the section, and the duration input is focused after the preset click so the user can press Tab → Space or Enter to submit if they want. If this turns out to be a regression in practice, the change is easy to flip back to one-click.

- **State reset surprise.** A user who had configured `action = preset` and `preset = X` and then clicks "30 min" loses both. Their timer will be created with `action = off` unless they notice and re-set the action. Mitigation: the preset click resets to the form's visible default state (`off`), so the user sees what the form is about to do. The change is reversible per click.

- **No regression test.** There is no automated test for the timers page UI; the only test (`test_localtime`) only verifies the file is loadable. A regression in the markup or the JS would not be caught by `pio test`. Mitigation: a manual smoke test (open the page, click each preset, click submit, verify a countdown appears in the Active Timers list) before merging. The change is small enough that a single review pass should catch obvious problems.

## Migration Plan

No data migration. The change is a static HTML / JS edit shipped in a single firmware release. Users who have the page open at the moment of upgrade will need to refresh, which is the same behaviour as any other page change.

Rollback: revert the diff to `data/timers.html`. No state is stored, so there is nothing to roll back beyond the file itself.
