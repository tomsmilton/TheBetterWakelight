#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// DMX512 output engine.
//
// Owns the 512-slot universe buffer and a FreeRTOS task that retransmits it
// continuously at ~40 Hz (DMX fixtures expect an uninterrupted refresh; many,
// including Neewer panels, blackout or hold-last-look if the stream stops).
//
// The rest of the firmware only ever calls setLook(): a fixture-level
// description (intensity, colour temperature, optional hue/sat) which is
// mapped onto DMX channels according to cfg.fixtureMode / cfg.dmxAddress.
// ---------------------------------------------------------------------------

// Hardware pins — identical on the devkit module build and the custom PCB.
constexpr int DMX_TX_PIN = 17;   // -> transceiver DI
constexpr int DMX_RX_PIN = -1;   // RO left unconnected (transmit-only)
constexpr int DMX_EN_PIN = 21;   // -> transceiver DE & /RE (high = drive bus)

struct Look {
  float intensity = 0.0f;   // 0..1 (0 = off)
  float cctK      = 2900;   // colour temperature in Kelvin
  float hue       = 30;     // degrees, only used in HSI mode
  float sat       = 1.0f;   // 0..1, only used in HSI mode
  bool  useHsi    = false;  // drive hue/sat channels instead of CCT
};

namespace DmxEngine {
  void begin();                 // install driver, start TX task
  void setLook(const Look& l);  // thread-safe: latest look wins
  Look currentLook();
  // Raw universe poke for the portal's "expert" page / debugging.
  void setRaw(uint16_t channel1based, uint8_t value);
  uint32_t packetsSent();
  // Flash writes (NVS/LittleFS) stall the cache and corrupt DMX timing on
  // Arduino builds — bracket them with pause()/resume().
  void pause();
  void resume();
}
