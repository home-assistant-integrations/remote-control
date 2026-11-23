## Why

`add-initial-setup` is implemented and tested on the host. The configuration
rules have 87 unit checks, and the captive DNS was checked with `dig`. What
can only be checked on a real M5Paper was split out into this change, so
that `add-initial-setup` can be finished without a device at hand: the
screen, the phone flow, persistence across power cycles and flashing, the
setup button, and the absence of the token in the log. The device checks of
`add-remote-ui` were moved here for the same reason: the remotes, paging,
live media tiles, the status bar, reconnecting, and the setup dialog.

The firmware also relies on M5Paper hardware behaviour that no spec states
yet and that was taken from library source code, not measured. The device
must stay on when running on battery (power hold on GPIO 2), the push button
must read low while pressed (GPIO 38), and the portrait screen with its QR
codes must be readable and scannable.

## What Changes

- Run the device checks of `add-initial-setup` (former tasks 5.4 and
  9.1–9.8) and of `add-remote-ui` (former tasks 1.1, 1.3–1.5 and 10.1–10.10)
  on an M5Paper.
- Add the hardware assumptions as requirements of a new capability,
  `m5paper-hardware`, and verify each one on the device.
- Fix any defect the checks reveal, in the code of `add-initial-setup` or
  `add-remote-ui`.

## Capabilities

### New Capabilities

- `m5paper-hardware`: Hardware behaviour the firmware depends on: power hold
  on battery, polarity of the setup button, and the portrait display with
  scannable QR codes.

### Modified Capabilities

None. The requirements of `device-configuration`, `setup-mode`,
`ha-connection`, `remote-layout`, `remote-ui` and `status-bar` are verified,
not changed. If a check fails, the fix goes into the code, and only
into a spec if the requirement itself turns out to be wrong.

## Non-goals

- New features, or changes to setup mode or the remote UI.
- Automated tests on the device (hardware-in-the-loop).
- Optimizing battery life. The one-hour drain is only measured, as input for
  the deep-sleep question of `add-remote-ui`.
- Testing on boards other than the original M5Paper (ESP32).

## Impact

- Needs an M5Paper connected by USB, a phone with a camera, the Wi-Fi and
  Home Assistant details for a real configuration file, and access to the
  `media-control` dashboard and the Apple TV.
- Code changes only if a check fails: `main/` and `components/`.
- `add-initial-setup` and `add-remote-ui` no longer contain device tasks and
  can be archived before this change.
