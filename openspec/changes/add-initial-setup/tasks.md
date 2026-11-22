## 1. Prerequisites and dependencies

- [ ] 1.1 Confirm that `setup-firmware-build` is applied: `make build` succeeds and produces `build/remote-control.bin`
- [ ] 1.2 Add `main/idf_component.yml` with `m5stack/m5gfx` and `joltwallet/littlefs`, pinned to the current versions, and confirm that `make build` still succeeds
- [ ] 1.3 Add `partitions.csv` (nvs 24 KB, phy_init 4 KB, factory 3 MB, config littlefs 64 KB) and select it in `sdkconfig.defaults` with `CONFIG_PARTITION_TABLE_CUSTOM=y`
- [ ] 1.4 Add a `make erase` target (`idf.py erase-flash`) and list it in `make help`

## 2. Configuration validation (pure C)

- [ ] 2.1 Create `components/config` with `config.h`: `config_t`, `config_errors_t` (fixed-size list of messages) and the `config_validate()` declaration
- [ ] 2.2 Implement the file checks: size of at most 4096 bytes, parses as JSON, top level is an object, `version` is 1, no unknown keys at any level
- [ ] 2.3 Implement the `wifi` rules: `ssid` 1 to 32 bytes, `password` empty or 8 to 63 characters
- [ ] 2.4 Implement the `home_assistant.url` rule: http/https scheme, a host, an optional port, no path, query or fragment, and removal of a single trailing `/`
- [ ] 2.5 Implement the `token` rule (three non-empty base64url parts, no whitespace) and the `dashboard` rule (1 to 64 characters from `[a-z0-9_-]`)
- [ ] 2.6 Make each error message follow the form `<dotted.field>: <rule>`, and never include the token's value

## 3. Host unit tests for validation

- [ ] 3.1 Add `test/host/test_config.c` and a `make test-host` target that compiles it with the host `cc` against cJSON from `toolchain/esp-idf`
- [ ] 3.2 Add tests for a valid file and for `docs/user/config.example.json`
- [ ] 3.3 Add tests for: not JSON, too large, wrong version, the three errors at once from the spec, and the misspelled key
- [ ] 3.4 Add tests for normalizing the URL's trailing slash and rejecting paths, queries and bad schemes
- [ ] 3.5 Add tests for a token with whitespace, a token with two parts, an empty dashboard and upper-case letters in the dashboard

## 4. Configuration storage

- [ ] 4.1 Implement `config_fs_init()`: mount the `config` partition with LittleFS, and format it if it does not mount
- [ ] 4.2 Implement `config_store()`: write `config.json.tmp`, call `fsync`, then `rename` it over `config.json`
- [ ] 4.3 Implement `config_load()`: read `config.json`, run `config_validate()`, and log the errors (without the token) on failure

## 5. Screen (M5GFX)

- [ ] 5.1 Create `main/screen.cpp` and `main/screen.h` with the C API `screen_init()`, `screen_show_message(const char *)` and `screen_show_configured(const config_t *)`
- [ ] 5.2 Implement `screen_show_setup(ssid, password, url)`: the heading, the three steps, and the network name and password as text
- [ ] 5.3 Add both QR codes to the setup screen: `WIFI:T:WPA;S:<ssid>;P:<password>;;` and the URL
- [ ] 5.4 Flash, then check by eye that the text is readable and both QR codes scan with a phone camera

## 6. Setup mode: network

- [ ] 6.1 Create `components/setup`, and add the network name `RemoteControl-XXXX` built from the AP MAC address
- [ ] 6.2 Generate the 12-character password with `esp_random()` between `bootloader_random_enable()` and `bootloader_random_disable()`, from the alphabet without look-alike characters
- [ ] 6.3 Start the WPA2 access point at `192.168.4.1` with DHCP, using `esp_netif` and `esp_wifi`
- [ ] 6.4 Add the DNS task on UDP port 53 that answers every A query with `192.168.4.1`

## 7. Setup mode: web server

- [ ] 7.1 Add `setup/page.html` (form, inline CSS and JS, `fetch` to `POST /config`, shows success or the error list) and embed it with `EMBED_TXTFILES`
- [ ] 7.2 Start `esp_http_server`, serve `GET /`, and redirect unknown paths with 302 to `http://192.168.4.1/`
- [ ] 7.3 Implement `POST /config`: answer 413 above 4096 bytes, otherwise read the body and validate it
- [ ] 7.4 On errors, answer 400 with `{"ok":false,"errors":[…]}` and change nothing
- [ ] 7.5 On success, store the file, answer 200 with `{"ok":true}`, and start the 1-second restart timer
- [ ] 7.6 Implement the restart task: `screen_show_message("Configuration saved, restarting")`, then `esp_restart()`

## 8. Boot flow

- [ ] 8.1 Add the GPIO 38 check in `main.c`: input, and enter setup mode if it is held low for 3 seconds
- [ ] 8.2 Wire up `app_main`: NVS, `config_fs_init`, screen, button check, `config_load`, then setup mode or `screen_show_configured`
- [ ] 8.3 Confirm with `make build` that the image fits in the 3 MB factory partition

## 9. Verification on the device

- [ ] 9.1 Run `make erase flash monitor` and confirm that the device enters setup mode on the first start and that the screen shows the values from the log
- [ ] 9.2 Join with a phone by QR code, and confirm that the page opens by itself or at `http://192.168.4.1/`
- [ ] 9.3 Upload an invalid file and confirm that every error is listed and that the network and password stay the same
- [ ] 9.4 Upload a valid file and confirm the success message, the restart message, the restart within 5 seconds, and the placeholder screen
- [ ] 9.5 Switch the device off and on, then run `make flash`, and confirm that the configuration is kept both times
- [ ] 9.6 Hold the push button at startup and confirm setup mode. Then restart without uploading and confirm that the old configuration is used
- [ ] 9.7 Search the serial log of all the steps above for the token and confirm it does not appear
- [ ] 9.8 Test with `curl`: POST 5000 bytes returns 413, and `GET /generate_204` returns a 302 to the upload page

## 10. Documentation

- [ ] 10.1 Write `docs/user/config.example.json`, matching the format in the spec
- [ ] 10.2 Write `docs/user/configuration.md`: fields and rules, the setup steps, the button for setup mode, the mobile-data tip, and the advice to use a dedicated non-admin token
- [ ] 10.3 Add the setup flow and a link to `docs/user/configuration.md` to `README.md`
