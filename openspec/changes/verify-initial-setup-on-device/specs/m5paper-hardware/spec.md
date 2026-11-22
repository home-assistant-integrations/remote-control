## ADDED Requirements

### Requirement: Power stays on when running on battery

The firmware SHALL hold the M5Paper's power-hold pin (GPIO 2) high from the
start of `app_main` until the firmware switches the device off on purpose.
The device MUST keep running on battery alone after the USB cable is
removed.

#### Scenario: Unplug during setup mode

- **WHEN** the device shows the setup screen and the USB cable is removed
- **THEN** the device keeps running, and the setup network stays visible on a
  phone for at least 5 minutes

#### Scenario: Start on battery

- **WHEN** the device is switched on without USB
- **THEN** it boots and shows the setup screen or the configured screen

### Requirement: Setup button polarity

The push button of the rocker switch (GPIO 38) SHALL read low while pressed
and high while released, through the board's pull-up. The firmware MUST NOT
depend on an internal pull-up, because GPIO 34–39 have none.

#### Scenario: Released

- **WHEN** the device starts and the push button is not touched
- **THEN** the firmware reads GPIO 38 as high and does not enter setup mode
  because of the button

#### Scenario: Pressed

- **WHEN** the push button is held during the start
- **THEN** the firmware reads GPIO 38 as low

### Requirement: Readable portrait display

The firmware SHALL draw in portrait orientation, 540 pixels wide and 960
pixels high, with the setup screen's heading at the top. All text on the
setup screen MUST be fully visible and not clipped at any edge.

#### Scenario: Orientation

- **WHEN** the setup screen is shown and the device is held with the USB port
  at the bottom
- **THEN** the heading "Setup" is at the top and the text reads left to
  right

#### Scenario: Long values

- **WHEN** the setup screen shows a network name of 18 characters and a
  password of 12 characters
- **THEN** both are shown completely on one line each

### Requirement: Scannable QR codes

Both QR codes of the setup screen SHALL scan with the built-in camera app of
a current iPhone and a current Android phone, from 15 to 40 cm away, on the
e-paper display.

#### Scenario: Join QR code

- **WHEN** the left QR code is scanned
- **THEN** the phone offers to join `RemoteControl-XXXX` without the password
  being typed

#### Scenario: URL QR code

- **WHEN** the right QR code is scanned
- **THEN** the phone offers to open `http://192.168.4.1/`
