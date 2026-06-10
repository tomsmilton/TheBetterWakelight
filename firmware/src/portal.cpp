#include "portal.h"
#include "config.h"
#include "dmx_engine.h"
#include "scheduler.h"
#include "web_page.h"
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

static WebServer server(80);

static const char* stateName(WakeState s) {
  switch (s) {
    case WakeState::SUNRISE: return "Sunrise";
    case WakeState::HOLD:    return "Hold";
    case WakeState::MANUAL:  return "Manual";
    default:                 return "Idle";
  }
}

static void sendJson(JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleState() {
  JsonDocument doc;
  doc["state"] = stateName(Scheduler::state());
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  char buf[32];
  strftime(buf, sizeof(buf), "%a %H:%M:%S", &lt);
  doc["time"] = (now > 1700000000) ? buf : "syncing…";
  doc["next"] = Scheduler::nextAlarmText();
  doc["pkts"] = DmxEngine::packetsSent();
  sendJson(doc);
}

static void handleGetConfig() {
  JsonDocument doc;
  JsonArray arr = doc["alarms"].to<JsonArray>();
  for (int i = 0; i < 7; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["on"] = cfg.alarms[i].enabled;
    o["h"]  = cfg.alarms[i].hour;
    o["m"]  = cfg.alarms[i].minute;
  }
  doc["ramp"]   = cfg.rampMinutes;
  doc["hold"]   = cfg.holdMinutes;
  doc["flevel"] = cfg.finalLevel;
  doc["scct"]   = cfg.startCctK;
  doc["fcct"]   = cfg.finalCctK;
  doc["addr"]   = cfg.dmxAddress;
  doc["mode"]   = cfg.fixtureMode;
  doc["kmin"]   = cfg.cctMinK;
  doc["kmax"]   = cfg.cctMaxK;
  sendJson(doc);
}

static void handleSetConfig() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "bad json");
    return;
  }
  if (doc["alarms"].is<JsonArray>()) {
    JsonArray arr = doc["alarms"];
    for (int i = 0; i < 7 && i < (int)arr.size(); i++) {
      cfg.alarms[i].enabled = arr[i]["on"] | false;
      cfg.alarms[i].hour    = constrain((int)(arr[i]["h"] | 7), 0, 23);
      cfg.alarms[i].minute  = constrain((int)(arr[i]["m"] | 0), 0, 59);
    }
  }
  if (doc["ramp"].is<int>())   cfg.rampMinutes = constrain((int)doc["ramp"], 1, 120);
  if (doc["hold"].is<int>())   cfg.holdMinutes = constrain((int)doc["hold"], 0, 480);
  if (doc["flevel"].is<int>()) cfg.finalLevel  = constrain((int)doc["flevel"], 5, 100);
  if (doc["scct"].is<int>())   cfg.startCctK   = constrain((int)doc["scct"], 1800, 20000);
  if (doc["fcct"].is<int>())   cfg.finalCctK   = constrain((int)doc["fcct"], 1800, 20000);
  if (doc["addr"].is<int>())   cfg.dmxAddress  = constrain((int)doc["addr"], 1, 512);
  if (doc["mode"].is<int>())   cfg.fixtureMode = constrain((int)doc["mode"], 0, 2);
  if (doc["kmin"].is<int>())   cfg.cctMinK     = constrain((int)doc["kmin"], 1800, 10000);
  if (doc["kmax"].is<int>())   cfg.cctMaxK     = constrain((int)doc["kmax"], 2000, 20000);
  cfg.save();
  JsonDocument ok; ok["ok"] = true; sendJson(ok);
}

static void handleManual() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "bad json");
    return;
  }
  Scheduler::enterManual();
  Look l;
  l.intensity = constrain((float)(doc["level"] | 0) / 100.0f, 0.0f, 1.0f);
  l.cctK      = constrain((int)(doc["cct"] | 3200), 1800, 20000);
  l.useHsi    = doc["hsi"] | false;
  l.hue       = constrain((float)(doc["hue"] | 30.0f), 0.0f, 360.0f);
  l.sat       = constrain((float)(doc["sat"] | 100) / 100.0f, 0.0f, 1.0f);
  DmxEngine::setLook(l);
  JsonDocument ok; ok["ok"] = true; sendJson(ok);
}

static void handleDemo() {
  Scheduler::startDemo(120);
  JsonDocument ok; ok["ok"] = true; sendJson(ok);
}

static void handleOff() {
  Scheduler::stopAndOff();
  JsonDocument ok; ok["ok"] = true; sendJson(ok);
}

static void handleWifiReset() {
  JsonDocument ok; ok["ok"] = true; sendJson(ok);
  delay(200);
  WiFi.disconnect(true, true);    // erase stored credentials
  delay(200);
  ESP.restart();
}

void Portal::begin() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", PORTAL_HTML);
  });
  server.on("/api/state",     HTTP_GET,  handleState);
  server.on("/api/config",    HTTP_GET,  handleGetConfig);
  server.on("/api/config",    HTTP_POST, handleSetConfig);
  server.on("/api/manual",    HTTP_POST, handleManual);
  server.on("/api/demo",      HTTP_POST, handleDemo);
  server.on("/api/off",       HTTP_POST, handleOff);
  server.on("/api/wifireset", HTTP_POST, handleWifiReset);
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
  if (MDNS.begin(cfg.hostname)) MDNS.addService("http", "tcp", 80);
}

void Portal::tick() { server.handleClient(); }
