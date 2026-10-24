#include "HalFrontlight.h"

#include <Logging.h>

#if FREEINK_CAP_FRONTLIGHT
#include <driver/gpio.h>
#ifdef FREEINK_FRONTLIGHT_LS
#include <driver/ledc.h>
#endif
#endif

HalFrontlight HalFrontlight::instance;

void HalFrontlight::begin(const uint8_t brightness, const uint8_t warmth, const bool on) {
  if (!manager.present()) return;

#if FREEINK_CAP_FRONTLIGHT
  const auto& fl = BoardConfig::ACTIVE.frontlight;
  if (!fl.viaPm1Pwm && !BoardConfig::hasI2cFrontlight()) {
    // Pad holds survive the deep-sleep reset and block LEDC output until released.
    for (const int8_t pin : {fl.gpio, fl.gpioWarm}) {
      if (pin == BoardConfig::PIN_UNASSIGNED) continue;
      gpio_hold_dis(static_cast<gpio_num_t>(pin));
    }
  }
#endif
  manager.begin();
  begun = true;
  lastBrightness = brightness > 100 ? 100 : brightness;
  manager.setColorTemperature(warmth > 100 ? 100 : warmth);
  lit = on;
  manager.setBrightness(lit ? lastBrightness : 0);
  LOG_INF("LIGHT", "Frontlight up: %u%% warm=%u%% %s", lastBrightness, manager.colorTemperature(), lit ? "on" : "off");
}

void HalFrontlight::prepareForDeepSleep() {
#if FREEINK_CAP_FRONTLIGHT
  const auto& fl = BoardConfig::ACTIVE.frontlight;
  if (fl.viaPm1Pwm || BoardConfig::hasI2cFrontlight() || fl.gpio == BoardConfig::PIN_UNASSIGNED) return;

  // Zero both duties and release the SDK's RC_FAST sleep request before stopping PWM.
  manager.setBrightness(0);
  lit = false;
  const uint32_t offLevel = fl.activeHigh ? 0 : 1;
#ifdef FREEINK_FRONTLIGHT_LS
  uint8_t channel = 0;
#endif
  for (const int8_t pin : {fl.gpio, fl.gpioWarm}) {
    if (pin == BoardConfig::PIN_UNASSIGNED) continue;
    const auto g = static_cast<gpio_num_t>(pin);
    gpio_hold_dis(g);
    if (begun) {
#ifdef FREEINK_FRONTLIGHT_LS
      // The SDK's light-sleep path owns fixed low-speed channels 0 (cool) and 1 (warm).
      ledc_stop(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(channel), offLevel);
#elif ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcDetach(pin);
#else
      ledcDetachPin(pin);
#endif
    }
#ifdef FREEINK_FRONTLIGHT_LS
    ++channel;
#endif
    // Select GPIO output instead of the LEDC matrix signal, then retain it through isolation.
    pinMode(pin, OUTPUT);
    gpio_set_level(g, offLevel);
    gpio_hold_en(g);
  }
#endif
}

void HalFrontlight::setBrightness(const uint8_t percent) {
  lastBrightness = percent > 100 ? 100 : percent;
  if (lit) manager.setBrightness(lastBrightness);
}

void HalFrontlight::setWarmth(const uint8_t warmPercent) {
  manager.setColorTemperature(warmPercent > 100 ? 100 : warmPercent);
}

void HalFrontlight::setOn(const bool on) {
  if (on == lit) return;
  lit = on;
  manager.setBrightness(lit ? lastBrightness : 0);
}
