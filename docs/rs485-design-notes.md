# RS-485 / DMX output stage — design decisions

Distilled from research (June 2026). Full sources in research transcript.

## Decisions

| Topic | Decision | Why |
|---|---|---|
| Library | `esp_dmx` v4.1.0, **pinned to `espressif32@6.x`** (Arduino core 2.0.17) | v4.1.0 does not compile on IDF5/core 3.x (PR #223 unmerged). Hardware-UART driven, correct E1.11 break/MAB timing, immune to Wi-Fi jitter. |
| DMX task | Own FreeRTOS task pinned to **core 1**, full 513-slot frame every 25 ms | Wi-Fi/lwIP live on core 0. Web handlers never call `dmx_send()` — they update a shared `Look`. |
| Flash writes | `dmx_driver_disable()`/`enable()` around NVS saves | Flash writes stall cache; DMX ISR is in flash on Arduino builds → corrupted frames. |
| Transceiver | **TI THVD1410DR** (SOIC-8, LCSC C2671345) for the PCB; SP3485/MAX3485-class 3.3 V breakout for the module build | 3.3 V native (no 5 V rail, no level concerns), 500 kbps slew-limited (clean EMI), ±18 kV IEC ESD on bus pins → no external TVS needed. |
| MAX485 (5 V) fallback | Acceptable transmit-only: V_IH(min)=2.0 V so 3.3 V DI is in spec; power it from 5 V, leave RO unconnected (it would swing 5 V) | The common red Amazon breakout works fine for TX-only. |
| Isolation | **None** | One fixture, bedroom, USB wall adapter (already mains-isolated). DMX512-A only *recommends* isolation; Enttec Open DMX class devices are non-isolated. |
| Termination | None fitted at transmitter; 120 Ω DNP footprint across A–B | Bus is terminated once at the far end (the lamp / a terminator plug). Fit only if RDM later. |
| Fail-safe bias | 680 Ω A→3.3 V, 680 Ω B→GND (fitted) | Cheap insurance through reset/boot gaps; matches SparkFun shield practice. |
| DE & /RE | Tied together, GPIO21, plus 10 kΩ pull-up to 3.3 V | Driver enabled through boot; pass GPIO21 as the RTS pin in `dmx_set_pin()`. |

## Pinout

```
ESP32 GPIO17 (UART1 TX) ──► DI
ESP32 GPIO21            ──► DE + /RE (tied, 10k pull-up to 3V3)
RO                      ──  n/c (transmit-only)
A  (D+)                 ──► XLR pin 3
B  (D−)                 ──► XLR pin 2
GND                     ──► XLR pin 1 (signal common)
XLR pins 4,5            ──  n/c (optional data link 2 in the spec)
```

DMX controller output uses a **female** XLR; the lamp's DMX IN is male.
