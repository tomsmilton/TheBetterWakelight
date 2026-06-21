// Enable-line probe  (env:enprobe — diagnostic, not production)
//
// Hypothesis: on this transmit-only board the DE//RE enable pin (GPIO21) must
// be held HIGH continuously. Letting esp_dmx own it as the "RTS" pin leaves the
// transceiver un-enabled during TX (self-check saw GPIO21 HIGH on 0/54638
// samples; the lamp "opens DMX then vanishes").
//
// This probe takes the enable pin AWAY from esp_dmx (rts = -1) and forces it
// HIGH as a plain GPIO, then sends a hard ON/OFF blink to the PL60C at DMX
// address 1, CCT mode. If the lamp now blinks, the fix is confirmed.
#include <Arduino.h>
#include <esp_dmx.h>

static const dmx_port_t PORT = DMX_NUM_1;
static const int TX_PIN = 17;
static const int EN_PIN = 21;

static uint8_t universe[DMX_PACKET_SIZE] = {0};   // slot 0 = start code 0x00

// PL60C @ address 1, CCT mode: slot1=mode(16), slot2=brightness, slot3=CCT,
// slot4=G/M tint(128 neutral).
static void setBrightness(uint8_t b) {
  universe[1] = 16;     // CCT mode select
  universe[2] = b;      // brightness
  universe[3] = 180;    // ~6500K-ish
  universe[4] = 128;    // neutral tint
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\nWakeLight enable-line probe");
  Serial.println("esp_dmx RTS = DISABLED; GPIO21 forced HIGH by software.");

  dmx_config_t config = DMX_CONFIG_DEFAULT;
  dmx_driver_install(PORT, &config, nullptr, 0);
  dmx_set_pin(PORT, TX_PIN, -1, -1);          // TX only, NO rts/enable pin

  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);                  // drive-enable: permanently ON
  Serial.printf("GPIO%d held HIGH (level reads %d)\n",
                EN_PIN, digitalRead(EN_PIN));
  Serial.println("Blinking lamp full <-> off every ~1.4 s. Reset to stop.");
}

void loop() {
  static bool on = false;
  on = !on;
  setBrightness(on ? 255 : 0);
  dmx_write(PORT, universe, DMX_PACKET_SIZE);
  dmx_send(PORT);
  dmx_wait_sent(PORT, DMX_TIMEOUT_TICK);
  Serial.printf("lamp = %s\n", on ? "FULL" : "off");

  // keep refreshing during the dwell so the stream never drops
  for (int i = 0; i < 56; i++) {
    dmx_write(PORT, universe, DMX_PACKET_SIZE);
    dmx_send(PORT);
    dmx_wait_sent(PORT, DMX_TIMEOUT_TICK);
    delay(2);
  }
}
