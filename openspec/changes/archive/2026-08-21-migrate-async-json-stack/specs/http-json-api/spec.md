## ADDED Requirements

### Requirement: A request body is parsed only after it has been fully received
An endpoint that accepts a JSON request body SHALL reassemble the complete body before deserializing it, regardless of how many TCP segments the body arrives in. An endpoint SHALL NOT deserialize a partial body, and SHALL NOT discard body data that arrives after the first segment.

#### Scenario: Body spanning multiple segments
- **WHEN** a client sends `POST /api/show` with a body of 3000 bytes that the network delivers in three segments
- **THEN** the handler is invoked once with the complete 3000-byte document deserialized
- **THEN** the request does not fail with a deserialization error

#### Scenario: Body within a single segment
- **WHEN** a client sends `POST /api/brightness` with the body `{"value":128}`
- **THEN** the handler is invoked once with that document
- **THEN** the response is identical to the response the endpoint returned before this change

#### Scenario: Large show parameters round-trip
- **WHEN** a ColorRanges show with enough colours and nested ranges to exceed one TCP segment is applied via `POST /api/show`
- **THEN** the show is applied with every colour and range present
- **WHEN** that show is then saved via `POST /api/presets` and reloaded via `POST /api/presets/load`
- **THEN** the restored parameters equal the ones that were applied

### Requirement: JSON document capacity is not fixed at compile time
No JSON document in the firmware SHALL be declared with a compile-time capacity. Serializing a response SHALL NOT silently truncate it, and deserializing a well-formed request SHALL NOT fail because of a document size limit.

#### Scenario: Status response carries full show parameters
- **WHEN** `GET /api/status` is requested while a show with large parameters is active
- **THEN** the response contains the complete `show_params` object
- **THEN** the response is well-formed JSON

#### Scenario: No fixed-capacity document types remain
- **WHEN** the firmware sources are searched for `StaticJsonDocument`, `DynamicJsonDocument`, or a `JSON_DOC_` capacity constant
- **THEN** no occurrence is found

#### Scenario: Deprecated document APIs are rejected at build time
- **WHEN** the ESP32 environment is built
- **THEN** a use of a deprecated ArduinoJson declaration fails the build rather than emitting a warning

### Requirement: Body-carrying requests must declare a JSON content type
An endpoint that accepts a JSON request body SHALL only handle requests whose `Content-Type` is `application/json`, compared case-insensitively. A body-carrying request that omits or misstates the header SHALL NOT reach the endpoint's handler. Every request the device's own web pages issue with a body SHALL send this header.

#### Scenario: Correct content type
- **WHEN** a client sends `POST /api/timers/countdown` with `Content-Type: application/json` and a valid body
- **THEN** the timer is set and the endpoint's normal response is returned

#### Scenario: Missing content type in station mode
- **WHEN** a client sends `POST /api/brightness` with a valid body and no `Content-Type` header
- **THEN** the brightness is not changed
- **THEN** the request is answered by the not-found handler rather than by the endpoint

#### Scenario: Missing content type in access-point mode
- **WHEN** the device is in access-point mode and a client sends a body-carrying request without `Content-Type: application/json`
- **THEN** the request is answered by the captive-portal redirect, not by the endpoint

#### Scenario: Bodyless endpoints are unaffected
- **WHEN** `POST /api/settings/factory-reset` or `POST /api/ota/update` is requested without a `Content-Type` header and without a body
- **THEN** the endpoint handles the request as before

### Requirement: Body-carrying routes match their URI exactly
An endpoint that accepts a JSON request body SHALL match only the exact request path it is registered for, not that path as a prefix. Correct dispatch SHALL NOT depend on the order in which endpoints are registered. Each such endpoint SHALL also accept only the HTTP methods it implements, rather than inheriting a wider default set.

#### Scenario: Nested path is not shadowed by its parent
- **WHEN** `POST /api/presets/load` is requested
- **THEN** it is handled by the preset-load endpoint
- **THEN** it is not handled by the `POST /api/presets` save endpoint, regardless of which was registered first

#### Scenario: Parent path is not shadowed by its child
- **WHEN** `POST /api/presets` is requested
- **THEN** it is handled by the preset-save endpoint

#### Scenario: Sibling paths sharing a prefix without a path boundary
- **WHEN** `POST /api/settings/device-name` is requested
- **THEN** it is handled by the device-name endpoint and not by the `POST /api/settings/device` endpoint

#### Scenario: Trailing segment does not match
- **WHEN** `POST /api/brightness/extra` is requested
- **THEN** it is not handled by the `POST /api/brightness` endpoint

#### Scenario: Unimplemented method is not accepted
- **WHEN** `PUT /api/brightness` is requested with a valid JSON body
- **THEN** it is not handled by the brightness endpoint, which implements only `POST`

### Requirement: Malformed and oversized request bodies are rejected by status code
A request whose body is not well-formed JSON SHALL be answered with `400`. A request whose body exceeds the accepted maximum length SHALL be answered with `413`. These responses carry a status code only and SHALL NOT be relied upon to carry a JSON error envelope. A request that is well-formed but semantically invalid SHALL continue to be answered by its endpoint with that endpoint's existing JSON error envelope.

#### Scenario: Malformed JSON
- **WHEN** a client sends `POST /api/show` with `Content-Type: application/json` and the body `{"name":`
- **THEN** the response status is 400
- **THEN** no show change occurs

#### Scenario: Oversized body
- **WHEN** a client sends a body-carrying request whose length exceeds the accepted maximum
- **THEN** the response status is 413
- **THEN** the device does not allocate a buffer for the full declared length

#### Scenario: Well-formed but invalid content
- **WHEN** a client sends `DELETE /api/timers` with the body `{"index":99}` and 99 is out of range
- **THEN** the response status and JSON error envelope are what the endpoint returned before this change

### Requirement: Successful responses are unchanged by transport-level handling
For every endpoint, a request that succeeded before this change SHALL produce a byte-identical response body after it.

#### Scenario: Status endpoint
- **WHEN** `GET /api/status` is requested with a given device state
- **THEN** the response body has the same fields, values and ordering as before the change

#### Scenario: Timers endpoint
- **WHEN** `GET /api/timers` is requested with a given timer configuration
- **THEN** the response body has the same fields, values and ordering as before the change

#### Scenario: Show list endpoint
- **WHEN** `GET /api/shows` is requested
- **THEN** the response lists the same shows with the same names and descriptions as before the change

### Requirement: Every served request is recorded
The device SHALL observe each handled HTTP request in a single place that applies to all endpoints regardless of how they are registered, and SHALL use that observation both to emit an access log entry and to record that the device has served at least one request.

#### Scenario: Request marks the device as having served traffic
- **WHEN** the device has just started and any HTTP endpoint is requested successfully
- **THEN** a subsequent check of whether the device has served a request returns true

#### Scenario: Observation covers all registration styles
- **WHEN** a body-carrying JSON endpoint is requested
- **THEN** it is observed identically to a plain `GET` endpoint

#### Scenario: OTA auto-confirm gate can be satisfied
- **WHEN** a firmware image is running unconfirmed and auto-confirmation requires that a request has been served
- **THEN** serving any HTTP request satisfies that condition

#### Scenario: Log entry does not read freed memory
- **WHEN** an access log entry is emitted for a request
- **THEN** the client address, URL and method it reports are the values from that request
