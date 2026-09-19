# timer-scheduling Specification

## Purpose
Defines how the device keeps local time and evaluates scheduled actions: the POSIX timezone representation that drives all local-time values, the behaviour of countdown timers and schedules across daylight-saving transitions, the HTTP surface for reading and writing the timezone, the timers page controls for picking a timezone by place, and the one-time migration of legacy offset and alarm state.
## Requirements
### Requirement: Local time is derived from a POSIX TZ string
The device SHALL store its timezone as a POSIX `TZ` string and SHALL derive all local-time values from it using the C library's timezone handling, so that daylight-saving transitions and sub-hour offsets are applied without further configuration. The string SHALL be at most 63 characters. The device SHALL NOT store a bare UTC offset as its timezone representation.

#### Scenario: Daylight saving applied automatically
- **WHEN** the stored timezone is `CET-1CEST,M3.5.0,M10.5.0/3` and the UTC epoch corresponds to 2026-01-15T06:00:00Z
- **THEN** the local time is 07:00:00
- **WHEN** the UTC epoch corresponds to 2026-07-15T05:00:00Z
- **THEN** the local time is 07:00:00

#### Scenario: Transition instant
- **WHEN** the stored timezone is `CET-1CEST,M3.5.0,M10.5.0/3` and the UTC epoch is 2026-03-29T00:59:59Z
- **THEN** the local time is 01:59:59
- **WHEN** one second elapses
- **THEN** the local time is 03:00:00

#### Scenario: Sub-hour offset
- **WHEN** the stored timezone is `IST-5:30` and the UTC epoch corresponds to 12:00:00Z
- **THEN** the local time is 17:30:00

#### Scenario: Zone without daylight saving
- **WHEN** the stored timezone is `<-03>3` and the UTC epoch corresponds to 15:00:00Z
- **THEN** the local time is 12:00:00 on every date of the year

#### Scenario: UTC-derived values are unaffected
- **WHEN** any timezone is configured
- **THEN** values obtained from `time()` and `gettimeofday()` remain UTC
- **THEN** TLS certificate validity checks performed during an OTA update are unaffected

### Requirement: Countdown timers are unaffected by timezone
A countdown timer SHALL be evaluated against the absolute UTC epoch and SHALL NOT be affected by a change of timezone or by a daylight-saving transition occurring during its countdown.

#### Scenario: Timezone changed mid-countdown
- **WHEN** a 2-hour countdown timer is running and the timezone is changed
- **THEN** the timer's remaining time is unchanged and it still triggers 2 hours after it was set

#### Scenario: Transition during countdown
- **WHEN** a 4-hour countdown timer is set at 00:00 local on a day where the clock jumps forward at 02:00
- **THEN** the timer triggers 4 real hours later, at 05:00 local

### Requirement: A schedule fires exactly once per local day
A schedule SHALL be evaluated against local wall-clock time and SHALL trigger at most once per local calendar day, and only on a local weekday selected in its weekday mask, and never while paused. The device SHALL record the local day on which each schedule last fired and SHALL use that record, not an absolute time, to suppress repeat triggers. The weekday and the day-of-year used for these checks SHALL be derived from the same local-time conversion.

#### Scenario: Ordinary day
- **WHEN** a schedule is set for 07:00 with every weekday selected and the local clock passes 07:00
- **THEN** the schedule triggers exactly once
- **THEN** it does not trigger again before the next local day

#### Scenario: Repeated wall-clock hour on a fall-back day
- **WHEN** a schedule is set for 02:30 and the local clock runs 02:00–02:59, is set back one hour, and runs 02:00–02:59 again
- **THEN** the schedule triggers at the first 02:30 only
- **THEN** the second 02:30 does not trigger it

#### Scenario: Wall-clock time skipped on a spring-forward day
- **WHEN** a schedule is set for 02:30 and the local clock jumps from 01:59:59 to 03:00:00
- **THEN** the schedule does not trigger on that day
- **THEN** it triggers normally at 02:30 on the following day

