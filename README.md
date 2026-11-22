# remote-control

`remote-control` is a firmware for the M5Stack M5Paper. It reads remote
control configurations from Home Assistant, displays them and allows the user
the trigger action.

## Building

The firmware is built with ESP-IDF. You don't need to install ESP-IDF
yourself: the Makefile downloads a pinned toolchain into `toolchain/` on the
first run and uses only that.

### Prerequisites

- macOS or Linux, in a directory whose path has no spaces
- `git`, `curl`, `make` and [`uv`](https://docs.astral.sh/uv/)
- About 2.5 GB of free disk space for `toolchain/`, and network access on the
  first run

### Build

```sh
make
```

The first run installs Python 3.12, ESP-IDF v5.5.6 and the ESP32 tools into
`toolchain/`, which takes a while. Later runs only build. The image ends up in
`build/remote-control.bin`.

### Targets

| Target | Effect |
| --- | --- |
| `make` / `make all` | Install the toolchain if needed, then build |
| `make toolchain` | Install the toolchain only |
| `make build` | Build the firmware |
| `make flash` | Flash the firmware to the device |
| `make monitor` | Open the serial monitor (exit with `Ctrl-]`) |
| `make menuconfig` | Open the ESP-IDF configuration menu |
| `make clean` | Remove build output, keep the toolchain |
| `make distclean` | Remove build output, downloaded components and `toolchain/` |
| `make help` | List targets and variables |

`flash` and `monitor` detect the serial port. To choose one, set `PORT`:

```sh
make flash monitor PORT=/dev/cu.usbserial-0001
```

Board settings (target, flash size, PSRAM) live in `sdkconfig.defaults`. The
generated `sdkconfig` is not under version control.
