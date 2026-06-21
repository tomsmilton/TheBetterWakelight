# The Better WakeLight (V2)

A small ESP32 board that plugs into a **Neewer PL60C** LED panel's 5-pin DMX‑IN
and ramps it from black to full daylight before your alarm — a sunrise lamp you
configure from a phone-friendly web page on your home Wi‑Fi.

> [!WARNING]
> **This firmware is rough bring‑up firmware, not a polished release.**
> It works — the DMX output stage is fixed and bench‑tested — but it has not
> been refined or cleaned up. A nicer, **prettier firmware is likely coming in
> July 2026**. If you want the tidy version, check back then. What's here is
> enough to build the board and make the lamp do a sunrise today.

**The custom PCB — XLR variant (v1.1), board-mount Neutrik 5-pin XLR:**

| Top | Bottom |
|:---:|:---:|
| ![Custom PCB v1.1 — top](hardware/pcb/render_top.png) | ![Custom PCB v1.1 — bottom](hardware/pcb/render_bottom.png) |

---

## Where everything is

| You want… | Go to |
|---|---|
| **Order the PCB** (JLCPCB) | [`hardware/pcb/`](hardware/pcb/) — see [Order the board](#order-the-board-jlcpcb) |
| **Flash the firmware** | [`firmware/`](firmware/) — see [Flash it](#flash-it-when-the-board-arrives) |
| **3D‑print the enclosure** | [`hardware/pcb/enclosure_box.stl`](hardware/pcb/enclosure_box.stl) + [`enclosure_lid.stl`](hardware/pcb/enclosure_lid.stl) |
| **Build it without a custom PCB** | [`hardware/module-build/BUILD_GUIDE.md`](hardware/module-build/BUILD_GUIDE.md) |
| **Parts / shopping list** | [`orders/ORDER_THIS_WEEK.md`](orders/ORDER_THIS_WEEK.md), [`orders/lcsc-parts.md`](orders/lcsc-parts.md) |
| **How the lamp's DMX works** | [`docs/dmx-profile.md`](docs/dmx-profile.md) |
| **Why the circuit is built this way** | [`docs/rs485-design-notes.md`](docs/rs485-design-notes.md) |
| **Schematic & wiring diagrams** | [`docs/schematic-diagram.svg`](docs/schematic-diagram.svg), [`docs/wiring-diagram.svg`](docs/wiring-diagram.svg) |
| **Edit/regenerate the board** | [`hardware/pcb/`](hardware/pcb/) — KiCad `wakelight.kicad_pcb` + `generate_board.py` / `route_board.py` |

---

## Order the board (JLCPCB)

The custom PCB is a 2‑layer board, 66 × 42 mm, with a board‑mount Neutrik NC5FAH
female 5‑pin XLR on the edge. Everything JLCPCB needs is already exported in
[`hardware/pcb/`](hardware/pcb/):

| File | What it is | Upload to JLC as |
|---|---|---|
| [`hardware/pcb/wakelight_gerbers.zip`](hardware/pcb/wakelight_gerbers.zip) | The board (copper, mask, silk, drills) | **Gerbers** |
| [`hardware/pcb/jlcpcb_bom.csv`](hardware/pcb/jlcpcb_bom.csv) | Bill of materials (LCSC part numbers) | **BOM** |
| [`hardware/pcb/jlcpcb_cpl.csv`](hardware/pcb/jlcpcb_cpl.csv) | Part positions / rotations | **CPL (Pick‑and‑Place)** |

**Steps**

1. Go to [jlcpcb.com](https://jlcpcb.com) → **Add gerber file** → upload
   [`wakelight_gerbers.zip`](hardware/pcb/wakelight_gerbers.zip).
2. Turn on **PCB Assembly**. When prompted, upload
   [`jlcpcb_bom.csv`](hardware/pcb/jlcpcb_bom.csv) (BOM) and
   [`jlcpcb_cpl.csv`](hardware/pcb/jlcpcb_cpl.csv) (CPL / Pick‑and‑Place).
3. Check the placement preview against
   [`render_top.png`](hardware/pcb/render_top.png) /
   [`render_bottom.png`](hardware/pcb/render_bottom.png), confirm parts, and order.

**Good to know**

- The **XLR connector (J2) is hand‑soldered** (through‑hole) — it is *not* in the
  assembly BOM. Order it separately (see [`orders/`](orders/)).
- The 120 Ω termination resistor (R11) is intentionally **not populated**; the
  lamp terminates the bus.
- An older screw‑terminal version of the board is archived in
  [`hardware/pcb/v1.0-terminal/`](hardware/pcb/v1.0-terminal/).

### Print the enclosure (optional)

[`enclosure_box.stl`](hardware/pcb/enclosure_box.stl) and
[`enclosure_lid.stl`](hardware/pcb/enclosure_lid.stl) (or the zip
[`enclosure_stl.zip`](hardware/pcb/enclosure_stl.zip)). Preview:
[`enclosure_preview.png`](hardware/pcb/enclosure_preview.png). Source:
[`enclosure.scad`](hardware/pcb/enclosure.scad).

---

## Flash it when the board arrives

The firmware is a [PlatformIO](https://platformio.org) project in
[`firmware/`](firmware/). Plug the board into your computer over USB‑C, then pick
one of these.

### Easiest — let Claude Code do it

Open a terminal **inside the `firmware/` folder** and run:

```
claude
```

Then tell it:

> *"Flash this firmware to my ESP32 over USB and help me bring it up."*

It will build, upload, and walk you through Wi‑Fi and lamp setup. (This board was
literally brought up this way.)

### Manual — PlatformIO

If you'd rather drive it yourself, install
[PlatformIO](https://platformio.org/install) (the CLI, or the VS Code extension),
then from the `firmware/` folder:

```
pio run -t upload        # build the production firmware and flash it
pio device monitor       # optional: watch the serial log @ 115200 baud
```

There's also a no‑Wi‑Fi self‑check that proves the DMX output works on the bench:

```
pio run -e selfcheck -t upload     # then watch serial; re-flash the default to restore production
```

### Set up the lamp

On the PL60C: **DMX mode ON, address 001** (long‑press MENU → DMX → ON, then
ADDR → 001). Plug the board's XLR into the panel's **DMX‑IN**.

### First boot / Wi‑Fi

The board opens a hotspot **`WakeLight-Setup`** (password `sunrise123`). Join it,
enter your home Wi‑Fi, then open **http://wakelight.local** to set per‑day alarms,
run the 2‑minute sunrise demo, and control brightness / colour temperature.

---

## How it works (short version)

- **Output stage:** ESP32 (GPIO17 TX) → THVD1410 RS‑485 driver → female 5‑pin XLR.
  It's transmit‑only, so the driver enable (GPIO21) is simply held HIGH in
  firmware. Diagrams in [`docs/`](docs/).
- **Sunrise:** a two‑phase dawn ramp that reaches 100 % at your alarm time, then
  holds for a configurable time and switches off.
- **Details:** [`docs/rs485-design-notes.md`](docs/rs485-design-notes.md) (circuit)
  and [`docs/dmx-profile.md`](docs/dmx-profile.md) (the PL60C's DMX channels).