#### Scenario: Schedule survives a timezone change
- **WHEN** a schedule is set for 07:00 and the timezone is changed to one with a different offset
- **THEN** the schedule still triggers at 07:00 in the newly configured local time

#### Scenario: Unselected weekday
- **WHEN** a schedule is set for 07:00 with Monday to Friday selected and the local clock passes 07:00 on a Saturday
- **THEN** the schedule does not trigger
- **THEN** it triggers at 07:00 on the following Monday

#### Scenario: Weekday agrees with day-of-year on a fall-back night
- **WHEN** a schedule is set for 02:30 with only Sunday selected in a zone whose fall-back transition occurs at 03:00 on a Sunday
- **THEN** both 02:30 instants of that night are evaluated as Sunday
- **THEN** the schedule triggers at the first 02:30 only

### Requirement: Timezone is read and written over HTTP as a POSIX string
`GET /api/timers` SHALL report the configured POSIX `TZ` string together with the abbreviation, offset in minutes and daylight-saving state currently in effect. `POST /api/timers/timezone` SHALL accept the timezone as a POSIX `TZ` string and SHALL reject a request that omits it or supplies a syntactically invalid one.

#### Scenario: Reading the timezone during daylight saving
- **WHEN** the stored timezone is `CET-1CEST,M3.5.0,M10.5.0/3` and daylight saving is currently in effect
- **THEN** `GET /api/timers` returns `timezone: "CET-1CEST,M3.5.0,M10.5.0/3"`, `tz_abbrev: "CEST"`, `tz_offset_minutes: 120` and `is_dst: true`

#### Scenario: Reading the timezone outside daylight saving
- **WHEN** the same timezone is stored and daylight saving is not in effect
- **THEN** `GET /api/timers` returns `tz_abbrev: "CET"`, `tz_offset_minutes: 60` and `is_dst: false`

#### Scenario: Setting a timezone
- **WHEN** a client sends `POST /api/timers/timezone` with `{"tz": "EST5EDT,M3.2.0,M11.1.0"}`
- **THEN** the response is `200` with `success: true`
- **THEN** the value is persisted and a subsequent `GET /api/timers` reports it
- **THEN** local time reflects the new zone within one scheduler iteration

#### Scenario: Missing or invalid timezone rejected
- **WHEN** a client sends `POST /api/timers/timezone` with a body that omits `tz`, or whose `tz` is empty, longer than 63 characters, or contains characters outside printable ASCII
- **THEN** the response is `400` with `success: false` and an error message
- **THEN** the stored timezone is unchanged

#### Scenario: Timezone applied without disturbing a live evaluation
- **WHEN** a timezone change is accepted while the scheduler is running
- **THEN** the change is applied by the scheduler task rather than by the request handler
- **THEN** no alarm evaluation observes a partially applied timezone

### Requirement: Timezone is chosen by place on the timers page
The timers page SHALL offer a list of named regions whose selectable value is the corresponding POSIX `TZ` string, SHALL provide a control that detects the browser's timezone and preselects the matching region without saving it, and SHALL provide an advanced field for entering a POSIX `TZ` string directly. Committing a selection SHALL require an explicit save action.

#### Scenario: Region list round-trips
- **WHEN** the page loads and the device reports a timezone matching one of the listed regions
- **THEN** that region is selected in the list
- **THEN** no additional request is needed to resolve the region's label

#### Scenario: Detecting the browser timezone
- **WHEN** the user activates the detect control and the browser reports an IANA zone matching a listed region
- **THEN** that region is preselected and visually indicated
- **THEN** no request is sent to the device until the user saves

#### Scenario: Browser timezone not listed
- **WHEN** the browser reports an IANA zone that matches no listed region
- **THEN** the page selects a region whose current offset and daylight-saving behaviour agree with the browser's, or if none does, leaves the selection unchanged and reports that the zone could not be matched

#### Scenario: Advanced string entered
- **WHEN** the user enters a POSIX `TZ` string in the advanced field and saves
- **THEN** that exact string is sent to the device
- **THEN** on reload the string is shown in the advanced field, since it matches no listed region

