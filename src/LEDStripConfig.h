#ifndef LED_STRIP_CONFIG_H
#define LED_STRIP_CONFIG_H

#include <Arduino.h>

namespace LEDStripConfig {
  // Number of LEDs at each side of the square mirror
  const int L = 13;

  // Number of LEDs of the full strip (4 * 13 = 52)
  const int N = (4 * L);

  const int DATA_PIN PIN_SPI_MOSI;
  const int CLK_PIN PIN_SPI_SCK;

} // namespace LEDStripConfig

#endif