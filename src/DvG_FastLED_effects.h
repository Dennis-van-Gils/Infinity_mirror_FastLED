/* DvG_LEDStripSegmentor.h

Dennis van Gils
16-11-2021
*/
#ifndef DVG_FASTLED_EFFECTS_H
#define DVG_FASTLED_EFFECTS_H

#include <Arduino.h>

#include "FastLED.h"
#include "FiniteStateMachine.h"

#include "DvG_ECG_simulation.h"
#include "DvG_FastLED_config.h"

extern CRGB leds[FastLEDConfig::N];
extern uint16_t s;

static uint16_t idx; // LED position index used in many for-loops
static uint32_t now; // To store `millis()` value used in many functions
uint8_t iHue = 0;    // Rotating hue used by many of the effects

#define EFFECT_NAME_LEN 64
char effect_name[EFFECT_NAME_LEN] = {"\0"};

/*-----------------------------------------------------------------------------
  HeartBeat

  A beating heart. You must call `generate_HeartBeat()` once in `setup()`.
  `idx_delay` determines the heart rate and can be changed at runtime.
------------------------------------------------------------------------------*/
#define ECG_N_SMP 100 // 100

namespace ECG {
  static float wave[ECG_N_SMP] = {0};
  static uint32_t tick = 0;
  static uint16_t idx = 0;
  static uint32_t idx_delay = 20;
} // namespace ECG

void generate_HeartBeat() {
  // Generate ECG wave data, output range [0 - 1]. Note that the `resting` state
  // of the heart is somewhere above 0. 0 is simply the minimum of the ECG
  // action potential.
  generate_ECG(ECG::wave, ECG_N_SMP);
}

void enter__HeartBeat() {
  snprintf(effect_name, EFFECT_NAME_LEN, "HeartBeat");
  ECG::idx = round(ECG_N_SMP / 6.);
  ECG::tick = millis();
  // fill_solid(leds, s, CRGB::Red);
}

void update__HeartBeat() {
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

  // Advance heart beat in time
  now = millis();
  if (now - ECG::tick > ECG::idx_delay) {
    ECG::idx = (ECG::idx + 1) % ECG_N_SMP;
    ECG::tick = now;
  }
}

State state__HeartBeat(enter__HeartBeat, update__HeartBeat);

/*-----------------------------------------------------------------------------
  Rainbow

  FastLED's built-in rainbow generator
------------------------------------------------------------------------------*/

void enter__Rainbow() { snprintf(effect_name, EFFECT_NAME_LEN, "Rainbow"); }

void update__Rainbow() { fill_rainbow(leds, s, iHue, 255 / (s - 1)); }

State state__Rainbow(enter__Rainbow, update__Rainbow);

/*-----------------------------------------------------------------------------
  Sinelon

  A colored dot sweeping back and forth, with fading trails
------------------------------------------------------------------------------*/

void enter__Sinelon() { snprintf(effect_name, EFFECT_NAME_LEN, "Sinelon"); }

void update__Sinelon() {
  // A colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, s, 4);
  idx = beatsin16(13, 0, s);
  leds[idx] += CHSV(iHue, 255, 255); // iHue, 255, 192
}

State state__Sinelon(enter__Sinelon, update__Sinelon);

/*-----------------------------------------------------------------------------
  BPM

  Colored stripes pulsing at a defined beats-per-minute
------------------------------------------------------------------------------*/

void enter__BPM() { snprintf(effect_name, EFFECT_NAME_LEN, "BPM"); }

void update__BPM() {
  CRGBPalette16 palette = PartyColors_p; // RainbowColors_p; // PartyColors_p;
  uint8_t bpm_ = 30;
  uint8_t beat = beatsin8(bpm_, 64, 255);
  for (idx = 0; idx < s; idx++) {
    leds[idx] = ColorFromPalette(palette, iHue + 128. / (s - 1) * idx,
                                 beat + 127. / (s - 1) * idx);
  }
}

State state__BPM(enter__BPM, update__BPM);

/*-----------------------------------------------------------------------------
  Juggle

  8 colored dots, weaving in and out of sync with each other
------------------------------------------------------------------------------*/

void enter__Juggle() { snprintf(effect_name, EFFECT_NAME_LEN, "Juggle"); }

void update__Juggle() {
  byte dothue = 0;
  fadeToBlackBy(leds, s, 20);
  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, s - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

State state__Juggle(enter__Juggle, update__Juggle);

/*-----------------------------------------------------------------------------
  FullWhite
------------------------------------------------------------------------------*/

void enter__FullWhite() { snprintf(effect_name, EFFECT_NAME_LEN, "FullWhite"); }

void update__FullWhite() { fill_solid(leds, s, CRGB::White); }

State state__FullWhite(update__FullWhite);

/*-----------------------------------------------------------------------------
  Stobe

  Needs Work. Bad design.
------------------------------------------------------------------------------*/

namespace Strobe {
  float FPS = 10;                              // flashes per second [Hz]
  uint16_t T_flash_delay = round(1000. / FPS); // [ms]
  uint16_t T_flash_length = 20;                // [ms]
} // namespace Strobe

void enter__Strobe() { snprintf(effect_name, EFFECT_NAME_LEN, "Strobe"); }

void update__Strobe() {
  EVERY_N_MILLISECONDS(Strobe::T_flash_delay) {
    FastLED.showColor(CRGB::White);
    FastLED.show();
    delay(Strobe::T_flash_length);
    FastLED.showColor(CRGB::Black);
    FastLED.show();
  }
}

State state__Strobe(enter__Strobe, update__Strobe);

/*-----------------------------------------------------------------------------
  Dennis
------------------------------------------------------------------------------*/

void enter__Dennis() { snprintf(effect_name, EFFECT_NAME_LEN, "Dennis"); }

void update__Dennis() {
  fadeToBlackBy(leds, s, 16);
  idx = beatsin16(15, 0, s - 1);
  // leds[idx] = CRGB::Blue;
  // leds[s - idx - 1] = CRGB::Blue;
  leds[idx] = CRGB::Red;
  leds[s - idx - 1] = CRGB::OrangeRed;
}

State state__Dennis(enter__Dennis, update__Dennis);

/*-----------------------------------------------------------------------------
  TestPattern

  First LED : Green
  Last  LED : Red
  In between: Alternating blue/yellow
------------------------------------------------------------------------------*/

void enter__TestPattern() {
  snprintf(effect_name, EFFECT_NAME_LEN, "TestPattern");
}

void update__TestPattern() {
  for (idx = 0; idx < s; idx++) {
    leds[idx] = (idx % 2 ? CRGB::Blue : CRGB::Yellow);
  }
  leds[0] = CRGB::Green;
  leds[s - 1] = CRGB::Red;
}

State state__TestPattern(enter__TestPattern, update__TestPattern);

#endif