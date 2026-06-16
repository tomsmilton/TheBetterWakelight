// RS485 / DMX output self-check  (env:selfcheck — not built into production)
//
// Exercises the REAL DmxEngine code path (same dmx_engine.cpp the firmware
// ships) and reports over serial @115200:
//   * esp_dmx driver install + pin assignment        (MCU -> transceiver DI)
//   * DE/-RE enable line is driven high (bus = drive) (GPIO21 -> transceiver)
//   * DMX frames actually clock out of the UART       (packet counter rate)
//   * a visible activity pattern on slot 1            (scope / lamp confirm)
//
// What it CANNOT prove on this transmit-only board: the differential A/B
// output itself. RO is unconnected (DMX_RX_PIN = -1), so there is no return
// path to loop back. That last hop needs a scope on XLR pin2/pin3 or the lamp.
#include <Arduino.h>
#include <driver/gpio.h>
#include "config.h"
#include "dmx_engine.h"

static void line() { Serial.println("------------------------------------------"); }

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
  line();
  Serial.println("WakeLight RS485/DMX self-check");
  line();

  cfg.fixtureMode = MODE_PL60C;          // defaults; no NVS needed
  Serial.printf("TX pin (->DI)      : GPIO%d\n", DMX_TX_PIN);
  Serial.printf("EN pin (->DE,/RE)  : GPIO%d\n", DMX_EN_PIN);
  Serial.printf("RX pin (RO)        : %d  (unconnected = transmit-only)\n", DMX_RX_PIN);

  Serial.print("[1] Installing esp_dmx driver + pins ... ");
  DmxEngine::begin();                    // real install + TX task
  delay(150);
  Serial.println("ok (no panic)");

  // [2] DE / -RE enable line. esp_dmx drives GPIO21 as a PERIPHERAL output via
  //     the GPIO matrix, which leaves the pad's input buffer disabled -- so a
  //     plain gpio_get_level() reads 0 no matter the real voltage. Force the
  //     input buffer on (INPUT_OUTPUT) so we can actually observe the pad, then
  //     poll fast for ~60 ms (> one frame+gap) to catch the driven-HIGH phase.
  gpio_set_direction((gpio_num_t)DMX_EN_PIN, GPIO_MODE_INPUT_OUTPUT);
  uint32_t highs = 0, samples = 0;
  for (uint32_t t = millis(); millis() - t < 60; ) {
    highs += gpio_get_level((gpio_num_t)DMX_EN_PIN); samples++;
  }
  bool en_observable = highs > 0;        // saw it HIGH = enable line confirmed
  Serial.printf("[2] DE//RE keying   : HIGH on %lu/%lu samples -> %s\n",
                (unsigned long)highs, (unsigned long)samples,
                en_observable ? "enable line IS driven during TX"
                              : "not observable in sw (defer to scope/lamp)");

  // [3] Frame liveness: count packets over a 2 s window. dmx_send ->
  //     dmx_wait_sent completing each loop means the UART is shifting out
  //     full DMX frames (break + 513 slots). Stuck bus -> wait_sent stalls
  //     and the rate collapses toward zero.
  Serial.println("[3] Measuring DMX frame rate (2.0 s) ...");
  Look l; l.intensity = 0.5f; l.cctK = 5600; DmxEngine::setLook(l);
  uint32_t p0 = DmxEngine::packetsSent();
  delay(2000);
  uint32_t p1 = DmxEngine::packetsSent();
  float hz = (p1 - p0) / 2.0f;
  Serial.printf("    packets: %lu -> %lu  (%u in 2 s = %.1f Hz)\n",
                (unsigned long)p0, (unsigned long)p1, (unsigned)(p1 - p0), hz);

  bool install_ok = true;                // got here without panic
  // dmx_wait_sent (~23 ms/frame) + 25 ms task delay => ~21 Hz by design.
  // DMX512 is valid anywhere from ~1 Hz up to ~44 Hz; accept a healthy band.
  bool rate_ok    = (hz >= 15.0f && hz <= 45.0f);
  bool sw_ok      = install_ok && rate_ok;   // what software can truly verify

  line();
  Serial.printf("RESULT  driver:%s  framerate:%s  enable:%s\n",
                install_ok ? "PASS" : "FAIL",
                rate_ok    ? "PASS" : "FAIL",
                en_observable ? "PASS" : "n/a (sw)");
  if (sw_ok) {
    Serial.printf("=> MCU->transceiver TX path is HEALTHY (%.1f Hz, frames\n", hz);
    Serial.println("   shifting out continuously). The differential A/B output");
    Serial.println("   and DE assertion still need a scope on XLR 2/3 or the");
    Serial.println("   lamp to confirm the final electrical hop end-to-end.");
  } else {
    Serial.println("=> CHECK FAILED — see failing line above.");
  }
  line();
  Serial.println("Now driving a 1 Hz blink pattern on slot 1 (watch status LED /");
  Serial.println("scope). Reset the board to stop. Re-flash env:esp32dev for prod.");
}

void loop() {
  static bool on = false;
  on = !on;
  Look l;
  l.intensity = on ? 0.8f : 0.0f;        // full <-> black on the rendered slot
  l.cctK = 5600;
  DmxEngine::setLook(l);
  Serial.printf("  pattern: slot1 = %s   packets=%lu\n",
                on ? "ON " : "off", (unsigned long)DmxEngine::packetsSent());
  delay(1000);
}
