## 1. Project skeleton

- [x] 1.1 Add the top-level `CMakeLists.txt` with `project(remote-control)` and the ESP-IDF `project.cmake` include
- [x] 1.2 Add `main/CMakeLists.txt` that registers `main.c`
- [x] 1.3 Add `main/main.c` with an `app_main` that logs the chip model, flash size and PSRAM size
- [x] 1.4 Add `sdkconfig.defaults` with target `esp32`, 16 MB flash, and PSRAM enabled with `malloc` support
- [x] 1.5 Add `build/`, `managed_components/`, `sdkconfig` and `sdkconfig.old` to `.gitignore` (`toolchain/` is already there)

## 2. Makefile: variables and checks

- [x] 2.1 Create the `Makefile` with the pinned `IDF_VERSION := v5.5.6` and `PYTHON_VERSION := 3.12`, the paths under `toolchain/`, the exported `IDF_PATH` and `IDF_TOOLS_PATH`, and the `PORT` variable
- [x] 2.2 Add a check that stops with a clear message when `$(CURDIR)` contains a space
- [x] 2.3 Add a check that stops and names the missing tool when `git`, `curl` or `uv` is not on the `PATH`
- [x] 2.4 Add the `help` target, listing targets and variables

## 3. Makefile: toolchain bootstrap

- [x] 3.1 Add the Python step: `uv python install $(PYTHON_VERSION)` into `toolchain/python`, and find the interpreter path
- [x] 3.2 Add the ESP-IDF step: remove any leftover `toolchain/esp-idf`, then make a shallow clone of `$(IDF_VERSION)` with shallow submodules
- [x] 3.3 Add the tools step: run `install.sh esp32` with the uv Python first on `PATH` and `IDF_TOOLS_PATH=toolchain/espressif`, then `idf_tools.py install cmake ninja`, because ESP-IDF installs those on macOS and Linux only on request
- [x] 3.4 Write `toolchain/.stamp` with both versions after success, and make the `toolchain` target reinstall when the stamp is missing or different

## 4. Makefile: build targets

- [x] 4.1 Add an `IDF` helper that runs `. export.sh` and `idf.py` in one shell, with the uv Python on `PATH`
- [x] 4.2 Add the `all` (default) and `build` targets, both depending on `toolchain`
- [x] 4.3 Add the `flash`, `monitor` and `menuconfig` targets, using `PORT` where it is set
- [x] 4.4 Add the `clean` target (`build/` only) and the `distclean` target (`build/`, `managed_components/`, `sdkconfig*`, `toolchain/`)

## 5. Verification

- [x] 5.1 Run `make toolchain` on a clean checkout. Check that `toolchain/` holds Python, ESP-IDF, the tools and `.stamp`, and that `~/.espressif` is unchanged
- [x] 5.2 Run `make build`. Check exit status 0 and that `build/remote-control.bin` exists
- [x] 5.3 Check `build/config/sdkconfig.h` for the ESP32 target, 16 MB flash and PSRAM
- [x] 5.4 Run `make` again and confirm nothing is downloaded and the build is incremental
- [x] 5.5 Run `make build` with `IDF_PATH` set to another directory in the shell and confirm the bundled ESP-IDF is used
- [x] 5.6 Run `make help`, `make clean` (toolchain kept) and the missing-`uv` check with a reduced `PATH`
- [x] 5.7 Run `git status` after a build, or `git check-ignore` if the directory is not a git repository, and confirm that no build or toolchain file is left untracked

## 6. Documentation

- [x] 6.1 Add a "Building" section to `README.md`: host prerequisites, `make`, targets, `PORT`, and the disk space needed for the first run
- [x] 6.2 Add entries under `## [Unreleased]` in `CHANGELOG.md`: under Added, the ESP-IDF project skeleton and the Makefile that installs the toolchain into `toolchain/`