#### Scenario: Current time displays the effective zone
- **WHEN** the page displays the device's current time
- **THEN** the heading shows the abbreviation and effective offset reported by the device, rather than a fixed offset label

### Requirement: A fixed-offset timezone is surfaced to the user
When the configured timezone expresses a fixed UTC offset with no daylight-saving rule and matches no listed region, the timers page SHALL inform the user that daylight saving will not be applied and SHALL prompt them to select a region.

#### Scenario: Migrated device
- **WHEN** the device reports timezone `UTC-1`
- **THEN** the page shows a notice that the timezone is a fixed UTC offset which will not follow daylight saving, and prompts the user to pick their region
- **THEN** the device continues to keep correct fixed-offset time until the user acts

#### Scenario: Listed fixed-offset region
- **WHEN** the device reports a fixed-offset timezone that does match a listed region, such as `JST-9`
- **THEN** no notice is shown, because the absence of daylight saving is correct for that region

### Requirement: Existing timezone and alarm state is migrated on first boot
On first boot of firmware supporting POSIX timezones, the device SHALL convert a previously stored whole-hour UTC offset into an equivalent fixed-offset POSIX `TZ` string, preserving the device's previous timekeeping behaviour, and SHALL remove the superseded stored value. The migration SHALL run at most once and SHALL be safe to interrupt.

#### Scenario: Legacy offset converted
- **WHEN** the device has a stored offset of `+1` and no stored POSIX string
- **THEN** the stored timezone becomes `UTC-1`, which is UTC+1
- **THEN** local time is identical to what the previous firmware produced
- **THEN** the superseded offset value is removed from storage

#### Scenario: Negative legacy offset converted
- **WHEN** the device has a stored offset of `-5`
- **THEN** the stored timezone becomes `UTC+5`, which is UTC−5

#### Scenario: Migration runs once
- **WHEN** the device boots again after a successful migration
- **THEN** the stored POSIX string is read unchanged and no conversion is performed

#### Scenario: Interrupted migration
- **WHEN** power is lost after the legacy offset is read but before the converted value is persisted
- **THEN** the next boot finds the legacy value still present and performs the conversion again, with the same result

#### Scenario: Never-configured device
- **WHEN** the device has neither a stored offset nor a stored POSIX string
- **THEN** the timezone defaults to UTC

#### Scenario: Daily-alarm bookkeeping reclaimed
- **WHEN** a stored daily alarm carries a duration value that the previous firmware had overwritten with trigger bookkeeping
- **THEN** the migration clears that value and initialises the alarm's last-fired record to a value matching no calendar day
- **THEN** the alarm is eligible to fire at its next occurrence

### Requirement: A schedule carries a weekday mask
Each schedule SHALL carry a seven-bit weekday mask in which bit *n* corresponds to the C library's `tm_wday` value *n*, with Sunday as bit 0. A mask of `0x7F` SHALL mean every day. The mask SHALL be persisted with the schedule. A schedule stored by firmware that predates the mask SHALL load with every day selected.

#### Scenario: Monday-to-Friday mask
- **WHEN** a schedule is created for Monday, Tuesday, Wednesday, Thursday and Friday
- **THEN** its stored mask is `0x3E`

#### Scenario: Legacy schedule loads as every day
- **WHEN** the device boots with a stored schedule that has no weekday-mask key in persistent storage
- **THEN** the schedule loads with mask `0x7F`
- **THEN** it fires at its time on every day, exactly as before the update

### Requirement: A schedule can be paused and resumed without losing its settings
A schedule SHALL have a paused state. A paused schedule SHALL retain its time, action, preset, weekday mask and slot, SHALL remain listed, and SHALL NOT trigger. Pausing or resuming SHALL NOT modify the record of the day on which the schedule last fired. The paused state SHALL be persisted. A schedule stored by firmware that predates the paused state SHALL load as not paused. Countdown timers SHALL NOT be pausable.

