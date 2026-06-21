#include "device_id.h"
#include "dismiss.h"
#include "dmx_out.h"
#include "http_ui.h"
#include "override.h"
#include "schedule.h"
#include "wifi_sntp.h"

#define OVERRIDE_ON_BYTE 255  // full brightness
#define OVERRIDE_ON_CCT 4000

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <time.h>

static const char *TAG = "wakelight";

static void ramp_task(void *arg) {
  (void)arg;
  const TickType_t period = pdMS_TO_TICKS(1000);
  TickType_t tick = xTaskGetTickCount();
  while (1) {
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    uint32_t sod = (uint32_t)lt.tm_hour * 3600 + (uint32_t)lt.tm_min * 60 + (uint32_t)lt.tm_sec;

    override_mode_t ov = override_get();
    if (ov == OVERRIDE_ON) {
      dmx_out_set(OVERRIDE_ON_BYTE, OVERRIDE_ON_CCT, 128);
    } else if (ov == OVERRIDE_OFF) {
      dmx_out_set(0, 2700, 128);
    } else if (ov == OVERRIDE_MANUAL) {
      uint8_t mpct = 0;
      uint16_t mk = 2700;
      uint8_t mgm = 128;
      override_get_manual(&mpct, &mk, &mgm);
      uint8_t mb = (mpct >= 100) ? 255 : (uint8_t)((uint32_t)mpct * 255 / 100);
      dmx_out_set(mb, mk, mgm);
    } else {
      // AUTO: follow schedule unless the user dismissed for today.
      if (dismiss_is_active()) {
        dmx_out_set(0, 2700, 128);
      } else {
        schedule_t s;
        schedule_get(&s);
        eval_t e;
        bool active = schedule_eval(&s, sod, &e);
        if (!active) {
          dmx_out_set(0, 2700, 128);
        } else if (e.is_fx) {
          dmx_out_set_fx(e.brightness_byte, e.fx_effect_byte, e.fx_params);
        } else {
          dmx_out_set(e.brightness_byte, e.cct_k, 128);
        }
      }
    }
    vTaskDelayUntil(&tick, period);
  }
}

void app_main(void) {
  ESP_LOGI(TAG, "wakelight booting");

  dmx_out_start();
  dmx_out_set(0, 2700, 128);

  schedule_t s;
  schedule_load(&s);
  dismiss_init();
  device_id_init();

  if (!wifi_sntp_start()) {
    ESP_LOGE(TAG, "wifi failed; continuing without network");
  } else {
    http_ui_start();
  }

  xTaskCreate(ramp_task, "ramp", 4096, NULL, 4, NULL);
}
