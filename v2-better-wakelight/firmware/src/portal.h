#pragma once

// Wi-Fi portal: serves the UI page and a small JSON API.
namespace Portal {
  void begin();   // start HTTP server + mDNS (http://wakelight.local)
  void tick();    // handle clients, call from loop()
}
