## 1. Preparation

- [ ] 1.1 Connect the M5Paper by USB and find its serial port (`ls /dev/cu.*`). Note it for `PORT=`
- [ ] 1.2 Write a real configuration file (Wi-Fi and Home Assistant details) outside the repository, and an invalid copy with at least three errors
- [ ] 1.3 Run `make erase flash` and start `make monitor` with its output saved to a log file in the scratch directory

## 2. Hardware (m5paper-hardware)

- [ ] 2.1 On the first start, confirm that the screen is in portrait orientation with "Setup" at the top and nothing clipped (also covers add-remote-ui 1.5)
- [ ] 2.2 Confirm that the network name on the screen matches the log (`access point RemoteControl-XXXX started`), that the password is shown completely, and that the log does not contain the password (former 9.1)
- [ ] 2.3 Remove the USB cable during setup mode and confirm that the device keeps running and the network stays visible for 5 minutes. Then switch it on without USB and confirm that it boots (also covers add-remote-ui 1.4)
- [ ] 2.4 Start without touching the push button and confirm a normal start. Start while holding it and confirm the log line `push button held`
- [ ] 2.5 Scan both QR codes with an iPhone and an Android phone (15–40 cm) and confirm the join and open-URL prompts (former 5.4)
- [ ] 2.6 Log, from M5Unified: the real-time clock time, the battery voltage, touch coordinates in portrait orientation, and `BtnA`/`BtnB`/`BtnC` presses. Confirm that BtnA is GPIO 37 and BtnC is GPIO 39 (add-remote-ui 1.3)

## 3. Setup flow (device-configuration, setup-mode)

- [ ] 3.1 Join by QR code and confirm that the upload page opens by itself or at `http://192.168.4.1/` (former 9.2)
- [ ] 3.2 Upload the invalid file and confirm that every error is listed and that the network and password stay the same (former 9.3)
- [ ] 3.3 Upload the valid file and confirm the success message, the restart message, and a restart within 5 seconds into the remote UI (former 9.4; also covers add-remote-ui 1.1)
- [ ] 3.4 Switch the device off and on, then run `make flash`, and confirm that the configuration is kept both times (former 9.5)
- [ ] 3.5 Hold the push button at startup and confirm setup mode. Then restart without uploading and confirm that the old configuration is used (former 9.6)
- [ ] 3.6 From a laptop on the setup network, confirm with `curl` that a POST of 5000 bytes to `/config` returns 413 and that `GET /generate_204` returns 302 to `http://192.168.4.1/` (former 9.8)

## 4. Remote UI (ha-connection, remote-layout, remote-ui, status-bar)

- [ ] 4.1 Start with your configuration and confirm the "connecting" screens, then the Wohnzimmer remote with the heading, media tile and five button rows (add-remote-ui 10.1)
- [ ] 4.2 Tap every D-pad button, Zurück, TV and both volume buttons, and confirm that the Apple TV reacts. Hold Zurück and TV and confirm the long-press actions (add-remote-ui 10.2)
- [ ] 4.3 Check that the umlauts in "Zurück", "Menü" and "Lautstärke" and all icons render correctly (add-remote-ui 10.3)
- [ ] 4.4 Play something on the Apple TV and confirm that the title and play/pause state update within 5 seconds (add-remote-ui 10.4)
- [ ] 4.5 Add a second view in Home Assistant and confirm that the device picks it up within 5 seconds, that page dots appear, and that swipes, arrows and the rocker page, in the right direction (add-remote-ui 10.5)
- [ ] 4.6 Restart on page 2 and confirm it comes back. Remove the view and confirm it falls back to page 1 (add-remote-ui 10.6)
- [ ] 4.7 Compare the status bar with a phone: time and date, Wi-Fi level, battery percentage. Turn off the router and confirm the off icon, then turn it on and confirm the reconnect (add-remote-ui 10.7)
- [ ] 4.8 Restart Home Assistant and confirm that the remote keeps working without user action (add-remote-ui 10.8)
- [ ] 4.9 Tap the gear: confirm that Cancel and the 15-second timeout close the dialog, and that Restart enters setup mode with the old configuration kept (add-remote-ui 10.9)
- [ ] 4.10 Run on battery alone for one hour and note the battery drop, for the deep sleep question (add-remote-ui 10.10)

## 5. Token check and wrap-up

- [ ] 5.1 Search the saved monitor log of the whole session for the token from the configuration file and confirm it does not appear. Then delete the log (former 9.7)
- [ ] 5.2 For every failed check: fix the code, rebuild, repeat the check, and add a note to the task
- [ ] 5.3 Add entries under `## [Unreleased]` in `CHANGELOG.md` for every fix made in 5.2 (under Fixed). Add none if all checks passed
