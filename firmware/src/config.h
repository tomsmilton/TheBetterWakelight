#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Persistent configuration (NVS-backed) for the WakeLight controller.
// ---------------------------------------------------------------------------

// Fixture DMX profile. The Neewer PL60C has a single personality where the
// base channel n SELECTS the operating mode (0-31 CCT, 32-63 HSI, ...), n+1 is
// always brightness, and the meaning of n+2.. depends on the mode — per the
// official Neewer "PL60C DMX Channel Table" (see docs/dmx-profile.md).
enum FixtureMode : uint8_t {
  MODE_PL60C = 0,     // native: n=mode select, n+1=dim, CCT or HSI sub-map
  MODE_GENERIC_CCT,   // simple fixtures: n=dimmer, n+1=CCT
  MODE_CUSTOM,        // user-defined channel offsets below
};

struct AlarmDay {
  bool     enabled = false;
  uint8_t  hour    = 7;      // alarm time = moment of full brightness
  uint8_t  minute  = 0;
};

struct Config {
  // --- fixture ---
  uint16_t dmxAddress   = 1;          // base address set on the lamp
  uint8_t  fixtureMode  = MODE_PL60C;
  // custom-mode channel offsets (0 = ch at base address). 255 = not present.
  uint8_t  chDimmer     = 0;
  uint8_t  chCct        = 1;
  uint8_t  chHue        = 255;
  uint8_t  chSat        = 255;
  // CCT range of the fixture in Kelvin, for mapping K -> DMX value.
  // PL60C: 2500K-10000K linear across 0-255 (official channel table).
  uint16_t cctMinK      = 2500;
  uint16_t cctMaxK      = 10000;

  // --- sunrise ---
  AlarmDay alarms[7];                  // 0 = Sunday ... 6 = Saturday (tm_wday)
  uint16_t rampMinutes   = 25;         // sunrise duration, ends at alarm time
  uint8_t  finalLevel    = 100;        // % brightness at alarm time
  uint16_t finalCctK     = 5600;       // colour temp at alarm time
  uint16_t startCctK     = 2500;       // colour temp at the start of the ramp
  uint16_t holdMinutes   = 60;         // stay on this long after alarm, then off

  // --- system ---
  char     tz[48]        = "GMT0BST,M3.5.0/1,M10.5.0";  // Europe/London
  char     ntpServer[48] = "pool.ntp.org";
  char     hostname[32]  = "wakelight";

  void load();
  void save();
  void reset();
};

extern Config cfg;
