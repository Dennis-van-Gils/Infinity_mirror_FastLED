/* DvG_FastLED_effects.h

Great background:
  https://github.com/FastLED/FastLED/wiki/Pixel-referenced

For inspiration:
  https://github.com/kitesurfer1404/WS2812FX
  https://www.youtube.com/watch?v=UZxY_BLSsGg&t=236s
  https://gist.github.com/kriegsman/a916be18d32ec675fea8

FastLED API reference:
  https://github.com/FastLED/FastLED/wiki
  http://fastled.io/docs/3.1/

Dennis van Gils
21-11-2021
*/
#ifndef DVG_FASTLED_EFFECTS_H
#define DVG_FASTLED_EFFECTS_H

#include <Arduino.h>
#include <algorithm>

#include "FastLED.h"
#include "FiniteStateMachine.h"

#include "DvG_ECG_simulation.h"
#include "DvG_FastLED_StripSegmenter.h"

using namespace std;

// External variables
extern CRGB leds[FastLEDConfig::N]; // Defined in `main.cpp`
extern FSM fsm;                     // Defined in `main.cpp`

CRGB leds_snapshot[FastLEDConfig::N]; // Snapshot in time
CRGB ledfx1[FastLEDConfig::N];        // Will be populated up to length `s1`
CRGB ledfx2[FastLEDConfig::N];        // Will be populated up to length `s2`
CRGB ledfx1_strip[FastLEDConfig::N];  // Full strip after segmenter on `ledfx1`
CRGB ledfx2_strip[FastLEDConfig::N];  // Full strip after segmenter on `ledfx2`

FastLED_StripSegmenter segmntr1;
uint16_t s1; // Will hold `s1 = segmntr1.get_base_numel()`

FastLED_StripSegmenter segmntr2;
uint16_t s2; // Will hold `s2 = segmntr1.get_base_numel()`

// Animation
// clang-format off
extern uint8_t IR_dist; // Defined in `main.cpp`
uint32_t fx_timebase = 0;
uint8_t  fx_hue      = 0;
uint8_t  fx_hue_step = 1;
// clang-format on

// State control
bool effect_is_at_startup = false;

static uint16_t idx; // LED position index used in many for-loops

/*------------------------------------------------------------------------------
  CRGB array functions
------------------------------------------------------------------------------*/

void create_leds_snapshot() {
  memcpy8(leds_snapshot, leds, CRGB_SIZE * FastLEDConfig::N);
}

void populate_ledfx1_strip() { segmntr1.process(ledfx1_strip, ledfx1); }
void populate_ledfx2_strip() { segmntr2.process(ledfx2_strip, ledfx2); }

void rotate_strip_90(CRGB *in) {
  std::rotate(in, in + FastLEDConfig::L, in + FastLEDConfig::N);
}

void clear_CRGBs(CRGB *in) { fill_solid(in, FastLEDConfig::N, CRGB::Black); }

void add_CRGBs(const CRGB *in_1, const CRGB *in_2, CRGB *out, uint16_t numel) {
  for (uint16_t idx = 0; idx < numel; idx++) {
    out[idx] = in_1[idx] + in_2[idx];
  }
}

void blend_CRGBs(const CRGB *in_1, const CRGB *in_2, CRGB *out, uint16_t numel,
                 fract8 amount_of_2) {
  for (uint16_t idx = 0; idx < numel; idx++) {
    out[idx] = blend(in_1[idx], in_2[idx], amount_of_2);
  }
}

bool is_all_black(CRGB *in, uint32_t numel) {
  for (uint16_t idx = 0; idx < numel; idx++) {
    if (in[idx]) {
      return false;
    }
  }
  return true;
}

/*------------------------------------------------------------------------------
  HeartBeat

  A beating heart. You must call `generate_HeartBeat()` once in `setup()`.
------------------------------------------------------------------------------*/
#define ECG_N_SMP 255 // == max uint8_t, so you can use `beat8()` for timing

namespace ECG {
  static float wave[ECG_N_SMP] = {0};
} // namespace ECG

void generate_HeartBeat() {
  // Generate ECG wave data, output range [0 - 1]. Note that the `resting`
  // state of the heart is somewhere above 0. 0 is simply the minimum of the
  // ECG action potential.
  generate_ECG(ECG::wave, ECG_N_SMP);
}

void enter__HeartBeat1() {
  segmntr1.set_style(StyleEnum::BI_DIR_SIDE2SIDE);

  create_leds_snapshot();
  clear_CRGBs(ledfx1);

  fx_timebase = millis();
}

