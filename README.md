# thermostat

In-wall touch panel for controlling mini-split heat pumps through Home
Assistant.

Nothing about your home is compiled into the firmware. A new panel comes up
running its own Wi-Fi network with a setup page on it; see
[First run](#first-run).

Built with [PlatformIO](https://platformio.org/) (Arduino framework),
[LVGL 9](https://lvgl.io/) for the UI, and
[Arduino_GFX](https://github.com/moononournation/Arduino_GFX) for the LCD.

## Hardware

[Waveshare ESP32-C6-Touch-LCD-1.47](https://docs.waveshare.com/ESP32-C6-Touch-LCD-1.47):
everything is on one board.

| Part     | Detail                                                        |
| -------- | ------------------------------------------------------------- |
| MCU      | ESP32-C6 (single-core RISC-V, 160MHz), 8MB flash, Wi-Fi 6, BLE 5 |
| Display  | 1.47" IPS, 172x320, JD9853 controller, SPI                    |
| Touch    | AXS5106L capacitive, I2C @ 0x63                               |
| Also     | QMI8658 IMU, TF card slot, battery charger (all unused so far) |

There's no wiring to do. The on-board pin assignments are in
[`include/config.h`](include/config.h).

The mini-splits run [mitsubishi2mqtt](https://github.com/gysmo38/mitsubishi2mqtt)
and are already in Home Assistant over MQTT, so this device never drives HVAC
equipment directly. The heat pumps run their own control loops; this panel
shows their state and sends commands over the same MQTT topics HA uses.

## First run

Flash the firmware (below) — there are no files to edit first.

1. The panel starts in **setup mode**, showing a QR code, the name of its own
   Wi-Fi network (`Thermostat-` plus the end of its MAC address) and a
   password generated fresh for this session. The screen stays awake.
2. **Scan the code** with a phone to join that network, or join it by hand and
   open `http://192.168.4.1`. The setup page opens by itself once joined:
   phones check for a captive portal, and the panel answers every DNS lookup
   with its own address.
3. **Step 1 of 3, Wi-Fi:** your **network** (picked from the ones in range)
   and its **password**, plus an optional **name for this panel** — what the
   panel calls itself on your network: the name in your router's device list,
   the `<name>.local` address, and the start of the MQTT client id the broker
   logs. It has nothing to do with the mini-split's name; blank means
   `thermostat`. **Next** connects, and only moves on if that works. If it
   doesn't, the page says why: network not found, password refused, or timed
   out.
4. **Step 2 of 3, MQTT:** the **broker** and **port** — Home Assistant's
   Mosquitto, normally `homeassistant.local:1883`, not port 8123, which is
   HA's web UI — and the **username and password** the mini-splits use.
   **Next** connects to the broker, and only moves on if that works, telling
   a refused login from an unreachable broker. Leave the login blank only if
   your broker allows anonymous clients.
5. **Step 3 of 3, mini-split:** tap the unit this panel controls. The list is
   the mini-splits Home Assistant advertises over MQTT, which the panel knows
   because step 2 succeeded. You can also skip and pick it later on the panel
   under **Settings → Mini-split → Change**. Either way the panel saves
   everything and restarts, coming up on your network showing that unit.

While step 1 is testing, the panel changes Wi-Fi channel to match your
network, which can drop your phone from the setup network for a moment. The
waiting pages reload themselves, so rejoining is all it takes. The panel's own
screen shows which step you're on throughout.

Everything is saved in the ESP32's flash (NVS), which survives reflashing the
firmware, so a panel is only set up once. To change anything later, use
**Settings → Wi-Fi setup**, which walks the same three steps with the saved
values filled in, or **Settings → MQTT setup** to change only the broker,
starting at step 2. That one needs no access point: the panel serves the pages
on the network it's already on and shows its own address as the QR code, so
your phone stays on your Wi-Fi. It's greyed out while Wi-Fi is down, because
testing a broker needs the network. Each password field says which it is: once one is saved,
leaving it blank keeps it; until then it's required. Closing setup with its X
restarts the panel, because the tests have by then taken over the radio and
the broker connection. `pio run -t erase` wipes the saved settings and sends
the panel back to step 1.

## Updates

A panel in a wall is awkward to reach, so it updates itself. New firmware is
published as a GitHub release of this repo, and the panel installs it over
Wi-Fi; the USB cable is only needed for the first flash.

### On the panel

**Settings → Firmware** shows the version running and which channel it's on,
what the last check found, and a button that does whichever of the two things
makes sense:

```
Firmware
1.4.2 (stable)
Update available: 1.5.0
[ Install 1.5.0 ]
[ Channel: Stable ]
```

The panel checks by itself a minute after the network comes up and once a day
after that, and **Check for updates** asks straight away. A check fetches one
small JSON file — the version available and where to get it — so it costs
almost nothing to do daily.

Tapping **Install** downloads the firmware into whichever of the two app slots
isn't running, showing the percentage as it goes, and restarts into it. The
screen stays awake for the download, and the panel keeps working throughout:
the download runs in a task of its own, so the UI stays responsive and the
mini-split stays controllable until the restart.

**A new firmware is on trial for its first minute.** The bootloader keeps the
one it replaced until the new one has run for `OTA_SELF_TEST_MS` with the
network up; only then does the panel say to keep it. One that crashes on the
way up, or that can't join Wi-Fi, is replaced by its predecessor on the next
boot. That's the safety net that makes updating a device you can't easily
reach a reasonable thing to do.

**Two channels**, under **Channel**:

- **Stable** — the tagged releases. What a panel should normally be on.
- **Beta** — rebuilt on every push to `dev`. Handy on a panel on the bench,
  and something to switch back off afterwards.

Switching channels checks the new one immediately, and installs whatever it
offers — including an older version, if the channel you switched to is behind.
Any version that isn't the one running counts as an update, which is what
makes going back down a channel work.

Nothing is checked or downloaded while setup mode is running: that has the
radio busy testing the network and broker being typed in.

Serial log: `Firmware: 1.4.2 (stable channel), running from app0`,
`Update: 1.5.0 available (running 1.4.2)`, `Update: downloading ...`, and
`Firmware: 1.5.0 passed its trial run and is now the one to keep`.

### Releasing

Two branches, each wired to a channel by
[`.github/workflows/release.yml`](.github/workflows/release.yml):

| Branch          | On a push                                       | Reaches           |
| --------------- | ----------------------------------------------- | ----------------- |
| `dev`           | rebuilds the rolling `beta` prerelease          | beta panels       |
| `main`, tagged  | publishes the release `v1.5.0`                  | stable panels     |
| anything else   | builds only, firmware kept as a run artifact    | nobody            |

Work happens on branches off `dev`, which
[`.github/workflows/ci.yml`](.github/workflows/ci.yml) builds on every push and
pull request. Merging into `dev` puts a build in front of any panel on the beta
channel. Shipping it is a tag:

```bash
git switch main && git merge --ff-only dev
git tag v1.5.0 && git push origin main --tags
```

Each release carries three files:

| File                   | What it's for                                            |
| ---------------------- | -------------------------------------------------------- |
| `manifest.json`        | version + download URL: all a panel fetches to check      |
| `firmware.bin`         | the app image, what a panel installs over the air         |
| `firmware.factory.bin` | bootloader + partitions + app, to flash a new panel at 0  |

Panels look for the manifest at a URL that doesn't change with the version —
`releases/latest/download/manifest.json` for stable, which is GitHub resolving
"the newest release that isn't a prerelease", and `releases/download/beta/` for
beta. Both URLs are in [`include/config.h`](include/config.h) and name this
repo, so a fork changes them. The manifest found there names an exact tag to
download from, so a release published between the check and the install can't
swap the firmware underneath it.

**Version numbers come from the tag** and nothing else edits them. The tag
`v1.5.0` builds a firmware that calls itself `1.5.0`; a beta build is numbered
from the last tag it's ahead of plus the run number, like `1.5.0-beta.41`.
[`tools/firmware_version.py`](tools/firmware_version.py) compiles that string
in as `FIRMWARE_VERSION`, and
[`tools/make_manifest.py`](tools/make_manifest.py) writes the same string into
the manifest, so the two can't drift apart. A local build instead describes
itself from git (`1.5.0-3-gb209436-dirty`), which never matches a manifest —
so a hand-built panel is never told it's out of date, and the settings page
always says exactly what's on it.

The `beta` release is deleted and recreated on each push so its tag follows
`dev`. A panel checking during the few seconds that takes reports a failed
check and tries again.

### Trust

The panel checks and downloads over HTTPS, verified against the certificate
bundle built into the ESP-IDF, so there's no CA certificate here to expire.
The image carries its own SHA-256, which `esp_ota` checks before the panel is
allowed to boot from it, so a truncated or corrupted download is refused
rather than installed. The manifest repeats that hash for checking a download
by hand.

This all assumes the repo is public: the panel sends no credentials, so a
private repo's release assets would 404.

### Flashing a new panel

A panel that has never run this firmware needs one flash over USB — after
that it can take care of itself. With PlatformIO, use **Upload** (below).
Without it, `firmware.factory.bin` from any release is the whole flash image:

```bash
esptool --chip esp32c6 write-flash 0x0 firmware.factory.bin
```

(`esptool.py --chip esp32c6 write_flash 0x0 firmware.factory.bin` on esptool 4
and earlier.)

## Working in VSCode

Open this folder in VSCode with the PlatformIO extension and use the
toolbar at the bottom:

- **✓ Build**: compile
- **→ Upload**: compile + flash over USB-C
- **🔌 Monitor**: serial monitor (115200 baud)

If an upload can't find the board, hold **BOOT**, tap **RESET**, release
**BOOT**, and upload again.

### From the command line

```bash
~/.platformio/penv/bin/pio run                  # build
~/.platformio/penv/bin/pio run -t upload        # build + flash
~/.platformio/penv/bin/pio device monitor       # serial monitor
~/.platformio/penv/bin/pio run -t erase         # also wipe the saved settings
```

## Layout

```
platformio.ini            board, platform, libraries, build flags
include/
  config.h                pins, timing, and the defaults the setup form starts with
  lv_conf.h               LVGL options (fonts, memory, QR code, logging)
src/
  main.cpp                wiring + loop only
  display/
    display.h/.cpp        LCD + touch -> LVGL glue, backlight sleep
    jd9853_init.h         LCD controller power-on register sequence
  touch/
    axs5106l.h/.cpp       touch controller driver
  climate/
    climate.h/.cpp        ClimateState: one mini-split's mode, fan, action, temps
    temperature.h/.cpp    setpoint stepping, formatting
    tracked.h             a value the panel sets and the unit reports back
    mini_split.h/.cpp     mitsubishi2mqtt topics: reads state, sends changes
  net/
    wifi_connection.h/.cpp connects and stays connected to Wi-Fi, mDNS
    wifi_status.h         connection snapshot the UI can display
    mqtt_connection.h/.cpp connects to the MQTT broker (ESP-IDF client)
    provisioning.h/.cpp   setup mode: the panel's own network and 3-step setup
    setup_pages.h/.cpp    the HTML those steps serve (rendering only)
    ota_updater.h/.cpp    checks for and installs firmware over the air
    ota_status.h          update snapshot + channel the UI can display
  storage/
    settings_store.h/.cpp what the panel was told about your home, saved in flash
  ui/
    ui.h/.cpp             LVGL thermostat screen
    overlay_screen.h/.cpp base for the full-screen pages (lifecycle, header)
    picker.h/.cpp         full-screen option grid (mode, fan)
    settings_screen.h/.cpp settings page: brightness, Wi-Fi, MQTT, mini-split, firmware
    setup_screen.h/.cpp   setup mode screen (QR code to join, step progress)
    list_screen.h/.cpp    scrollable list of labelled choices (units, louvres)
    airflow_screen.h/.cpp both louvres, behind the bottom-centre button
    widgets.h/.cpp        shared flat button, overlay + icon helpers
    fonts/                GENERATED icon fonts, temperature font, fonts.h (see below)
tools/
  generate_fonts.py       regenerates src/ui/fonts/
  firmware_version.py     stamps the build with FIRMWARE_VERSION (PlatformIO pre-script)
  make_manifest.py        writes the manifest.json a panel checks against
.github/workflows/
  ci.yml                  builds every branch and pull request
  release.yml             publishes the stable and beta releases panels update from
hardware/                 enclosure, mounting notes
```

### Fonts

LVGL can't load font files at runtime, so fonts are compiled in as C files of
pre-rendered glyphs. `tools/generate_fonts.py` builds the custom ones in
`src/ui/fonts/` with LVGL's `lv_font_conv`, including only the glyphs we use:

- **Icons** (`mdi_icons_24/32/40`): [Material Design Icons](https://pictogrammers.com/library/mdi/),
  the set Home Assistant uses (Apache 2.0). `fonts.h` has an `ICON_*` name
  for each. To add one, edit the `ICONS` list in the script.
- **Target temperature** (`temperature_48`): Montserrat 48px (the typeface of
  LVGL's built-in fonts, SIL OFL 1.1) with only `0-9 . ° F C`. That's ~6KB
  instead of ~97KB for LVGL's full built-in 48px font. Any other character
  (like the `--` shown while the target is unknown) falls back to
  `lv_font_montserrat_20`.

The text fonts at 14px and 20px are LVGL's built-ins, enabled in
`include/lv_conf.h`. Run the script from the project root (needs Node.js):

```bash
python3 tools/generate_fonts.py
```

How it fits together:

- **Display:** `main.cpp` calls `Display::begin()`, which starts the LCD over
  SPI, the touch controller over I2C, and LVGL. From then on the loop calls
  `Display::tick()`. LVGL redraws whatever changed through `flushCb` and polls
  the touch controller through `touchReadCb`.
- **Nothing private is compiled in.** `SettingsStore` holds the network, panel
  name, broker login and chosen mini-split in flash. `Provisioning` collects
  them in setup mode; `main.cpp` saves them and restarts, which is simpler and
  more predictable than reconfiguring Wi-Fi, MQTT and its subscriptions while
  they're running.
- **State flows one way.** `MiniSplit` owns the `ClimateState`, filled in
  from MQTT, and `main.cpp` hands it to `Ui::show()` whenever it changes.
  When you tap a control, the UI doesn't change the state itself. It calls
  back into `main.cpp` (`setTargetTemp`, `setMode`, `setFanMode`), which
  passes the request to `MiniSplit` and shows the result. The settings page's
  buttons work the same way.
- **Temperatures are in the mini-split's own unit.** The unit (°F or °C),
  setpoint limits and +/- step all come from the mini-split's Home Assistant
  discovery config. They're set on the mini-split, and the panel just
  reflects them; it never converts.
- **MQTT runs in its own task.** ESP-IDF's MQTT client (built into the
  Arduino core) connects, reconnects and sends in the background, so a slow
  broker can't freeze the UI. Received messages are queued and handled in
  `loop()`.

## Roadmap

Built one step at a time:

1. **Board bring-up** ✓ The screen draws, touch works, and colors are right.
   Responsiveness tuning: flat widget styles (no shadows or animated
   transitions), 16ms touch/refresh period, `-O2`, 80MHz SPI. The FPS/CPU
   overlay is toggled with `LV_USE_PERF_MONITOR` in `lv_conf.h`.
2. **Thermostat screen** ✓ One mini-split, portrait layout.
   - Top-left: settings button. Opens the settings page, whose title bar
     stays put while its sections scroll: Screen Brightness (a slider,
     applied live and saved to flash when you let go), Wi-Fi (status, and a
     button for setup), MQTT (broker, status, and a button for broker-only
     setup) and Mini-split (unit, status, and a button to change it). The X in
     its top-left corner closes it.
   - Top center: current temperature with its unit (e.g. 72°F).
   - Center: target temperature, with triangle arrows above (up) and below
     (down). Faded and locked in Off, Dry and Fan modes, where it doesn't
     apply (Mitsubishi units have no setpoint in Dry: the unit decides when
     to dehumidify. The value they report is the one Heat/Cool will use).
   - Bottom-left: mode button. Its icon shows the current mode (Off, Heat,
     Cool, Auto, Dry, Fan), and the whole background takes on the mode's
     color (black for Off). Buttons are translucent so they suit any
     background.
   - Bottom-right: fan button. Its icon shows the current fan speed (Auto,
     Quiet, Low, Middle, Medium, High).
   - Tapping either opens a full-screen picker; tapping an option selects it
     and closes the picker. If nothing is chosen within 10 seconds
     (`PICKER_TIMEOUT_MS`), the picker closes without changing anything.

   Touch is interrupt-driven: the driver only reads the controller after it
   signals a new report on its INT pin. Polling it constantly made it stop
   answering after about a minute.
3. **Wi-Fi** ✓ Connect and stay connected.
   - Connects in the background at startup; the UI is usable immediately.
   - The Arduino core reconnects by itself after most drops. Because it gives
     up on a few failures (e.g. `AUTH_FAIL`, which some routers report after
     rebooting), the panel also starts a fresh attempt after 30s offline
     (`WIFI_RETRY_INTERVAL_MS`).
   - The panel name from setup is its hostname, so it's easy to find in your
     router's device list, and it answers to `<name>.local` (mDNS).
   - Top-right Wi-Fi icon, like a phone's: 1-4 bars when connected (-60 /
     -70 / -80 dBm thresholds, `signalBars()` in `net/wifi_status.h`), or a
     crossed-out icon when not. Updates every 5s.
   - Settings page shows the network, IP address and signal strength (with
     the same thresholds: strong / good / weak / very weak).
   - Serial log: `Wi-Fi: connected, IP ..., signal ... dBm`; failures show the
     core's `Reason: ...` line (e.g. `201 - NO_AP_FOUND` = network name not
     found, `15 - 4WAY_HANDSHAKE_TIMEOUT` = usually a wrong password).
4. **MQTT ↔ mini-split** ✓ Show the mini-split's real state and send changes.
   - Reads, for the chosen unit:
     - `homeassistant/climate/<name>/config`: temperature unit, setpoint
       limits (50-90°F here) and step (1°)
     - `mitsubishi2mqtt/<name>/availability`: online / offline
     - `mitsubishi2mqtt/<name>/settings`: setpoint, mode, fan (retained, so
       it arrives as soon as the panel connects)
     - `mitsubishi2mqtt/<name>/state`: the same plus room temperature and
       what the unit is doing, every 30s. After a restart the room
       temperature shows `--` until the first one arrives.
   - Sends to `mode/set`, `temp/set` and `fan/set` under
     `mitsubishi2mqtt/<name>/`, with the same values HA sends (`heat_cool`,
     `diffuse`, `72`, ...).
   - Changes show on screen immediately. Quick +/- taps are sent as one
     change once they pause for 1s (`SETPOINT_SEND_DELAY_MS`), so the unit
     beeps once. Until the unit reports the new value, older reports are
     ignored for up to 5s (`PENDING_HOLD_MS`), so the screen doesn't jump
     back.
   - The background shows what the unit is doing (`action` in its `state`
     messages, every 30s): in Heat, orange while heating and dark red once
     it's idle at the setpoint; in Cool, blue while cooling and deep blue
     when idle; in Dry, teal while drying and dark teal when idle. Auto
     stays green because mitsubishi2mqtt always reports it as idle. Right
     after a mode change the unit guesses (e.g. "cooling"), and its next
     report corrects it.
   - Until the unit is online (no Wi-Fi, broker unreachable, unit offline, or
     no unit picked), the last known values stay on screen, faded, and the
     controls are locked. The settings page says which of those it is.
   - Serial log: `MQTT: connected`, `Mini-split: OfficeHVAC online`,
     `Mini-split: room 82°F, target 77°F, mode Off (off), fan Auto`, and each
     command sent. A wrong login logs `broker refused the connection`.
5. **Setup on the device** ✓ No network name, password, broker
   login or unit name compiled into the firmware, so the same image runs on
   any panel and nothing private lives in git.
   - Setup mode runs an access point, a DNS server that answers every lookup
     with the panel's own address, and a web server with the three steps. See
     [First run](#first-run) for the flow.
   - **Each step tests what it collected** before moving on, so a typo is
     caught on the page where it was made: step 1 connects to the network
     (`SETUP_WIFI_TEST_TIMEOUT_MS`), step 2 to the broker
     (`SETUP_MQTT_TEST_TIMEOUT_MS`), and step 3 can only list units because
     step 2 worked. The pages that wait reload themselves, and each has a
     Back link.
   - The access point runs in `AP_STA` mode so the panel can hold a station
     connection of its own to run those tests. It changes channel to match
     the network being tested, which can briefly drop the phone; that's what
     the self-reloading pages recover from.
   - Two ways in, as `Provisioning::Mode`. **Full** (first run, or "Wi-Fi
     setup") runs the panel's own network from step 1. **BrokerOnly** ("MQTT
     setup") skips the access point and the captive DNS entirely, serves the
     pages over the working network from step 2, and shows the panel's address
     as the QR code. Both end at step 3, so the unit can be re-picked after a
     broker change.
   - Nothing is saved until the last step. `Provisioning` collects a draft,
     and `MqttConnection::stop()` lets a failed login be retried with new
     credentials. `WifiConnection::pause()` keeps the saved-network retry
     from pulling the radio back mid-test.
   - The setup network is WPA2 with a password generated fresh each time
     setup starts, using characters that can't be misread (no `l`/`1`,
     `O`/`0`). It's shown on the panel with a QR code that carries both the
     name and the password, so a phone joins by scanning.
   - The form lists the networks in range. The scan runs before the access
     point starts, because scanning takes the radio away for a couple of
     seconds.
   - Finishing restarts the panel (`SETUP_RESTART_DELAY_MS` later, so the
     phone can load the "All set" page first), which is simpler than
     reconfiguring Wi-Fi, MQTT and its subscriptions in place.
   - The units come from the discovery topic
     (`homeassistant/climate/+/config`, up to `MINI_SPLIT_MAX_UNITS`), so
     they can be picked in step 3 or later on the panel under **Settings →
     Mini-split → Change**. Picking one there saves it and restarts too.
   - A rejected broker login (return code 4 or 5) is called out as such: the
     settings page says "MQTT login refused: fix it in Wi-Fi setup" instead of
     retrying silently behind "Connecting to MQTT...".
   - Serial log: `Setup: 12 networks in range`,
     `Setup: network "Thermostat-39FEFF", password ..., page http://192.168.4.1/`,
     and on each boot the network, panel name, broker and MQTT username (never
     the passwords).
6. **Polish** ← *current*: enclosure, and maybe a local temperature sensor.
   - Updates over the air ✓ The panel installs its own firmware from this
     repo's GitHub releases, so a panel in a wall never needs the USB cable
     twice. See [Updates](#updates) for the whole of it.
     - **Settings → Firmware** shows the version, the channel and what the
       last check found, with one button that checks or installs depending
       on which there is to do.
     - Checks a minute after the network comes up
       (`OTA_FIRST_CHECK_DELAY_MS`) and once a day after that
       (`OTA_CHECK_INTERVAL_MS`), fetching only a few hundred bytes of JSON.
     - The check and the download both run in a task of their own, so a slow
       or unreachable GitHub never stalls the UI, the way MQTT already works.
       `loop()` only reads the status they publish.
     - Two channels, **Stable** (tagged releases) and **Beta** (every push to
       `dev`), saved in flash like the rest of the settings. Any version
       other than the one running counts as an update, so switching to a
       channel that's behind offers the way back down too.
     - TLS is verified against the certificate bundle in the ESP-IDF, so
       there's no CA certificate in the repo to expire, and `esp_ota` checks
       the image's own SHA-256 before the panel may boot from it.
     - New firmware is on trial for `OTA_SELF_TEST_MS` with the network up
       before the panel tells the bootloader to keep it. One that crashes or
       can't join Wi-Fi is rolled back on the next boot. The Arduino core
       would otherwise accept a new image the instant it booted, which is
       what `verifyRollbackLater()` in `ota_updater.cpp` overrides.
     - GitHub Actions builds every branch, publishes the rolling `beta`
       prerelease from `dev` and a release from each `v*` tag. The version
       is compiled in and written into the manifest from the same variable,
       so a panel can't be offered a version the firmware doesn't call
       itself.
   - Display sleep ✓ The backlight turns off after 15s without a touch
     (`DISPLAY_SLEEP_TIMEOUT_MS`). The tap that wakes it is swallowed until
     the finger lifts, so it can't press anything. Only the backlight goes
     off; LVGL keeps the screen updated underneath, and waking is instant.
     The setup screen blocks sleep while it's up, so its QR code stays
     scannable.
