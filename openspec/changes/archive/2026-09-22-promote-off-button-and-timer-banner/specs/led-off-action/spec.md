## MODIFIED Requirements

### Requirement: Control page exposes an Off action that switches the strip to all-black

The control page SHALL display a single Off button as a circular floating action positioned at the top-right of `.content`, anchored at `top: 24px; right: 24px` relative to its positioning context. The button SHALL be a 48×48 circle, SHALL carry the `btn-danger` red palette, SHALL render a power-symbol SVG icon (line + arc, IEC 60417-5009) as its only visible content, and SHALL carry `aria-label="Turn off LEDs"` and `title="Turn off LEDs"` to describe the action to assistive technologies and pointer-only users. The button SHALL NOT live inside `.status-bar` and SHALL NOT be a sibling of the Current Show or Brightness status items. Activating the button SHALL change the active show to `Solid` with a single black colour (`colors=[[0,0,0]]`) by issuing a `POST /api/show` request with that payload. The button SHALL send exactly one request per activation.

#### Scenario: User activates Off
- **WHEN** the user activates the Off button on the control page
- **THEN** the browser sends `POST /api/show` with body `{"name":"Solid","params":{"colors":[[0,0,0]]}}`
- **THEN** the LED strip renders all pixels at colour (0, 0, 0) within one render cycle
- **THEN** the device returns 200 to the request

#### Scenario: Off button is disabled while the request is in flight
- **WHEN** the user activates the Off button and the request has not yet completed
- **THEN** the button is disabled and ignores further activations until the request settles
- **WHEN** the request completes (success or failure)
- **THEN** the button is re-enabled

#### Scenario: Off button is a top-right floating action
- **WHEN** the control page is rendered
- **THEN** the Off button is positioned absolutely at the top-right of `.content`
- **THEN** the Off button is not a child of `.status-bar`

#### Scenario: Off button is icon-only with a textual fallback
- **WHEN** the control page is rendered
- **THEN** the Off button's visible content is a single power-symbol SVG icon
- **THEN** the button carries `aria-label="Turn off LEDs"` and `title="Turn off LEDs"` as accessible labels
