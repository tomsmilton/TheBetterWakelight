#include "schedule.h"

#include "cJSON.h"
#include "dismiss.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "sched";
static const char *NVS_NS = "wakelight";
static const char *NVS_KEY = "schedule";

static schedule_t g_cur;
static SemaphoreHandle_t g_lock;

// Effect-byte values are the centre of each 14-byte n+2 tile per the Neewer
// PL60C manual (info/command table.pdf). param_count is how many of n+3..n+8
// the effect actually consumes; trailing slots in waypoint.fx.p are unused.
const fx_def_t FX_TABLE[FX_KIND_COUNT] = {
  [FX_LIGHTNING]      = {.id = "lightning",      .effect_byte = 7,   .param_count = 3},
  [FX_PAPARAZZI]      = {.id = "paparazzi",      .effect_byte = 20,  .param_count = 4},
  [FX_DEFECTIVE_BULB] = {.id = "defective_bulb", .effect_byte = 34,  .param_count = 4},
  [FX_EXPLOSION]      = {.id = "explosion",      .effect_byte = 48,  .param_count = 5},
  [FX_WELDING]        = {.id = "welding",        .effect_byte = 62,  .param_count = 5},
  [FX_CCT_FLASH]      = {.id = "cct_flash",      .effect_byte = 76,  .param_count = 4},
  [FX_HUE_FLASH]      = {.id = "hue_flash",      .effect_byte = 90,  .param_count = 4},
  [FX_CCT_PULSE]      = {.id = "cct_pulse",      .effect_byte = 104, .param_count = 4},
  [FX_HUE_PULSE]      = {.id = "hue_pulse",      .effect_byte = 118, .param_count = 4},
  [FX_COP_CAR]        = {.id = "cop_car",        .effect_byte = 132, .param_count = 3},
  [FX_CANDLELIGHT]    = {.id = "candlelight",    .effect_byte = 146, .param_count = 6},
  [FX_HUE_LOOP]       = {.id = "hue_loop",       .effect_byte = 160, .param_count = 4},
  [FX_CCT_LOOP]       = {.id = "cct_loop",       .effect_byte = 174, .param_count = 4},
  [FX_INT_LOOP]       = {.id = "int_loop",       .effect_byte = 188, .param_count = 5},
  [FX_TV_SCREEN]      = {.id = "tv_screen",      .effect_byte = 202, .param_count = 5},
  [FX_FIREWORKS]      = {.id = "fireworks",      .effect_byte = 216, .param_count = 4},
  [FX_PARTY]          = {.id = "party",          .effect_byte = 240, .param_count = 3},
};

fx_kind_t fx_kind_from_id(const char *id) {
  if (!id) return FX_KIND_COUNT;
  for (int i = 0; i < FX_KIND_COUNT; i++) {
    if (strcmp(FX_TABLE[i].id, id) == 0) return (fx_kind_t)i;
  }
  return FX_KIND_COUNT;
}

static void default_schedule(schedule_t *s) {
  memset(s, 0, sizeof(*s));
  s->enabled = false;
  s->count = 5;
  s->points[0] = (waypoint_t){.minute_of_day = 6 * 60 + 30, .type = WP_CCT, .brightness_pct = 0,   .cct = {.cct_k = 2500}};
  s->points[1] = (waypoint_t){.minute_of_day = 6 * 60 + 40, .type = WP_CCT, .brightness_pct = 10,  .cct = {.cct_k = 3000}};
  s->points[2] = (waypoint_t){.minute_of_day = 6 * 60 + 50, .type = WP_CCT, .brightness_pct = 30,  .cct = {.cct_k = 3000}};
  s->points[3] = (waypoint_t){.minute_of_day = 6 * 60 + 55, .type = WP_CCT, .brightness_pct = 50,  .cct = {.cct_k = 3000}};
  s->points[4] = (waypoint_t){.minute_of_day = 7 * 60 + 0,  .type = WP_CCT, .brightness_pct = 100, .cct = {.cct_k = 3000}};
}

static void sort_points(schedule_t *s) {
  for (int i = 1; i < s->count; i++) {
    waypoint_t w = s->points[i];
    int j = i - 1;
    while (j >= 0 && s->points[j].minute_of_day > w.minute_of_day) {
      s->points[j + 1] = s->points[j];
      j--;
    }
    s->points[j + 1] = w;
  }
}

static void clamp_point(waypoint_t *w) {
  if (w->minute_of_day > 1439) w->minute_of_day = 1439;
  if (w->brightness_pct > 100) w->brightness_pct = 100;
  if (w->type == WP_CCT) {
    if (w->cct.cct_k < 2500) w->cct.cct_k = 2500;
    if (w->cct.cct_k > 10000) w->cct.cct_k = 10000;
  } else if (w->type == WP_FX) {
    if (w->fx.kind >= FX_KIND_COUNT) w->fx.kind = 0;
    // Param bytes are raw 0-255; nothing to clamp at this layer (the fixture
    // has its own per-effect "Reserved" sub-ranges that we don't enforce).
  } else {
    // Unknown type → coerce to safe CCT default.
    w->type = WP_CCT;
    w->cct.cct_k = 2700;
  }
}

