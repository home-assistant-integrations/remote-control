## 1. Preparation

- [ ] 1.1 Connect the M5Paper by USB and find its serial port (`ls /dev/cu.*`). Note it for `PORT=`
- [ ] 1.2 Write a real configuration file (Wi-Fi and Home Assistant details) outside the repository, and an invalid copy with at least three errors
- [ ] 1.3 Run `make erase flash` and start `make monitor` with its output saved to a log file in the scratch directory

## 2. Hardware (m5paper-hardware)

- [ ] 2.1 On the first start, confirm that the screen is in portrait orientation with "Setup" at the top and nothing clipped
- [ ] 2.2 Confirm that the network name on the screen matches the log (`access point RemoteControl-XXXX started`), that the password is shown completely, and that the log does not contain the password (former 9.1)
- [ ] 2.3 Remove the USB cable during setup mode and confirm that the device keeps running and the network stays visible for 5 minutes. Then switch it on without USB and confirm that it boots
- [ ] 2.4 Start without touching the push button and confirm a normal start. Start while holding it and confirm the log line `push button held`
- [ ] 2.5 Scan both QR codes with an iPhone and an Android phone (15–40 cm) and confirm the join and open-URL prompts (former 5.4)

## 3. Setup flow (device-configuration, setup-mode)

- [ ] 3.1 Join by QR code and confirm that the upload page opens by itself or at `http://192.168.4.1/` (former 9.2)
- [ ] 3.2 Upload the invalid file and confirm that every error is listed and that the network and password stay the same (former 9.3)
- [ ] 3.3 Upload the valid file and confirm the success message, the restart message, a restart within 5 seconds, and the placeholder screen (former 9.4)
- [ ] 3.4 Switch the device off and on, then run `make flash`, and confirm that the configuration is kept both times (former 9.5)
- [ ] 3.5 Hold the push button at startup and confirm setup mode. Then restart without uploading and confirm that the old configuration is used (former 9.6)
- [ ] 3.6 From a laptop on the setup network, confirm with `curl` that a POST of 5000 bytes to `/config` returns 413 and that `GET /generate_204` returns 302 to `http://192.168.4.1/` (former 9.8)

## 4. Token check and wrap-up

- [ ] 4.1 Search the saved monitor log for the token from the configuration file and confirm it does not appear. Then delete the log (former 9.7)
- [ ] 4.2 For every failed check: fix the code, rebuild, repeat the check, and add a note to the task
- [ ] 4.3 Add entries under `## [Unreleased]` in `CHANGELOG.md` for every fix made in 4.2 (under Fixed). Add none if all checks passed
