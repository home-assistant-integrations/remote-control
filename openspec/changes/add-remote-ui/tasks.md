## 1. Prerequisites and platform check

- [ ] 1.1 Confirm that `setup-firmware-build` and `add-initial-setup` are applied: setup mode works and a configuration is stored
- [ ] 1.2 Confirm that `m5stack/m5unified` is in `idf_component.yml` and `M5.begin()` runs in `app_main` (both done by `add-initial-setup`), and that `make build` succeeds
- [ ] 1.3 On the device, log for M5Unified: the real-time clock time, the battery voltage, touch coordinates, and `BtnA`/`BtnB`/`BtnC` presses. Record which GPIO each button is
- [ ] 1.4 Run on battery only, without USB, and confirm that the device stays on (power hold on GPIO 2)
- [ ] 1.5 Confirm that the setup screen from `add-initial-setup` still renders (it already uses M5Unified)
- [ ] 1.6 Add `espressif/esp_websocket_client` to `idf_component.yml` and confirm that `make build` succeeds

## 2. Assets generator

- [ ] 2.1 Add `tools/gen_assets.py` and a `make assets` target that runs it with the uv Python and installs Pillow into `toolchain/`
- [ ] 2.2 Download the MDI webfont and its name-to-codepoint metadata at a pinned version, caching them in `toolchain/assets-cache/`
- [ ] 2.3 Add `assets/icons.txt` with every icon in your dashboard, the status-bar icons (wifi strength 1–4, wifi-off, wifi-alert, battery steps, battery-alert, cog, chevron-left, chevron-right, play, pause, skip-previous, skip-next, power, volume-minus, volume-plus) and about 60 common remote, media and home icons
- [ ] 2.4 Render each icon as 4-bit grayscale at 32 and 48 pixels into `components/assets/icons_gen.c`, with a sorted name table
- [ ] 2.5 Add `icon_find(name, size)` with binary search, and a host test that finds `mdi:chevron-up` and rejects an unknown name
- [ ] 2.6 Generate VLW fonts from Noto Sans at 20, 28 and 36 pixels, covering Latin-1, Latin Extended-A and `‹ › · …`, into `components/assets/fonts_gen.c`
- [ ] 2.7 Fetch the `posix_tz_db` table at a pinned commit, generate `tz_table_gen.c`, and add `tz_posix_for(iana)` with a host test for `Europe/Berlin`
- [ ] 2.8 Add the licence notices for MDI, Noto Sans and `posix_tz_db` to `assets/LICENSES.md`

## 3. Layout parser (pure C, host-tested)

- [ ] 3.1 Create `components/layout/layout.h` with `layout_t`, `remote_t`, `element_t`, `button_t`, `call_t` and the limits (16 remotes, 64 buttons)
- [ ] 3.2 Copy your dashboard's `lovelace/config` into `test/host/fixtures/media-control.json` and add the `test_layout` target to `make test-host`
- [ ] 3.3 Implement the view-to-remote mapping with the title fallbacks (`title`, then `path`, then "Remote N"), with tests for one view and for three views
- [ ] 3.4 Implement card discovery for masonry (`cards[]`) and sections (`sections[].cards[]`) views, skipping and logging unsupported cards, with tests
- [ ] 3.5 Implement `separator` and `button` cards, with tests
- [ ] 3.6 Implement `sub-buttons` rows from `sub_button.bottom[].group[]` with `justify_content`, `width`, `custom_height`, `show_icon` and `show_name`, with a test that the fixture gives rows of 1, 3, 1, 2 and 2 buttons
- [ ] 3.7 Implement the `media-player` tile with its implicit controls and the `hide.*_button` flags, with a test that the fixture has no volume controls
- [ ] 3.8 Implement the limits and their log messages, with a test using 80 buttons

## 4. Action mapping (pure C, host-tested)

- [ ] 4.1 Implement `action_to_call(action, entity, call_t *out)` for `perform-action`: split `domain.service`, `target` and `data`
- [ ] 4.2 Add the legacy `call-service` form (`service`, `service_data`), `toggle` (`homeassistant.toggle` with `entity`) and `none` or missing
- [ ] 4.3 Map the UI-only actions (`more-info`, `navigate`, `url`) to no call
- [ ] 4.4 Add host tests: every tap and hold action in the fixture gives the expected `domain`, `service` and target, and `back` holds give `…_back_long_press`

## 5. Home Assistant connection

- [ ] 5.1 Add `components/ha/wifi.c`: station mode from the configuration, DHCP, backoff from 1 to 60 seconds, connection events to the UI queue
- [ ] 5.2 Move the verified `ha_ws.c` from the API document into `components/ha`, and derive the WebSocket URL from `home_assistant.url`
- [ ] 5.3 Add `ha_client.c`: reconnect with backoff, stop on `auth_invalid` and post "token rejected"
- [ ] 5.4 After `auth_ok`, read `get_config` and post `time_zone`
- [ ] 5.5 After `auth_ok`, read `lovelace/config`, parse it, and post the layout. On `config_not_found`, post "dashboard not found"
- [ ] 5.6 Subscribe to `lovelace_updated` and read the layout again when the event names your dashboard
- [ ] 5.7 Subscribe with `subscribe_entities` to the layout's media players, post changes, and replace the subscription when the layout changes
- [ ] 5.8 Implement `ha_call(call_t)`: send `call_service`, post the result with success and message, and fail at once with "not connected" when offline

## 6. Status bar

