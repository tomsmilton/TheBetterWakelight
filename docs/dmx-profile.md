# Neewer PL60C — DMX profile (from the official Neewer channel table)

Source: Neewer "PL60C DMX Channel Table" PDF (doc 4-08-00-002013, 2024-04-24),
fetched from Neewer's support CDN; corroborated by the PL60C user manual and an
ENTTEC support thread. Full citations in the research transcript.

## The fixture

- NEEWER PL60C, 60 W RGBCW panel (R/G/B/Cool-White/Warm-White emitters)
- CCT 2500K–10000K (±100K), G/M −50…+50, CRI≥96
- **DMX-IN: male 5-pin XLR** chassis; DMX-OUT: female 5-pin XLR (for daisy-chain)
- Pinout: 1 = common/ground, 2 = Data−, 3 = Data+, 4/5 = not assigned (standard E1.11)
- No RDM. Address + DMX ON/OFF set in the lamp menu (long-press MODE/MENU)
- **On DMX signal loss the lamp holds the last look** (it does not black out)

## Channel map (n = DMX start address, default 1)

One personality. Channel n always selects the operating mode; n+1 is always
brightness. Reserve 9 channels worst-case.

| n value | Mode |
|---|---|
| 0–31 | CCT |
| 32–63 | HSI |
| 64–95 | FX (18 effects, sub-channel tables) |
| 96–127 | Gel (Rosco/LEE) |
| 128–159 | RGBCW (direct 5-emitter) |
| 160–191 | XY (CIE x/y) |

### CCT mode (what WakeLight uses for the sunrise)

| Ch | Function | Mapping |
|---|---|---|
| n | Mode select | write **16** (constant) |
| n+1 | Brightness | 0–255 = 0–100% |
| n+2 | Colour temperature | 0–255 linear 2500K → 10000K (≈29.4 K/step) |
| n+3 | G/M tint | 0 = −50 (green)… 128 ≈ 0 … 255 = +50 (magenta) |

### HSI mode (manual colour in the portal)

| Ch | Function | Mapping |
|---|---|---|
| n | Mode select | write **48** (constant) |
| n+1 | Intensity | 0–255 = 0–100% |
| n+2 | Hue | 0–255 = 0–360° |
| n+3 | Saturation | 0–255 = 0–100% |

## Lamp setup for WakeLight (one-time)

1. Long-press **MODE/MENU** → set **DMX → ON**
2. Menu → **DMX ADDR → 001** (or set with the SET dial in the DMX screen)
3. Optional: menu → **DIMMING CURVE → Linear** — WakeLight applies its own
   perceptual dawn curve, so a linear fixture curve gives the most predictable
   low-end ramp
4. Plug the WakeLight's female 5-pin XLR into **DMX-IN**

To use the lamp normally again (panel/app/2.4G), set DMX → OFF.
