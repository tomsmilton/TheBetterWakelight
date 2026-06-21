# Order list A — module build (wakes you THIS WEEK)

Click through, add to basket, done. Total ≈ £45–60. Everything except the
XLR route arrives next day with Prime.

## Basket 1 — Amazon (Prime next-day)

| ✓ | Item | Link | ~£ |
|---|------|------|----|
| ☐ | ESP32 dev board, USB-C (AZDelivery 5-pack; you need 1) | https://www.amazon.co.uk/dp/B0D8WB3C22 | 25 |
| ☐ | MAX485 RS-485 modules, 6-pack (backstop transceiver) | https://www.amazon.co.uk/dp/B09SLTNQD1 | 7 |
| ☐ | ELEGOO Dupont jumper kit 120pc | https://www.amazon.co.uk/dp/B01EV70C78 | 6 |
| ☐ | QWORK ABS project box 100×60×25, 6-pack | https://www.amazon.co.uk/dp/B0F1N83XDB | 9 |

Single-board alternative for the ESP32 (micro-USB):
https://www.amazon.co.uk/dp/B07ZZFXRTY (~£10)

## Basket 2 — The Pi Hut (order BEFORE 2pm for next-day)

| ✓ | Item | Link | £ |
|---|------|------|---|
| ☐ | Waveshare RS485 Board **3.3 V** (preferred transceiver) | https://thepihut.com/products/rs485-board | 3.90 |

Low stock when checked — if gone, the MAX485 6-pack from basket 1 covers you.

## Basket 3 — the XLR connection (pick ONE route, or both as insurance)

| ✓ | Route | Items | ~£ |
|---|-------|-------|----|
| ☐ | **C1 (best):** CPC Farnell next-working-day | Neutrik NC5FXX-BAG female 5-pin XLR — https://cpc.farnell.com/neutrik/nc5fxx-bag/xlr-socket-5p-black/dp/AV12853 (£9.43). Solder 3 pins to ~1 m of 2-core+screen cable. | 10 |
| ☐ | C2 (no 5-pin soldering): all-Amazon | Adam Hall DHM 0020 5-pin-F→3-pin-M adapter https://www.amazon.co.uk/dp/B00GVG3IJW + any Prime 3-pin XLR mic cable (cut the male end off) | 17 |
| ☐ | C3: proper DMX lead to cut | Pulse 5 m 5-pin DMX M–F https://www.amazon.co.uk/dp/B017W23GWM — cut off the male end | 14 |

USB power: any 5 V phone charger + cable you already own.

Then follow `hardware/module-build/BUILD_GUIDE.md` (20-minute assembly) and
flash `firmware/` with `pio run -t upload`.

# Order list B — custom PCB (the finished product, ~1 week with express)

The current board is **v1.1**: 66 × 42 mm with a PCB-mount Neutrik NC5FAH
female 5-pin XLR — a standard male-to-female DMX lead plugs straight in, no
screw terminal or pigtail. (The earlier 55 mm screw-terminal variant is
archived in `hardware/pcb/v1.0-terminal/`.)

**Also order the connector itself** (hand-soldered, 6 joints): Neutrik
**NC5FAH** — cheapest verified UK source is Rapid Electronics 20-1840 at
**£3.11** with free shipping: 
https://www.rapidonline.com/neutrik-nc5fah-d-5-pin-xlr-female-socket-dmx-20-1840
(NC5FAH1 is the successor with an identical footprint — either works.)
Plus one standard 5-pin DMX lead (male→female) to reach the lamp.

At https://cart.jlcpcb.com/quote :

1. Upload `hardware/pcb/wakelight_gerbers.zip`
   - 2 layers, 66 × 42 mm (auto-detected), Qty 5, any colour (green is cheapest/fastest)
   - Leave everything else default
2. Tick **PCB Assembly** → Economic, Top side, Qty 2 (or 5)
   - Upload BOM: `hardware/pcb/jlcpcb_bom.csv`
   - Upload CPL: `hardware/pcb/jlcpcb_cpl.csv`
   - In the part-confirmation screen, check each line matched (see
     `orders/lcsc-parts.md` for verified part numbers and substitutions)
   - R11 (120 Ω termination) is intentionally absent — DNP
   - J2 (the XLR) is intentionally absent from BOM/CPL — you solder it
   - In the placement-preview screen, rotate any part JLC shows misoriented
     (USB-C and the ESP32 module are the ones to eyeball: tongue off-board,
     antenna off-board)
3. Shipping: DHL/UPS express (~£15, 2–5 days). Keep order under £135 so VAT
   is collected at checkout (nothing at the door).

Expected all-in for 5 boards / 2 assembled: **£35–55 + shipping**.

The PCB takes the same firmware and web portal as the module build. Once the
assembled boards arrive, solder the NC5FAH into J2 (it only fits one way —
two locating bosses), flash over USB-C, and plug a DMX lead from the board
straight into the lamp's DMX-IN.
