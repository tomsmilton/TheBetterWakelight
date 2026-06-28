# V2 firmware UI redesign — mockup

The agreed direction for the rebuilt V2 portal UI, before we touch firmware.
Open [`mockups/11-home.html`](mockups/11-home.html) directly in a browser (or
serve the folder) — no backend; controls are faked so the *feel* is real.

Scenario throughout: wake **07:00, 30-minute sunrise**, lamp named **Bedroom**.

## Layout

A rich **Home** holds the everyday controls, with emoji bottom tabs to the
advanced pages.

- **Home** — status hero; **Wake-up alarm** on/off with a scroller time picker
  and a full-width *End today's wake-up*; **Turn the light on now** (greys out
  until toggled; its override is linked to the Manual tab's Light toggle).
- **Schedule** — the **Sunrise shape** editor: pick a maths curve
  (Sigmoid / Linear / Ease-in / Ease-out / Exponential); outer handles pin where
  the rise starts & finishes; the middle handle (locked at 50%) skews the curve
  by warping the input — `b(t) = f(warp((t−t0)/(t1−t0)))` — while keeping the
  shape's character. Plus a **colour-through-the-ramp** picker constrained to the
  PL60C's real range (2500–10000 K). Then sunrise length / final brightness /
  stay-on / second alarm.
- **Manual** — full manual controller (brightness, White-CCT ⇄ HSI colour) linked
  to Home's override, plus the lamp's 17 effects (popular grid + full list).
- **Settings** — lamp name/address, DMX address, fixture profile (locked to this
  board for now), colour range, time/Wi-Fi.

## Next

Wire this into the real firmware: map controls to the existing HTTP API and
serialise the curve as `{shape, t0, t1, skew}` + wake time.