#### Scenario: Paused schedule does not fire
- **WHEN** a schedule set for 07:00 is paused and the local clock passes 07:00
- **THEN** the schedule does not trigger
- **THEN** the schedule is still present in the device's timer list with its settings unchanged

#### Scenario: Resume after today's firing does not refire
- **WHEN** a schedule set for 07:00 fired at 07:00, is paused at 08:00, and is resumed at 09:00 the same local day
- **THEN** the schedule does not trigger again that day
- **THEN** it triggers at 07:00 the next selected day

#### Scenario: Resume before today's time fires today
- **WHEN** a schedule set for 07:00 has been paused for several days and is resumed at 06:59 on a selected weekday
- **THEN** the schedule triggers at 07:00 that day

#### Scenario: Paused state survives a reboot
- **WHEN** a schedule is paused and the device reboots
- **THEN** the schedule loads as paused and does not trigger

#### Scenario: Legacy schedule loads as armed
- **WHEN** the device boots with a stored schedule that has no paused key in persistent storage
- **THEN** the schedule loads as not paused

### Requirement: A schedule can be updated in place
Setting a schedule at the index of an occupied schedule slot SHALL replace that schedule's time, action, preset and weekday mask while keeping the slot and the schedule's paused state. The updated schedule SHALL be eligible to fire at its next occurrence, including later the same day.

#### Scenario: Update keeps the slot
- **WHEN** slot 2 holds a schedule for 07:00 and a schedule request for 08:00 names index 2
- **THEN** slot 2 holds a schedule for 08:00 and no other slot is changed

#### Scenario: Update keeps the paused state
- **WHEN** a paused schedule is updated to a new time
- **THEN** the schedule remains paused

#### Scenario: Update is eligible today
- **WHEN** a schedule that fired at 07:00 today is updated at 09:00 to 10:00
- **THEN** it triggers at 10:00 today

### Requirement: Schedule weekday mask and paused state are exposed over HTTP
`POST /api/timers/schedule` (with `POST /api/timers/alarm` accepted as an alias for existing clients) SHALL accept an optional integer `days` in the range 1 to 127 as the weekday mask, defaulting to 127 when omitted, and SHALL reject a value of 0 or a value above 127. `POST /api/timers/pause` SHALL accept `{"index": n, "paused": bool}` and SHALL set the paused state of the schedule in slot *n*, rejecting an index that is out of range, an empty slot, or a countdown timer. `GET /api/timers` SHALL report `paused` and `days_mask` for every occupied slot.

#### Scenario: Create with weekday mask
- **WHEN** `POST /api/timers/schedule` is called with `{"hour":7,"minute":0,"days":62}`
- **THEN** the response has `success: true`
- **THEN** `GET /api/timers` reports that schedule with `days_mask: 62` and `paused: false`

#### Scenario: Days omitted defaults to every day
- **WHEN** `POST /api/timers/schedule` is called without `days`
- **THEN** the schedule is created with `days_mask: 127`

#### Scenario: Empty mask rejected
- **WHEN** `POST /api/timers/schedule` is called with `"days": 0`
- **THEN** the response status is 400 and the body has `success: false`
- **THEN** no slot is changed

#### Scenario: Pause a schedule
- **WHEN** `POST /api/timers/pause` is called with `{"index":1,"paused":true}` and slot 1 holds a schedule
- **THEN** the response has `success: true`
- **THEN** `GET /api/timers` reports slot 1 with `paused: true`

#### Scenario: Pause a countdown rejected
- **WHEN** `POST /api/timers/pause` is called for a slot holding a countdown timer
- **THEN** the response status is 400 and the body has `success: false`
- **THEN** the countdown continues unchanged

#### Scenario: Pause an empty slot rejected
- **WHEN** `POST /api/timers/pause` is called for an unoccupied slot
- **THEN** the response status is 400 and the body has `success: false`

### Requirement: The device provides twelve timer slots
The device SHALL provide twelve timer slots, each holding a countdown timer or a schedule. Every slot SHALL be persisted under keys no longer than the persistent store's limit. Slots written by firmware that provided four slots SHALL load unchanged.

