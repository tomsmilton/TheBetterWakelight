// WakeLight — sunrise alarm controller for a DMX LED panel (Neewer PL60C).
//
// Boot flow:
//   1. Try saved Wi-Fi. If none/unreachable, open a setup AP "WakeLight-Setup"
//      (captive portal, password "sunrise123") to collect home Wi-Fi creds.
//   2. Sync time over NTP with Europe/London DST rules.
//   3. Start the DMX transmit task (continuous 40 Hz refresh) and web portal
//      at http://wakelight.local/
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "config.h"
#include "dmx_engine.h"
#include "scheduler.h"
#include "portal.h"

static const int STATUS_LED = 2;   // devkit blue LED; net-named LED on the PCB

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, HIGH);          // solid while connecting

  cfg.load();

  // DMX starts immediately so the lamp sees a valid (all-zero) stream even
  // before Wi-Fi is up — avoids the fixture flagging "no signal".
  DmxEngine::begin();

  WiFi.setHostname(cfg.hostname);
  WiFiManager wm;
  wm.setConfigPortalTimeout(300);          // retry saved creds every 5 min
  wm.setConnectTimeout(20);
  while (!wm.autoConnect("WakeLight-Setup", "sunrise123")) {
    Serial.println("WiFi portal timed out, retrying…");
  }
  Serial.printf("WiFi up: %s\n", WiFi.localIP().toString().c_str());

  configTzTime(cfg.tz, cfg.ntpServer, "time.cloudflare.com");

  Scheduler::begin();
  Portal::begin();
  digitalWrite(STATUS_LED, LOW);
}

void loop() {
  Portal::tick();
  Scheduler::tick();

  // Status LED: short blip every 2 s in idle, fast blink during sunrise.
  uint32_t period = (Scheduler::state() == WakeState::SUNRISE) ? 250 : 2000;
  digitalWrite(STATUS_LED, (millis() % period) < 60 ? HIGH : LOW);
  delay(2);
}
