#include "dmx_out.h"

#include "esp_dmx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#define TX_PIN 17
#define RX_PIN 16
#define EN_PIN 4

// Neewer PL60C, DMX address 1.
//
// Mode 1 (CCT): slots [mode, brightness, CCT, G/M].
//   Mode byte = 0 selects the only sub-mode (CCT) within the fixture's Mode 1.
//
// Mode 3 (FX): slots [mode, brightness, effect_select, p1..p6].
//   Mode byte 56-63 (we use 60) puts the fixture into FX-mode interpretation,
//   effect_select picks one of the 17 named effects, p1..p6 are the per-effect
//   settings. To use FX waypoints the fixture must be physically set to Mode 3
//   on its menu — Mode 1 (4-channel CCT) ignores the higher slots.
#define FIXTURE_ADDR 1
#define FX_MODE_BYTE 60

static const char *TAG = "dmx";
static const dmx_port_t kDmxPort = DMX_NUM_1;
static uint8_t g_frame[DMX_PACKET_SIZE];

// Cross-core spinlock guarding g_state. The DMX sender runs on core 1 prio 10
// and reads every 30 ms; ramp_task on core 0 prio 4 writes ~1 Hz; live WS may
// also write. portMUX is the standard ESP-IDF idiom for short critical
// sections that need cross-core safety, since taskENTER_CRITICAL with a
// FreeRTOS mutex doesn't sync between cores.
static portMUX_TYPE g_state_mux = portMUX_INITIALIZER_UNLOCKED;
static struct {
  uint8_t is_fx;
  uint8_t force_off;
  uint8_t brightness;     // 0-255
  // CCT path
  uint8_t cct_byte;       // 0-255 (already mapped from kelvin)
  uint8_t gm_byte;        // 0-255, 128 neutral
  // FX path
  uint8_t fx_effect;      // value for slot 3 (n+2)
  uint8_t fx_p[DMX_FX_PARAM_COUNT];  // slots 4..9 (n+3..n+8)
} g_state = {.gm_byte = 128};  // neutral G/M default

static uint8_t cct_to_byte(uint16_t cct_k) {
  if (cct_k <= 2500) return 0;
  if (cct_k >= 10000) return 255;
  return (uint16_t)(((uint32_t)(cct_k - 2500) * 255) / 7500);
}

static void sender_task(void *arg) {
  (void)arg;
  const TickType_t period = pdMS_TO_TICKS(30);
  TickType_t tick = xTaskGetTickCount();
  while (1) {
    // Snapshot under the lock so the lamp sees a coherent frame, then write
    // the slots outside the critical section to keep it as short as possible.
    bool is_fx, force_off;
    uint8_t bright, cct, gm, fx_effect;
    uint8_t fx_p[DMX_FX_PARAM_COUNT];
    portENTER_CRITICAL(&g_state_mux);
    is_fx = g_state.is_fx;
    force_off = g_state.force_off;
    bright = g_state.brightness;
    cct = g_state.cct_byte;
    gm = g_state.gm_byte;
    fx_effect = g_state.fx_effect;
    memcpy(fx_p, g_state.fx_p, DMX_FX_PARAM_COUNT);
    portEXIT_CRITICAL(&g_state_mux);

    uint8_t *f = &g_frame[FIXTURE_ADDR];
    if (is_fx) {
      f[0] = FX_MODE_BYTE;
      f[1] = force_off ? 0 : bright;
      f[2] = fx_effect;
      memcpy(&f[3], fx_p, DMX_FX_PARAM_COUNT);
    } else {
      f[0] = 0;
      f[1] = force_off ? 0 : bright;
      f[2] = cct;
      f[3] = gm;
      // Zero the FX-only slots so a stale Mode-3 frame doesn't bleed through
      // when the lamp is in 4-channel mode and we revert to CCT.
      memset(&f[4], 0, DMX_FX_PARAM_COUNT - 1);
    }

    dmx_write(kDmxPort, g_frame, DMX_PACKET_SIZE);
    dmx_send_num(kDmxPort, DMX_PACKET_SIZE);
    dmx_wait_sent(kDmxPort, DMX_TIMEOUT_TICK);
    vTaskDelayUntil(&tick, period);
  }
}

void dmx_out_start(void) {
  dmx_config_t config = DMX_CONFIG_DEFAULT;
  if (!dmx_driver_install(kDmxPort, &config, NULL, 0)) {
    ESP_LOGE(TAG, "driver install failed");
    return;
  }
  dmx_set_pin(kDmxPort, TX_PIN, RX_PIN, EN_PIN);
  memset(g_frame, 0, sizeof(g_frame));
  // Priority 10: above ramp_task (4) and plenty of headroom, still below
  // esp-idf's critical housekeeping tasks.
  xTaskCreatePinnedToCore(sender_task, "dmx_tx", 3072, NULL, 10, NULL, 1);
  ESP_LOGI(TAG, "started on UART%d", kDmxPort);
}

void dmx_out_set(uint8_t brightness_byte, uint16_t cct_k, uint8_t gm_byte) {
  portENTER_CRITICAL(&g_state_mux);
  g_state.is_fx = 0;
  g_state.force_off = 0;
  g_state.brightness = brightness_byte;
  g_state.cct_byte = cct_to_byte(cct_k);
  g_state.gm_byte = gm_byte;
  portEXIT_CRITICAL(&g_state_mux);
}

void dmx_out_set_fx(uint8_t brightness_byte, uint8_t effect_byte,
                    const uint8_t params[DMX_FX_PARAM_COUNT]) {
  portENTER_CRITICAL(&g_state_mux);
  g_state.is_fx = 1;
  g_state.force_off = 0;
  g_state.brightness = brightness_byte;
  g_state.fx_effect = effect_byte;
  memcpy(g_state.fx_p, params, DMX_FX_PARAM_COUNT);
  portEXIT_CRITICAL(&g_state_mux);
}

void dmx_out_off(void) {
  portENTER_CRITICAL(&g_state_mux);
  g_state.force_off = 1;
  portEXIT_CRITICAL(&g_state_mux);
}
