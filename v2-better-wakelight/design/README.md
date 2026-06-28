# V2 firmware UI redesign — mockups

Static HTML mockups exploring directions for the rebuilt V2 portal UI, before we
touch firmware. Open [`mockups/index.html`](mockups/index.html) for the gallery,
or any file in `mockups/` directly. No backend — controls are faked so the *feel*
is real (toggles, sliders, day pills, dials, tabs all respond).

Same scenario across all ten so they compare fairly: wake **07:00, Mon–Fri,
30-minute sunrise**, lamp named **Bedroom**.

## The ten directions

| # | Name | Idea | Leans |
|---|------|------|-------|
| 01 | Alarm list | Phone-alarm list, master toggle, expandable rows | recurring-first, familiar |
| 02 | Sunrise hero | One hero card: time + sunrise arc + two sliders | minimal, visual |
| 03 | Status dashboard | Lamp state + Auto/On/Off first, summary cards below | status / on-off first |
| 04 | Timeline curve | Refined V1 brightness+CCT waypoint editor | power-user, closest to V1 |
| 05 | Light dial | Draggable brightness ring + temp + scenes; alarm secondary | light-first, tactile |
| 06 | Bedtime clock | Drag the sun handle on a 24h dial; dark theme | distinctive, direct-manip |
| 07 | Tabbed app | Bottom tabs: Home · Alarm · Light · More | app-like, scales |
| 08 | Setup wizard | Guided 4-step: when / days / gentleness / confirm | onboarding, guided |
| 09 | Warm minimal | Editorial serif, one calm screen + Advanced disclosure | calm, restrained |
| 10 | Scenes & widgets | Widget board with one-tap scenes | glanceable dashboard |

## What we're deciding

Which aesthetic and which interaction model best serve the three core jobs:
**set the light · set a recurring wake · turn on/off**. Advanced features
(effects, DMX address, fixture profile, extra alarms) should stay hidden.

Tom clicks through, comments per mockup; we converge on a direction (likely a
hybrid) and *then* rebuild the firmware UI.
