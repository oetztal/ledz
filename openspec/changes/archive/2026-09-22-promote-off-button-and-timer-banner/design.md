## Context

The control page (`data/control.html`) currently renders four siblings inside `.status-bar` (`data/common.css:218-244`): Current Show, Brightness, an `Off` button (`btn btn-danger` with the text "Off"), and a `timerStatusItem` cell (`control.html:39-42`) that toggles to visible whenever the timer fetch returns at least one enabled timer. With four items, `justify-content: space-between` distributes them evenly, and the appearance of the fourth item pushes the Off button out of the rightmost slot.

The `.container` wrapper at `common.css:17-23` carries `overflow: hidden` and `border-radius: 15px`. Anything that needs to read as "floating outside the rounded card" is clipped by the radius, so the Off action must live inside the container — pinned, not external.

The `/timers` page (`data/timers.html`) already establishes the visual grammar for a "rich timer" element: the timezone band at `timers.html:20-84` uses the header's purple gradient with `padding: 20px; border-radius: 12px;` white text, and each Active Timers card at `timers.html:401-414` is a left-bordered card with a coloured type badge, mono time, action description and day dots. The home page banner reuses both.

The existing `updateTimerDisplay()` (`control.html:1330-1365`) reads `cachedTimers`, filters expired, sorts countdowns-then-schedules, takes the head, and writes a one-line summary into `#timerStatus`. The 1 s poll that calls it (`control.html:780`) is the natural driver for the new banner.

## Goals / Non-Goals

**Goals:**
- Make the Off button a stable top-right corner action that does not move when timers appear.
- Give next-timer information its own dedicated UI region on the home page with enough visual room to convey type, time, action and weekday.
- Reuse existing HTTP contracts and existing timers-page visual language; no C++ changes.
- Add a single "Manage →" link to `/timers` from the banner. Pause/edit/cancel stay on `/timers`.
- Keep keyboard and screen-reader behaviour solid: button reachable, icon has accessible label, banner readable.

**Non-Goals:**
- Adding inline pause/cancel controls to the home page.
- Showing a "+N more" indicator for additional timers beyond the next one.
- A hardware (touch controller) off gesture.
- Changing the persistence model or the Solid-black payload.
- Reorganising the timers-page Active Timers list.
- Adding new HTTP endpoints or extending `GET /api/timers`.

## Decisions

### Status bar reverts to two cells

Removing the Off button and the timer cell from `.status-bar` leaves Current Show and Brightness. Two cells with `justify-content: space-between` distribute to the left and right edges with an empty centre; that is visually acceptable because the absolute-positioned Off button is now the right-anchored element. No CSS change is required beyond leaving the existing `.status-bar` rule alone — fewer children simply means a wider gap.

**Alternatives considered:**
- A third "Status: Off/On" placeholder cell: rejected — surfaces no information that isn't already in Current Show + brightness slider.
- Anchor the Off button inside `.status-bar` (absolute relative to the bar): rejected — the design needs Off to feel like an action, not a status item.

### Off button sits in `.content`, position: absolute, top: 24px right: 24px

The `.content` div has `padding: 30px` and a flex column of its children. Setting `.content { position: relative; }` (additive) gives the Off button a positioning context. The button is placed at `top: 24px; right: 24px` to sit just inside the page padding and visually aligned with the status bar's right edge. A drop shadow (`0 4px 12px rgba(0,0,0,0.18)`) gives the FAB a slight elevation. The shadow's outer-most pixels land inside the container's `border-radius: 15px`, so the existing `overflow: hidden` does not clip them.

**Alternatives considered:**
- `position: fixed` relative to the viewport: rejected — the content area below is short and there is no scrollable body to "follow" the user through; a fixed FAB would also overlap the brightness slider on narrow viewports.
- Anchoring to `.header`: rejected — `.header` is a sibling of `.content` (not a parent), so absolute positioning relative to it would require relocating the button out of `.content` and across a CSS boundary that the rest of the layout doesn't expect.
- Pulling Off fully outside the container as a body-level FAB: rejected — `overflow: hidden` on `.container` clips it, defeating the design intent.

