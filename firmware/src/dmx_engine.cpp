#include "dmx_engine.h"
#include "config.h"
#include <esp_dmx.h>

static const dmx_port_t DMX_PORT = DMX_NUM_1;

static uint8_t  universe[DMX_PACKET_SIZE] = {0};   // slot 0 = start code 0x00
static Look     look;
static portMUX_TYPE lookMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t txCount = 0;
static volatile bool paused = false;

static uint8_t clamp255(float v) {
  if (v <= 0) return 0;
  if (v >= 255) return 255;
  return (uint8_t)lroundf(v);
}

// PL60C mode-select values (official channel table: n value picks the mode).
static const uint8_t PL60C_MODE_CCT = 16;   // 0-31  -> CCT mode
static const uint8_t PL60C_MODE_HSI = 48;   // 32-63 -> HSI mode

// Map the abstract Look onto DMX slots according to the configured profile.
static void renderLook(const Look& l) {
  uint16_t base = cfg.dmxAddress;            // 1-based slot of first channel
  auto set = [&](uint8_t offset, uint8_t val) {
    if (offset == 255) return;
    uint16_t slot = base + offset;           // universe[] index == slot number
    if (slot >= 1 && slot <= 512) universe[slot] = val;
  };

  uint8_t dim = clamp255(l.intensity * 255.0f);
  float span = (float)(cfg.cctMaxK - cfg.cctMinK);
  float cctNorm = span > 0 ? ((l.cctK - cfg.cctMinK) / span) : 0;
  uint8_t cct = clamp255(cctNorm * 255.0f);
  uint8_t hue = clamp255(l.hue / 360.0f * 255.0f);
  uint8_t sat = clamp255(l.sat * 255.0f);

  switch (cfg.fixtureMode) {
    case MODE_PL60C:
      // n: mode select, n+1: brightness, then per-mode sub-map.
      if (l.useHsi) {
        set(0, PL60C_MODE_HSI); set(1, dim); set(2, hue); set(3, sat);
      } else {
        set(0, PL60C_MODE_CCT); set(1, dim); set(2, cct);
        set(3, 128);                          // G/M tint: 128 = neutral
      }
      break;
    case MODE_GENERIC_CCT:
      set(0, dim); set(1, cct);
      break;
    case MODE_CUSTOM:
      set(cfg.chDimmer, dim);
      set(cfg.chCct, cct);
      if (l.useHsi) { set(cfg.chHue, hue); set(cfg.chSat, sat); }
      break;
  }
}

static void dmxTask(void*) {
  for (;;) {
    if (paused) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
    Look l;
    taskENTER_CRITICAL(&lookMux);
    l = look;
    taskEXIT_CRITICAL(&lookMux);
    renderLook(l);

    dmx_write(DMX_PORT, universe, DMX_PACKET_SIZE);
    dmx_send(DMX_PORT);
    dmx_wait_sent(DMX_PORT, DMX_TIMEOUT_TICK);
    txCount = txCount + 1;
    vTaskDelay(pdMS_TO_TICKS(25));                      // ~40 Hz refresh
  }
}

void DmxEngine::begin() {
  dmx_config_t config = DMX_CONFIG_DEFAULT;
  dmx_driver_install(DMX_PORT, &config, nullptr, 0);
  dmx_set_pin(DMX_PORT, DMX_TX_PIN, DMX_RX_PIN, DMX_EN_PIN);
  xTaskCreatePinnedToCore(dmxTask, "dmx_tx", 4096, nullptr, 5, nullptr, 1);
}

void DmxEngine::setLook(const Look& l) {
  taskENTER_CRITICAL(&lookMux);
  look = l;
  taskEXIT_CRITICAL(&lookMux);
}

Look DmxEngine::currentLook() {
  taskENTER_CRITICAL(&lookMux);
  Look l = look;
  taskEXIT_CRITICAL(&lookMux);
  return l;
}

void DmxEngine::setRaw(uint16_t ch, uint8_t value) {
  if (ch >= 1 && ch <= 512) universe[ch] = value;
}

uint32_t DmxEngine::packetsSent() { return txCount; }

void DmxEngine::pause() {
  paused = true;
  dmx_wait_sent(DMX_PORT, pdMS_TO_TICKS(50));   // let in-flight packet finish
  dmx_driver_disable(DMX_PORT);
}

void DmxEngine::resume() {
  dmx_driver_enable(DMX_PORT);
  paused = false;
}
