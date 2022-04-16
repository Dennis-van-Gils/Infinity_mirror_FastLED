#ifndef DVG_FASTLED_CONFIG_H
#define DVG_FASTLED_CONFIG_H

#include <Arduino.h>

namespace FLC {
  /* Infinity mirror with 4 equal sides of length `L`.

            L
       ┌────<────┐
       │         │
    L  v         ^  L
       │         │
       0────>────┘
            L
  */

  const int L = 13;    // Number of LEDs at each side of the square mirror
  const int N = 4 * L; // Number of LEDs of the full strip (= 4 * L)

  const int PIN_DATA PIN_SPI_MOSI;
  const int PIN_CLK PIN_SPI_SCK;

  const ESPIChipsets LED_TYPE = APA102;
  const EOrder COLOR_ORDER = BGR;
  const LEDColorCorrection COLOR_CORRECTION = TypicalSMD5050;

  const uint16_t MAX_REFRESH_RATE = 250; // FPS
} // namespace FLC

#endif