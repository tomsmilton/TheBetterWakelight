#include "scheduler.h"
#include "config.h"
#include "dmx_engine.h"
#include <time.h>
#include <math.h>

static WakeState st = WakeState::IDLE;
static Look   manualLook;                 // remembered override look
static int    dismissYday = -1;           // tm_yday the user dismissed; -1 = none
static int    curBright = 0;              // last applied brightness %  (status)
static int    curCct    = 2900;           // last applied CCT K         (status)

static const char* DAYS[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

static bool clockOk() { return time(nullptr) > 1700000000; }   // sane after NTP
static float clamp01(float v){ return v < 0 ? 0 : v > 1 ? 1 : v; }

// --- brightness shape: the same maths the web UI draws --------------------

static float fnEval(uint8_t fn, float u) {
  u = clamp01(u);
  switch (fn) {
    case CURVE_LINEAR:  return u;
    case CURVE_EASEIN:  return powf(u, 2.2f);
    case CURVE_EASEOUT: return 1.0f - powf(1.0f - u, 2.2f);
    case CURVE_EXPO: { const float a = 3.2f; return (expf(a * u) - 1.0f) / (expf(a) - 1.0f); }
    default: {            // CURVE_SIGMOID (logistic, normalised to hit 0 and 1)
      const float k = 9.0f;
      float L0 = 1.0f / (1.0f + expf(k * 0.5f));
      float L1 = 1.0f / (1.0f + expf(-k * 0.5f));
      float L  = 1.0f / (1.0f + expf(-k * (u - 0.5f)));
      return (L - L0) / (L1 - L0);
    }
  }
}

// u where fnEval(fn,u) == 0.5, by bisection (functions are monotonic).
static float fnMedian(uint8_t fn) {
  float lo = 0, hi = 1;
  for (int i = 0; i < 30; i++) { float m = (lo + hi) / 2; if (fnEval(fn, m) < 0.5f) lo = m; else hi = m; }
  return (lo + hi) / 2;
}

// Smooth monotone warp through (0,0),(sc,u0),(1,1) — Fritsch–Carlson, 3 points.
static float warp(float s, float sc, float u0) {
  sc = sc < 0.06f ? 0.06f : sc > 0.94f ? 0.94f : sc;
  s  = clamp01(s);
  float h0 = sc, h1 = 1.0f - sc;
  float d0 = u0 / h0, d1 = (1.0f - u0) / h1;
  float m0 = d0, m2 = d1, m1;
  if (d0 * d1 <= 0) m1 = 0;
  else { float w1 = 2 * h1 + h0, w2 = h1 + 2 * h0; m1 = (w1 + w2) / (w1 / d0 + w2 / d1); }
  float x0, y0, x1, y1, ma, mb, h;
  if (s <= sc) { x0 = 0;  y0 = 0;  x1 = sc; y1 = u0; ma = m0; mb = m1; h = h0; }
  else         { x0 = sc; y0 = u0; x1 = 1;  y1 = 1;  ma = m1; mb = m2; h = h1; }
  float t = (s - x0) / h, t2 = t * t, t3 = t2 * t;
  float h00 = 2*t3 - 3*t2 + 1, h10 = t3 - 2*t2 + t, h01 = -2*t3 + 3*t2, h11 = t3 - t2;
  return h00*y0 + h10*h*ma + h01*y1 + h11*h*mb;
}

// Brightness fraction (0..1 of finalLevel) at window progress t in 0..1.
float Scheduler::curveAt(float t) {
  float t0 = cfg.t0Pct / 100.0f, t1 = cfg.t1Pct / 100.0f;
  if (t1 - t0 < 0.02f) t1 = t0 + 0.02f;
  float s = (t - t0) / (t1 - t0);
  if (s <= 0) return 0;
  if (s >= 1) return 1;
  float u0 = (cfg.curveFn == CURVE_LINEAR) ? 0.5f : fnMedian(cfg.curveFn);
  float sc = cfg.skewPct / 100.0f;
  return fnEval(cfg.curveFn, warp(s, sc, u0));
}

// --- target for "right now" ------------------------------------------------

struct Target { bool active; bool hold; float bright; float cct; };

static time_t alarmWakeEpoch(const Alarm& a, time_t dayRef) {
  struct tm lt; localtime_r(&dayRef, &lt);
  if (!(a.days & (1 << lt.tm_wday))) return 0;
  lt.tm_hour = a.wakeMin / 60; lt.tm_min = a.wakeMin % 60; lt.tm_sec = 0; lt.tm_isdst = -1;
  return mktime(&lt);
}

static Target computeTarget(time_t now) {
  Target none{false, false, 0, 0};
  if (!cfg.scheduleOn) return none;
  for (int ai = 0; ai < 2; ai++) {
    const Alarm& a = cfg.alarms[ai];
    if (!a.enabled) continue;
    // Check today and yesterday so a late-night hold can cross midnight.
    for (int back = 0; back <= 1; back++) {
      time_t wake = alarmWakeEpoch(a, now - (time_t)back * 86400);
      if (!wake) continue;
      time_t winStart = wake - (time_t)cfg.sunriseMin * 60;
      time_t holdEnd  = wake + (time_t)cfg.holdMinutes * 60;
      if (now < winStart || now >= holdEnd) continue;
      struct tm wt; localtime_r(&wake, &wt);
      if (dismissYday == wt.tm_yday) continue;             // skipped today
      float finalFrac = cfg.finalLevel / 100.0f;
      if (now < wake) {                                    // ramping
        float t = (float)(now - winStart) / (float)(wake - winStart);
        float bf = Scheduler::curveAt(t);
        float cct = cfg.startCctK + (cfg.endCctK - cfg.startCctK) * bf;
        return {true, false, finalFrac * bf, cct};
      }
      return {true, true, finalFrac, (float)cfg.endCctK}; // holding at full
    }
  }
  return none;
}

// Epoch of the next alarm strictly after 'from' (for the "next alarm" text).
static time_t nextWakeEpoch(time_t from) {
  time_t best = 0;
  for (int d = 0; d < 8; d++) {
    time_t ref = from + (time_t)d * 86400;
    for (int ai = 0; ai < 2; ai++) {
      const Alarm& a = cfg.alarms[ai];
      if (!a.enabled) continue;
      time_t wake = alarmWakeEpoch(a, ref);
      if (wake > from && (!best || wake < best)) {
        struct tm wt; localtime_r(&wake, &wt);
        if (dismissYday == wt.tm_yday) continue;
        best = wake;
      }
    }
  }
  return best;
}

void Scheduler::begin() {}

void Scheduler::tick() {
  static uint32_t lastMs = 0;
  if (millis() - lastMs < 1000) return;
  lastMs = millis();
  if (!clockOk()) return;

  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  if (dismissYday != -1 && dismissYday != lt.tm_yday) dismissYday = -1;  // new day

  if (st == WakeState::MANUAL) {                 // override owns the lamp
    curBright = (int)lroundf(manualLook.intensity * 100.0f);
    curCct = (int)manualLook.cctK;
    return;
  }

  Target tg = computeTarget(now);
  if (tg.active) {
    Look l; l.intensity = tg.bright; l.cctK = tg.cct;
    DmxEngine::setLook(l);
    curBright = (int)lroundf(tg.bright * 100.0f);
    curCct = (int)tg.cct;
    st = tg.hold ? WakeState::HOLD : WakeState::SUNRISE;
  } else {
    if (st != WakeState::IDLE) DmxEngine::setLook(Look{});   // just ended -> off
    curBright = 0;
    st = WakeState::IDLE;
  }
}

WakeState Scheduler::state() { return st; }
bool Scheduler::timeValid() { return clockOk(); }
int  Scheduler::curBrightnessPct() { return curBright; }
int  Scheduler::curCctK() { return curCct; }

long Scheduler::secondsToNextSunrise() {
  if (!clockOk()) return -1;
  time_t now = time(nullptr);
  time_t wake = nextWakeEpoch(now);
  if (!wake) return -1;
  return (long)(wake - (time_t)cfg.sunriseMin * 60 - now);
}

String Scheduler::nextAlarmText() {
  if (!clockOk()) return "clock not set";
  if (!cfg.scheduleOn) return "alarm off";
  time_t wake = nextWakeEpoch(time(nullptr));
  if (!wake) return "none";
  struct tm lt; localtime_r(&wake, &lt);
  char buf[20];
  snprintf(buf, sizeof(buf), "%s %02d:%02d", DAYS[lt.tm_wday], lt.tm_hour, lt.tm_min);
  return String(buf);
}

void Scheduler::setOverride(const Look& l) {
  manualLook = l;
  DmxEngine::setLook(l);
  st = WakeState::MANUAL;
}

void Scheduler::clearOverride() {
  if (st == WakeState::MANUAL) { st = WakeState::IDLE; DmxEngine::setLook(Look{}); }
}

bool Scheduler::overrideOn() { return st == WakeState::MANUAL; }
Look Scheduler::overrideLook() { return manualLook; }

void Scheduler::dismissToday() {
  if (!clockOk()) return;
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  dismissYday = lt.tm_yday;
  if (st == WakeState::SUNRISE || st == WakeState::HOLD) {
    DmxEngine::setLook(Look{});
    st = WakeState::IDLE;
    curBright = 0;
  }
}

bool Scheduler::dismissedToday() {
  if (!clockOk()) return false;
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  return dismissYday == lt.tm_yday;
}
