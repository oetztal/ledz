## RENAMED Requirements

- FROM: `### Requirement: A daily alarm fires exactly once per local day`
- TO: `### Requirement: A schedule fires exactly once per local day`

## MODIFIED Requirements

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

## ADDED Requirements

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
