#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// mDNS identity + peer discovery, so several WakeLights coexist on one network:
//   - each lamp publishes a unique hostname (its name's slug, or wakelight-XXXX)
//   - a _wakelight._tcp service with TXT {name, slug} is the discovery vehicle
//   - the first lamp also delegates "wakelight.local" as a shared entry point
//   - /api/peers PTR-queries the service to list every lamp on the LAN
// Ported from the V1 firmware (src/http_ui.c + device_id.c).
// ---------------------------------------------------------------------------

namespace Discovery {
  void begin();             // mdns_init + identity + services (after Wi-Fi up)
  void applyIdentity();     // recompute host/instance/TXT (call after a rename)
  const char* defaultHost();// "wakelight-XXXX" (MAC-based, always unique)
  const char* chosenHost(); // the hostname actually published (slug / slug-N / default)
  const char* slug();       // slug of the friendly name
  bool holdsWakelight();    // true if this lamp answers wakelight.local
  void buildPeers(JsonDocument& doc);  // fills doc["peers"] (self first)
}