#### Scenario: Slot indices up to eleven are accepted
- **WHEN** a schedule request names index 11
- **THEN** the request succeeds and `GET /api/timers` reports twelve entries

#### Scenario: Legacy slots survive the upgrade
- **WHEN** the device boots with four slots stored by the previous firmware
- **THEN** those four slots load with their previous contents and slots 4 to 11 are empty

### Requirement: Remaining time to a schedule honours the weekday mask
The remaining-seconds value reported for a schedule SHALL be the wall-clock seconds until its next occurrence on a selected weekday, looking at most seven days ahead. For a paused schedule the value SHALL be 0.

#### Scenario: Next occurrence skips unselected days
- **WHEN** it is Friday 12:00 local and a schedule is set for 07:00 with Monday to Friday selected
- **THEN** the reported remaining time is the wall-clock seconds until Monday 07:00

#### Scenario: Next occurrence is today
- **WHEN** it is Monday 06:00 local and a schedule is set for 07:00 with Monday selected
- **THEN** the reported remaining time is 3600

#### Scenario: Paused schedule reports zero
- **WHEN** a schedule is paused
- **THEN** its reported remaining time is 0

### Requirement: The timers page shows a single Countdown timer section with preset shortcuts

The timers page SHALL present a countdown timer control as a single section whose heading reads "Countdown timer". The section SHALL contain a row of four preset duration shortcuts above the duration input, labelled "15 min", "30 min", "1 hour" and "2 hours", and SHALL contain a form for arbitrary durations and actions below the preset row. The form SHALL expose a duration input in minutes, an action select with options "Turn Off LEDs" and "Load preset", a conditional preset selector shown only when the action is "Load preset", and a submit button labelled "Set Countdown Timer". The submit button SHALL be the only path that creates a countdown timer. No separate "Quick Off Timer" section SHALL be present.

#### Scenario: Page layout
- **WHEN** the timers page is displayed
- **THEN** the page shows one Countdown timer section, not two
- **THEN** the section's heading reads "Countdown timer"
- **THEN** the section contains a preset row of four buttons and a form, in that order

#### Scenario: Preset buttons styled as secondary shortcuts
- **WHEN** the preset row is rendered
- **THEN** each preset button uses the secondary small button styling and reads as visually subordinate to the "Set Countdown Timer" submit button

### Requirement: A preset click pre-fills the countdown form and does not submit

Activating a preset button SHALL write the corresponding number of minutes into the duration input, SHALL set the action select to "Turn Off LEDs", SHALL hide the preset selector group, and SHALL focus the duration input. Activating a preset button SHALL NOT send a request to the device.

#### Scenario: Pre-fill duration in minutes
- **WHEN** the user activates the "30 min" preset
- **THEN** the duration input shows `30`
- **THEN** no POST is sent to `/api/timers/countdown`

#### Scenario: Pre-fill resets action to off
- **WHEN** the user had previously set the action to "Load preset" with a selected preset and then activates any preset button
- **THEN** the action select shows "Turn Off LEDs"
- **THEN** the preset selector group is hidden

#### Scenario: Focus moves to the duration input
- **WHEN** the user activates any preset button
- **THEN** the duration input receives keyboard focus so the value can be edited immediately

### Requirement: The Countdown timer section's submit button creates the timer

Submitting the Countdown timer section's form, regardless of whether the duration was typed in or selected via a preset, SHALL create a single countdown timer via `POST /api/timers/countdown` using the form's current duration, action and preset values. The active timers list SHALL refresh after a successful submission.

#### Scenario: Submit after preset click
- **WHEN** the user activates the "15 min" preset and then clicks "Set Countdown Timer"
- **THEN** a countdown with duration 15 minutes and action "Turn Off LEDs" is created
- **THEN** the active timers list shows the new countdown

#### Scenario: Submit with edited preset duration
- **WHEN** the user activates the "30 min" preset, changes the duration input to 45, and clicks "Set Countdown Timer"
- **THEN** a countdown with duration 45 minutes and action "Turn Off LEDs" is created

