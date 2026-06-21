#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCHEDULE_MAX_POINTS 10
#define FX_PARAM_COUNT 6  // Candlelight needs 6 effect-setting bytes (n+3..n+8).

typedef enum {
  WP_CCT = 0,  // brightness + CCT, interpolates with neighbouring CCT points.
  WP_FX  = 1,  // fires the lamp's Mode-3 effect; holds (no interpolation).
} wp_type_t;

// Stable identifiers for each named Mode-3 effect. Order is locked because
// fx_id is persisted in NVS; new effects must be appended at the end.
typedef enum {
  FX_LIGHTNING = 0,
  FX_PAPARAZZI,
  FX_DEFECTIVE_BULB,
  FX_EXPLOSION,
  FX_WELDING,
  FX_CCT_FLASH,
  FX_HUE_FLASH,
  FX_CCT_PULSE,
  FX_HUE_PULSE,
  FX_COP_CAR,
  FX_CANDLELIGHT,
  FX_HUE_LOOP,
  FX_CCT_LOOP,
  FX_INT_LOOP,
  FX_TV_SCREEN,
  FX_FIREWORKS,
  FX_PARTY,
  FX_KIND_COUNT,
} fx_kind_t;

typedef struct {
  uint16_t minute_of_day;  // 0..1439
  uint8_t  type;           // wp_type_t
  uint8_t  brightness_pct; // 0..100 (channel n+1 maps this to 0..255)
  union {
    struct {
      uint16_t cct_k;      // 2500..10000
    } cct;
    struct {
      uint8_t kind;        // fx_kind_t
      uint8_t p[FX_PARAM_COUNT];  // raw bytes for n+3..n+8 (effect-specific)
    } fx;
  };
} waypoint_t;

typedef struct {
  bool enabled;
  uint8_t count;           // 0..SCHEDULE_MAX_POINTS
  waypoint_t points[SCHEDULE_MAX_POINTS];
} schedule_t;

// Interpolated/held output for a single moment in the schedule.
typedef struct {
  bool is_fx;               // true → use fx_* fields; false → use cct_* fields
  uint8_t brightness_byte;  // 0..255 (DMX scale, both paths)
  // CCT path
  uint16_t cct_k;
  // FX path
  uint8_t fx_effect_byte;   // value to write to channel n+2
  uint8_t fx_params[FX_PARAM_COUNT];  // values for n+3..n+8
} eval_t;

// Per-effect descriptor. Indexed by fx_kind_t. Read-only.
typedef struct {
  const char *id;       // stable string id (also used in JSON)
  uint8_t effect_byte;  // mid of the n+2 tile that activates the effect
  uint8_t param_count;  // how many of FX_PARAM_COUNT this effect actually uses
} fx_def_t;

extern const fx_def_t FX_TABLE[FX_KIND_COUNT];

// Look up by string id. Returns FX_KIND_COUNT (out of range) on miss.
fx_kind_t fx_kind_from_id(const char *id);

// Load from NVS into `out`. Returns false if no stored schedule (out is seeded
// with a sensible default in that case).
bool schedule_load(schedule_t *out);

// Validate, sort by minute_of_day, persist to NVS. Returns true on success.
bool schedule_save(schedule_t *s);

// Get a snapshot of the currently active schedule (thread-safe copy).
void schedule_get(schedule_t *out);

// Compute the lamp setpoint for a given second-of-day (0..86399). Returns true
// if inside the schedule window (first waypoint onward); false means the lamp
// should be off. CCT segments interpolate in DMX-byte/kelvin space at 1 Hz;
// FX waypoints are held step-wise until the next waypoint.
bool schedule_eval(const schedule_t *s, uint32_t sec_of_day, eval_t *out);

// Serialize/deserialize to JSON. `buf` must be big enough (~1.5 KB is plenty
// for 10 waypoints).
int  schedule_to_json(const schedule_t *s, char *buf, size_t buflen);
bool schedule_from_json(const char *json, size_t len, schedule_t *out);
