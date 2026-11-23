## Why

After setup the device shows only a placeholder screen. The purpose of
`remote-control` is to turn the remotes built in Home Assistant into a
physical remote control on the M5Paper. It also needs to show at a glance
whether it is connected, what time it is and how much battery is left, and
let the user reach setup mode without a button held at startup.

## What Changes

- Connect to Wi-Fi and keep one authenticated WebSocket connection to Home
  Assistant, with automatic reconnect.
- Read the configured dashboard. Each view of the dashboard is one remote.
- Draw the Bubble Card cards of a view: `separator`, `button`,
  `media-player` (with its implicit play/pause, previous, next and power
  controls), and `sub-buttons` rows. Show icons and names. Skip unsupported
  cards.
- Run a button's `tap_action` on tap and its `hold_action` on a long press,
  following the mapping in `docs/dev/home-assistant-api.md`.
- Page between remotes when the dashboard has more than one view, with the
  rocker switch, swipes or arrows, and show which page is active.
- Keep media player tiles up to date: title, state and cover art.
- Reload the layout when the dashboard is edited in Home Assistant.
- Add a status bar at the top of the screen with the Wi-Fi signal, date and
  time (from NTP, in Home Assistant's time zone, kept by the real-time clock),
  the battery level, and a setup button that restarts into setup mode after a
  confirmation.
- Replace the placeholder screen of `add-initial-setup`, and show clear
  screens for "connecting", "token rejected" and "dashboard not found".

## Capabilities

### New Capabilities

- `ha-connection`: Wi-Fi station mode, the WebSocket session (auth, commands,
  subscriptions), reconnecting, and calling services.
- `remote-layout`: Turning the dashboard configuration into remotes, pages and
  buttons, and reloading on changes.
- `remote-ui`: Drawing remotes, paging, touch input, running actions, live
  media tiles and error screens.
- `status-bar`: Wi-Fi, date and time, battery and the setup button at the top
  of every remote page.

### Modified Capabilities

- `setup-mode`: it can also be entered from the status bar's setup button.
- `device-configuration`: with a valid configuration, the device shows the
  remotes instead of the placeholder screen.

## Non-goals

- Card types other than the four listed, and conditional or stacked cards.
- `double_tap_action`, sliders, and editing the layout on the device.
- Evaluating Jinja templates in card options such as `state_content`. A
  built-in default text is used instead.
- Deep sleep and power optimization beyond Wi-Fi modem sleep.
- Translating the UI. Texts are in English.
- More than one dashboard.

## Impact

- Depends on `setup-firmware-build` and `add-initial-setup` (configuration,
  LittleFS, M5Unified screen module, partition table).
- New components: `espressif/esp_websocket_client`, and `m5stack/m5unified`
  (clock, battery, touch, buttons), already added by `add-initial-setup`. Built-in:
  `esp_sntp`.
- New generated assets: Material Design Icons as bitmaps (Apache 2.0), made by
  a script run with the toolchain's Python, and a time-zone table.
