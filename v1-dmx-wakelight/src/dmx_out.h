#pragma once

#include <stdint.h>

#define DMX_FX_PARAM_COUNT 6  // matches schedule.h's FX_PARAM_COUNT (n+3..n+8)

// Start the DMX driver + continuous sender task.
void dmx_out_start(void);

// CCT path. brightness_byte: 0-255 (DMX scale); cct_k: 2500-10000;
// gm_byte: 0-255 (128 = neutral, 0 = full green, 255 = full magenta).
// Writes [mode=0, b, cct, gm] to slots 1-4. Thread-safe.
void dmx_out_set(uint8_t brightness_byte, uint16_t cct_k, uint8_t gm_byte);

// FX path. Writes [mode=60 (Mode-3 select), b, effect_byte, p[0..5]] to
// slots 1-9. Lamp must be physically in Mode 3 for FX bytes to apply.
// Thread-safe.
void dmx_out_set_fx(uint8_t brightness_byte, uint8_t effect_byte,
                    const uint8_t params[DMX_FX_PARAM_COUNT]);

// Force the fixture dark without touching the last desired setpoint.
void dmx_out_off(void);
