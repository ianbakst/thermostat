# hardware

Enclosures and circuit boards for the two devices, split the same way the
firmware is: [`panel/`](panel) for the wall panel, [`unit/`](unit) for the
controller that goes inside the mini-split, and [`common/`](common) for the
parts both use.

```
panel/
  enclosure/        wall plate and back box
  pcb/wall-panel/   KiCad project
unit/
  enclosure/        case for the board inside the indoor unit
  adapter/          CN105 harness: connector, divider, bulk capacitor
common/
  kicad/            symbols, footprints and 3D models used by both boards
```

**Keep the sources, and the exports people actually use.** CAD sources
(FreeCAD, OpenSCAD, Fusion) plus the STL or 3MF someone prints, and for a
board the KiCad project plus the gerbers that were sent for manufacture. A
revision that was ordered is worth tagging in its directory name or a note
here, so a board in hand can be matched to the files it came from.

**KiCad noise is ignored, the project isn't.** The schematic, board and
`.kicad_pro` are tracked; backups, `fp-info-cache`, `.kicad_prl` (per-user
settings) and lock files are in the repo's `.gitignore`. If STEP files and
gerber archives start to dominate the repo, this is the directory to put
behind Git LFS.

## Devices

| | Wall panel | Mini-split controller |
| --- | --- | --- |
| Board | Waveshare ESP32-C6-Touch-LCD-1.47 | Seeed XIAO ESP32-C3 |
| Firmware | [`../panel`](../panel) | [`../unit`](../unit) |
| Mounting | in-wall, single-gang; **a plastic box, not metal** — the panel's antenna is on the board | inside the indoor unit, with the antenna routed outside the chassis |
| Power | USB-C, or 5V behind the plate | 5V from the CN105 connector (12V through a buck converter if 5V sags) |

### The unit's adapter

The electrical notes that drove this design are in
[`../unit/README.md`](../unit/README.md): CN105 carries 5V signals and the
ESP32-C3 is not 5V-tolerant, so the board's receive line wants a divider
(1.8kΩ and 3.3kΩ), and Wi-Fi draws in spikes, so the 5V rail wants 100–470µF
of bulk capacitance nearby. Those three parts plus the connector are the whole
adapter, which is why it's worth one small board rather than flying leads
inside a heat pump.

**Pinouts differ between mini-split models**, and on some units CN105 is
already taken by a factory Wi-Fi adapter. Measure before building a harness
for a unit you haven't opened before.
