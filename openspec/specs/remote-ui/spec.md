# remote-ui Specification

## Purpose

How remotes are shown and used on the e-paper screen: layout, button
appearance, tap and hold, feedback, paging, the remembered page, live media
tiles, status screens and ghosting control.

## Requirements

### Requirement: Screen layout

In portrait orientation (540 × 960 pixels) the screen SHALL be divided, from
top to bottom, into:

- the status bar
- a title row with the remote's title and the page indicator
- the content area, with the elements of the current remote stacked
  vertically, full width

If the content is taller than the content area, the content MUST be scaled
down evenly to fit, to no less than 60 % of its size. Anything that is still
too tall is cut off and logged.

#### Scenario: Your remote fits

- **WHEN** the `Wohnzimmer` remote is shown
- **THEN** the heading, the media tile and all five button rows are visible
  without scrolling

### Requirement: Button appearance

A button SHALL be drawn as a rounded rectangle with its icon and/or name
centered. A button with an icon that is not in the built-in icon set MUST show
its name instead. If it has no name either, it shows the icon name without
`mdi:`.

#### Scenario: Known icon

- **WHEN** a button has `icon: mdi:chevron-up`
- **THEN** the up-arrow icon is drawn

#### Scenario: Unknown icon

- **WHEN** a button has `icon: mdi:some-rare-icon` and the name "Extra"
- **THEN** the text "Extra" is drawn instead of an icon

#### Scenario: Umlauts

- **WHEN** a button is named "Zurück"
- **THEN** the "ü" is drawn correctly

### Requirement: Tap and hold

Touching a button and releasing it within 500 ms SHALL run its `tap_action`.
Holding it for 500 ms or longer MUST run its `hold_action` once, when the
500 ms are reached. Actions are mapped to service calls as described in
`docs/dev/home-assistant-api.md` (`perform-action`, `call-service`, `toggle`
and `none`). UI actions (`more-info`, `navigate`, `url`) do nothing.

#### Scenario: Short press

- **WHEN** the user taps "Hoch"
- **THEN** `button.press` is called for `button.wohnzimmer_apple_tv_wohnzimmer_up`

#### Scenario: Long press

- **WHEN** the user holds "Zurück" for one second
- **THEN** `button.press` is called once, for
  `button.wohnzimmer_apple_tv_wohnzimmer_back_long_press`

#### Scenario: Hold action none

- **WHEN** the user holds "Hoch", whose `hold_action` is `none`
- **THEN** no service is called

### Requirement: Touch feedback

When a press is recognized, the button SHALL be shown inverted with a fast
partial refresh within 150 ms, and restored after the call finishes. If the
call fails, a message with the error MUST appear at the bottom of the screen
for 3 seconds.

#### Scenario: Call fails

- **WHEN** a service call returns an error
- **THEN** the button is restored and the error message is shown for 3
  seconds

#### Scenario: Offline press

- **WHEN** the user presses a button while Home Assistant is not connected
- **THEN** the message "Not connected" is shown and nothing is queued

### Requirement: Paging between remotes

If there is more than one remote, the device SHALL let the user switch with:

- the rocker switch: GPIO 37 (`BtnL` in M5EPD) for the previous remote, GPIO
  39 (`BtnR`) for the next. The physical direction is checked on the device
  and swapped if it feels reversed in portrait orientation
- a horizontal swipe of at least 100 pixels in the content area
- the `‹` and `›` arrows in the title row

Paging wraps around from the last remote to the first. The page indicator
shows one dot per remote, with the current one filled. With only one remote,
paging is disabled and the arrows and dots are hidden.

#### Scenario: Next remote

- **WHEN** remote 1 of 3 is shown and the user swipes left
- **THEN** remote 2 is shown with a full refresh, and the second dot is filled

#### Scenario: Wrap around

- **WHEN** the last remote is shown and the user presses the rocker towards
  "next"
- **THEN** the first remote is shown

#### Scenario: Single remote

- **WHEN** the dashboard has one view
- **THEN** there are no arrows or dots, and swipes do nothing

### Requirement: Remember the current page

The index of the current remote SHALL be kept in NVS and restored at startup
if that remote still exists.

#### Scenario: Restart on page 2

- **WHEN** the device restarts while remote 2 is shown
- **THEN** it shows remote 2 again after startup

### Requirement: Live media tile

A media tile SHALL show the player's name, its state line, and its cover art
if `entity_picture` is set (scaled, in grayscale), or else its icon. The state
line is `media_title · app_name`, or whichever of the two is present, or else
the state (`playing`, `paused`, `off`, …). The tile MUST be redrawn with a
partial refresh when these values change, at most once every 5 seconds.
Play/pause shows the pause icon while the player is `playing`.

#### Scenario: Title changes

- **WHEN** the Apple TV starts a new episode
- **THEN** the tile shows the new title within 5 seconds, without a full
  refresh of the screen

#### Scenario: No cover art

- **WHEN** `entity_picture` is missing
- **THEN** the tile shows the `mdi:apple` icon in place of the cover

### Requirement: Status screens

Instead of a remote, the device SHALL show a full-screen message, with the
status bar still at the top, for each of these states:

| State | Message |
| --- | --- |
| Wi-Fi not connected yet | "Connecting to `<ssid>` …" |
| Home Assistant not reachable | "Connecting to Home Assistant …" with the URL |
| Token rejected | "Home Assistant rejected the token. Tap the setup button to upload a new configuration." |
| Dashboard not found | "Dashboard `<dashboard>` not found." |
| Dashboard without views or cards | "The dashboard has no remotes." |

If the connection is lost after a remote was shown, the remote MUST stay on
screen, and the status bar shows the loss.

#### Scenario: First connection

- **WHEN** the device starts and Wi-Fi is not connected yet
- **THEN** the screen shows "Connecting to `<ssid>` …"

#### Scenario: Lost connection

- **WHEN** Home Assistant becomes unreachable while a remote is shown
- **THEN** the remote stays visible and the Wi-Fi or connection icon in the
  status bar shows the problem

### Requirement: Ghosting control

Partial refreshes SHALL be followed by a full refresh of the screen after 20
partial refreshes, or after 10 minutes since the last full refresh, whichever
comes first. The full refresh happens only while no touch is active.

#### Scenario: Many presses

- **WHEN** the user presses buttons 25 times
- **THEN** exactly one full refresh happens in between
