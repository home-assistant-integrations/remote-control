## ADDED Requirements

### Requirement: Toolchain bootstrap into toolchain/

The build SHALL download and install every tool it needs (Python, ESP-IDF,
the Xtensa ESP32 compiler, CMake, Ninja and esptool) into the `toolchain/`
directory of the repository. It MUST NOT write tools or Python packages
outside `toolchain/`, apart from the caches of `uv` and `git`.

#### Scenario: First build on a clean checkout

- **WHEN** a developer runs `make` in a fresh checkout without `toolchain/`
- **THEN** the Makefile installs Python, ESP-IDF and the ESP-IDF tools into
  `toolchain/` and then builds the firmware

#### Scenario: Nothing written to the home directory

- **WHEN** the toolchain is installed
- **THEN** `~/.espressif` is neither created nor changed

### Requirement: Pinned toolchain versions

The ESP-IDF release and the Python version SHALL be pinned in the Makefile.
Every checkout of the same commit MUST install the same versions.

#### Scenario: Version is fixed

- **WHEN** two developers run `make toolchain` on the same commit
- **THEN** both get the same ESP-IDF tag and the same Python minor version

#### Scenario: Version change

- **WHEN** the pinned ESP-IDF version in the Makefile changes and a developer
  runs `make`
- **THEN** the Makefile installs the new version instead of reusing the old
  one

### Requirement: Idempotent toolchain installation

The toolchain installation SHALL run only when `toolchain/` lacks the pinned
versions. A repeated `make` MUST NOT download anything again.

#### Scenario: Second build

- **WHEN** the toolchain is complete and the developer runs `make` again
- **THEN** no download happens and only the firmware build runs

### Requirement: Builds use only the bundled toolchain

All Make targets that build, flash or configure the firmware SHALL run with
the environment of the ESP-IDF in `toolchain/`, independent of any ESP-IDF,
compiler or Python installed on the host or set in the shell environment.

#### Scenario: Host has another ESP-IDF

- **WHEN** `IDF_PATH` in the developer's shell points to a different ESP-IDF
- **THEN** `make` still builds with the ESP-IDF from `toolchain/`

### Requirement: Firmware compiles for the M5Paper

The repository SHALL contain an ESP-IDF project for the ESP32 of the M5Paper
(16 MB flash, 8 MB PSRAM) that compiles without errors to a flashable image.

#### Scenario: Successful build

- **WHEN** a developer runs `make build`
- **THEN** the build exits with status 0 and produces
  `build/remote-control.bin`

#### Scenario: Board settings applied

- **WHEN** the project is configured from `sdkconfig.defaults`
- **THEN** the target is `esp32`, the flash size is 16 MB and PSRAM is enabled

### Requirement: Developer Make targets

The Makefile SHALL provide these targets:

| Target | Effect |
| --- | --- |
| `all` (default) | Install the toolchain if needed, then build |
| `toolchain` | Install the toolchain only |
| `build` | Build the firmware |
| `flash` | Flash the firmware to the device on `PORT` |
| `monitor` | Open the serial monitor on `PORT` |
| `menuconfig` | Open the ESP-IDF configuration menu |
| `clean` | Remove build output, keep the toolchain |
| `distclean` | Remove build output and `toolchain/` |
| `help` | List the targets and variables |

#### Scenario: Flash to a given port

- **WHEN** a developer runs `make flash PORT=/dev/cu.usbserial-0001`
- **THEN** the firmware is flashed to the device on that port

#### Scenario: Clean keeps the toolchain

- **WHEN** a developer runs `make clean`
- **THEN** `build/` is removed and `toolchain/` stays untouched

#### Scenario: Distclean removes everything

- **WHEN** a developer runs `make distclean`
- **THEN** `build/`, `managed_components/`, `sdkconfig` and `toolchain/` are
  removed

### Requirement: Missing host prerequisites are reported

Before installing, the Makefile SHALL check for the host tools it relies on
(`git`, `curl`, `uv`) and stop with a message that names the missing tool.

#### Scenario: uv missing

- **WHEN** `uv` is not on the `PATH` and a developer runs `make`
- **THEN** the Makefile stops before downloading and prints that `uv` is
  required

### Requirement: Build artifacts are not committed

Build output, downloaded components, the generated `sdkconfig` and
`toolchain/` SHALL be ignored by git.

#### Scenario: Clean working tree after a build

- **WHEN** a developer runs `make` and then `git status`
- **THEN** no file created by the toolchain or the build is listed as
  untracked
