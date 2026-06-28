#pragma once
#include <Arduino.h>
#include "dmx_engine.h"

// ---------------------------------------------------------------------------
// Wake scheduler + sunrise ramp engine.
//
//   IDLE     lamp dark, waiting for the next alarm window
//   SUNRISE  ramping; reaches finalLevel exactly at the alarm time
//   HOLD     full level for cfg.holdMinutes after the alarm, then off
//   MANUAL   the portal override owns the lamp until it is switched off
//
// Each tick recomputes the target from the wall clock, so the lamp is correct
// even across reboots mid-sunrise. The brightness ramp follows the configured
// maths function f(), repositioned by a skew warp, exactly matching the web UI.
// ---------------------------------------------------------------------------

enum class WakeState : uint8_t { IDLE, SUNRISE, HOLD, MANUAL };

namespace Scheduler {
  void begin();
  void tick();                       // call from loop(), cheap (1 Hz internally)

  WakeState state();
  bool   timeValid();
  long   secondsToNextSunrise();     // <0 if none / no clock
  String nextAlarmText();            // "Thu 07:00" or "none"

  // Live readout of what the lamp is currently doing (for the status pill).
  int    curBrightnessPct();
  int    curCctK();

  // Manual override (shared by the Home "turn on now" and the Manual tab).
  void   setOverride(const Look& l); // enter MANUAL and show this look
  void   clearOverride();            // back to AUTO (recomputes next tick)
  bool   overrideOn();
  Look   overrideLook();             // last manual look (for restoring)

  // End today's wake-up early / skip it; auto-clears at the next day.
  void   dismissToday();
  bool   dismissedToday();

  // Evaluate the brightness fraction (0..1) of the configured curve at window
  // progress t in 0..1. Exposed so the portal can echo the shape if needed.
  float  curveAt(float t);
}
