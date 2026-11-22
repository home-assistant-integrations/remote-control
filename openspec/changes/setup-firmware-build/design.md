## Context

`remote-control` is meant to become an M5Paper firmware, but the repository
holds only documentation (`docs/dev/home-assistant-api.md`) and a Bruno
collection. There is no source code and no build. The C examples in the API
document use ESP-IDF APIs (`esp_http_client`, `esp_websocket_client`, cJSON).

The M5Paper has an ESP32-D0WDQ6-V3, 16 MB flash and 8 MB PSRAM, and a USB
serial bridge for flashing.

Host constraints seen on the development machine:

- macOS on Apple Silicon (arm64). Linux hosts should work too.
- Homebrew offers only Python 3.15, which is newer than ESP-IDF supports.
- `uv`, `git`, `curl` and `make` are installed.
- An old PlatformIO setup exists under `~/.platformio`. Its ESP-IDF build
  failed while running CMake during the API work, so it is not a reliable
  base.

## Goals / Non-Goals

**Goals:**

- `make` on a fresh checkout installs a pinned toolchain into `toolchain/` and
  builds a bootable image.
- The build never depends on tools in the user's home directory or `PATH`,
  apart from `git`, `curl`, `make` and `uv`.
- Repeated builds do not download anything.

**Non-Goals:**

- Application code (display, Wi-Fi, Home Assistant client).
- CI, containers, Windows hosts.
- Custom partition tables or OTA.

## Decisions

### ESP-IDF as the framework

Use plain ESP-IDF with C sources.

- It matches the C examples in the API document, and `esp_websocket_client`
  and `esp_http_client` are first-class ESP-IDF components.
- Alternatives:
  - Arduino-ESP32 with M5EPD would make display support easy, but it brings in
    C++ and Arduino's own build.
  - PlatformIO stores its toolchain in `~/.platformio`, which breaks the
    `toolchain/` requirement, and it already failed on this machine.
- Display libraries that support ESP-IDF (M5GFX) can be added as components
  later.

### Pin ESP-IDF v5.5.6

The newest release is v6.1.1. v5.5 is the last 5.x line and is still
maintained. Third-party components such as M5GFX and the
`espressif/esp_websocket_client` registry component are known to work with
5.x, while 6.x changed several APIs. Moving to 6.x is a one-line change of
`IDF_VERSION` later, once the display library is chosen.

### Python 3.12 installed by uv into toolchain/python

ESP-IDF creates its own virtual environment from whatever `python3` it is
started with. Host Python 3.15 is too new, so the Makefile runs
`uv python install 3.12` with `UV_PYTHON_INSTALL_DIR=toolchain/python` and puts
that interpreter first on `PATH` for `install.sh` and `export.sh`.

- Alternative: require the user to install Python 3.12 themselves. Rejected,
  because then the build depends on the host again.
- Alternative: download a python-build-standalone archive with `curl`.
  Rejected, because `uv` already does this, with checksums, and it is
  installed.

### Directory layout and environment

```text
toolchain/
  python/        uv-managed CPython 3.12
  esp-idf/       git clone of v5.5.6 (shallow, with submodules)
  espressif/     IDF_TOOLS_PATH: compilers, CMake, Ninja, Python venv
  .stamp         records IDF and Python versions of a finished install
```

The Makefile exports `IDF_PATH` and `IDF_TOOLS_PATH` pointing into
`toolchain/`, so a different ESP-IDF set in the user's shell is overridden.
Every recipe that calls `idf.py` runs `. $(IDF_PATH)/export.sh` and `idf.py`
in the same shell, because Make starts a new shell for each recipe line.

`install.sh esp32` installs the tools for the ESP32 target only, which avoids
downloading the RISC-V compilers.

### Stamp file for idempotence and version changes

`toolchain/.stamp` is written only after a complete install. It contains
`IDF_VERSION` and `PYTHON_VERSION`. The `toolchain` target compares the stamp
with the pinned values. A missing or different stamp removes `esp-idf/` and
reinstalls. Interrupted installs therefore restart cleanly instead of leaving
a half-cloned tree.

### Project skeleton

- `project(remote-control)` in the top-level `CMakeLists.txt`, so the image is
  `build/remote-control.bin`.
- `main/main.c` with an `app_main` that logs the chip and PSRAM size. That is
  enough to see on the serial monitor that the image boots and that PSRAM
  works.
- `sdkconfig.defaults`: target `esp32`, 16 MB flash, PSRAM enabled with
  `malloc` support. The generated `sdkconfig` is ignored by git, so
  `sdkconfig.defaults` stays the single source of board settings.

## Risks / Trade-offs

- [Large first download, about 1.5 to 2 GB, slow on bad connections] →
  Shallow clone with shallow submodules, ESP32 tools only, and no reinstall
  unless versions change.
- [GitHub or Espressif download servers unreachable] → The stamp is written
  only on success, so `make` can simply be run again.
- [ESP-IDF does not support paths with spaces] → The Makefile checks
  `$(CURDIR)` and stops with a clear message.
- [uv cache in the home directory lies outside `toolchain/`] → It is a cache,
  not a tool the build runs. The spec allows it.
- [ESP-IDF 5.5 drops support for a future macOS or Python] → Python is pinned
  through uv. The ESP-IDF pin can be raised in one place.

## Migration Plan

There is nothing to migrate. To roll back, remove the new files and
`toolchain/`.

## Open Questions

- Which display library: M5GFX as an ESP-IDF component, or M5EPD through the
  Arduino component? This decides when to move to ESP-IDF 6.x. It belongs in a
  later change.
- Partition layout for OTA and assets: later change.
