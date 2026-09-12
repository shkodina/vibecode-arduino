// Ранний старт отдельно от .ino: Arduino preprocessor иначе ломает прототипы.
#include "config.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/gpio.h"

static void disableBrownoutDetector() {
  CLEAR_PERI_REG_MASK(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_ENA);
#ifdef RTC_CNTL_BROWN_OUT_RST_ENA
  CLEAR_PERI_REG_MASK(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_RST_ENA);
#endif
}

static void holdMosfetOff() {
  gpio_hold_dis(static_cast<gpio_num_t>(LED_PWM_PIN));
  gpio_reset_pin(static_cast<gpio_num_t>(LED_PWM_PIN));
  gpio_set_direction(static_cast<gpio_num_t>(LED_PWM_PIN), GPIO_MODE_OUTPUT);
  gpio_set_level(static_cast<gpio_num_t>(LED_PWM_PIN), 0);
}

struct EarlyBrownoutOff {
  EarlyBrownoutOff() {
    disableBrownoutDetector();
  }
} earlyBrownoutOff;

extern "C" void initVariant() {
  disableBrownoutDetector();
  holdMosfetOff();
}
