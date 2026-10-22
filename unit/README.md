# unit — mini-split controller firmware

The firmware for the board inside the mini-split. It speaks the indoor unit's
own protocol over its CN105 service connector, and publishes what it sees for
the [wall panel](../panel) and Home Assistant.

**Status: controllable from Home Assistant, the panels and its own page.** It
reads the heat pump, joins the network it was given during setup, publishes
the unit [on the broker](#home-assistant-and-mqtt) where Home Assistant finds
it by itself, and serves [pages](#its-pages-on-your-network) where the unit
can be controlled and its settings changed. Check the controls against the
handheld remote before relying on them: this is the first code here that
moves real equipment.

## Hardware

| Part  | Detail                                                                 |
| ----- | ---------------------------------------------------------------------- |
| Board | [Seeed XIAO ESP32-C3](https://wiki.seeedstudio.com/xiao_esp32c3_getting_started/) |
| MCU   | ESP32-C3 (single-core RISC-V, 160MHz), 320KB RAM, 4MB flash             |
| Radio | Wi-Fi 4, BLE 5, **external antenna** on a U.FL connector                |
| Link  | CN105 connector on the indoor unit, 2400 baud 8E1                       |

**Attach the antenna before powering it up.** The board has no antenna of its
own, so running it without one gives terrible reception and isn't good for the
radio. The antenna is also the point: it can be routed outside the unit's
chassis, which is what reception inside a metal enclosure needs.

### Wiring to CN105

The pins are in [`include/config.h`](include/config.h).

| CN105 pin | Signal  | XIAO pin   | Notes                                     |
| --------- | ------- | ---------- | ----------------------------------------- |
| 1         | 12V     | —          | unused                                    |
| 2         | GND     | GND        | common ground, required                   |
| 3         | 5V      | 5V         | what the board runs from                  |
| 4         | unit TX | D7 (GPIO20) | the unit's transmit line, into the board  |
| 5         | unit RX | D6 (GPIO21) | the board's transmit line, into the unit  |

**Measure your own unit before wiring.** Pinouts differ between models, and
the connector is sometimes already occupied by a factory Wi-Fi adapter. With
the unit powered and nothing plugged into CN105, the transmit pin idles high,
so it reads about 5V or about 3.3V and tells you the signal level.

Two things worth adding for a board that lives in a wall:

- **A divider on the receive line.** The signal lines are 5V and the ESP32-C3
  is not 5V-tolerant. 1.8kΩ from the unit's transmit pin to the board's RX,
  and 3.3kΩ from that pin to ground. At 2400 baud, speed is a non-issue.
- **A bulk capacitor** on the 5V supply, 100–470µF plus a 0.1µF. Wi-Fi draws
  in spikes, and a sagging supply shows up as resets or weak transmission. If
  5V still sags, take the 12V line through a small buck converter instead.

**The USB port and the heat pump don't share a serial port here.** Logs go
over USB-C (the C3's built-in USB) and the heat pump has the hardware UART to
itself, so a unit can be watched while it's plugged into equipment. The
ESP8266 boards these replace couldn't do that.

## Building and flashing

From the repo root:

```bash
~/.platformio/penv/bin/pio run -d unit                  # build
~/.platformio/penv/bin/pio run -d unit -t upload        # build + flash over USB
~/.platformio/penv/bin/pio device monitor -d unit       # serial monitor (115200)
```

In VSCode, open [`thermostat.code-workspace`](../thermostat.code-workspace)
and pick **unit** in the project switcher at the bottom of the window.

USB is only needed once per board. After that it
[updates itself](#firmware-updates) from this repo's releases.

What a connected unit logs, once a second when something changes:

```
Mini-split controller 1.5.0
Heat pump: connecting over CN105 (RX 20, TX 21, 2400 baud)
Heat pump: connected
Heat pump: mode heat, target 22.0°C, room 20.5°C, fan auto, vane AUTO, wideVane |, heating
```

The values are the ones the panel and Home Assistant use, not the heat pump's
own words — that translation is the part worth checking against the remote.

## First run

Nothing about your home is compiled in, so a freshly flashed board starts in
**setup mode**: it runs its own Wi-Fi network with one page on it.

1. **Join its network**, `Minisplit-` plus the end of the board's MAC
   address. It has no password. Then open `http://192.168.4.1/`. Phones check
   for a captive portal and the controller answers every DNS lookup with its
   own address, so the page usually opens by itself.
2. **Fill it in:** your Wi-Fi, a name for this unit, the MQTT broker and its
   login, how temperatures are reported (°F here, 50–90, step 1), and — when
   replacing an existing board — the Home Assistant id below. Then choose a
   **settings password**, at least 8 characters. It guards
   [the unit's settings](#its-pages-on-your-network), not its controls.
3. **Save.** It restarts, joins your network, connects to the broker, and
   turns up in Home Assistant by itself. The serial log says whether each
   step worked, including when a broker refuses the login, and
   [its settings page](#its-pages-on-your-network) says so too from then on.

Settings live in the ESP32's flash (NVS), which survives reflashing the
firmware, so a controller is only set up once.

**The setup network is open on purpose.** Setup mode only runs on a blank
board or after the BOOT button is held, and that button is inside the
mini-split behind a screwed-on cover. Whoever can start setup mode already
has the unit open, and a password would have to be read off a USB log that an
installed unit isn't plugged into. The cost: what's typed into the setup page
crosses the air unencrypted, so someone nearby listening at that moment could
pick up the Wi-Fi and broker passwords.

**The name matters more than it looks.** It's the hostname, the `<name>.local`
address, and the middle of every topic: `mitsubishi2mqtt/<name>/state`. Home
Assistant and the panels know a unit by it.

## Its pages on your network

Once it's on your network, a unit serves two pages at `http://<name>.local/`.
Its IP address works too; the serial log prints it.

- **`/` is the controls:** mode, setpoint, fan and both vanes, the room
  temperature, and what the unit is doing. It updates by itself, and the
  background follows the mode the way the panel's screen does. A change shows
  dimmed until the unit reports it back, a few seconds later. If the unit
  never reports it back, the page goes back to showing what the unit does
  report.
- **`/settings` is what it's doing and how it's set up:** the heat pump, the
  Wi-Fi and the broker connection, and the same form as setup mode, so an
  installed unit can be checked on and reconfigured without opening it up.
  Saving restarts it with the new settings. Reload it for the latest status.

**The controls are open to anyone on your network; the settings need the
password** chosen in setup mode. A visitor can turn the heat up, but can't
see or change the Wi-Fi, the broker login or anything else.

- **Log in once per browser.** It stays logged in for 30 days after its last
  visit to the settings, and through restarts and updates. **Log out** is at
  the bottom of the settings. Each unit has its own login, so a new phone logs
  in to each one once. Using the same password on all of them is fine.
- **Changing it** takes the current one, so a phone left logged in can't be
  used to change it. Changing it logs out every other browser, which also
  makes it the way to log out a lost phone.
- **Forgotten it?** [Setup mode](#back-to-setup-mode) sets a new one without
  asking for the old, since getting there means opening the unit.
- **After 5 wrong passwords in a row**, logins are refused for 30 seconds.
- **A unit set up before there was a password has none**, so its settings
  stay shut and the page says to hold BOOT and choose one.
- **What the unit keeps is a salted hash**, never the password itself. The
  pages are plain HTTP, though, so the password crosses your Wi-Fi
  unencrypted when you log in, and so does the login your browser keeps.
  The unit has no clock, so it can't expire that login itself: the browser
  drops it after 30 days, and a copy of it would keep working until the
  password changes.

Some things to know:

- **The unit only takes setpoints from 16 to 31°C (61–88°F).** A setpoint
  outside that is clamped. Older units given an out-of-range setpoint jump to
  31°C, so the clamp is deliberate. The page offers the limits from
  `/settings`, so set those to 61 and 88 if you don't want buttons that go
  further than the unit will.
- **Saved passwords never reach the page.** The password boxes start empty,
  and an empty one keeps the saved password, but only for the same network,
  or the same broker and username. Change either of those and the password
  has to be typed again, so a saved one is never sent somewhere new.
- **Other websites can't use it.** The controls, logging in and saving only
  work from the controller's own addresses (`<name>.local` or its IP), so
  another website open in your browser can't reach them. Opened at any other
  address, such as a name your router gave it, the page says where to go
  instead.
- **It's for your home network.** Reaching it from outside means going in
  through something that already does that securely, such as a VPN or Home
  Assistant. Don't forward a port to it.
- **The heat pump line on `/settings` is in °C**, as the unit reports it, the
  same as the serial log. The controls use the unit set during setup.

The controls talk to the unit over a WebSocket on port 81, in JSON. That's
the same API the panels will use where there's no broker. The format is
written down in
[`mini_split_protocol.h`](../shared/thermostat_common/src/protocol/mini_split_protocol.h).

## Firmware updates

A unit inside a mini-split is awkward to reach, so it updates itself from
this repo's GitHub releases, the same way the panel does. It's all on
`/settings`, under **Firmware**: the version running, the channel, and what
the last check found, with **Check now**, **Install**, and a button to switch
channel.

- **Two channels:** *stable* is the tagged releases off `main`; *beta* is a
  build of every push to `dev`, for trying things out. A unit starts on
  stable.
- **It checks by itself** a minute after joining the network, then once a
  day. **It never installs by itself**: nothing changes until someone
  presses Install.
- **Only releases can be installed.** It downloads what its channel's
  `unit-manifest.json` points at, over TLS, and the image's own checksum is
  verified before it's used. There's no way to upload firmware to it.
- **The heat pump keeps running during the download.** At the end the unit
  restarts into the new firmware, which takes a few seconds; Home Assistant
  shows it unavailable meanwhile.
- **New firmware is on trial for its first minute.** If it can't join Wi-Fi,
  or the unit restarts before then, it goes back to the firmware it replaced,
  so a bad update can't strand a unit in the wall. Leave it alone for that
  minute: saving settings then would roll it back.
- **A board flashed by hand is always offered the release.** Any version
  other than the one running counts, and a USB build's version, such as
  `1.5.0-3-gfe059de-dirty`, never matches a release's.

What it logs:

```
Firmware: 1.5.0 (stable channel), running from app0
Update: checking https://github.com/ianbakst/thermostat/releases/latest/download/unit-manifest.json
Update: 1.5.1 available (running 1.5.0)
Update: downloading https://github.com/ianbakst/thermostat/releases/download/v1.5.1/unit-firmware.bin
Update: 1.5.1 installed, restarting into it
Firmware: just updated, on trial for 60s
Firmware: 1.5.1 passed its trial run and is now the one to keep
```

## Home Assistant and MQTT

The topics and payloads are the ones mitsubishi2mqtt used, written down in
[`mini_split_protocol.h`](../shared/thermostat_common/src/protocol/mini_split_protocol.h),
so Home Assistant and the panels work with it unchanged. For a unit named
`OfficeHVAC`:

| Topic                                   | What                                                                 |
| --------------------------------------- | -------------------------------------------------------------------- |
| `homeassistant/climate/OfficeHVAC/config` | HA's discovery config, retained, so HA makes the entity by itself |
| `mitsubishi2mqtt/OfficeHVAC/availability` | `online` / `offline`, retained                                   |
| `mitsubishi2mqtt/OfficeHVAC/settings`   | mode, setpoint, fan and both vanes, retained, on every change        |
| `mitsubishi2mqtt/OfficeHVAC/state`      | the same plus the room, what it's doing and the compressor's speed, on every change and every 30s |
| `mitsubishi2mqtt/OfficeHVAC/<setting>/set` | changes: `mode`, `temp`, `fan`, `vane`, `wideVane`              |

What the log says on connecting:

```
MQTT: connected
MQTT: sent Home Assistant's discovery config (1449 bytes)
MQTT: OfficeHVAC is online
```

and for each change sent to it, `MQTT: change from the broker: mode/set cool`
then `Heat pump: change accepted`.

Where it differs from mitsubishi2mqtt, on purpose:

- **"online" means the heat pump is answering.** If the indoor unit stops
  answering, the controller says `offline`, so HA shows the entity as
  unavailable and a panel says it's offline, instead of both showing the
  last settings as if they were live. The broker says `offline` for it
  (its last will) if it drops off the network. mitsubishi2mqtt said `online`
  whenever it was on the broker.
- **HA reads the settings from the retained `settings` topic**, rather than
  from `state`, so it shows them as soon as it starts instead of up to 30
  seconds later.
- **The wide vane shows up in HA** as the horizontal swing mode. HA versions
  from before HA had one ignore it.
- **A change is checked the same way as one from its page**, and one it can't
  make is logged and dropped. That covers a word that isn't in the protocol
  (they're case-sensitive, as HA sends them), and a setpoint outside the
  limits from setup, which mitsubishi2mqtt replaced with 23°C. Nothing is
  shown until the unit reports the change back, a few seconds later, where
  mitsubishi2mqtt published the change straight away as if it had already
  happened.
- **Every mode is offered**, including heat. mitsubishi2mqtt had a setting to
  leave heat out for units that can't heat.
- **Some topics aren't taken:** `remote_temp/set` (a room temperature from
  another sensor), and mitsubishi2mqtt's `system/set` reboot, debug and
  custom-packet topics. Messages on them are logged and ignored.
- **Renaming a unit cleans up after itself.** The first time it connects
  under a new name, it clears what the old name left on the broker. Without
  that, HA would keep the old entity and refuse the new one for having the
  same id, and panels would go on listing the old name.

## Back to setup mode

This is for a unit that can't get on the network to show its pages, because
the Wi-Fi password changed or the page was given settings that don't work,
and for a forgotten settings password: leave the box empty to keep the one
there is, or type a new one to replace it.

**Hold the BOOT button for 5 seconds while the board is running.** It
restarts into setup mode with the form filled in from what's saved (except
the passwords, as above). The board has no LED, so the sign it worked is the
open `Minisplit-` network appearing in your phone's Wi-Fi list, the same one
as on the first run.

- **Not while it's powering up.** The BOOT button also tells the chip to wait
  for new firmware, so if it's held through a reset or power-up, that's what
  happens, and the board looks dead until it's reset again.
- **Press RESET to leave setup mode** without changing anything. Setup mode
  lasts only for the one boot after the button, so the board goes back to
  the saved settings.

## Replacing an ESP8266 in a unit

The MQTT topics and payloads are unchanged, so a panel and Home Assistant see
the same unit — **provided two things come across with it**.

**1. The same name.** Type the old unit's name into setup, exactly. That keeps
every topic identical, so the panel needs no changes.

**2. The same Home Assistant id.** This is the one that bites. The old
firmware set each entity's `unique_id` from *its own chip id*, so a new board
would arrive in HA as a second entity — `climate.officehvac_2` — and every
dashboard and automation would still point at the old, now-dead one.

**Capture the ids before you unplug anything.** They're in the retained
discovery messages on the broker, so while the current boards are alive:

```bash
mosquitto_sub -h homeassistant.local -u <user> -P <password> \
  -t 'homeassistant/climate/+/config' -W 3 \
  | jq -r '"\(.device.ids)\t\(.unique_id)"'
```

That prints one line per unit: its name and the id to paste into the new
board's setup page. Keep the list somewhere; it can't be recovered once the
old board is gone and the retained message has been overwritten.

Then the swap is: flash the C3, set it up with that name and id, power down
the unit, move the CN105 connector over, power it back up. The retained
discovery config, `settings` and `availability` are overwritten by whichever
board is alive, so nothing has to be cleaned off the broker.

## Layout

```
platformio.ini            board, platform, pinned libraries, build flags
include/
  config.h                CN105 pins, timings, and the defaults the setup form starts with
src/
  main.cpp                wiring + loop only
  climate/
    heat_pump.h/.cpp      the CN105 link, and the only place that knows the
                          heat pump's own vocabulary
    temperature_units.h   the unit's °C to and from what everything else sees
  input/
    setup_button.h/.cpp   the BOOT button, held to get back to setup mode
  net/
    provisioning.h/.cpp   setup mode: the controller's own network and its one page
    mqtt_api.h/.cpp       the unit on the broker: HA discovery, state, and the /set topics
    web_pages.h/.cpp      its pages on the home network: controls, status, settings
    control_page.h        the controls page's markup and script
    direct_api.h/.cpp     the WebSocket the controls (and later panels) drive it through
    protocol_json.h/.cpp  state and changes as the protocol's JSON, for both of the above
    own_address.h/.cpp    whether a browser request came from its own page
    page_login.h/.cpp     the settings password, and the logins browsers keep
    settings_form.h/.cpp  the settings form setup mode and /settings both show
  storage/
    settings_store.h/.cpp what this unit was told about your home, saved in flash
```

The case and the CN105 adapter board are in
[`../hardware/unit`](../hardware/unit).

Shared with the panel, one level up: [`../shared/thermostat_common`](../shared)
(Wi-Fi, the MQTT client, the updater, and the protocol both firmwares speak), `../tools`
(the version stamp and manifest writer) and `../.github/workflows`.

## How it fits together

- **`HeatPumpLink` is the only thing that knows Mitsubishi's words.** The
  control board talks about `HEAT` with the power off, fan speed `2` and vane
  `SWING`; everything above translates to `heat_cool`, `middle` and `Swing`,
  the vocabulary in
  [`mini_split_protocol.h`](../shared/thermostat_common/src/protocol/mini_split_protocol.h).
  Vane and wide-vane values happen to be identical on both sides, so those
  tables are listed once, in the shared header.
- **Off is not a mode on the control board.** Power and mode are separate
  there, and a unit that's off still reports the mode it will return to, so
  power is checked first.
- **Unknown is not zero.** The unit reports a room temperature of 0 before its
  sensor has been read, which becomes "unknown" here rather than 0°C.
- **In Auto, the unit won't say which way it's working.** It reports only that
  it's running, so heating and cooling are told apart by comparing the room
  with the setpoint, and with no reading yet the answer is Idle.
- **A missing unit is not an error.** If the indoor unit doesn't answer, the
  wait between attempts doubles up to 15 minutes and then keeps retrying
  forever, so a unit that comes back hours later is picked up without anyone
  power-cycling the board.
- **A change sends only what was asked for.** The CN105 library sends every
  setting that differs from its own idea of what was last requested, which it
  doesn't keep up to date. So `HeatPumpLink::apply()` resets that to what the
  unit reports first. Otherwise turning the fan up from the page would also
  undo whatever was last done with the remote.
- **What's shown is what the unit reports.** A change isn't reflected until
  the unit's next report, a few seconds later. The page dims the value it
  asked for until then, but the controller itself never assumes a change
  took.
- **A change is a change, whichever way it came.** A message on a `/set`
  topic becomes the same JSON a page sends, so both get the same checks in
  `protocol_json::readCommand()` and go to the unit through the same
  `HeatPumpLink::apply()`.
- **Everything runs in `loop()`.** The pages, the control API, MQTT's
  messages and the heat pump link take turns, so a command goes from the
  message to the unit with nothing to lock. The cost: while the unit
  acknowledges a change, a second or two, nothing else is served. The
  broker connection itself has its own task, so it stays up meanwhile.

## The CN105 library, and its licence

The framing underneath — packet types, checksums, the serial link — is
[SwiCago/HeatPump](https://github.com/SwiCago/HeatPump), pinned by commit in
`platformio.ini`.

**It is GPL-3.0**, which means this firmware is GPL-3.0 as long as it's linked
in: distributing a binary obliges you to offer the source. That's why it sits
behind [`climate/heat_pump.h`](src/climate/heat_pump.h) and nothing else
includes it. Writing our own implementation of the protocol, which is well
documented, would mean rewriting only that pair of files and would leave the
firmware unencumbered.

The WebSocket library,
[links2004/WebSockets](https://github.com/Links2004/arduinoWebSockets), is
LGPL-2.1, which fits under either licence.

## History

This replaces a vendored copy of
[mitsubishi2mqtt](https://github.com/gysmo38/mitsubishi2mqtt) (commit
`2c9d1d0`, the last on its master branch) which ran on the Wemos D1 mini
boards originally in the units. It was dropped rather than forked:

- **Upstream is dormant.** Its last functional change was in August 2024, and
  it has stale pull requests including a real use-after-free fix.
- **The maintained fork is protocol-incompatible.** It drops the `settings`
  topic, retains nothing, and renames `wideVane/set`, all of which break a
  panel. Its ESP8266 target doesn't build against current libraries either.
- **The ESP8266 was the limit.** It left about 45KB of heap with everything
  loaded; this board has around 300KB free, which is what a local WebSocket
  API for panels and phones needs.

The MQTT topics and payloads it established are kept, so a panel and Home
Assistant see no difference. They're written down in the shared protocol
header.

## What's next

1. ~~Wi-Fi, MQTT and setup~~ ✓, reusing the panel's shared modules, plus
   pages on the home network and the BOOT button back to setup.
2. ~~Controls on its own page~~ ✓, over the WebSocket API panels will use.
3. ~~Publishing state~~ ✓: retained `settings` and `availability` with a last
   will, periodic `state`, and the Home Assistant discovery config.
4. ~~Accepting commands on the `/set` topics~~ ✓, through the same
   `HeatPumpLink::apply()` the page uses.
5. ~~Updating itself over the air from its page~~ ✓, behind a login on that
   page.
   - ~~The settings password~~ ✓. The controls stay open.
   - ~~Updates from this repo's GitHub releases~~ ✓, stable or beta, checked
     and installed from `/settings`. Nothing can be uploaded to it.
6. Panels connecting to the WebSocket API directly, so they work with no Home
   Assistant and no broker at all. That replaces the panel's MQTT mode.
7. Pairing panels to their unit, once a panel has a room sensor, so only a
   paired panel's readings are used.
