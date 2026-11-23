## MODIFIED Requirements

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

- **WHEN** the stored configuration passes validation, no setup button is
  held and no setup request is stored
- **THEN** the device does not enter setup mode, connects to Wi-Fi and Home
  Assistant, and shows the remotes of the configured dashboard
