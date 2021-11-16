/* DvG_LEDStripSegmentor.h

Dennis van Gils
16-11-2021
*/
#ifndef DVG_FASTLED_EFFECTS_H
#define DVG_FASTLED_EFFECTS_H

#include <Arduino.h>

#include "DvG_ECG_simulation.h"
#include "DvG_FastLED_config.h"
#include "FastLED.h"

static uint16_t idx; // LED position index used in many for-loops
static uint32_t now; // To store `millis()` value used in many functions
uint8_t iHue = 0;    // Rotating hue used by many of the effects

/*-----------------------------------------------------------------------------
  ECG heart beat
------------------------------------------------------------------------------*/
#define ECG_N_SMP 100 // 100

namespace ECG {
  static float wave[ECG_N_SMP] = {0};
  static uint32_t tick = 0;
  uint16_t idx = 0;        // Not static, because can also be set from main()
  uint32_t idx_delay = 20; // Not static, because can also be set from main()
} // namespace ECG

void generate_heart_beat() {
  // Generate ECG wave data, output range [0 - 1]
  generate_ECG(ECG::wave, ECG_N_SMP);
}

void heart_beat(struct CRGB(*leds), uint16_t s) {
  uint8_t iTry = 4;

  switch (iTry) {
    case 1:
      // Nice with segment style 5
      fadeToBlackBy(leds, s, 8);
      // idx = ECG::idx * s / (ECG_N_SMP - 1);
      idx = round((1 - ECG::wave[ECG::idx]) * (s - 1));
      leds[idx] += CHSV(HUE_RED, 255, uint8_t(ECG::wave[ECG::idx] * 200));
      break;

    case 2:
      fadeToBlackBy(leds, s, 8);
      for (idx = 0; idx < s; idx++) {
        uint8_t intens = round(ECG::wave[ECG::idx] * 100);
        leds[idx] += CHSV(HUE_RED, 255, 0 + intens);
      }
      break;

    case 3:
      fadeToBlackBy(leds, s, 8);
      for (idx = 0; idx < s; idx++) {
        uint8_t intens = round(ECG::wave[ECG::idx] * 100);
        if (intens > 30) {
          leds[idx] += CHSV(HUE_RED, 255, intens);
        }
      }
      break;

    case 4:
      fadeToBlackBy(leds, s, 8);
      for (idx = 0; idx < s; idx++) {
        uint8_t intens = round(ECG::wave[ECG::idx] * 100);
        if (intens > 15) {
          leds[idx] += CHSV(HUE_RED, 255, intens);
        }
      }
      break;
  }

  now = millis();
  if (now - ECG::tick > ECG::idx_delay) {
    ECG::idx = (ECG::idx + 1) % ECG_N_SMP;
    ECG::tick = now;
  }
}

/*-----------------------------------------------------------------------------
  FastLED effects
------------------------------------------------------------------------------*/

void rainbow(struct CRGB(*leds), uint16_t s) {
  // FastLED's built-in rainbow generator
  fill_rainbow(leds, s, iHue, 255 / (s - 1));
}

void sinelon(struct CRGB(*leds), uint16_t s) {
  // A colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, s, 4);
  idx = beatsin16(13, 0, s);
  leds[idx] += CHSV(iHue, 255, 255); // iHue, 255, 192
}

void bpm(struct CRGB(*leds), uint16_t s) {
  // Colored stripes pulsing at a defined beats-per-minute
  CRGBPalette16 palette = PartyColors_p; // RainbowColors_p; // PartyColors_p;
  uint8_t bpm_ = 30;
  uint8_t beat = beatsin8(bpm_, 64, 255);
  for (idx = 0; idx < s; idx++) {
    leds[idx] = ColorFromPalette(palette, iHue + 128. / (s - 1) * idx,
                                 beat + 127. / (s - 1) * idx);
  }
}

void juggle(struct CRGB(*leds), uint16_t s) {
  // 8 colored dots, weaving in and out of sync with each other
  byte dothue = 0;
  fadeToBlackBy(leds, s, 20);
  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, s - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void full_white(struct CRGB(*leds), uint16_t s) {
  fill_solid(leds, s, CRGB::White);
}

void strobe(struct CRGB(*leds), uint16_t s) {
  if (iHue % 16) {
    fill_solid(leds, s, CRGB::Black);
  } else {
    CRGBPalette16 palette = PartyColors_p; // PartyColors_p;
    for (idx = 0; idx < s; idx++) {        // 9948
      leds[idx] =
          ColorFromPalette(palette, iHue + (idx * 2), iHue + (idx * 10));
    }
    // fill_solid(leds, s, CRGB::White);
  }
}

void dennis(struct CRGB(*leds), uint16_t s) {
  fadeToBlackBy(leds, s, 16);
  idx = beatsin16(15, 0, s - 1);
  // leds[idx] = CRGB::Blue;
  // leds[s - idx - 1] = CRGB::Blue;
  leds[idx] = CRGB::Red;
  leds[s - idx - 1] = CRGB::OrangeRed;
}

void test_pattern(struct CRGB(*leds), uint16_t s) {
  for (idx = 0; idx < s; idx++) {
    leds[idx] = (idx % 2 ? CRGB::Blue : CRGB::Yellow);
  }
  leds[0] = CRGB::Red;
  leds[s - 1] = CRGB::Green;
}

#endif