#include "portal.h"
#include "config.h"
#include "dmx_engine.h"
#include "scheduler.h"
#include "discovery.h"
#include "web_page.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

static WebServer server(80);

static void sendJson(JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}
static bool readBody(JsonDocument& doc) {
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "bad json");
    return false;
  }
  return true;
}
static void ok() { JsonDocument d; d["ok"] = true; sendJson(d); }

// --------------------------------------------------------------------------

static const char* stateLabel() {
  if (Scheduler::overrideOn())      return "Light on";
  if (!cfg.scheduleOn)              return "Alarm off";
  switch (Scheduler::state()) {
    case WakeState::SUNRISE:        return "Sunrise";
    case WakeState::HOLD:           return "Holding";
    default:                        return Scheduler::dismissedToday() ? "Done for today" : "Standing by";
  }
}

static void handleStatus() {
  JsonDocument doc;
  doc["label"]       = stateLabel();
  doc["scheduleOn"]  = cfg.scheduleOn;
  doc["override"]    = Scheduler::overrideOn();
  doc["dismissed"]   = Scheduler::dismissedToday();
  doc["time_valid"]  = Scheduler::timeValid();
  doc["brightness"]  = Scheduler::curBrightnessPct();
  doc["cct"]         = Scheduler::curCctK();
  doc["next"]        = Scheduler::nextAlarmText();
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  char buf[16];
  strftime(buf, sizeof(buf), "%H:%M", &lt);
  doc["now"] = Scheduler::timeValid() ? buf : "--:--";
  sendJson(doc);
}

static void alarmToJson(JsonObject o, const Alarm& a) {
  o["on"]   = a.enabled;
  o["days"] = a.days;
  o["wake"] = a.wakeMin;
}
static void alarmFromJson(JsonVariantConst o, Alarm& a) {
  if (o.isNull()) return;
  a.enabled = o["on"] | a.enabled;
  if (o["days"].is<int>()) a.days = (uint8_t)((int)o["days"] & 0x7F);
  if (o["wake"].is<int>()) a.wakeMin = constrain((int)o["wake"], 0, 1439);
}

static void handleGetSchedule() {
  JsonDocument doc;
  doc["scheduleOn"] = cfg.scheduleOn;
  JsonArray arr = doc["alarms"].to<JsonArray>();
  for (int i = 0; i < 2; i++) alarmToJson(arr.add<JsonObject>(), cfg.alarms[i]);
  doc["curveFn"]    = cfg.curveFn;
  doc["sunriseMin"] = cfg.sunriseMin;
  doc["t0"]         = cfg.t0Pct;
  doc["t1"]         = cfg.t1Pct;
  doc["skew"]       = cfg.skewPct;
  doc["finalLevel"] = cfg.finalLevel;
  doc["startCct"]   = cfg.startCctK;
  doc["endCct"]     = cfg.endCctK;
  doc["hold"]       = cfg.holdMinutes;
  doc["kmin"]       = cfg.cctMinK;
  doc["kmax"]       = cfg.cctMaxK;
  sendJson(doc);
}

static void handlePutSchedule() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  if (doc["scheduleOn"].is<bool>()) cfg.scheduleOn = doc["scheduleOn"];
  if (doc["alarms"].is<JsonArray>()) {
    JsonArrayConst arr = doc["alarms"];
    for (int i = 0; i < 2 && i < (int)arr.size(); i++) alarmFromJson(arr[i], cfg.alarms[i]);
  }
  if (doc["curveFn"].is<int>())    cfg.curveFn     = constrain((int)doc["curveFn"], 0, 4);
  if (doc["sunriseMin"].is<int>()) cfg.sunriseMin  = constrain((int)doc["sunriseMin"], 1, 120);
  if (doc["t0"].is<int>())         cfg.t0Pct       = constrain((int)doc["t0"], 0, 90);
  if (doc["t1"].is<int>())         cfg.t1Pct       = constrain((int)doc["t1"], 10, 100);
  if (doc["skew"].is<int>())       cfg.skewPct     = constrain((int)doc["skew"], 6, 94);
  if (doc["finalLevel"].is<int>()) cfg.finalLevel  = constrain((int)doc["finalLevel"], 5, 100);
  if (doc["startCct"].is<int>())   cfg.startCctK   = constrain((int)doc["startCct"], 2500, 10000);
  if (doc["endCct"].is<int>())     cfg.endCctK     = constrain((int)doc["endCct"], 2500, 10000);
  if (doc["hold"].is<int>())       cfg.holdMinutes = constrain((int)doc["hold"], 0, 480);
  cfg.save();
  handleGetSchedule();
}

