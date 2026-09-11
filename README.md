# thermostat

In-wall touch panel for controlling mini-split heat pumps through Home
Assistant.

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

The mini-splits are already controlled by Home Assistant over MQTT, so this
device never drives HVAC equipment directly. The heat pumps run their own
control loops; this panel will just show their state and send commands.

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
```

## Layout

```
platformio.ini            board, platform, libraries, build flags
include/
  config.h                on-board pin assignments and timing
  lv_conf.h               LVGL options (fonts, memory, logging)
src/
  main.cpp                wiring + loop only
  display/
    display.h/.cpp        LCD + touch -> LVGL glue
    jd9853_init.h         LCD controller power-on register sequence
  touch/
    axs5106l.h/.cpp       touch controller driver
  climate/
    climate.h/.cpp        ClimateState: one mini-split's mode, fan, temps
    temperature.h/.cpp    °F/°C conversion, setpoint stepping, formatting
  storage/
    settings_store.h/.cpp panel settings saved in flash (NVS)
  ui/
    ui.h/.cpp             LVGL thermostat screen
    picker.h/.cpp         full-screen option grid (mode, fan)
    settings_screen.h/.cpp settings page (units)
    widgets.h/.cpp        shared flat button, overlay + icon helpers
    icons/                GENERATED icon font + ICON_* names (see below)
tools/
  generate_icons.py       regenerates src/ui/icons/ from Material Design Icons
hardware/                 enclosure, mounting notes
```

### Icons

Icons come from [Material Design Icons](https://pictogrammers.com/library/mdi/),
the set Home Assistant uses (Apache 2.0). LVGL can't load icon files at
runtime, so `tools/generate_icons.py` converts just the icons we use into
LVGL fonts (`src/ui/icons/mdi_icons_<size>.c`, one per size) and writes
`icons.h` with an `ICON_*` name for each. To add or change one, edit the `ICONS` list in the
script and run it from the project root (needs Node.js):

```bash
python3 tools/generate_icons.py
```

How it fits together:

- **Display:** `main.cpp` calls `Display::begin()`, which starts the LCD over
  SPI, the touch controller over I2C, and LVGL. From then on the loop calls
  `Display::tick()`. LVGL redraws whatever changed through `flushCb` and polls
  the touch controller through `touchReadCb`.
- **State flows one way.** `main.cpp` owns the `ClimateState` and hands it to
  `Ui::show()` to draw. When you tap a control, the UI doesn't change the
  state itself. It calls back into `main.cpp` (`setTargetTemp`, `setMode`,
  `setFanMode`) with what you asked for. `main.cpp` updates the state and
  calls `show()` again. In step 4 those callbacks send commands to Home
  Assistant instead, and `show()` runs when the mini-split reports back.
  Settings work the same way: the settings page calls `setTempUnit`, and
  `main.cpp` saves it and calls `Ui::showTempUnit()`.
- **Temperatures are stored in °C.** The unit (chosen in Settings, saved in
  flash; `DEFAULT_TEMP_UNIT` in `config.h` until then) only changes what's
  displayed and the +/- step (1°F or 0.5°C).

## Roadmap

Built one step at a time:

1. **Board bring-up** ✓ The screen draws, touch works, and colors are right.
   Responsiveness tuning: flat widget styles (no shadows or animated
   transitions), 16ms touch/refresh period, `-O2`, 80MHz SPI. The FPS/CPU
   overlay is toggled with `LV_USE_PERF_MONITOR` in `lv_conf.h`.
2. **Thermostat screen** ← *current*: one mini-split, portrait layout.
   - Top-left: settings button. Opens the settings page: units (°F / °C),
     saved in flash. Choosing doesn't close the page; the X in its top-left
     corner does. Switching units rounds the target to the nearest 1°F or
     0.5°C.
   - Top-right: current temperature with its unit (e.g. 72°F).
   - Center: target temperature, with triangle arrows above (up) and below
     (down). Faded and locked in Off and Fan modes, where it doesn't apply.
   - Bottom-left: mode button. Its icon and color show the current mode (Off,
     Heat, Cool, Auto, Dry, Fan).
   - Bottom-right: fan button. Its icon shows the current fan mode (Low,
     Medium, High, Auto, Off, Ambient).
   - Tapping either opens a full-screen picker; tapping an option selects it
     and closes the picker.

   Data is placeholder (71° indoor, 70° target, Heat). Taps change it on the
   device and are logged to the serial monitor.

   Touch is interrupt-driven: the driver only reads the controller after it
   signals a new report on its INT pin. Polling it constantly made it stop
   answering after about a minute.
3. **Wi-Fi**: connect and reconnect, with credentials kept out of git.
4. **MQTT ↔ Home Assistant**: show the mini-splits' real state and send
   mode/setpoint changes.
5. **Polish**: dim/sleep the backlight, OTA updates, enclosure, and maybe a
   local temperature sensor.
