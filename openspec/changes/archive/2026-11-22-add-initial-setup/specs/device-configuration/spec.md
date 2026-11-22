## ADDED Requirements

### Requirement: Configuration file format

The configuration SHALL be a UTF-8 JSON object of this form:

```json
{
  "version": 1,
  "wifi": {
    "ssid": "MyNetwork",
    "password": "secret-password"
  },
  "home_assistant": {
    "url": "http://homeassistant.local:8123",
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOi...",
    "dashboard": "media-control"
  }
}
```

All fields shown are required. The format MUST be documented in
`docs/user/configuration.md`, with the example in
`docs/user/config.example.json`.

#### Scenario: Example file is valid

- **WHEN** `docs/user/config.example.json` is checked by the firmware's
  validation
- **THEN** it reports no errors

### Requirement: Configuration validation

The firmware SHALL accept a configuration only if all of these rules hold:

| Field | Rule |
| --- | --- |
| whole file | at most 4096 bytes, parses as a JSON object, no keys other than those in the format |
| `version` | the number `1` |
| `wifi.ssid` | string of 1 to 32 bytes |
| `wifi.password` | string, empty (open network) or 8 to 63 characters |
| `home_assistant.url` | string starting with `http://` or `https://`, with a host, optional port, and no path, query or fragment. A single trailing `/` is allowed and removed |
| `home_assistant.token` | string of three non-empty base64url parts separated by `.`, without whitespace |
| `home_assistant.dashboard` | string of 1 to 64 characters from `a`–`z`, `0`–`9`, `-` and `_` |

Validation MUST report every violated rule, not only the first. Each error
MUST name the field, as a dotted path, and the rule it violates.

#### Scenario: Valid file

- **WHEN** a file follows every rule
- **THEN** validation succeeds with no errors

#### Scenario: Several errors

- **WHEN** a file has `"wifi": {"ssid": "", "password": "short"}` and lacks
  `home_assistant.token`
- **THEN** validation fails with three errors: for `wifi.ssid`,
  `wifi.password` and `home_assistant.token`

#### Scenario: Not JSON

- **WHEN** the file is not valid JSON
- **THEN** validation fails with one error that says the file is not valid JSON

#### Scenario: Misspelled key

- **WHEN** the file contains `"wifi": {"ssid": "x", "pasword": "12345678"}`
- **THEN** validation fails with errors for the unknown key `wifi.pasword` and
  the missing `wifi.password`

#### Scenario: Trailing slash in URL

- **WHEN** `home_assistant.url` is `http://homeassistant.local:8123/`
- **THEN** validation succeeds and the stored URL is
  `http://homeassistant.local:8123`

### Requirement: Persistent storage

A valid configuration SHALL be stored as `config.json` on a LittleFS file
system in a dedicated `config` flash partition. It MUST survive restarts and
power loss. The write MUST be atomic: an interrupted write leaves the previous
configuration, or none, but never a partial file.

#### Scenario: Survives power cycle

- **WHEN** a valid configuration was stored and the device is switched off and
  on
- **THEN** the same configuration is loaded

#### Scenario: Interrupted write

- **WHEN** power is lost while a new configuration is being written
- **THEN** at the next start either the old or the new configuration is
  loaded, and it passes validation

#### Scenario: Firmware update keeps configuration

- **WHEN** a new firmware image is flashed with `make flash` without erasing
  the flash
- **THEN** the stored configuration is still there

### Requirement: Configuration check at startup

At every start, the firmware SHALL load the stored configuration and run the
same validation as for uploads. If no configuration exists, or it fails
validation, the firmware MUST enter setup mode. Otherwise it MUST continue
with normal operation.

#### Scenario: First start

- **WHEN** the device starts with an empty `config` partition
- **THEN** it enters setup mode

#### Scenario: Corrupt configuration

- **WHEN** the stored `config.json` fails validation
- **THEN** the device enters setup mode and logs the validation errors on the
  serial console

#### Scenario: Valid configuration

- **WHEN** the stored configuration passes validation and no setup button is
  held
- **THEN** the device does not enter setup mode, and it shows a placeholder
  screen with the Home Assistant URL and the dashboard name

### Requirement: Token is not exposed

The firmware MUST NOT write the token to the serial log, show it on the
screen, or return it over HTTP.

#### Scenario: Log output

- **WHEN** the configuration is loaded or validated
- **THEN** the serial log does not contain the token
