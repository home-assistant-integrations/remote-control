## Context

`add-initial-setup` built setup mode and verified it on the host: unit tests
of the configuration rules (`make test-host`), the captive DNS against `dig`,
and a warning-free firmware build. These checks were left for a device:

- the screen and the QR codes (former 5.4)
- the end-to-end flow and persistence (former 9.1–9.8)

These hardware facts were taken from the M5Unified and M5GFX sources and
never measured:

- `M5.begin()` holds GPIO 2 for the M5Paper.
- GPIO 38 is pulled up on the board and reads low when pressed.
- The panel is landscape natively, and the firmware rotates it to portrait.

## Goals / Non-Goals

**Goals:**

- Every requirement of `device-configuration`, `setup-mode` and
  `m5paper-hardware` that needs hardware is checked once on a real device,
  with the result recorded in `tasks.md`.
- Defects are fixed in the code, and each fix is checked again.

**Non-Goals:**

- Automating these checks.
- Battery life measurements.

## Decisions

### Manual checks, recorded in the tasks

Each task names the step and the expected result. When a task is ticked, a
short note goes after it if anything was unexpected, for example
"GPIO 38 inverted, fixed in main.c". Hardware-in-the-loop automation would
need a second board and a camera rig. That is out of proportion for one
device.

### Order: hardware first, then the flow

The hardware checks (power, button, orientation) come first, because a
failure there invalidates the flow checks. For example, with the button
polarity reversed, the device would always enter setup mode.

### curl for the HTTP edge cases

The 413 response and the captive redirect are checked with `curl` from a
laptop joined to the setup network. A phone can't send a body larger than the
page allows, and doesn't show redirects.

### Token check in the log

`make monitor` output is saved to a file for the whole session (`tee`), and
then searched for the token from the uploaded configuration file.

## Risks / Trade-offs

- [The test file holds a real token, and the log file could leak it] → The log
  stays in the scratch directory, outside the repository, and is deleted after
  the token check.
- [`make erase` deletes a configuration you may want to keep] → This is the
  first flash with the new partition table, so there is nothing to keep yet.

## Open Questions

None.
