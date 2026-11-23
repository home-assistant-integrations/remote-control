# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Remote controls from Home Assistant. Every view of the configured dashboard
  is one remote. Bubble Card separators, buttons, media players (with cover
  art, title and app, and their play/pause, previous, next, power and volume
  controls) and sub-button rows are drawn on the e-paper screen. A tap runs a
  button's tap action, holding it for half a second its hold action. The
  button shows the press until Home Assistant has answered, and errors appear
  at the bottom of the screen.
- Paging between remotes with swipes, the arrows next to the title or the
  rocker switch. The device remembers the last remote across restarts.
- Changes to the dashboard in Home Assistant appear on the device within
  seconds, without a restart. Media player tiles update live.
- Status bar with Wi-Fi signal, date and time (NTP, in Home Assistant's time
  zone, kept by the real-time clock), battery level and a setup button that
  restarts into setup mode after a confirmation.
- Status screens for connecting, a rejected token, and a missing or empty
  dashboard. The connection to Wi-Fi and Home Assistant is restored by itself
  after an outage.
- About 100 built-in Material Design Icons and Noto Sans fonts with umlauts.
  Icons are added through `assets/icons.txt` and `make assets`.
- Setup mode. When no valid configuration is stored, the device opens its own
  WPA2 Wi-Fi network with a random password and shows the network, the
  password and two QR codes (join the network, open the page) on the screen.
  An upload page, which phones open by themselves, takes the configuration
  file. Every error in the file is listed, and a valid file is stored and the
  device restarts with it.
- Configuration file with Wi-Fi network, Home Assistant URL, token and
  dashboard, documented in `docs/user/configuration.md` with an example. The
  file is stored in its own flash partition, survives power loss and firmware
  updates, and is checked again at every start.
- Holding the push button for 3 seconds at startup enters setup mode, to
  change the configuration.
- The device stays switched on when it runs on battery.
- `make erase`, `make reconfigure` and `make test-host` (unit tests of the
  configuration checks, run on the computer).
- Firmware project for the M5Paper (ESP-IDF, ESP32 with 16 MB flash and PSRAM).
  It builds an image that boots and logs the chip, flash and PSRAM size.
- `make` installs a pinned toolchain (ESP-IDF v5.5.6, Python 3.12, ESP32
  compiler and tools) into `toolchain/` on the first run and builds with it.
  Targets for flashing, the serial monitor, configuration and cleaning are
  included, and `make help` lists them.
- README with a short description of the project and build instructions.
- Apache License 2.0.
- Developer guide to the Home Assistant API for remote controls
  (`docs/dev/home-assistant-api.md`). It covers authentication for REST and
  WebSocket with the error responses, reading dashboards and entity states,
  mapping dashboard actions to service calls, and webhooks. Every use case has
  a C example for ESP-IDF.
- Bruno collection in `test/bruno` with one request for each use case in the
  API guide. The token is read from `test/bruno/.env`, which is not under
  version control.
- OpenSpec change proposals for the firmware build (`setup-firmware-build`),
  the first-start setup with configuration upload (`add-initial-setup`), its
  checks on the device (`verify-initial-setup-on-device`), and the remote
  control UI with status bar (`add-remote-ui`).

### Changed

- With a valid configuration, the device shows the remotes instead of the
  placeholder screen.
- New partition table with a 3 MB app partition and a 64 KB `config`
  partition. A device flashed with an earlier build needs `make erase` once
  before `make flash`.
- `make build` regenerates `sdkconfig` when `sdkconfig.defaults` changes, so
  board settings always come from `sdkconfig.defaults`.

### Fixed

- `make` on a fresh checkout ran the ESP-IDF installer with the host's Python
  instead of the bundled Python 3.12, so the following build could not find
  its Python environment.
