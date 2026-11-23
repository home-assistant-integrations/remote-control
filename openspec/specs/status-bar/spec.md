# status-bar Specification

## Purpose

The bar at the top of every screen: Wi-Fi signal, date and time, battery
level and the setup button, and how they are kept up to date.

## Requirements

### Requirement: Status bar contents

The top 48 pixels of every remote page and status screen SHALL hold a bar
with, from left to right:

- the Wi-Fi icon
- the date and time
- the battery icon and percentage
- the setup button

The bar MUST be separated from the content by a line.

#### Scenario: Normal operation

- **WHEN** a remote is shown
- **THEN** the bar shows all four items

### Requirement: Wi-Fi indicator

The Wi-Fi icon SHALL show the signal strength in four levels from the RSSI:

| RSSI | Level |
| --- | --- |
| at least −55 dBm | 4 |
| −56 to −67 dBm | 3 |
| −68 to −78 dBm | 2 |
| below −78 dBm | 1 |

It MUST show a crossed-out icon while Wi-Fi is disconnected, and a
Wi-Fi icon with an exclamation mark while Wi-Fi is up but Home Assistant is
not connected. It is updated at least every 30 seconds and on every change of
connection state.

#### Scenario: Wi-Fi lost

- **WHEN** the Wi-Fi connection drops
- **THEN** the crossed-out icon appears within 2 seconds

#### Scenario: Home Assistant down

- **WHEN** Wi-Fi is connected but the WebSocket connection is lost
- **THEN** the icon with the exclamation mark is shown

### Requirement: Date and time

The bar SHALL show the local date and time as `DD.MM.YYYY HH:MM` (24-hour
format) and update it within 5 seconds after each minute changes.

- Time is synchronized with SNTP (`pool.ntp.org`) after Wi-Fi connects, and
  every 6 hours after that.
- The time zone is Home Assistant's `time_zone`, converted to a POSIX rule
  with a built-in table. If the zone is unknown or not yet received, UTC is
  used.
- After every sync the time is written to the M5Paper's real-time clock
  (BM8563). At startup the system clock is set from it, so the time shows
  before Wi-Fi is up.
- Until a valid time is known, the bar shows `--.--.---- --:--`.

#### Scenario: Normal display

- **WHEN** the local time is 22 November 2026, 20:15, in Europe/Berlin
- **THEN** the bar shows `22.11.2026 20:15`

#### Scenario: Daylight saving

- **WHEN** the clocks change on the last Sunday of March in Europe/Berlin
- **THEN** the time shown jumps from 01:59 to 03:00

#### Scenario: No network after restart

- **WHEN** the device restarts without Wi-Fi and the real-time clock holds a
  valid time
- **THEN** the bar shows the correct local time

### Requirement: Battery indicator

The bar SHALL show the battery level as an icon with five steps and as a
percentage, calculated from the battery voltage. The voltage is measured on
the M5Paper's battery input (GPIO 35, ADC with calibration, through the
board's 1:2 divider) and averaged over 16 samples. The level is linear between
3.3 V (0 %) and 4.15 V (100 %), clamped to 0–100 %. It is updated every 60
seconds. Below 10 % the icon MUST be shown as empty with an exclamation mark.

#### Scenario: Full battery

- **WHEN** the measured voltage is 4.15 V or higher
- **THEN** the bar shows a full battery and `100%`

#### Scenario: Low battery

- **WHEN** the level drops below 10 %
- **THEN** the empty battery icon with an exclamation mark is shown

### Requirement: Setup button in the status bar

The rightmost item of the bar SHALL be a gear icon of at least 48 × 48
pixels. Tapping it MUST show a confirmation dialog, "Restart into setup mode
to upload a new configuration?", with the buttons "Cancel" and "Restart".
"Restart" starts setup mode. "Cancel", or no input for 15 seconds, closes the
dialog.

#### Scenario: Confirm

- **WHEN** the user taps the gear and then "Restart"
- **THEN** the device restarts into setup mode

#### Scenario: Cancel

- **WHEN** the user taps the gear and then "Cancel"
- **THEN** the dialog closes and the remote is shown unchanged

#### Scenario: Accidental tap

- **WHEN** the user taps the gear and touches nothing for 15 seconds
- **THEN** the dialog closes by itself

### Requirement: Partial bar updates

Changes to the bar SHALL redraw only the bar area with a partial refresh, and
MUST NOT redraw the rest of the screen.

#### Scenario: Minute changes

- **WHEN** the minute changes while a remote is shown
- **THEN** only the bar area is refreshed