void update__HeartBeat1() {
  s1 = segmntr1.get_base_numel();

  fadeToBlackBy(ledfx1, s1, 8);

  uint8_t ECG_idx = beat8(30, fx_timebase);
  idx = round((1 - ECG::wave[ECG_idx]) * (s1 - 1));
  ledfx1[idx] += CHSV(HUE_RED, 255, uint8_t(ECG::wave[ECG_idx] * 200));
  populate_ledfx1_strip();

  fadeToBlackBy(leds_snapshot, FastLEDConfig::N, 4);
  add_CRGBs(leds_snapshot, ledfx1_strip, leds, FastLEDConfig::N);
}

/*------------------------------------------------------------------------------
  HeartBeat2
------------------------------------------------------------------------------*/

void enter__HeartBeat2() {
  segmntr1.set_style(StyleEnum::FULL_STRIP);
  segmntr2.set_style(StyleEnum::BI_DIR_SIDE2SIDE);

  create_leds_snapshot();
  clear_CRGBs(ledfx1);
  clear_CRGBs(ledfx2);

  fx_timebase = millis();
}

void update__HeartBeat2() {
  static uint8_t heart_rate = 30;
  s1 = segmntr1.get_base_numel();
  s2 = segmntr2.get_base_numel();

  fadeToBlackBy(leds_snapshot, FastLEDConfig::N, 4);
  fadeToBlackBy(ledfx1, s1, 8);
  fadeToBlackBy(ledfx2, s2, 8);

  // Effect 1
  uint8_t ECG_idx = beat8(heart_rate, fx_timebase);
  uint8_t intens = round(ECG::wave[ECG_idx] * 100);

  for (idx = 0; idx < s1; idx++) {
    if (intens > 15) {
      ledfx1[idx] += CHSV(HUE_RED, 255, intens);
    }
  }

  // Effect 2
  uint16_t idx2 =
      round(beat8(heart_rate / 2, fx_timebase) / 255. * (FastLEDConfig::N - 1));
  ledfx2[idx2] = CRGB::White;
  // ledfx2[idx2] += CHSV(HUE_RED, 255, 255);

  populate_ledfx1_strip();
  populate_ledfx2_strip();

  rotate_strip_90(ledfx2_strip);
  add_CRGBs(ledfx1_strip, ledfx2_strip, ledfx1_strip, FastLEDConfig::N);

  // Final mix
  add_CRGBs(leds_snapshot, ledfx1_strip, leds, FastLEDConfig::N);
}

State state__HeartBeat1("HeartBeat1", enter__HeartBeat1, update__HeartBeat1);
State state__HeartBeat2("HeartBeat2", enter__HeartBeat2, update__HeartBeat2);

/*------------------------------------------------------------------------------
  Rainbow

  FastLED's1 built-in rainbow generator
------------------------------------------------------------------------------*/

void enter__Rainbow() {
  // segmntr1.set_style(StyleEnum::PERIO_OPP_CORNERS_N4);
  // segmntr1.set_style(StyleEnum::COPIED_SIDES);
}

void update__Rainbow() {
  s1 = segmntr1.get_base_numel();
  fill_rainbow(ledfx1, s1, fx_hue, 255 / (s1 - 1));
  fx_hue_step = beatsin16(10, 1, 20);
  segmntr1.process(leds, ledfx1);
}

State state__Rainbow("Rainbow", enter__Rainbow, update__Rainbow);

/*------------------------------------------------------------------------------
  Sinelon

  A colored dot sweeping back and forth, with fading trails
------------------------------------------------------------------------------*/

void enter__Sinelon() {
  // segmntr1.set_style(StyleEnum::BI_DIR_SIDE2SIDE);
}

void update__Sinelon() {
  s1 = segmntr1.get_base_numel();
  fadeToBlackBy(ledfx1, s1, 4);
  idx = beatsin16(13, 0, s1);
  ledfx1[idx] += CHSV(fx_hue, 255, 255); // fx_hue, 255, 192
  segmntr1.process(leds, ledfx1);
}

State state__Sinelon("Sinelon", enter__Sinelon, update__Sinelon);

/*------------------------------------------------------------------------------
  BPM

  Colored stripes pulsing at a defined beats-per-minute
------------------------------------------------------------------------------*/

void enter__BPM() {
  // segmntr1.set_style(StyleEnum::HALFWAY_PERIO_SPLIT_N2);
}

void update__BPM() {
  s1 = segmntr1.get_base_numel();
  CRGBPalette16 palette = PartyColors_p; // RainbowColors_p; // PartyColors_p;
  uint8_t bpm_ = 30;
  uint8_t beat = beatsin8(bpm_, 64, 255);
  for (idx = 0; idx < s1; idx++) {
    ledfx1[idx] = ColorFromPalette(palette, fx_hue + 128. / (s1 - 1) * idx,
                                   beat + 127. / (s1 - 1) * idx);
  }
  segmntr1.process(leds, ledfx1);
}

