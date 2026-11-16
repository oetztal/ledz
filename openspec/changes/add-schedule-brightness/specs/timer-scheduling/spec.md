## MODIFIED Requirements

### Requirement: A schedule can be updated in place
Setting a schedule at the index of an occupied schedule slot SHALL replace that schedule's time, action, preset, weekday mask and optional brightness while keeping the slot and the schedule's paused state. An update that supplies no brightness SHALL store no brightness value, replacing any value the schedule previously carried, so the updated schedule does not change the device's brightness when it fires. The updated schedule SHALL be eligible to fire at its next occurrence, including later the same day.

#### Scenario: Update keeps the slot
- **WHEN** slot 2 holds a schedule for 07:00 and a schedule request for 08:00 names index 2
- **THEN** slot 2 holds a schedule for 08:00 and no other slot is changed

#### Scenario: Update keeps the paused state
- **WHEN** a paused schedule is updated to a new time
- **THEN** the schedule remains paused

#### Scenario: Update is eligible today
- **WHEN** a schedule that fired at 07:00 today is updated at 09:00 to 10:00
- **THEN** it triggers at 10:00 today

#### Scenario: Update replaces the brightness
- **WHEN** a schedule with brightness 64 is updated by a request that supplies brightness 200
- **THEN** the updated schedule fires at brightness 200

#### Scenario: Update without a brightness clears it
- **WHEN** a schedule with brightness 64 is updated in place by a request that supplies no brightness
- **THEN** the updated schedule has no brightness value set
- **THEN** firing it does not change the device's brightness

## ADDED Requirements

### Requirement: A schedule can carry an optional brightness value
Each schedule SHALL carry a brightness value with a distinct "not set" state, separate from the value 0. When a schedule whose action is "Load preset" fires and a brightness value is set, the device SHALL set its global brightness to that value, exactly as a brightness change from the control page would, and SHALL persist it as the device's current brightness. When a schedule fires with no brightness value set, the device SHALL leave the current brightness unchanged. A schedule whose action is "Turn off LEDs" SHALL ignore any brightness value it carries and SHALL NOT change the global brightness. A schedule stored by firmware that predates the brightness value SHALL load with no brightness value set and, when it fires, SHALL leave the current brightness unchanged.

#### Scenario: Firing with a brightness set overrides the current brightness
- **WHEN** the current brightness is 200 and a schedule with the action "Load preset" and brightness 64 fires
- **THEN** the device's brightness becomes 64

#### Scenario: Firing without a brightness leaves it unchanged
- **WHEN** the current brightness is 200 and a schedule with no brightness value fires
- **THEN** the device's brightness remains 200

#### Scenario: Zero is a valid brightness
- **WHEN** the current brightness is 200 and a schedule with the action "Load preset" and brightness 0 fires
- **THEN** the device's brightness becomes 0

#### Scenario: Brightness applies alongside a preset load
- **WHEN** a schedule with brightness 64 and the action "Load preset" fires
- **THEN** the preset is loaded and the device's brightness becomes 64

#### Scenario: Turn-off schedule ignores the brightness
- **WHEN** the current brightness is 200 and a schedule with the action "Turn off LEDs" and brightness 64 fires
- **THEN** the LEDs are turned off and the device's brightness remains 200

#### Scenario: Legacy schedule leaves brightness unchanged
- **WHEN** the device boots with a stored schedule that has no brightness value
- **THEN** the schedule loads with no brightness value set
- **THEN** firing it does not change the device's brightness

### Requirement: Schedule brightness is set and read over HTTP
`POST /api/timers/schedule` (with `POST /api/timers/alarm` accepted as an alias for existing clients) SHALL accept an optional integer `brightness` in the range 0 to 255. When `brightness` is present and valid it SHALL become the schedule's brightness value; when it is absent or `null` the schedule SHALL be stored with no brightness value set, replacing any value previously stored on that schedule, including when an existing schedule is updated in place, so the updated schedule does not change the device's brightness when it fires. A supplied `brightness` that is not an integer in the range 0 to 255 SHALL be rejected with status 400, and no slot SHALL be changed. `GET /api/timers` SHALL report `brightness` for every occupied schedule as an integer from 0 to 255 when set, and as `null` when no brightness value is set. Countdown timers SHALL NOT report a brightness.

#### Scenario: Create with a brightness
- **WHEN** `POST /api/timers/schedule` is called with `{"hour":7,"minute":0,"brightness":64}`
- **THEN** the response has `success: true`
- **THEN** `GET /api/timers` reports that schedule with `brightness: 64`

#### Scenario: Create without a brightness
- **WHEN** `POST /api/timers/schedule` is called without `brightness`
- **THEN** `GET /api/timers` reports that schedule with `brightness: null`

#### Scenario: Update clears the brightness when it is omitted
- **WHEN** a schedule with brightness 64 is updated through `POST /api/timers/schedule` without `brightness`
- **THEN** `GET /api/timers` reports that schedule with `brightness: null`

#### Scenario: Out-of-range brightness rejected
- **WHEN** `POST /api/timers/schedule` is called with `"brightness": 256`
- **THEN** the response status is 400 and the body has `success: false`
- **THEN** no slot is changed

#### Scenario: Negative brightness rejected
- **WHEN** `POST /api/timers/schedule` is called with `"brightness": -1`
- **THEN** the response status is 400 and the body has `success: false`
- **THEN** no slot is changed