### Off icon: inline SVG, line + arc (IEC 60417-5009), `stroke="currentColor"`

```svg
<svg viewBox="0 0 24 24" width="22" height="22"
     fill="none" stroke="currentColor" stroke-width="2.2"
     stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
  <path d="M12 3v9"/>
  <path d="M18.4 5.6a9 9 0 1 1-12.8 0"/>
</svg>
```

The SVG is rendered inline so the button colour (`#d9534f`) drives the stroke, keeping the existing `btn-danger`-red palette with one CSS variable's worth of plumbing. A `title="Turn off LEDs"` tooltip and `aria-label="Turn off LEDs"` give screen-reader and mouse-only users an equivalent affordance. The existing `setStripOff` function (`control.html:1059-1080`) is reused unchanged.

**Alternatives considered:**
- `⏻` (Unicode Power Symbol, U+23FB): rejected — renders with the system font, not the page's; weight and stroke style are inconsistent across platforms.
- `⏹` (Stop Square, U+23F9): rejected — semantically "stop the show", not "off"; reads as a media-player button.
- Custom SVG without `currentColor`: rejected — would force hardcoded colour and decouple the button from the existing red palette.

### Off button is a 48×48 circle

48 px meets the existing 44 px minimum touch-target rule from `web-ui-controls` and is the conventional FAB diameter in Material/iOS design systems. A pill or rounded-square would feel like a toolbar button rather than a corner action. The button keeps the existing red (`#d9534f`); a one-letter white icon at 22 px is large enough to read on a 48 px background.

**Alternatives considered:**
- 40×40 square with 6 px radius: rejected — slightly below the existing touch-target minimum and reads like a toolbar button.
- Icon-only inside the existing `btn-danger` text-button chrome: rejected — defeats the "promote to corner action" intent; text chrome would push the icon left and waste horizontal space.

### Banner lives between `.status-bar` and the Brightness control group

DOM order:
```html
<div class="status-bar">…</div>              <!-- 2 cells -->
<section class="content">
  <button id="offButton" class="off-button">…</button>  <!-- positioned absolute -->
  <div id="timerBanner" class="section" hidden>…</div>  <!-- only renders when timers exist -->
  <div class="control-group">Brightness …</div>
  …
</section>
```

The banner reuses the header's gradient, `padding: 20px; border-radius: 12px;` for visual continuity with the timezone band on `/timers`. It contains: a coloured type badge (TIMER / SCHEDULE / PAUSED) on the left, a mono time next to it, a right-aligned "Manage →" anchor to `/timers`, an action description line beneath, and a day-dots row for schedules. A small `margin-bottom: 20px` separates it from the Brightness control.

**Alternatives considered:**
- Putting the banner inside the status bar: rejected — the status bar has fixed-height children (`text-align: center`, `status-value: 18px font-weight bold`) that prevent it from gracefully containing a flexible-height rich card.
- Putting the banner below the Brightness slider: rejected — the Brightness control is the next visual landmark and the next timer is more "global" status; surfacing it above the slider establishes priority.

### Banner displays the next timer only

The existing sort key (countdowns first, then schedules by time of day; slot index breaks ties) at `control.html:336-343` is reused for the banner's head element. Showing only the next timer avoids visual noise and matches the user's mental model ("what's next?"). Multi-timer context, if needed, lives on `/timers`.

**Alternatives considered:**
- "+N more" link next to the time: deferred — easy follow-up but adds visual weight to a single-purpose banner. Revisit if user feedback indicates the next-only view is too thin.
- Stacking all timers in the banner: rejected — duplicates the Active Timers list and pushes the Brightness control off the fold.

### Multi-timer signalling: filtered by enable, no information loss

