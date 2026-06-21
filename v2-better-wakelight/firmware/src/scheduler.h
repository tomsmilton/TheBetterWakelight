#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Wake scheduler + sunrise ramp engine.
//
// State machine:
//   IDLE     lamp dark, waiting for the next enabled alarm
//   SUNRISE  ramping; reaches full configured level exactly at alarm time
//   HOLD     full level, for cfg.holdMinutes after the alarm, then back to IDLE
//   MANUAL   user drove the lamp from the portal; scheduler hands-off until
//            the user turns the lamp off or taps "resume schedule"
//
// The sunrise itself runs in two perceptual phases (like a real dawn):
//   0..35%   deep warm glow fading up from black at the lamp's warmest CCT
//   35..100% brightness ramps to finalLevel while CCT sweeps
//            startCctK -> finalCctK
// Brightness uses a power curve (x^3) so the early glow is gentle.
// ---------------------------------------------------------------------------

enum class WakeState : uint8_t { IDLE, SUNRISE, HOLD, MANUAL };

namespace Scheduler {
  void begin();
  void tick();                       // call from loop(), cheap

  WakeState state();
  // Seconds until the next enabled alarm fires (sunrise start), <0 if none.
  long secondsToNextSunrise();
  String nextAlarmText();            // "Thu 07:00" or "none"

  void startDemo(uint16_t seconds);  // compressed sunrise for testing
  void stopAndOff();                 // kill any ramp/manual, lamp off, IDLE
  void enterManual();                // portal took over
}
