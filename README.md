# thermostat

Two firmwares that together control mini-split heat pumps: an in-wall touch
panel, and the controller board inside the mini-split it talks to.

| Firmware           | Runs on                                      | What it does                                                                 |
| ------------------ | -------------------------------------------- | ---------------------------------------------------------------------------- |
| [`panel/`](panel)  | Waveshare ESP32-C6-Touch-LCD-1.47             | The wall panel: shows the mini-split's state and sends changes. [README](panel/README.md) |
| [`unit/`](unit)    | Seeed XIAO ESP32-C3 inside the mini-split     | Speaks the heat pump's CN105 protocol, MQTT, and serves its own controls. [README](unit/README.md) |

Today the panel reaches the mini-split through an MQTT broker, normally Home
Assistant's. The panel never needed Home Assistant itself, only the broker:
every topic it uses is published by the unit firmware. Making that broker
optional is what the two live in one repo for.

Nothing about your home is compiled into either firmware. Both come up running
their own Wi-Fi network with a setup page on it.

## Working on it

Each firmware is its own PlatformIO project, because they build for different
boards (an ESP32-C6 panel and an ESP32-C3 controller). What they have in
common is in [`shared/`](shared/README.md). Build them from the repo root by
naming the directory:

```bash
~/.platformio/penv/bin/pio run -d panel              # build the panel
~/.platformio/penv/bin/pio run -d unit               # build the unit
~/.platformio/penv/bin/pio run -d panel -t upload    # build + flash over USB
```

In VSCode, open [`thermostat.code-workspace`](thermostat.code-workspace)
rather than the repo root. The PlatformIO extension works one project at a
time, and the workspace gives it both, with a project switcher at the bottom
of the window. Opening the root folder instead leaves it with no
`platformio.ini` to work from.

Each firmware's own README covers its hardware, wiring, first run and layout.

## Layout

```
panel/                  wall panel firmware (its own platformio.ini, src, docs)
unit/                   mini-split controller firmware
shared/                 libraries both firmwares pull in (see shared/README.md)
  thermostat_common/    Wi-Fi, MQTT, and the protocol the two speak
hardware/               enclosures and boards, split panel / unit / common
tools/                  build scripts shared by both
  firmware_version.py   stamps a build with FIRMWARE_VERSION (PlatformIO pre-script)
  make_manifest.py      writes the manifest a panel checks for updates against
.github/workflows/
  ci.yml                builds both firmwares on every branch and pull request
  release.yml           publishes the releases, with an image for each device
thermostat.code-workspace   opens both projects in VSCode
```

Scripts that only matter to one firmware live with it, like
[`panel/tools/generate_fonts.py`](panel/tools/generate_fonts.py).

## Releases

One tag builds both firmwares from the same commit, and they carry the same
version, so a release is a set of images that belong together. Assets are
named per device, because a panel must never be handed the unit's image:

| Asset                        | For                                                        |
| ---------------------------- | ---------------------------------------------------------- |
| `panel-manifest.json`        | version + download URL: all a panel fetches when it checks  |
| `panel-firmware.bin`         | what a panel installs over the air                          |
| `panel-firmware.factory.bin` | whole flash image, to flash a new panel over USB at 0       |
| `unit-firmware.bin`          | the mini-split controller's app image                       |
| `unit-firmware.factory.bin`  | its whole flash image, to flash a new controller board at 0 |

Releases also carry `manifest.json`, `firmware.bin` and
`firmware.factory.bin`: the same panel files under the names panels in the
field still look for. They can go once every panel has installed a build that
reads `panel-manifest.json`.

Channels, versioning and how a panel decides to update are in
[the panel's README](panel/README.md#updates).