`fetchTimers()` already filters by `t.enabled`. The banner hides when the filtered list is empty. There is no need for a "no timers" placeholder inside the banner — full hidden is the no-state.

**Alternatives considered:**
- Show "No active timers" placeholder when filters produce zero: rejected — a "no timers" placeholder competes visually with the bar's free space; absence carries the same information.

### JS refactor: `updateTimerDisplay()` writes into the banner

The new function reads `cachedTimers` + `timersFetchedAt`, picks the head using the same sort as `control.html:336-343`, and updates four DOM nodes: `#timerBannerBadge` (text + background tint), `#timerBannerTime` (mono formatted string), `#timerBannerAction` (action text), `#timerBannerDays` (inner HTML of the seven-dot, seven-letter row when the head is a schedule). The 1 s `setInterval` that calls it (`control.html:780`) is unchanged. `fetchTimers()` (`control.html:1318-1328`) is unchanged.

**Day-dot helper:** the existing `renderDayDots(mask)` at `timers.html:225-230` lives in the timers page's script. The home page gets its own copy of the same function (six lines: two `map` calls and a concatenation), keeping the gzipped HTML pages self-contained. Refactoring across pages would require either a shared `/common.js` (a new web asset) or inlining a script tag — both out of scope.

### Accessibility & motion

- The Off button is a real `<button>` (not a div), so keyboard reach and Space/Enter activation come for free.
- `aria-label` and `title` describe the action to assistive tech and pointer-only users.
- The button's existing `:disabled` rule at `control.html:1060` is preserved on `.off-button` with the dimmed visual.
- The hover lift (`transform: translateY(-1px)`) is short and reversible; it is suppressed under `prefers-reduced-motion` consistent with the existing rule at `common.css:394-399`.

## Risks / Trade-offs

- **[Risk]** Drop shadow on the FAB could be clipped by `.container { overflow: hidden; border-radius: 15px }` if the button is placed too close to a corner → **Mitigation**: button sits at `top: 24px; right: 24px`, well inside the radius. Worst case the shadow gets trimmed by a couple of pixels, but the visual reads as elevated rather than flush.
- **[Risk]** Status bar with only two cells reads as "empty middle" → **Mitigation**: the Off button is visually anchored top-right and behaves as the rightmost element; the empty centre reads as breathing room, not as a missing cell. Two-cell layouts with space-between are common in card UIs.
- **[Risk]** Power-symbol SVG may not be universally understood → **Mitigation**: `aria-label` and `title` provide a textual fallback for assistive tech and pointer-only users; tooltip appears on hover. The icon is conventional enough in current web/UI that first-time users will recognise it in context.
- **[Risk]** Inline icon adds ~1 KB to control.html pre-gzip → **Mitigation**: trivial against the existing ~50 KB pre-gzip page; gzips well; no measurable firmware cost.
- **[Risk]** Multi-timer display truncated to "next only" hides the existence of other timers → **Mitigation**: "Manage →" link goes to `/timers` where the full list lives. Acceptable trade-off for a streamlined home page.
- **[Risk]** Day-dot helper duplicated across pages → **Mitigation**: keep both copies; flag as a future cleanup candidate (extract to `/common.js`) but stay within the change's scope.

## Migration Plan

None. The change is purely a redesign of the existing Off button location/visual plus an additive banner; both new DOM nodes are hidden by default, both HTTP contracts are unchanged, no firmware version needs bumping. Deployed with whatever release bundles the regenerated `control_gz.h`.

## Open Questions

- Whether to suppress the hover lift under `prefers-reduced-motion` more aggressively than the existing keyframe-based approach at `common.css:394-399`. The current rule already covers `.toggle-switch` and `.brightness-value`; adding `.off-button` to the same pattern is one extra selector and resolves during implementation.
- Whether the day-dots row's `data-on` empty attribute (a hint used by the timers page's CSS, not present on the home page) is needed. The home page owns its own CSS for the banner; default styling will be re-derived from the gradient context rather than copied from the timers page.
