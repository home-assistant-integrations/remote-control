## Why

A freshly flashed M5Paper knows neither the Wi-Fi network nor the Home
Assistant URL, the token or the dashboard. Without a setup path, these values
would have to be compiled into the firmware. The device needs a way to receive
its configuration from the user without a computer, cable or rebuild.

## What Changes

- At every start, the firmware checks whether a valid configuration is stored.
  If there is none, it starts setup mode instead of the remote control.
- In setup mode, the device opens its own Wi-Fi access point, protected by a
  random password, and runs a web server.
- The e-paper screen shows the network name, the password, the URL of the
  upload page, and a QR code that joins the network.
- The upload page accepts a JSON configuration file. The firmware checks it,
  and the page shows every error it finds.
- A valid file is stored in a new flash partition and the device restarts with
  it. An invalid file changes nothing and the device stays in setup mode.
- Holding a button while the device starts enters setup mode even when a
  configuration exists, for example to fix a wrong Wi-Fi password.
- The configuration format is documented with an example file.

## Capabilities

### New Capabilities

- `device-configuration`: The configuration file format, its validation rules,
  persistent storage, and the check at startup.
- `setup-mode`: The access point, upload web page, setup screen, the restart
  after a successful upload, and entering setup mode with a button.

### Modified Capabilities

None. `openspec/specs/` is empty.

## Non-goals

- Testing the Wi-Fi credentials, Home Assistant URL or token before
  restarting. The check covers format and values only.
- Editing single values in the browser. Only whole files are uploaded.
- Downloading the stored configuration. It contains the token.
- Flash encryption or encrypted storage of the token.
- Setup over Bluetooth, USB or the SD card.
- The remote control itself. After a successful setup the device shows a
  placeholder screen.

## Impact

- Depends on the change `setup-firmware-build`, which provides the ESP-IDF
  project and the Makefile.
- New ESP-IDF components: `m5stack/m5gfx` (display, QR code) and
  `joltwallet/littlefs` (file system), both from the component registry.
  Built-in components used: `esp_wifi`, `esp_http_server`, `esp_netif`, `nvs_flash`, `json`.
- New partition table `partitions.csv` with a `config` partition. Changing it
  erases the flash layout once.
- New source modules under `main/` or `components/`: configuration, setup
  mode, screen.
- New documentation: `docs/user/configuration.md` and
  `docs/user/config.example.json`.
