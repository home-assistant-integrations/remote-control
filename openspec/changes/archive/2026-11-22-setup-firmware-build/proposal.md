## Why

The repository has documentation and test collections but no firmware project,
so nothing can be compiled for the M5Paper. Every developer would have to
install ESP-IDF, its compilers and a suitable Python by hand, and in a version
that happens to match. A single `make` that fetches a pinned toolchain into the
repository makes builds reproducible and lets new contributors start in one
step.

## What Changes

- Add a top-level `Makefile` that downloads and installs everything the build
  needs into `toolchain/` (already ignored by git): a pinned Python, a pinned
  ESP-IDF release, and the ESP-IDF tools (Xtensa compiler, CMake, Ninja,
  esptool).
- Add an ESP-IDF project skeleton for the M5Paper (ESP32, 16 MB flash, 8 MB
  PSRAM): `CMakeLists.txt`, `main/` with a minimal `app_main`, and
  `sdkconfig.defaults`.
- Add Make targets to build, flash, monitor, configure and clean, all running
  with the toolchain from `toolchain/` and never with tools installed on the
  host.
- Extend `.gitignore` for build output (`build/`, `managed_components/`,
  `sdkconfig`, `sdkconfig.old`).
- Document the build in `README.md`.

## Capabilities

### New Capabilities

- `firmware-build`: Bootstrapping the pinned toolchain into `toolchain/` and
  compiling, flashing and cleaning the firmware through `make`.

### Modified Capabilities

None. `openspec/specs/` is empty.

## Non-goals

- No application functionality: no display driver, Wi-Fi, Home Assistant
  client or UI. The skeleton only has to compile and boot.
- No CI pipeline, container image or Windows support. macOS and Linux hosts
  only.
- No over-the-air updates or custom partition layout beyond what the skeleton
  needs.
- No use of host-installed ESP-IDF, PlatformIO or Arduino.

## Impact

- New files: `Makefile`, `CMakeLists.txt`, `main/CMakeLists.txt`,
  `main/main.c`, `sdkconfig.defaults`.
- Changed files: `.gitignore`, `README.md`.
- Disk: the first `make` downloads about 1.5 to 2 GB into `toolchain/`.
- Network access to GitHub, Espressif's download servers and the Python
  package index is needed on the first run only.
- Host prerequisites: `git`, `curl`, `make` and `uv`; everything else comes
  from `toolchain/`.