State state__BPM("BPM", enter__BPM, update__BPM);

/*------------------------------------------------------------------------------
  Juggle

  8 colored dots, weaving in and out of sync with each other
------------------------------------------------------------------------------*/

void enter__Juggle() {
  // segmntr1.set_style(StyleEnum::PERIO_OPP_CORNERS_N4);
}

void update__Juggle() {
  s1 = segmntr1.get_base_numel();
  byte dothue = 0;
  fadeToBlackBy(ledfx1, s1, 20);
  for (int i = 0; i < 8; i++) {
    ledfx1[beatsin16(i + 7, 0, s1 - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
  segmntr1.process(leds, ledfx1);
}

State state__Juggle("Juggle", enter__Juggle, update__Juggle);

/*------------------------------------------------------------------------------
  FullWhite
------------------------------------------------------------------------------*/

void enter__FullWhite() {
  // segmntr1.set_style(StyleEnum::FULL_STRIP);
}

void update__FullWhite() {
  s1 = segmntr1.get_base_numel();
  fill_solid(leds, FastLEDConfig::N, CRGB::White);
}

State state__FullWhite("FullWhite", update__FullWhite);

/*------------------------------------------------------------------------------
  Stobe

  Needs Work. Bad design.
------------------------------------------------------------------------------*/

namespace Strobe {
  float FPS = 10;                              // flashes per second [Hz]
  uint16_t T_flash_delay = round(1000. / FPS); // [ms]
  uint16_t T_flash_length = 20;                // [ms]
} // namespace Strobe

void enter__Strobe() {
  // segmntr1.set_style(StyleEnum::FULL_STRIP);
}

void update__Strobe() {
  s1 = segmntr1.get_base_numel();
  EVERY_N_MILLISECONDS(Strobe::T_flash_delay) {
    FastLED.showColor(CRGB::White);
    FastLED.show();
    delay(Strobe::T_flash_length);
    FastLED.showColor(CRGB::Black);
    FastLED.show();
  }
}

State state__Strobe("Strobe", enter__Strobe, update__Strobe);

/*------------------------------------------------------------------------------
  Dennis
------------------------------------------------------------------------------*/

void enter__Dennis() {
  // segmntr1.set_style(StyleEnum::HALFWAY_PERIO_SPLIT_N2);
  segmntr1.set_style(StyleEnum::UNI_DIR_SIDE2SIDE);
  fill_solid(ledfx1, FastLEDConfig::N, CRGB::Black);
  create_leds_snapshot();

  fx_timebase = millis();
  effect_is_at_startup = true;
}

void update__Dennis() {
  s1 = segmntr1.get_base_numel();
  // static uint8_t blend_amount = 0;

  if (0) {
    // fill_solid(ledfx1, FastLEDConfig::N, CRGB::Black);
    fadeToBlackBy(ledfx1, FastLEDConfig::N, 16);

    idx = beatsin16(15, 0, s1 - 1);
    ledfx1[idx] = CRGB::Red;
    ledfx1[s1 - idx - 1] = CRGB::OrangeRed;
    segmntr1.process(leds, ledfx1);

  } else {

    /*
    if (effect_is_at_startup) {
      effect_is_at_startup = false;
      blend_amount = 0;
    } else {
      if (blend_amount < 255) {
        blend_amount++;
      }
    }
    */

    fadeToBlackBy(ledfx1, s1, 12); // 16

    idx = beatsin16(15, 0, s1 - 1, fx_timebase); // 15
    ledfx1[idx] = CRGB::Red;
    ledfx1[s1 - idx - 1] = CRGB::OrangeRed;
    populate_ledfx1_strip();

    fadeToBlackBy(leds_snapshot, FastLEDConfig::N, 1);
    blend_CRGBs(leds_snapshot, ledfx1_strip, leds, FastLEDConfig::N, 127);
  }
}

State state__Dennis("Dennis", enter__Dennis, update__Dennis);

/*------------------------------------------------------------------------------
  TestPattern

  First LED : Green
  Last  LED : Red
  In between: Alternating blue/yellow
------------------------------------------------------------------------------*/

void enter__TestPattern() {
  // segmntr1.set_style(StyleEnum::COPIED_SIDES);
}

void update__TestPattern() {
  s1 = segmntr1.get_base_numel();
  for (idx = 0; idx < s1; idx++) {
    ledfx1[idx] = (idx % 2 ? CRGB::Blue : CRGB::Yellow);
  }
  ledfx1[0] = CRGB::Green;
  ledfx1[s1 - 1] = CRGB::Red;
  segmntr1.process(leds, ledfx1);
}

State state__TestPattern("TestPattern", enter__TestPattern,
                         update__TestPattern);

#endif