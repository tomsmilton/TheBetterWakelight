#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Persistent configuration (NVS-backed) for the WakeLight controller.
//
// Model mirrors the web UI 1:1:
//   - up to two alarms, each just a single wake time (fires every day)
//   - one shared "sunrise shape": a maths curve (function + rise window + skew)
//     that ramps brightness 0 -> finalLevel and sweeps colour startCct -> endCct
//   - hold time after wake, then off
// The lamp is a Neewer PL60C (see docs/dmx-profile.md).
// ---------------------------------------------------------------------------

enum FixtureMode : uint8_t {
  MODE_PL60C = 0,     // native: n=mode select, n+1=dim, then per-mode sub-map
  MODE_GENERIC_CCT,   // simple fixtures: n=dimmer, n+1=CCT
  MODE_CUSTOM,        // user-defined channel offsets below
};

// Brightness ramp shape. Indices are persisted — append only.
enum CurveFn : uint8_t {
  CURVE_SIGMOID = 0,
  CURVE_LINEAR,
  CURVE_EASEIN,
  CURVE_EASEOUT,
  CURVE_EXPO,
};

struct Alarm {
  bool     enabled = false;
  uint16_t wakeMin = 7 * 60;  // minute-of-day the lamp reaches full brightness (fires daily)
};

struct Config {
  // --- fixture ---
  uint16_t dmxAddress   = 1;          // base address set on the lamp
  uint8_t  fixtureMode  = MODE_PL60C;
  uint8_t  chDimmer     = 0;          // custom-mode channel offsets (255 = absent)
  uint8_t  chCct        = 1;
  uint8_t  chHue        = 255;
  uint8_t  chSat        = 255;
  uint16_t cctMinK      = 2500;       // PL60C: 2500K-10000K linear across 0-255
  uint16_t cctMaxK      = 10000;

  // --- alarms ---
  bool     scheduleOn   = true;       // master arm/disarm of the wake alarm
  Alarm    alarms[2];                 // [0] = primary, [1] = optional second

  // --- sunrise shape (shared by both alarms) ---
  uint8_t  curveFn      = CURVE_SIGMOID;
  uint16_t sunriseMin   = 30;         // ramp window length, ends at wake time
  uint8_t  t0Pct        = 0;          // rise starts at this % of the window
  uint8_t  t1Pct        = 100;        // rise reaches full at this % of the window
  uint8_t  skewPct      = 50;         // 50 = natural; skews where the 50% point lands
  uint8_t  finalLevel   = 100;        // % brightness at full
  uint16_t startCctK    = 2500;       // colour at the start of the rise
  uint16_t endCctK      = 5600;       // colour at full brightness
  uint16_t holdMinutes  = 15;         // stay on after wake, then off

  // --- system ---
  char     tz[48]        = "GMT0BST,M3.5.0/1,M10.5.0";  // Europe/London
  char     ntpServer[48] = "pool.ntp.org";
  char     hostname[32]  = "wakelight";
  char     name[32]      = "WakeLight";   // friendly name shown in the UI

  Config() {
    // Primary alarm on by default: 07:00, every day.
    alarms[0].enabled = true;
    alarms[0].wakeMin = 7 * 60;
  }

  void load();
  void save();
  void reset();
};

extern Config cfg;