- [ ] 6.1 Add `statusbar.cpp`: draw the 48-pixel bar, the separator line and the four slots, and refresh only the bar area
- [ ] 6.2 Draw the Wi-Fi icon from the RSSI thresholds, plus the off and alert states. Update it every 30 seconds and on connection events
- [ ] 6.3 Set the system time from the real-time clock at startup, and show `--.--.---- --:--` while the time is invalid
- [ ] 6.4 Start SNTP after Wi-Fi connects, resync every 6 hours, and write the time to the real-time clock after each sync
- [ ] 6.5 Apply the time zone with `tz_posix_for()`, falling back to UTC, and draw `DD.MM.YYYY HH:MM` on a timer aligned to each minute
- [ ] 6.6 Add battery measurement: 16 samples, linear 3.3 to 4.15 V, five icon steps and a percentage, updated every 60 seconds, with the alert icon below 10 %
- [ ] 6.7 Draw the gear icon with its 48 × 48-pixel touch area and post a tap to the UI task

## 7. Setup dialog and setup request

- [ ] 7.1 Add `dialog.cpp`: a centered modal box with text and "Cancel" and "Restart" buttons, drawn with `epd_quality`
- [ ] 7.2 Add the dialog's 15-second timeout, and restore the remote after "Cancel" or the timeout
- [ ] 7.3 On "Restart", write `setup_req=1` to NVS and call `esp_restart()`
- [ ] 7.4 Extend the boot flow of `add-initial-setup`: if `setup_req` is set, erase it and enter setup mode while keeping the stored configuration

## 8. Remote rendering

- [ ] 8.1 Add `ui_task.cpp`: the event queue, the state (current remote, layout, connection) and dispatch to the draw functions
- [ ] 8.2 Implement the measure pass (natural heights and widths) and the place pass (scale factor 0.6 to 1, hit rectangles), with a host test that the fixture fits at scale 1
- [ ] 8.3 Draw the title row: the title, and the `‹ ›` arrows and page dots when there are several remotes
- [ ] 8.4 Draw headings and button rows: rounded rectangles, icon and/or name, the text fallback for unknown icons
- [ ] 8.5 Draw the media tile: name, state line, the icon in place of the cover, and the control row with play or pause chosen by state
- [ ] 8.6 Fetch cover art when `entity_picture` changes (HTTP GET without a token, at most 200 KB) and draw it scaled in grayscale
- [ ] 8.7 Redraw the media tile with a partial refresh on state events, at most every 5 seconds
- [ ] 8.8 Draw the status screens (connecting to Wi-Fi, connecting to HA, token rejected, dashboard not found, no remotes), keeping the remote on screen when the connection is lost later

## 9. Input and paging

- [ ] 9.1 Add `input.cpp`: poll the touch screen every 20 ms and recognize tap, hold (500 ms, fires once) and swipe (at least 100 pixels horizontally, twice the vertical distance)
- [ ] 9.2 Hit-test presses against the button rectangles, the arrows, the gear and the dialog buttons, and post the matching UI events
- [ ] 9.3 On a button press: invert it with `epd_fastest`, call `ha_call` with its tap or hold call, and restore it when the result arrives
- [ ] 9.4 Show the error message at the bottom of the screen for 3 seconds when a call fails or the device is offline
- [ ] 9.5 Implement paging with wrap-around, from swipes, arrows and the two rocker positions, with a full refresh on page change
- [ ] 9.6 Store the current page index in NVS on page change, and restore it at startup if it still exists
- [ ] 9.7 Add the ghosting counter: a full refresh after 20 partial refreshes or 10 minutes, once the touch screen is released

## 10. Verification on the device

- [ ] 10.1 Start with your configuration and confirm the "connecting" screens, then the Wohnzimmer remote with the heading, media tile and five button rows
- [ ] 10.2 Tap every D-pad button, Zurück, TV and both volume buttons, and confirm that the Apple TV reacts. Hold Zurück and TV and confirm the long-press actions
- [ ] 10.3 Check that the umlauts in "Zurück", "Menü" and "Lautstärke" and all icons render correctly
- [ ] 10.4 Play something on the Apple TV and confirm that the title and play/pause state update within 5 seconds
- [ ] 10.5 Add a second view in Home Assistant and confirm that the device picks it up within 5 seconds, that page dots appear, and that swipes, arrows and the rocker page, in the right direction
- [ ] 10.6 Restart on page 2 and confirm it comes back. Remove the view and confirm it falls back to page 1
- [ ] 10.7 Compare the status bar with a phone: time and date, Wi-Fi level, battery percentage. Turn off the router and confirm the off icon, then turn it on and confirm the reconnect
- [ ] 10.8 Restart Home Assistant and confirm that the remote keeps working without user action
- [ ] 10.9 Tap the gear: confirm that Cancel and the 15-second timeout close the dialog, and that Restart enters setup mode with the old configuration kept
- [ ] 10.10 Run on battery alone for one hour and note the battery drop, for the deep sleep question

## 11. Documentation

- [ ] 11.1 Add the supported card types, the icon fallback and paging to `docs/user/configuration.md`
- [ ] 11.2 Add a note to `docs/dev/home-assistant-api.md` that the firmware uses `call_service` over WebSocket, with the message format and the error response
- [ ] 11.3 Describe `make assets` and how to add icons to `assets/icons.txt` in `README.md`
- [ ] 11.4 Add entries under `## [Unreleased]` in `CHANGELOG.md`: under Added, the remotes from the dashboard, paging, the status bar and the setup button.
