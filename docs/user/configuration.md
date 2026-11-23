# Configuration

The M5Paper needs to know your Wi-Fi network and how to reach Home Assistant.
You give it this information once, as a small JSON file that you upload from
your phone or computer.

## The configuration file

```json
{
  "version": 1,
  "wifi": {
    "ssid": "MyNetwork",
    "password": "secret-password"
  },
  "home_assistant": {
    "url": "http://homeassistant.local:8123",
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJleGFtcGxlIn0.c2lnbmF0dXJl",
    "dashboard": "media-control"
  }
}
```

Start from [`config.example.json`](config.example.json) and replace the
values. All fields are required.

| Field | Meaning | Rule |
| --- | --- | --- |
| `version` | Format version | Always `1` |
| `wifi.ssid` | Name of your Wi-Fi network | 1 to 32 bytes |
| `wifi.password` | Wi-Fi password | Empty for an open network, otherwise 8 to 63 characters |
| `home_assistant.url` | Address of Home Assistant, as you open it in the browser | Starts with `http://` or `https://`, with host and optional port, and nothing after it. A trailing `/` is removed |
| `home_assistant.token` | Long-lived access token | The complete token, as Home Assistant shows it |
| `home_assistant.dashboard` | URL path of the dashboard that holds your remotes | 1 to 64 characters from `a`–`z`, `0`–`9`, `-` and `_` |

Rules for the whole file:

- At most 4096 bytes, UTF-8, one JSON object.
- No other keys. A misspelled key, such as `pasword`, is reported as unknown.

### Getting the token

1. In Home Assistant, open your profile (click your name at the bottom left),
   then the **Security** tab.
2. Under **Long-lived access tokens**, choose **Create token**, give it a name
   such as "M5Paper", and copy the token. Home Assistant shows it only once.

A token has all the rights of the user who created it. Create a separate
user without administrator rights for the device, and create the token while
logged in as that user. If the device is lost, delete the token in the same
place.

### Finding the dashboard

Open the dashboard in Home Assistant and look at the address bar. In
`http://homeassistant.local:8123/media-control/0`, the dashboard is
`media-control`.

## Setting up the device

On the first start, and whenever no valid configuration is stored, the device
starts **setup mode**:

1. The screen shows the name and password of a Wi-Fi network that the device
   opens itself, `RemoteControl-XXXX`, and two QR codes.
2. Scan the left QR code with your phone camera to join that network, or join
   it by hand with the password from the screen.
3. The upload page usually opens by itself. If not, scan the right QR code or
   open `http://192.168.4.1/`.
4. Choose your configuration file and tap **Upload**.

If the file has errors, the page lists every one of them, for example
`wifi.password: must be empty or 8 to 63 characters`. Fix the file and upload
it again. The device stays in setup mode and keeps the same network and
password.

If the file is valid, the page says that the configuration was saved, the
screen shows "Configuration saved, restarting", and the device restarts with
the new configuration.

If the page doesn't open, turn off mobile data on your phone. Some phones
leave a Wi-Fi network without internet access and use mobile data instead.

## Building remotes in Home Assistant

The device shows the dashboard named in `home_assistant.dashboard`. Every
view of that dashboard is one remote, in the dashboard's order, titled with
the view's title.

Build the views with [Bubble Card](https://github.com/Clooos/Bubble-Card).
The device draws these card types:

| Bubble Card type | On the device |
| --- | --- |
| `separator` | A heading with icon and name |
| `button` | One button. Without a `tap_action`, a tap toggles the card's entity |
| `media-player` | A tile with name, cover art, and title and app (or the state), plus power, previous, play/pause and next buttons, and volume down and up. Each control can be hidden with `hide: <control>_button: true` |
| `sub-buttons` | One row per entry under `sub_button` → `bottom`, with the buttons of its `group`. `justify_content: fill` divides the row evenly; otherwise the buttons keep their `width` |

Other cards are skipped. A button uses its `name`, `icon`, `show_icon`,
`show_name`, `width` and `custom_height`. Tap runs the `tap_action`, holding
for half a second runs the `hold_action`. `perform-action`, `call-service`
and `toggle` are supported. `more-info`, `navigate` and `url` do nothing on
the device. Templates in card options, such as `state_content`, are not
evaluated.

Icons are Material Design Icons (`mdi:...`). The firmware contains about 100
of them. A button whose icon is not among them shows its name instead. See
`assets/icons.txt` for the list.

Changes to the dashboard appear on the device within a few seconds, without
a restart.

## Using the remote

- **Buttons:** tap, or hold for half a second for the long-press action. The
  button turns black until Home Assistant has answered. If the action fails,
  a message appears at the bottom for 3 seconds.
- **Several remotes:** swipe left or right, tap the arrows next to the title,
  or move the rocker switch up or down. The dots under the title show which
  remote you are on. After a restart, the device shows the remote you used
  last.
- **Status bar:** Wi-Fi signal, date and time, battery level, and the setup
  button (gear). A crossed-out Wi-Fi icon means no network. A Wi-Fi icon with
  an exclamation mark means Home Assistant is not reachable.

The date and time come from the internet (NTP), in the time zone set in Home
Assistant. The device keeps them in its clock while switched off.

## Changing the configuration later

Tap the gear at the top right and confirm **Restart**. The device restarts in
setup mode with a new password. Upload the new file as described above.

You can also hold the push button of the rocker switch (press it in) while
you switch the device on, and keep holding it for 3 seconds. Use this if the
remote does not start, for example because the Wi-Fi password in the file is
wrong.

Until a valid new file is uploaded, the old configuration stays. If you
restart the device without uploading anything, it starts with the old
configuration.

The configuration survives restarts, empty batteries and firmware updates
with `make flash`. Only `make erase` removes it.

## Security

- The setup network is protected by a password that is shown only on the
  device's screen.
- The token is stored in the device's flash memory without encryption.
  Anyone who has the device and a USB cable can read it. Use a token of a
  non-admin user and delete it in Home Assistant if the device is lost.
- The device never shows the token on the screen, writes it to the log or
  sends it back over the network.
