# ha-connection Specification

## Purpose

How the device stays connected: Wi-Fi station mode, one authenticated
WebSocket session to Home Assistant, reconnecting after outages, calling
services, and following entity states and Home Assistant's time zone.

## Requirements

### Requirement: Wi-Fi station connection

With a valid configuration, the device SHALL connect to `wifi.ssid` with
`wifi.password` and obtain an address through DHCP. After a disconnect it MUST
retry, with the delay doubling from 1 second up to at most 60 seconds.

#### Scenario: Connect at startup

- **WHEN** the device starts with a valid configuration and the network is in
  range
- **THEN** it connects and gets an IP address

#### Scenario: Network temporarily lost

- **WHEN** the access point disappears and returns after 2 minutes
- **THEN** the device reconnects within 60 seconds after it returns, without a
  restart

### Requirement: Authenticated WebSocket session

Once Wi-Fi is up, the device SHALL open one WebSocket connection to the
WebSocket URL derived from `home_assistant.url` (`http` → `ws`, `https` →
`wss`, plus `/api/websocket`). It authenticates as described in
`docs/dev/home-assistant-api.md`: `auth` is the first message and has no
`id`. Commands use increasing `id`s and wait for their `result`.

#### Scenario: Successful authentication

- **WHEN** Home Assistant answers `auth_required` and the token is valid
- **THEN** the device receives `auth_ok` and starts sending commands

#### Scenario: Token rejected

- **WHEN** Home Assistant answers `auth_invalid`
- **THEN** the device stops reconnecting and reports "token rejected" to the
  UI

### Requirement: Session recovery

If the WebSocket connection drops, or Home Assistant restarts, the device
SHALL reconnect with the same backoff as Wi-Fi. After each new `auth_ok` it
MUST renew all subscriptions and read the layout again.

#### Scenario: Home Assistant restart

- **WHEN** Home Assistant restarts while the device is connected
- **THEN** the device reconnects, subscribes again, and the remote keeps
  working without user action

### Requirement: Calling services

The device SHALL call services over the open WebSocket connection with
`{"type":"call_service","domain":…,"service":…,"service_data":…,"target":…}`.
It reports the `success` of the `result`, and the error `message` if there is
one, to the caller.

#### Scenario: Button press

- **WHEN** a `button.press` with target `button.wohnzimmer_apple_tv_wohnzimmer_up`
  is called
- **THEN** a `call_service` message is sent and the result is reported as
  successful

#### Scenario: Unknown service

- **WHEN** a service that does not exist is called
- **THEN** the call fails and reports `Service <domain>.<service> not found.`

#### Scenario: Not connected

- **WHEN** a service is called while the WebSocket connection is down
- **THEN** the call fails at once with "not connected" and is not queued

### Requirement: Entity state subscription

The device SHALL subscribe with `subscribe_entities` to the entities that the
current layout shows state for, and pass every change to the UI. When the
layout changes, the subscription MUST be replaced with one for the new set of
entities.

#### Scenario: Media title changes

- **WHEN** the media player in the layout starts playing a new title
- **THEN** the UI receives the new `media_title` within 2 seconds

### Requirement: Home Assistant configuration

After authentication, the device SHALL read `get_config` and keep
`time_zone` for the status bar.

#### Scenario: Time zone known

- **WHEN** `get_config` returns `"time_zone":"Europe/Berlin"`
- **THEN** the status bar uses Central European time with daylight saving
