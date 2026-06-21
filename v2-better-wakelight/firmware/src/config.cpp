#include "config.h"
#include "dmx_engine.h"
#include <Preferences.h>

Config cfg;

static Preferences prefs;
static const char* NVS_NS  = "wakelight";
static const char* NVS_KEY = "cfg3";        // bump suffix if struct layout changes

void Config::load() {
  prefs.begin(NVS_NS, true);
  if (prefs.isKey(NVS_KEY) && prefs.getBytesLength(NVS_KEY) == sizeof(Config)) {
    prefs.getBytes(NVS_KEY, this, sizeof(Config));
  }
  prefs.end();
}

void Config::save() {
  DmxEngine::pause();                 // NVS write would corrupt DMX timing
  prefs.begin(NVS_NS, false);
  prefs.putBytes(NVS_KEY, this, sizeof(Config));
  prefs.end();
  DmxEngine::resume();
}

void Config::reset() {
  prefs.begin(NVS_NS, false);
  prefs.clear();
  prefs.end();
  *this = Config();
}