bool schedule_load(schedule_t *out) {
  if (!g_lock) g_lock = xSemaphoreCreateMutex();

  nvs_handle_t h;
  if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
    default_schedule(out);
    xSemaphoreTake(g_lock, portMAX_DELAY);
    g_cur = *out;
    xSemaphoreGive(g_lock);
    return false;
  }
  size_t len = 0;
  esp_err_t err = nvs_get_blob(h, NVS_KEY, NULL, &len);
  if (err != ESP_OK || len == 0 || len > 2048) {
    nvs_close(h);
    default_schedule(out);
    xSemaphoreTake(g_lock, portMAX_DELAY);
    g_cur = *out;
    xSemaphoreGive(g_lock);
    return false;
  }
  char *buf = malloc(len + 1);
  if (!buf) { nvs_close(h); default_schedule(out); return false; }
  nvs_get_blob(h, NVS_KEY, buf, &len);
  buf[len] = 0;
  nvs_close(h);

  bool ok = schedule_from_json(buf, len, out);
  free(buf);
  if (!ok) {
    ESP_LOGW(TAG, "stored schedule invalid; using default");
    default_schedule(out);
  }
  xSemaphoreTake(g_lock, portMAX_DELAY);
  g_cur = *out;
  xSemaphoreGive(g_lock);
  return ok;
}

bool schedule_save(schedule_t *s) {
  if (!g_lock) g_lock = xSemaphoreCreateMutex();
  if (s->count > SCHEDULE_MAX_POINTS) return false;
  for (int i = 0; i < s->count; i++) clamp_point(&s->points[i]);
  sort_points(s);

  char buf[1536];
  int n = schedule_to_json(s, buf, sizeof(buf));
  if (n <= 0) return false;

  nvs_handle_t h;
  if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
  esp_err_t err = nvs_set_blob(h, NVS_KEY, buf, n);
  if (err == ESP_OK) err = nvs_commit(h);
  nvs_close(h);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs save failed: %d", err);
    return false;
  }

  xSemaphoreTake(g_lock, portMAX_DELAY);
  g_cur = *s;
  xSemaphoreGive(g_lock);

  // If the new schedule's first waypoint is later today, drop any "done for
  // today" — saving a forward-looking schedule is the user's signal they
  // want it to fire.
  if (s->enabled && s->count > 0) {
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    uint16_t mod_now = lt.tm_hour * 60 + lt.tm_min;
    if (s->points[0].minute_of_day > mod_now) dismiss_reset();
  }
  return true;
}

void schedule_get(schedule_t *out) {
  if (!g_lock) g_lock = xSemaphoreCreateMutex();
  xSemaphoreTake(g_lock, portMAX_DELAY);
  *out = g_cur;
  xSemaphoreGive(g_lock);
}

// Interpolation is done in DMX-byte space (0-255) for brightness instead of
// whole percents, so each step through the ramp is ~0.4% instead of the
// 1%-that-maps-to-2-3-DMX-units jumps you get when interpolating in percent.
static uint16_t pct_to_b255(uint8_t pct) {
  if (pct >= 100) return 255;
  return (uint16_t)((uint32_t)pct * 255 / 100);
}

// Render a single waypoint into eval_t (no interpolation). Used when we want
// to "hold" a waypoint between segments where interpolation isn't valid
// (FX → anything, anything → FX, past last waypoint).
static void emit_waypoint(const waypoint_t *w, eval_t *out) {
  out->brightness_byte = (uint8_t)pct_to_b255(w->brightness_pct);
  if (w->type == WP_FX) {
    out->is_fx = true;
    const fx_def_t *def = &FX_TABLE[w->fx.kind < FX_KIND_COUNT ? w->fx.kind : 0];
    out->fx_effect_byte = def->effect_byte;
    memcpy(out->fx_params, w->fx.p, FX_PARAM_COUNT);
    out->cct_k = 2700;  // unused, keep deterministic
  } else {
    out->is_fx = false;
    out->cct_k = w->cct.cct_k;
    out->fx_effect_byte = 0;
    memset(out->fx_params, 0, FX_PARAM_COUNT);
  }
}

