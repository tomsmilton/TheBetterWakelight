#include "scheduler.h"
#include "config.h"
#include "dmx_engine.h"
#include <time.h>

static WakeState st = WakeState::IDLE;
static time_t sunriseStart = 0;     // epoch when current ramp began
static time_t sunriseEnd   = 0;     // epoch of alarm (full level)
static time_t holdUntil    = 0;
static time_t lastFiredAlarm = 0;   // dedupe: don't re-trigger the same alarm
static bool demoRun = false;

static const char* DAYS[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

static bool timeValid() {
  return time(nullptr) > 1700000000;   // sane only after NTP sync
}

// Epoch of the next enabled alarm strictly after 'from'.
static time_t nextAlarmEpoch(time_t from) {
  for (int d = 0; d < 8; d++) {                  // today + 7 days
    time_t probe = from + (time_t)d * 86400;
    struct tm lt; localtime_r(&probe, &lt);
    const AlarmDay& a = cfg.alarms[lt.tm_wday];
    if (!a.enabled) continue;
    lt.tm_hour = a.hour; lt.tm_min = a.minute; lt.tm_sec = 0;
    lt.tm_isdst = -1;                            // let libc resolve BST/GMT
    time_t cand = mktime(&lt);
    if (cand > from) return cand;
  }
  return 0;
}

// Two-phase dawn curve. p in 0..1, outputs intensity 0..1 and CCT in K.
static void dawnCurve(float p, float& intensity, float& cctK) {
  float fullLevel = cfg.finalLevel / 100.0f;
  if (p < 0.35f) {
    float q = p / 0.35f;
    intensity = (q * q * q) * 0.10f;             // up to 10% of scale, gently
    cctK = cfg.startCctK;
  } else {
    float q = (p - 0.35f) / 0.65f;
    intensity = 0.10f + (q * q) * (fullLevel - 0.10f);
    cctK = cfg.startCctK + q * (float)(cfg.finalCctK - cfg.startCctK);
  }
  if (intensity > fullLevel) intensity = fullLevel;
}

static void applyRamp(time_t now) {
  float total = (float)(sunriseEnd - sunriseStart);
  float p = total > 0 ? (float)(now - sunriseStart) / total : 1.0f;
  if (p < 0) p = 0;
  if (p > 1) p = 1;
  float inten, cct;
  dawnCurve(p, inten, cct);
  Look l;
  l.intensity = inten;
  l.cctK = cct;
  DmxEngine::setLook(l);
}

void Scheduler::begin() {}

void Scheduler::tick() {
  static uint32_t lastMs = 0;
  if (millis() - lastMs < 1000) return;          // 1 Hz is plenty
  lastMs = millis();
  if (!timeValid()) return;

  time_t now = time(nullptr);

  switch (st) {
    case WakeState::IDLE: {
      time_t alarm = nextAlarmEpoch(now - 1);
      if (alarm && alarm != lastFiredAlarm &&
          now >= alarm - (time_t)cfg.rampMinutes * 60) {
        sunriseStart = alarm - (time_t)cfg.rampMinutes * 60;
        sunriseEnd = alarm;
        lastFiredAlarm = alarm;
        st = WakeState::SUNRISE;
      }
      break;
    }
    case WakeState::SUNRISE:
      applyRamp(now);
      if (now >= sunriseEnd) {
        holdUntil = sunriseEnd + (demoRun ? 30 : (time_t)cfg.holdMinutes * 60);
        demoRun = false;
        st = WakeState::HOLD;
      }
      break;
    case WakeState::HOLD:
      if (now >= holdUntil) {
        DmxEngine::setLook(Look{});              // off
        st = WakeState::IDLE;
      }
      break;
    case WakeState::MANUAL:
      break;                                     // portal owns the lamp
  }
}

WakeState Scheduler::state() { return st; }

long Scheduler::secondsToNextSunrise() {
  if (!timeValid()) return -1;
  time_t now = time(nullptr);
  time_t alarm = nextAlarmEpoch(now);
  if (!alarm) return -1;
  return (long)(alarm - (time_t)cfg.rampMinutes * 60 - now);
}

String Scheduler::nextAlarmText() {
  if (!timeValid()) return "clock not set";
  time_t alarm = nextAlarmEpoch(time(nullptr));
  if (!alarm) return "none";
  struct tm lt; localtime_r(&alarm, &lt);
  char buf[20];
  snprintf(buf, sizeof(buf), "%s %02d:%02d", DAYS[lt.tm_wday], lt.tm_hour, lt.tm_min);
  return String(buf);
}

void Scheduler::startDemo(uint16_t seconds) {
  time_t now = time(nullptr);
  sunriseStart = now;
  sunriseEnd = now + seconds;
  demoRun = true;
  st = WakeState::SUNRISE;
}

void Scheduler::stopAndOff() {
  DmxEngine::setLook(Look{});
  st = WakeState::IDLE;
}

void Scheduler::enterManual() { st = WakeState::MANUAL; }
