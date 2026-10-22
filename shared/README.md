# shared — code both firmwares use

PlatformIO libraries pulled in by [`../panel`](../panel) and
[`../unit`](../unit). Each project names this directory in its
`platformio.ini`:

```ini
lib_extra_dirs = ../shared
lib_deps = thermostat_common
```

A library's `src/` is on the include path, so a file here at
`thermostat_common/src/net/wifi_connection.h` is included as
`"net/wifi_connection.h"` — the same path it had when it lived in the panel's
own `src/`, which is why moving it changed no call sites.

## thermostat_common

| Path                               | What it is                                                                 |
| ---------------------------------- | -------------------------------------------------------------------------- |
| `src/protocol/mini_split_protocol.h` | The MQTT contract: topics, payload vocabulary, JSON keys, and the enums they map to |
| `src/net/wifi_connection.h/.cpp`   | Joins the saved network and stays on it, with mDNS                          |
| `src/net/wifi_status.h`            | Connection snapshot, for a UI or a status page                              |
| `src/net/mqtt_connection.h/.cpp`   | ESP-IDF MQTT client in its own task, with retained publishes and a last will |
| `src/net/ota_updater.h/.cpp`       | Checks a GitHub release's manifest and installs the firmware it names, in its own task, with rollback |
| `src/net/ota_status.h`             | Update snapshot and channel, for a UI or a status page                      |

This is only worth having because both firmwares run the same Arduino core on
an ESP32. The mini-split's controller was an ESP8266 to begin with, where
nothing here would have compiled.

**Rules of thumb for what belongs here:**

- **No `config.h`.** A shared module takes its tunables as arguments, so each
  firmware keeps its own pins and timings in its own `include/config.h`. That
  is why `WifiConnection::begin()` takes a retry interval, and
  `MqttConnection::begin()` and `OtaUpdater::begin()` each take an `Options`.
- **Both firmwares must actually use it.** Something only the panel needs
  (the LVGL UI, the display and touch drivers) stays in the panel. Something
  only the mini-split needs (the CN105 link) stays in the unit.
- **The protocol header is the contract**, so a change to a topic name or a
  payload value is a change to both firmwares at once, and is visible as such
  in the diff.
