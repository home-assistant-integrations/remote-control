## Context

This change builds on two changes that are not implemented yet:

- `setup-firmware-build`: ESP-IDF v5.5, the Makefile and `toolchain/`.
- `add-initial-setup`: configuration (`wifi`, `home_assistant.url`, `token`,
  `dashboard`), LittleFS, `partitions.csv` with a 3 MB app, M5GFX behind
  `screen.h`, and setup mode.

`docs/dev/home-assistant-api.md` defines the Home Assistant side. It covers
WebSocket authentication, reading `lovelace/config`, finding buttons, and
mapping `tap_action` and `hold_action` to service calls, and its C examples
were verified.

Facts checked against the live Home Assistant (2026.11.2):

- The `media-control` dashboard has one sections view, "Wohnzimmer", with a
  `separator`, a `media-player` card (`hide.volume_button: true`) and a
  `sub-buttons` card.
- In `sub_button.bottom[]` there are five rows with 1, 3, 1, 2 and 2 buttons.
  The rows use `justify_content` `center` or `fill`, and the buttons use
  `width`, `custom_height`, `show_icon` and `show_name`.
- `get_config` returns `time_zone: "Europe/Berlin"` and `language: "de"`.
- `subscribe_events` for `lovelace_updated` is accepted.
- A failing `call_service` returns `success:false` with `error.code`
  `not_found` and the message `Service x.y not found.`

M5Paper hardware: 540 × 960 IT8951 e-paper with 16 gray levels, GT911 touch
controller, BM8563 real-time clock (I²C on GPIO 21 and 22), battery input on
GPIO 35 through a 1:2 divider, a rocker switch on GPIO 37, 38 and 39, and a
power-hold pin on GPIO 2.

## Goals / Non-Goals

**Goals:**

- Your dashboard looks and works on the M5Paper the way it does in the HA app.
- A button press reaches Home Assistant fast, without a new TCP connection
  each time.
- The layout parser and the action mapping can be tested on the host.

**Non-Goals:**

See the proposal: other card types, double tap, sliders, Jinja templates,
deep sleep, translations, more than one dashboard.

## Decisions

### A remote is a view of the configured dashboard

- Alternative: list several dashboards in the configuration file. That would
  change the format of `add-initial-setup`, and it doesn't match how Home
  Assistant groups things: views are tabs inside one dashboard, and yours
  already has a view per room.
- With this choice the configuration stays the same, and adding a remote in
  Home Assistant is just adding a view.

### One WebSocket connection for everything

Service calls use `call_service` over the open WebSocket instead of the REST
calls described in the API document.

- The connection is needed anyway, for `subscribe_entities` and
  `lovelace_updated`.
- A REST call per press means a new TCP connection, and with HTTPS also a TLS
  handshake, every time. That adds hundreds of milliseconds on the ESP32.
- The `tap_action`/`hold_action` mapping rules in the API document stay the
  same. Only the transport differs: `domain` and `service` are separate
  fields, `target` and `data` become `target` and `service_data`.
- REST is used only to fetch cover art, an HTTP GET on `entity_picture` with
  no token.

### M5Unified for clock, battery, touch and buttons

`add-initial-setup` already brings in `m5stack/m5unified` for the display and
the power hold. This change also uses what it offers for the M5Paper:

- the real-time clock
- battery voltage
- touch input, through M5GFX
- the three rocker switch positions as `BtnA`/`BtnB`/`BtnC`
- the power-hold pin

All five would otherwise need separate drivers. Doing it without M5Unified
would also mean sharing the I²C bus between the touch driver of M5GFX and an
ESP-IDF I²C driver for the clock. M5Unified already coordinates that.

`screen.h` keeps its C interface.

### Parse once into C structs, then forget the JSON

`layout_parse(const cJSON *config, layout_t *out)` turns the dashboard into:

```text
layout_t { remote_t remotes[≤16] }
remote_t { title, element_t elements[] }
element_t = heading | media_tile | button_row
button_t  { name, icon, show_icon, show_name, width, height,
            call_t tap, call_t hold, rect_t hit }
call_t    { domain, service, service_data_json, target_json } or none
```

Actions are mapped once at parse time into ready-made `call_service`
parameters, following the rules in the API document. The cJSON tree is freed
after parsing. The structs live in PSRAM. The parser and mapping are pure C
(cJSON and libc only), so they run on the host against a copy of your
dashboard as a test fixture.

### Two-pass layout

1. **Measure:** each element gets a natural height in CSS pixels. Defaults:
   heading 40, media tile 160 plus a 64-pixel control row, a button row as
   high as its tallest button (default 56), 8 pixels between elements.
   Button widths come from `width`, or for `fill` rows from an equal share.
2. **Place:** scale factor `s = min(1, available / needed)`, but not below
   0.6. Content width is 540 − 2 × 16 pixels. One CSS pixel is one screen
   pixel; at 540 pixels wide, Bubble Card sizes look about the way they do on
   a phone.

Every button's `hit` rectangle is stored during placement and used for touch
hit-testing.

### One UI task owns the display

M5GFX is not thread-safe, and e-paper refreshes take hundreds of
milliseconds. A single UI task does all drawing. It reads an event queue that
other parts of the firmware write to:

