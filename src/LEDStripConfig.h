#ifndef LED_STRIP_CONFIG_H
#define LED_STRIP_CONFIG_H

#include <Arduino.h>

namespace LEDStripConfig {
  /* Infinity mirror, square with equal sides

         L
     ----------
     |        |
   L |        | L
     |        |
     ----------
         L

  */

  const int L = 13;    // Number of LEDs at each side of the square mirror
  const int N = 4 * L; // Number of LEDs of the full strip (= 4 * L)

  const int PIN_DATA PIN_SPI_MOSI;
  const int PIN_CLK PIN_SPI_SCK;

  const ESPIChipsets LED_TYPE = APA102;
  const EOrder COLOR_ORDER = BGR;
  const LEDColorCorrection COLOR_CORRECTION = TypicalSMD5050;

  const uint8_t BRIGHTNESS = 64;
  const uint16_t DELAY = round(1000 / 120); // 1000 / FPS, FPS = 120
} // namespace LEDStripConfig

#endif