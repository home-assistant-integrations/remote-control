## ADDED Requirements

### Requirement: Setup access point

In setup mode, the device SHALL open a WPA2-protected Wi-Fi access point.

- The network name is `RemoteControl-XXXX`. `XXXX` is the last two bytes of
  the device's MAC address in upper-case hex.
- The password has 12 characters, drawn from an alphabet without look-alike
  characters (no `0`, `O`, `1`, `l`, `I`). It is generated with the hardware
  random number generator each time setup mode starts.
- The device has the address `192.168.4.1` and gives clients addresses through
  DHCP.

#### Scenario: Join the network

- **WHEN** a phone joins `RemoteControl-XXXX` with the password from the screen
- **THEN** it gets an address and reaches `http://192.168.4.1/`

#### Scenario: New password per setup session

- **WHEN** setup mode starts twice
- **THEN** each session uses a different password

### Requirement: Captive portal

In setup mode, the device SHALL answer every DNS query with `192.168.4.1`, so
that phones open the upload page on their own after joining.

#### Scenario: Any host name

- **WHEN** a client resolves any host name while connected to the setup
  network
- **THEN** the answer is `192.168.4.1`, and an HTTP request to that host
  returns the upload page or redirects to it

### Requirement: Setup screen

In setup mode, the e-paper screen SHALL show:

- the heading "Setup"
- the steps: join the network, open the URL, upload the configuration file
- the network name and the password as text
- a QR code that joins the network (`WIFI:T:WPA;S:<ssid>;P:<password>;;`)
- the URL `http://192.168.4.1/` as text and as a second QR code

#### Scenario: First start

- **WHEN** the device enters setup mode
- **THEN** the screen shows the network name, the password, both QR codes and
  the URL

#### Scenario: Scan to join

- **WHEN** a phone camera scans the network QR code
- **THEN** the phone offers to join `RemoteControl-XXXX` without the password
  being typed

### Requirement: Upload page

The device SHALL serve, at `GET /`, an HTML page with a file picker and an
upload button. The page MUST work in current mobile and desktop browsers and
MUST NOT load resources from the internet. The setup network has no internet
access.

The page sends the chosen file to `POST /config` as the request body. It then
shows either a success message or the list of errors from the response.

#### Scenario: Open the page

- **WHEN** a browser on the setup network opens `http://192.168.4.1/`
- **THEN** it shows the upload form, without loading anything from outside
  the device

### Requirement: Configuration upload

`POST /config` SHALL validate the request body with the rules of
`device-configuration`.

| Result | Response | Device |
| --- | --- | --- |
| Valid | 200, `{"ok":true}` | stores the configuration and restarts |
| Invalid | 400, `{"ok":false,"errors":["<field>: <rule>", …]}` | stays in setup mode, stored configuration unchanged |
| Larger than 4096 bytes | 413, `{"ok":false,"errors":["file: larger than 4096 bytes"]}` | stays in setup mode, stored configuration unchanged |

#### Scenario: Valid file

- **WHEN** the user uploads a valid configuration file
- **THEN** the page shows that the configuration was saved and the device
  restarts

#### Scenario: Invalid file

- **WHEN** the user uploads a file with errors
- **THEN** the page lists every error, and the device stays in setup mode
  with the same network and password

#### Scenario: Retry after error

- **WHEN** the user fixes the file after an error and uploads it again
- **THEN** it is accepted without restarting setup mode

### Requirement: Restart after successful upload

After storing a valid configuration, the device SHALL update the screen to
"Configuration saved, restarting". It MUST restart only after the HTTP
response has been sent, at the latest 5 seconds after the upload.

#### Scenario: Restart into normal mode

- **WHEN** a valid configuration was uploaded
- **THEN** the browser receives the success response, the screen shows the
  restart message, and the device restarts and does not enter setup mode
  again

### Requirement: Enter setup mode with a button

If the push button of the M5Paper's rocker switch (GPIO 38) is held for 3
seconds during startup, the device SHALL enter setup mode, even with a valid
configuration. The stored configuration MUST stay in use until a new valid
file is uploaded.

#### Scenario: Fix a wrong Wi-Fi password

- **WHEN** a configuration exists, the user holds the push button during
  startup and uploads a corrected file
- **THEN** the new configuration replaces the old one and the device restarts
  with it

#### Scenario: Leave setup mode without upload

- **WHEN** the user enters setup mode with the button and then restarts the
  device without uploading
- **THEN** the device starts normally with the existing configuration
