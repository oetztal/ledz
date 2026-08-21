## ADDED Requirements

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

### Requirement: A daily alarm fires exactly once per local day
A daily alarm SHALL be evaluated against local wall-clock time and SHALL trigger at most once per local calendar day. The device SHALL record the local day on which each alarm last fired and SHALL use that record, not an absolute time, to suppress repeat triggers.

#### Scenario: Ordinary day
- **WHEN** a daily alarm is set for 07:00 and the local clock passes 07:00
- **THEN** the alarm triggers exactly once
- **THEN** it does not trigger again before the next local day

#### Scenario: Repeated wall-clock hour on a fall-back day
- **WHEN** a daily alarm is set for 02:30 and the local clock runs 02:00–02:59, is set back one hour, and runs 02:00–02:59 again
- **THEN** the alarm triggers at the first 02:30 only
- **THEN** the second 02:30 does not trigger it

#### Scenario: Wall-clock time skipped on a spring-forward day
- **WHEN** a daily alarm is set for 02:30 and the local clock jumps from 01:59:59 to 03:00:00
- **THEN** the alarm does not trigger on that day
- **THEN** it triggers normally at 02:30 on the following day

#### Scenario: Alarm survives a timezone change
- **WHEN** a daily alarm is set for 07:00 and the timezone is changed to one with a different offset
- **THEN** the alarm still triggers at 07:00 in the newly configured local time

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