bool schedule_eval(const schedule_t *s, uint32_t sod, eval_t *out) {
  memset(out, 0, sizeof(*out));
  out->cct_k = 2700;
  if (!s->enabled || s->count == 0) return false;
  uint32_t first_sod = (uint32_t)s->points[0].minute_of_day * 60;
  uint32_t last_sod  = (uint32_t)s->points[s->count - 1].minute_of_day * 60;
  if (sod < first_sod) return false;
  if (sod >= last_sod) {
    emit_waypoint(&s->points[s->count - 1], out);
    return true;
  }
  for (int i = 0; i + 1 < s->count; i++) {
    const waypoint_t *a = &s->points[i];
    const waypoint_t *b = &s->points[i + 1];
    uint32_t a_sod = (uint32_t)a->minute_of_day * 60;
    uint32_t b_sod = (uint32_t)b->minute_of_day * 60;
    if (sod < a_sod || sod >= b_sod) continue;

    // Only CCT→CCT segments interpolate; everything else holds the start
    // waypoint's state until we hit b's time.
    if (a->type == WP_CCT && b->type == WP_CCT) {
      uint32_t span = b_sod - a_sod;
      uint32_t pos = sod - a_sod;
      int32_t ab = pct_to_b255(a->brightness_pct);
      int32_t bb = pct_to_b255(b->brightness_pct);
      int32_t dc = (int32_t)b->cct.cct_k - (int32_t)a->cct.cct_k;
      out->is_fx = false;
      out->brightness_byte = (uint8_t)(ab + ((bb - ab) * (int32_t)pos) / (int32_t)span);
      out->cct_k = (uint16_t)(a->cct.cct_k + (dc * (int32_t)pos) / (int32_t)span);
    } else {
      emit_waypoint(a, out);
    }
    return true;
  }
  return false;
}

int schedule_to_json(const schedule_t *s, char *buf, size_t buflen) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddBoolToObject(root, "enabled", s->enabled);
  cJSON *arr = cJSON_AddArrayToObject(root, "points");
  for (int i = 0; i < s->count; i++) {
    const waypoint_t *w = &s->points[i];
    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "m", w->minute_of_day);
    cJSON_AddNumberToObject(p, "b", w->brightness_pct);
    if (w->type == WP_FX) {
      cJSON_AddStringToObject(p, "t", "fx");
      const fx_def_t *def = &FX_TABLE[w->fx.kind < FX_KIND_COUNT ? w->fx.kind : 0];
      cJSON_AddStringToObject(p, "fx", def->id);
      cJSON *pa = cJSON_AddArrayToObject(p, "p");
      for (int j = 0; j < FX_PARAM_COUNT; j++) {
        cJSON_AddItemToArray(pa, cJSON_CreateNumber(w->fx.p[j]));
      }
    } else {
      // CCT is the default; omit "t" for compactness + back-compat with old
      // clients that don't know about types.
      cJSON_AddNumberToObject(p, "k", w->cct.cct_k);
    }
    cJSON_AddItemToArray(arr, p);
  }
  bool ok = cJSON_PrintPreallocated(root, buf, buflen, 0);
  cJSON_Delete(root);
  return ok ? (int)strlen(buf) : -1;
}

bool schedule_from_json(const char *json, size_t len, schedule_t *out) {
  (void)len;
  cJSON *root = cJSON_Parse(json);
  if (!root) return false;
  memset(out, 0, sizeof(*out));
  cJSON *en = cJSON_GetObjectItem(root, "enabled");
  out->enabled = cJSON_IsTrue(en);
  cJSON *arr = cJSON_GetObjectItem(root, "points");
  bool ok = cJSON_IsArray(arr);
  if (ok) {
    int n = cJSON_GetArraySize(arr);
    if (n > SCHEDULE_MAX_POINTS) n = SCHEDULE_MAX_POINTS;
    out->count = n;
    for (int i = 0; i < n; i++) {
      cJSON *p = cJSON_GetArrayItem(arr, i);
      cJSON *m = cJSON_GetObjectItem(p, "m");
      cJSON *b = cJSON_GetObjectItem(p, "b");
      cJSON *t = cJSON_GetObjectItem(p, "t");
      if (!cJSON_IsNumber(m) || !cJSON_IsNumber(b)) { ok = false; break; }
      waypoint_t *w = &out->points[i];
      w->minute_of_day = (uint16_t)m->valueint;
      w->brightness_pct = (uint8_t)b->valueint;

      bool is_fx = cJSON_IsString(t) && strcmp(t->valuestring, "fx") == 0;
      if (is_fx) {
        cJSON *fx = cJSON_GetObjectItem(p, "fx");
        cJSON *pa = cJSON_GetObjectItem(p, "p");
        if (!cJSON_IsString(fx)) { ok = false; break; }
        fx_kind_t k = fx_kind_from_id(fx->valuestring);
        if (k >= FX_KIND_COUNT) { ok = false; break; }
        w->type = WP_FX;
        w->fx.kind = (uint8_t)k;
        memset(w->fx.p, 0, FX_PARAM_COUNT);
        if (cJSON_IsArray(pa)) {
          int pn = cJSON_GetArraySize(pa);
          if (pn > FX_PARAM_COUNT) pn = FX_PARAM_COUNT;
          for (int j = 0; j < pn; j++) {
            cJSON *pv = cJSON_GetArrayItem(pa, j);
            if (cJSON_IsNumber(pv)) {
              int v = pv->valueint;
              if (v < 0) v = 0;
              if (v > 255) v = 255;
              w->fx.p[j] = (uint8_t)v;
            }
          }
        }
      } else {
        cJSON *k = cJSON_GetObjectItem(p, "k");
        if (!cJSON_IsNumber(k)) { ok = false; break; }
        w->type = WP_CCT;
        w->cct.cct_k = (uint16_t)k->valueint;
      }
      clamp_point(w);
    }
    if (ok) sort_points(out);
  }
  cJSON_Delete(root);
  return ok;
}