```text
input task (touch 20 ms, buttons) ──┐
ha task (state, layout, results) ───┼─> ui queue ─> UI task ─> M5GFX
esp_timer (minute, battery 60 s) ───┘                 │
                                                      └─> ha task: call requests
```

A service call is a request to the ha task. Its result comes back as an
event, so a slow Home Assistant never blocks drawing.

### Refresh modes

| Use | M5GFX mode |
| --- | --- |
| Changing page, dialogs, full refresh against ghosting | `epd_quality` |
| Media tile, status bar | `epd_fast` |
| Button press feedback | `epd_fastest` |

A counter of partial refreshes and a 10-minute timer trigger the full
refresh against ghosting. That refresh waits until the touch screen is
released.

### Touch gestures

| Gesture | Recognized when |
| --- | --- |
| Tap | Released within 500 ms and moved less than 20 pixels |
| Hold | Still touching at 500 ms. Fires once, then waits for release |
| Swipe | Moved horizontally at least 100 pixels, more than twice as far as vertically |

A press is assigned to the button whose `hit` rectangle contains its start
point.

### Icons: curated Material Design Icons, generated ahead of time

Bubble Card uses Material Design Icon names. All of MDI (about 7,400 icons)
at 48 pixels with 4 bits per pixel would take about 8.5 MB, which doesn't fit
the 3 MB app partition.

- `tools/gen_assets.py` renders the icons named in `assets/icons.txt` from the
  MDI webfont with Pillow, as 4-bit grayscale at 32 and 48 pixels. The list
  holds every icon your dashboard uses now, common remote, media and home
  icons, and the status-bar icons, about 100 in all.
- The output `components/assets/icons_gen.c` is committed, so a normal build
  doesn't need Pillow.
- `make assets` runs the script with the uv Python from `toolchain/`, and
  caches the downloaded font in `toolchain/`.
- An unknown icon falls back to the name text, as the spec requires.
- Alternative: render SVG paths on the device. Rejected, because a vector
  renderer would be more code and slow on e-paper redraws.

### Fonts with umlauts

The same script generates smooth VLW fonts (anti-aliased fonts that M5GFX
loads from memory) from Noto Sans (SIL Open Font License). They cover Latin-1
and Latin Extended-A, plus `‹ › · …`, in 20, 28 and 36 pixels. The built-in
M5GFX fonts are ASCII-only or CJK-sized.

### Time zone table

`gen_assets.py` also turns the IANA → POSIX table from `posix_tz_db` (MIT
licence, pinned commit) into `tz_table_gen.c`. That is about 460 entries,
roughly 15 KB, looked up by binary search. ESP-IDF's `setenv("TZ", …)` needs
the POSIX rule, not the IANA name that Home Assistant returns.

### Setup request through NVS

The dialog writes `setup_req=1` to NVS and restarts. The boot flow of
`add-initial-setup` checks it next to the held button, then erases it. Using
NVS keeps the request even if power drops between the confirmation and the
restart.

### Module layout

```text
components/ha/        wifi.c, ha_ws.c (from the API document), ha_client.c
components/layout/    layout.c, actions.c (pure C, host-tested)
components/assets/    icons_gen.c, fonts_gen.c, tz_table_gen.c, lookups
components/ui/        ui_task.cpp, render.cpp, statusbar.cpp, input.cpp,
                      dialog.cpp, media_tile.cpp
tools/gen_assets.py   icons, fonts, time-zone table
assets/icons.txt      curated icon list
test/host/            test_layout.c, test_actions.c, fixtures/media-control.json
```

## Risks / Trade-offs

- [On battery, the M5Paper switches itself off unless GPIO 2 is held high.
  `add-initial-setup` does not do this] → `M5.begin()` holds the pin. The
  verification tasks include a test on battery alone.
- [Wi-Fi stays on, so the battery lasts hours rather than days] → Modem sleep
  (`WIFI_PS_MIN_MODEM`) keeps the WebSocket open. Deep sleep with wake on
  touch is an open question.
- [M5Unified may not support every M5Paper peripheral under ESP-IDF 5.5, for
  example the clock or battery] → Task 1.2 checks each peripheral before UI
  work begins. Fallback: an ESP-IDF I²C driver for the BM8563, and the ADC
  directly.
- [A dashboard larger than the screen is shrunk or cut off] → Accepted for a
  first version. The log names the cut-off elements.
- [Icons outside the curated list show as text] → Add names to
  `assets/icons.txt` and run `make assets`.
- [Your dashboard's `state_content` Jinja template is ignored] → The default
  state line shows the same information for your media player.
- [A large `lovelace/config` exhausts memory] → Frames are reassembled in
  PSRAM. Limits of 16 remotes and 64 buttons per remote.

## Migration Plan

There are no deployed devices. The setup request flag is new, and an older
boot flow ignores it.

## Open Questions

- Deep sleep with wake on touch (GT911 interrupt on GPIO 36): reconnecting
  takes 2 to 4 seconds. Is that acceptable for a remote?
- Should the date format and UI texts follow Home Assistant's `language`
  (`de`)?
- Should the device read the layout of other card types, such as plain HA
  `button` cards, in a later change?
