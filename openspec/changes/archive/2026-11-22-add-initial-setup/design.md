## Context

This change builds on `setup-firmware-build`, which creates the ESP-IDF v5.5
project, the Makefile and a minimal `app_main`. That change left the display
library open. This change needs a screen, so it decides that question.

A fresh device has no Wi-Fi credentials, so it cannot join the user's
network. The URL the user opens must therefore be served on a network the
device creates itself.

Hardware facts that matter here:

- ESP32 with 16 MB flash and 8 MB PSRAM.
- 540 × 960 e-paper with an IT8951 controller. A full refresh takes about 1 to
  2 seconds and the image stays without power.
- A rocker switch with three positions on GPIO 37, 38 (push) and 39. These are
  input-only pins and the board pulls them up.

## Goals / Non-Goals

**Goals:**

- Setup works with a phone or laptop and a browser alone, with no app, cable
  or SD card.
- Configuration errors are reported precisely and fixed without a restart.
- Validation is shared between upload and startup, and can be tested on the
  host.

**Non-Goals:**

See the proposal: no connection test, no editing in the browser, no
download, no encryption, no BLE, USB or SD setup, no remote-control UI.

## Decisions

### Setup over the device's own access point

The device runs as a WPA2 access point at `192.168.4.1` with a DHCP server
(`esp_netif` default AP).

- Alternatives:
  - BLE provisioning needs a phone app.
  - SmartConfig and WPS transfer only the Wi-Fi credentials, not a file.
  - An SD card was ruled out in the proposal.
- WPA2 instead of an open network: without a password, anyone in range could
  upload a configuration. The password appears only on the screen, so joining
  requires seeing the device.
- The password is drawn from `esp_random()`, between
  `bootloader_random_enable()` and `bootloader_random_disable()`. Before the
  radio runs, the ESP32's random number generator delivers true randomness
  only in that mode.

### Captive portal: DNS catch-all and redirect

A small UDP task on port 53 answers every A query with `192.168.4.1`. It
follows the ESP-IDF example `protocols/http_server/captive_portal`. The HTTP
server answers unknown paths with `302` to `http://192.168.4.1/`. Phones then
open the page by themselves, and they are less likely to drop the network for
having no internet.

### Upload as a raw JSON body, not a multipart form

The page reads the chosen file in JavaScript and sends it with `fetch` to
`POST /config` as `Content-Type: application/json`. The device then needs no
multipart parser. It checks `content_len` against 4096 before reading, and
answers 413 if it is larger. The HTML, with inline CSS and JS, is embedded
with `EMBED_TXTFILES` and references nothing external.

### Storage: LittleFS file in its own partition

`config.json` lives on LittleFS (`joltwallet/littlefs`) in a `config`
partition.

- LittleFS survives power loss by design. Writes go to `config.json.tmp`,
  which is then renamed over `config.json`. A rename in LittleFS is atomic.
- Alternative: NVS keys. NVS is built in and can be encrypted, but every new
  field would need its own key and a migration. A file maps directly to what
  the user uploads. NVS remains an option for encryption later.
- The `config` partition is not part of the flashed image (no
  `FLASH_IN_PROJECT`), so `make flash` does not overwrite it.

New `partitions.csv` for the 16 MB flash:

| Name | Type | SubType | Size |
| --- | --- | --- | --- |
| nvs | data | nvs | 24 KB |
| phy_init | data | phy | 4 KB |
| factory | app | factory | 3 MB |
| config | data | littlefs | 64 KB |

The remaining space is left free for OTA and assets in later changes. The
default 1 MB app partition is too small once Wi-Fi, the HTTP server and M5GFX
are linked.

### Display with M5Unified, behind a C interface

`m5stack/m5unified` includes M5GFX, which drives the M5Paper's IT8951 under
plain ESP-IDF, has fonts, and draws QR codes (`qrcode()`). This resolves the
display question of `setup-firmware-build` while staying on ESP-IDF 5.5.

- `M5.begin()` also holds the power pin (GPIO 2) high. Without that, the
  M5Paper switches itself off when it runs on battery.
- `add-remote-ui` needs M5Unified anyway, for the clock, battery, touch and
  buttons. Starting with it avoids rewriting the screen module there.
- M5Unified is C++. It is wrapped in `screen.cpp` with a small C API
  (`screen_show_setup()`, `screen_show_message()`,
  `screen_show_configured()`), so the rest of the firmware stays C.
- The setup button is read with M5Unified's button for GPIO 38. If that
  button object turns out to be a different pin, the code reads GPIO 38
  directly.
- Alternatives:
  - M5EPD is Arduino-only.
  - Plain M5GFX would leave out the power hold, and `add-remote-ui` would have
    to replace it.

### Validation as a pure C module

`config_validate(const char *json, size_t len, config_t *out, errors_t *errs)`
depends only on cJSON and libc. Upload and startup share it, and it can be
compiled and tested on the host, as was done for the API examples. It collects
every error instead of stopping at the first, and returns `config_t` with the
URL normalized.

### Restart after the response

The `POST /config` handler stores the file, sends `200`, and starts a one-shot
`esp_timer` of 1 second. The timer callback hands over to a small task, which
draws "Configuration saved, restarting" (about 2 seconds on e-paper) and calls
`esp_restart()`. The 5-second limit in the spec leaves enough room.

### Boot flow

```text
app_main
  ├─ init NVS, LittleFS (format config partition if it doesn't mount), screen
  ├─ button 38 held 3 s? ──yes──> setup mode
  ├─ load + validate config.json ──missing/invalid──> setup mode
  └─ valid ──> placeholder screen (URL, dashboard)
```

### Module layout

```text
components/config/   config_validate, config_load, config_store (C)
components/setup/    access point, DNS task, HTTP server, upload page (C)
main/screen.cpp      M5Unified wrapper (C++)
main/main.c          boot flow
test/host/           host unit tests for config_validate
```

## Risks / Trade-offs

- [The Wi-Fi password in the file is wrong, so the device can never connect]
  → The user holds the button at startup and uploads a fixed file.
  Switching to setup mode automatically after repeated failures is an open
  question.
- [Setup mode has no timeout, and the access point drains the battery in
  hours] → Accepted for now. Setup normally takes minutes. Open question.
- [Some phones keep using mobile data because the setup network has no
  internet] → The captive portal reduces this. The user documentation says to
  turn off mobile data if the page doesn't open.
- [M5Unified 0.3.0 may not build with ESP-IDF 5.5] → The first task adds the
  dependency and builds before any other work. If it fails, pin an older
  version or use it as a git dependency.
- [The token is stored in plain text in flash] → Anyone with the device and a
  serial cable can read it. Out of scope here (non-goal). The documentation
  recommends a dedicated non-admin user, as the API document does.
- [A new partition table makes existing flash contents invalid] → There are no
  devices in the field yet. Run `make erase` once before the first
  `make flash`.

## Migration Plan

There are no deployed devices. Developers flash once with a full erase so the
new partition table and an empty `config` partition take effect. Add a
`make erase` target if `setup-firmware-build` does not have one.

## Open Questions

- Should setup mode start by itself after N failed Wi-Fi connections or a
  rejected token?
- Should setup mode time out to save the battery, and what should the device
  do then: sleep, or show the setup screen without Wi-Fi?
- Should the token be encrypted (NVS encryption or flash encryption) in a
  later change?
