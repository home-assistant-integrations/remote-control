## 1. Prerequisites and dependencies

- [x] 1.1 Confirm that `setup-firmware-build` is applied: `make build` succeeds and produces `build/remote-control.bin`
- [x] 1.2 Add `main/idf_component.yml` with `m5stack/m5unified` and `joltwallet/littlefs`, pinned to the current versions, and confirm that `make build` still succeeds
- [x] 1.3 Add `partitions.csv` (nvs 24 KB, phy_init 4 KB, factory 3 MB, config littlefs 64 KB) and select it in `sdkconfig.defaults` with `CONFIG_PARTITION_TABLE_CUSTOM=y`
- [x] 1.4 Add a `make erase` target (`idf.py erase-flash`) and list it in `make help`

## 2. Configuration validation (pure C)

- [x] 2.1 Create `components/config` with `config.h`: `config_t`, `config_errors_t` (fixed-size list of messages) and the `config_validate()` declaration
- [x] 2.2 Implement the file checks: size of at most 4096 bytes, parses as JSON, top level is an object, `version` is 1, no unknown keys at any level
- [x] 2.3 Implement the `wifi` rules: `ssid` 1 to 32 bytes, `password` empty or 8 to 63 characters
- [x] 2.4 Implement the `home_assistant.url` rule: http/https scheme, a host, an optional port, no path, query or fragment, and removal of a single trailing `/`
- [x] 2.5 Implement the `token` rule (three non-empty base64url parts, no whitespace) and the `dashboard` rule (1 to 64 characters from `[a-z0-9_-]`)
- [x] 2.6 Make each error message follow the form `<dotted.field>: <rule>`, and never include the token's value

## 3. Host unit tests for validation

- [x] 3.1 Add `test/host/test_config.c` and a `make test-host` target that compiles it with the host `cc` against cJSON from `toolchain/esp-idf`
- [x] 3.2 Add tests for a valid file and for `docs/user/config.example.json`
- [x] 3.3 Add tests for: not JSON, too large, wrong version, the three errors at once from the spec, and the misspelled key
- [x] 3.4 Add tests for normalizing the URL's trailing slash and rejecting paths, queries and bad schemes
- [x] 3.5 Add tests for a token with whitespace, a token with two parts, an empty dashboard and upper-case letters in the dashboard

## 4. Configuration storage

- [x] 4.1 Implement `config_fs_init()`: mount the `config` partition with LittleFS, and format it if it does not mount
- [x] 4.2 Implement `config_store()`: write `config.json.tmp`, call `fsync`, then `rename` it over `config.json`
- [x] 4.3 Implement `config_load()`: read `config.json`, run `config_validate()`, and log the errors (without the token) on failure

## 5. Screen (M5Unified)

- [x] 5.1 Create `main/screen.cpp` and `main/screen.h` with the C API `screen_init()`, `screen_show_message(const char *)` and `screen_show_configured(const config_t *)`
- [x] 5.2 Implement `screen_show_setup(ssid, password, url)`: the heading, the three steps, and the network name and password as text
- [x] 5.3 Add both QR codes to the setup screen: `WIFI:T:WPA;S:<ssid>;P:<password>;;` and the URL

## 6. Setup mode: network

- [x] 6.1 Create `components/setup`, and add the network name `RemoteControl-XXXX` built from the AP MAC address
- [x] 6.2 Generate the 12-character password with `esp_random()` between `bootloader_random_enable()` and `bootloader_random_disable()`, from the alphabet without look-alike characters
- [x] 6.3 Start the WPA2 access point at `192.168.4.1` with DHCP, using `esp_netif` and `esp_wifi`
- [x] 6.4 Add the DNS task on UDP port 53 that answers every A query with `192.168.4.1`

## 7. Setup mode: web server

- [x] 7.1 Add `setup/page.html` (form, inline CSS and JS, `fetch` to `POST /config`, shows success or the error list) and embed it with `EMBED_TXTFILES`
- [x] 7.2 Start `esp_http_server`, serve `GET /`, and redirect unknown paths with 302 to `http://192.168.4.1/`
- [x] 7.3 Implement `POST /config`: answer 413 above 4096 bytes, otherwise read the body and validate it
- [x] 7.4 On errors, answer 400 with `{"ok":false,"errors":[…]}` and change nothing
- [x] 7.5 On success, store the file, answer 200 with `{"ok":true}`, and start the 1-second restart timer
- [x] 7.6 Implement the restart task: `screen_show_message("Configuration saved, restarting")`, then `esp_restart()`

## 8. Boot flow

- [x] 8.1 Add the GPIO 38 check in `main.c`: input, and enter setup mode if it is held low for 3 seconds
- [x] 8.2 Wire up `app_main`: NVS, `config_fs_init`, screen, button check, `config_load`, then setup mode or `screen_show_configured`
- [x] 8.3 Confirm with `make build` that the image fits in the 3 MB factory partition

## 9. Documentation

- [x] 9.1 Write `docs/user/config.example.json`, matching the format in the spec
- [x] 9.2 Write `docs/user/configuration.md`: fields and rules, the setup steps, the button for setup mode, the mobile-data tip, and the advice to use a dedicated non-admin token
- [x] 9.3 Add the setup flow and a link to `docs/user/configuration.md` to `README.md`
- [x] 9.4 Add entries under `## [Unreleased]` in `CHANGELOG.md`: under Added, setup mode (access point, upload page, setup screen, setup button) and the configuration file. Under Changed, the new partition table that needs `make erase` once
