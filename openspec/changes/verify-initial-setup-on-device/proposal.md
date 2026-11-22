## Why

`add-initial-setup` is implemented and tested on the host. The configuration
rules have 87 unit checks, and the captive DNS was checked with `dig`. What
can only be checked on a real M5Paper was split out into this change, so
that `add-initial-setup` can be finished without a device at hand: the
screen, the phone flow, persistence across power cycles and flashing, the
setup button, and the absence of the token in the log.

The firmware also relies on M5Paper hardware behaviour that no spec states
yet and that was taken from library source code, not measured. The device
must stay on when running on battery (power hold on GPIO 2), the push button
must read low while pressed (GPIO 38), and the portrait screen with its QR
codes must be readable and scannable.

## What Changes

- Run the device checks of `add-initial-setup` (former tasks 5.4 and
  9.1–9.8) on an M5Paper.
- Add the hardware assumptions as requirements of a new capability,
  `m5paper-hardware`, and verify each one on the device.
- Fix any defect the checks reveal, in the code of `add-initial-setup`.

## Capabilities

### New Capabilities

- `m5paper-hardware`: Hardware behaviour the firmware depends on: power hold
  on battery, polarity of the setup button, and the portrait display with
  scannable QR codes.

### Modified Capabilities

None. The requirements of `device-configuration` and `setup-mode` are
verified, not changed. If a check fails, the fix goes into the code, and only
into a spec if the requirement itself turns out to be wrong.

## Non-goals

- New features or changes to setup mode.
- Automated tests on the device (hardware-in-the-loop).
- Measuring battery life. That belongs to `add-remote-ui` (task 10.10).
- Testing on boards other than the original M5Paper (ESP32).

## Impact

- Needs an M5Paper connected by USB, a phone with a camera, and the Wi-Fi
  and Home Assistant details for a real configuration file.
- Code changes only if a check fails: `main/`, `components/setup/`,
  `components/config/`.
- `add-initial-setup` no longer contains device tasks and can be archived
  before this change.
