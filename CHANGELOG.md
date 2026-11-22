# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

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

- New partition table with a 3 MB app partition and a 64 KB `config`
  partition. A device flashed with an earlier build needs `make erase` once
  before `make flash`.
- `make build` regenerates `sdkconfig` when `sdkconfig.defaults` changes, so
  board settings always come from `sdkconfig.defaults`.
