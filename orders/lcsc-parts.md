# JLCPCB parts — verified 2026-06-10

Already baked into `hardware/pcb/jlcpcb_bom.csv`. Re-check the matched-parts
screen when uploading; JLC reshuffles Basic/Extended periodically.

| Ref | Part | LCSC # | Library | Note |
|---|---|---|---|---|
| U1 | ESP32-WROOM-32E-N8 | **C701342** | Extended | (C701343 is the 16MB variant — wrong) |
| U2 | CH340C SOP-16 | C84681 | Extended | Listing flagged EOL but 80k stock — fine for this run; CH340K/X for a future rev |
| U3 | AMS1117-3.3 | C6186 | Basic | |
| U4 | THVD1410DR | C2671345 | Extended | RS-485 driver, ±18 kV ESD |
| J1 | USB-C TYPE-C-31-M-12 | C165948 | Extended | |
| J2 | KF301-5.0-3P screw terminal | **C474882** | Extended | (C474881 is the 2-pin — wrong). 5.0 mm pitch in a 5.08 footprint: 0.08 mm/pin offset, fits fine in the 1.3 mm holes |
| Q1,Q2 | SS8050 | C2150 | Basic | |
| R 0603 | 5.1k C23186 · 10k C25804 · 1k C21190 · 680R C23228 | | Basic | |
| C | 100n C14663 · 1u C15849 · 10u C15850 · 22u C45783 · 4.7u C1779 | | Basic | |
| D1 | Red LED 0603 KT-0603R | C2286 | Basic | |
| D2 | Blue LED 0603 KT-0603B | **C2288** | Extended | (C72041 nearly out of stock) |
| SW1,SW2 | XKB TS-1187A-B-A-B | C318884 | Basic | |
| R11 | 120R termination | — | — | **Intentionally DNP** — fit only if you ever do RDM |

- Both U1 (module) and J2 (THT terminal) are supported on **Economic** assembly;
  JLC auto-adds a small assembly-fixture fee for the module.
- 6 extended parts → ~$18 in feeder fees; per-board parts ≈ $6.6.
- Rotation: U1's KiCad↔JLC library mismatch is **already corrected in the CPL**
  (verified against the placement preview 2026-06-10 — JLC value 0° puts the
  antenna off the left edge). Still re-check the preview after re-upload:
  U1 antenna overhanging LEFT, J1 tongue off the BOTTOM edge, U3 tab toward
  the top edge, D1/D2 pin-1 (cathode) markers on the lower pad of each.
- U2: JLC may substitute their in-stock CH340C listing (e.g. C7464026) for the
  EOL-flagged C84681 — fine, as long as the matched part is CH340**C**, SOP-16.
