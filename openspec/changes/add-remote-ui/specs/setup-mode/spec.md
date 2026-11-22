## ADDED Requirements

### Requirement: Enter setup mode from the status bar

When the user confirms the setup dialog in the status bar, the device SHALL
store a one-time setup request (an NVS flag) and restart. At the next start,
the flag MUST cause setup mode exactly as the held push button does: the
stored configuration stays in use until a valid new file is uploaded. The flag
is cleared as soon as setup mode starts, so a later restart without an upload
starts normally.

#### Scenario: Setup requested from the remote

- **WHEN** the user confirms "Restart" in the setup dialog
- **THEN** the device restarts and shows the setup screen with a new network
  password

#### Scenario: Restart without upload

- **WHEN** setup mode was entered from the status bar and the device is
  restarted without an upload
- **THEN** it starts normally with the existing configuration
