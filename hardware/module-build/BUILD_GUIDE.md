# WakeLight module build — "lamp wakes you this week"

ESP32 devkit + RS-485 breakout + 5-pin XLR, in a small ABS box. No soldering
iron strictly required if you use the screw-terminal breakout + Dupont wires,
but two soldered XLR pins make it far more reliable — budget 20 minutes total.

## Shopping list (UK, fastest route)

Order group A **and** one item from group B **and** one route from group C.

### A — core electronics (all Amazon Prime next-day)

| Item | Listing | ~Price |
|---|---|---|
| ESP32 dev board (USB-C) | AZDelivery ESP32 NodeMCU Dev Kit C, CP2102, USB-C — [amazon.co.uk/dp/B0D8WB3C22](https://www.amazon.co.uk/dp/B0D8WB3C22) (5-pack) or classic micro-USB [B07ZZFXRTY](https://www.amazon.co.uk/dp/B07ZZFXRTY) | £25 (5-pk) / £10 (1) |
| Dupont jumper kit | ELEGOO 120 pcs M-F/M-M/F-F — [amazon.co.uk/dp/B01EV70C78](https://www.amazon.co.uk/dp/B01EV70C78) | £6 |
| Project box | QWORK ABS 100×60×25 mm, 6-pack — [amazon.co.uk/dp/B0F1N83XDB](https://www.amazon.co.uk/dp/B0F1N83XDB) | £9 |
| USB power | Any 5 V USB phone charger you already own + USB cable | £0 |

### B — RS-485 transceiver (pick one)

| Option | Listing | Notes |
|---|---|---|
| **Preferred: 3.3 V SP3485 board** | Waveshare RS485 Board (3.3 V) — [thepihut.com/products/rs485-board](https://thepihut.com/products/rs485-board), £3.90 | Native 3.3 V. Order **by 2 pm** for next-day. Low stock — if gone, use the MAX485 option. |
| Backstop: MAX485 module 6-pack | JZK MAX485 — [amazon.co.uk/dp/B09SLTNQD1](https://www.amazon.co.uk/dp/B09SLTNQD1), ~£7 Prime | 5 V part: power VCC from the devkit **5 V/VIN pin** (not 3V3). DI accepts 3.3 V fine. Leave RO unconnected. |

Order both (~£11) if tomorrow matters — one of them will be on the doormat.

### C — the XLR connection to the lamp (pick one, or both as insurance)

The lamp's DMX IN is a male 5-pin XLR chassis socket, so you need a **female**
5-pin connector on your cable.

| Route | Parts | Notes |
|---|---|---|
| **C1: proper connector, next-day** | Neutrik NC5FXX-BAG from CPC Farnell, £9.43 — [cpc.farnell.com/dp/AV12853](https://cpc.farnell.com/neutrik/nc5fxx-bag/xlr-socket-5p-black/dp/AV12853) + ~1 m of any 2-core-plus-screen cable (an old mic/USB cable works at 1 m) | Next working day if ordered before evening cutoff. Solder 3 pins. |
| C2: all-Amazon adapter route | Adam Hall DHM 0020 5-pin-F→3-pin-M adapter — [amazon.co.uk/dp/B00GVG3IJW](https://www.amazon.co.uk/dp/B00GVG3IJW) (~£9, sold by Amazon) + any Prime 3-pin XLR mic cable (~£8): cut off the male end, wire the bare end to the breakout | No 5-pin soldering; mic cable is wrong impedance but fine at bedroom lengths (<5 m). |
| C3: 5-pin DMX lead to cut | Pulse 5 m 5-pin DMX lead M-F — [amazon.co.uk/dp/B017W23GWM](https://www.amazon.co.uk/dp/B017W23GWM) ~£14 | Cut off the **male** end, keep the female; proper 120 Ω cable. Check Prime badge. |

Total: roughly **£45–60** depending on routes.

## Wiring

```
                 ESP32 DevKitC                    RS-485 breakout
                ┌──────────────┐                 ┌───────────────┐
USB charger ──► │ USB-C        │           ┌───► │ VCC*          │
                │              │           │     │               │   A/D+ ──► XLR pin 3
                │ 3V3 ─────────┼───────────┤(*)  │               │   B/D− ──► XLR pin 2
                │ GND ─────────┼───────────┼───► │ GND           │   GND ───► XLR pin 1
                │ GPIO17 ──────┼───────────┼───► │ DI            │
                │ GPIO21 ──────┼───────────┼─┬─► │ DE            │   XLR pins 4,5: n/c
                │              │           │ └─► │ /RE (tie to DE)│
                └──────────────┘           │     │ RO    (n/c)   │
                                                 └───────────────┘
(*) VCC: 3V3 pin for the SP3485 board, 5V/VIN pin for a MAX485 module.
```

- Waveshare SP3485 board: it has `RSE` (direction) instead of separate DE//RE —
  wire `RSE` to GPIO21. If there's a jumper for auto-direction, set manual.
- MAX485 module: DE and /RE are separate header pins next to each other —
  bridge them with one Dupont F-F wire folded over, or a solder blob, then one
  wire to GPIO21.
- Twist the A/B pair if you're making your own cable. Screen/third core = GND.

## Cable to the lamp

Female 5-pin XLR, viewed from the **solder-bucket side** of an NC5FXX
(buckets labelled 1–5 on the insert):

| XLR pin | Signal | Wire |
|---|---|---|
| 1 | Ground / screen | GND |
| 2 | Data − | B / D− |
| 3 | Data + | A / D+ |
| 4, 5 | not used | leave empty |

## First power-up

1. Flash the firmware first (see `firmware/` — `pio run -t upload` with the
   board on USB).
2. Join the Wi-Fi network **WakeLight-Setup** (password `sunrise123`) from your
   phone; a captive portal asks for your home Wi-Fi. After it connects, browse
   to **http://wakelight.local**.
3. On the lamp: menu → DMX mode, set address **001**, channel mode CCT
   (see `docs/dmx-profile.md` once lamp research lands), and enable DMX control.
4. Portal → Status → **2-min sunrise demo**. The panel should glow up from
   nothing to full warm-to-cool. Set your alarms, done.

## Box

Drill the 100×60×25 box: one hole for the USB cable, one 8–10 mm hole (or
slot) for the XLR tail with a knot/cable-tie as strain relief. The devkit and
breakout sit on a strip of double-sided foam tape. Nothing gets warm.