static void handleOverride() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  bool on = doc["on"] | false;
  if (on) {
    Look l = Scheduler::overrideLook();
    if (l.intensity < 0.02f && !l.useFx) { l = Look{}; l.intensity = 0.65f; l.cctK = 3000; }
    Scheduler::setOverride(l);
  } else {
    Scheduler::clearOverride();
  }
  ok();
}

static void handleManual() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  Look l;
  l.intensity = constrain((float)(doc["level"] | 0) / 100.0f, 0.0f, 1.0f);
  l.useHsi    = doc["hsi"] | false;
  l.cctK      = constrain((int)(doc["cct"] | 3200), 2500, 10000);
  l.gm        = constrain((float)(doc["gm"] | 0), -50.0f, 50.0f);
  l.hue       = constrain((float)(doc["hue"] | 30.0f), 0.0f, 360.0f);
  l.sat       = constrain((float)(doc["sat"] | 100) / 100.0f, 0.0f, 1.0f);
  Scheduler::setOverride(l);
  ok();
}

static void handleEffect() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  Look l;
  l.useFx     = true;
  l.intensity = constrain((float)(doc["level"] | 80) / 100.0f, 0.0f, 1.0f);
  l.fxEffect  = (uint8_t)constrain((int)(doc["fx"] | 0), 0, 255);
  if (doc["p"].is<JsonArray>()) {
    JsonArrayConst p = doc["p"];
    for (int i = 0; i < 6 && i < (int)p.size(); i++)
      l.fxParams[i] = (uint8_t)constrain((int)p[i], 0, 255);
  }
  Scheduler::setOverride(l);
  ok();
}

static void handleDismiss() { Scheduler::dismissToday(); ok(); }

static void handleGetDevice() {
  JsonDocument doc;
  doc["name"]  = cfg.name;
  doc["host"]  = Discovery::chosenHost();        // the .local that actually resolves
  doc["slug"]  = Discovery::slug();
  doc["holds"] = Discovery::holdsWakelight();    // also answers wakelight.local
  sendJson(doc);
}
static void handlePutDevice() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  if (doc["name"].is<const char*>()) {
    strlcpy(cfg.name, doc["name"].as<const char*>(), sizeof(cfg.name));
    cfg.save();
    Discovery::applyIdentity();                  // republish hostname/instance/TXT
  }
  handleGetDevice();
}

static void handlePeers() {
  JsonDocument doc;
  Discovery::buildPeers(doc);
  sendJson(doc);
}

static void handleGetFixture() {
  JsonDocument doc;
  doc["addr"] = cfg.dmxAddress;
  doc["mode"] = cfg.fixtureMode;
  doc["kmin"] = cfg.cctMinK;
  doc["kmax"] = cfg.cctMaxK;
  sendJson(doc);
}
static void handlePutFixture() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  if (doc["addr"].is<int>()) cfg.dmxAddress = constrain((int)doc["addr"], 1, 512);
  if (doc["kmin"].is<int>()) cfg.cctMinK    = constrain((int)doc["kmin"], 1800, 10000);
  if (doc["kmax"].is<int>()) cfg.cctMaxK    = constrain((int)doc["kmax"], 2000, 20000);
  cfg.save();
  handleGetFixture();
}

static void handleWifiReset() {
  ok();
  delay(200);
  WiFi.disconnect(true, true);
  delay(200);
  ESP.restart();
}

void Portal::begin() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", PORTAL_HTML); });
  server.on("/api/status",   HTTP_GET,  handleStatus);
  server.on("/api/schedule", HTTP_GET,  handleGetSchedule);
  server.on("/api/schedule", HTTP_PUT,  handlePutSchedule);
  server.on("/api/override", HTTP_POST, handleOverride);
  server.on("/api/manual",   HTTP_POST, handleManual);
  server.on("/api/effect",   HTTP_POST, handleEffect);
  server.on("/api/dismiss",  HTTP_POST, handleDismiss);
  server.on("/api/device",   HTTP_GET,  handleGetDevice);
  server.on("/api/device",   HTTP_PUT,  handlePutDevice);
  server.on("/api/peers",    HTTP_GET,  handlePeers);
  server.on("/api/fixture",  HTTP_GET,  handleGetFixture);
  server.on("/api/fixture",  HTTP_PUT,  handlePutFixture);
  server.on("/api/wifireset",HTTP_POST, handleWifiReset);
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
  Discovery::begin();
}

void Portal::tick() { server.handleClient(); }
