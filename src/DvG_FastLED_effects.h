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
CRGB fx1[FastLEDConfig::N];           // Will be populated up to length `s1`
CRGB fx2[FastLEDConfig::N];           // Will be populated up to length `s2`
CRGB fx1_strip[FastLEDConfig::N];     // Full strip after segmenter on `fx1`
CRGB fx2_strip[FastLEDConfig::N];     // Full strip after segmenter on `fx2`

FastLED_StripSegmenter segmntr1;
uint16_t s1; // Will hold `s1 = segmntr1.get_base_numel()`

FastLED_StripSegmenter segmntr2;
uint16_t s2; // Will hold `s2 = segmntr1.get_base_numel()`

// Recurring animation variables
// clang-format off
bool     fx_starting = false;
uint32_t fx_timebase = 0;
uint8_t  fx_hue      = 0;
uint8_t  fx_hue_step = 1;
// clang-format on

// IR distance sensor
extern uint8_t IR_dist;           // Defined in `main.cpp`
extern const uint8_t IR_MIN_DIST; // Defined in `main.cpp`
extern const uint8_t IR_MAX_DIST; // Defined in `main.cpp`

static uint16_t idx; // LED position index used in many for-loops

/*------------------------------------------------------------------------------
  CRGB array functions
------------------------------------------------------------------------------*/

void create_leds_snapshot() {
  memcpy8(leds_snapshot, leds, CRGB_SIZE * FastLEDConfig::N);
}

void populate_fx1_strip() {
  segmntr1.process(fx1_strip, fx1);
}
void populate_fx2_strip() {
  segmntr2.process(fx2_strip, fx2);
}

void rotate_strip_90(CRGB *in) {
  std::rotate(in, in + FastLEDConfig::L, in + FastLEDConfig::N);
}

void clear_CRGBs(CRGB *in) {
  fill_solid(in, FastLEDConfig::N, CRGB::Black);
}

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

  // Offset the start of the ECG wave
  std::rotate(ECG::wave, ECG::wave + 44, ECG::wave + ECG_N_SMP);
}

void enter__HeartBeat1() {
  segmntr1.set_style(StyleEnum::BI_DIR_SIDE2SIDE);
  create_leds_snapshot();
  clear_CRGBs(fx1);
  fx_timebase = millis();
}

void update__HeartBeat1() {
  s1 = segmntr1.get_base_numel();
  uint8_t ECG_idx;
  float ECG_ampl;

  ECG_idx = beat8(30, fx_timebase);
  ECG_ampl = ECG::wave[ECG_idx];
  ECG_ampl = max(ECG_ampl, 0.12); // Suppress ECG depolarization from the wave

  idx = round((1 - ECG_ampl) * (s1 - 1));
  fx1[idx] += CHSV(HUE_RED, 255, uint8_t(ECG::wave[ECG_idx] * 200));
  populate_fx1_strip();

  add_CRGBs(leds_snapshot, fx1_strip, leds, FastLEDConfig::N);

  EVERY_N_MILLIS(10) {
    fadeToBlackBy(leds_snapshot, FastLEDConfig::N, 5);
    fadeToBlackBy(fx1, s1, 10);
  }
}

/*------------------------------------------------------------------------------
  HeartBeat2
------------------------------------------------------------------------------*/

void enter__HeartBeat2() {
  segmntr1.set_style(StyleEnum::PERIO_OPP_CORNERS_N2);
  segmntr2.set_style(StyleEnum::FULL_STRIP);
  create_leds_snapshot();
  clear_CRGBs(fx1);
  clear_CRGBs(fx2);
  fx_timebase = millis();
  fx_hue = 0;
}

void update__HeartBeat2() {
  s1 = segmntr1.get_base_numel();
  s2 = segmntr2.get_base_numel();
  static uint8_t heart_rate = 30;
  uint8_t ECG_idx;
  uint8_t intens;
  uint16_t idx2;

  // Effect 1
  idx = round(beatsin8(heart_rate / 2, 0, 255, fx_timebase) / 255. * (s1 - 1));
  if ((idx < s1 / 4) | (idx > s1 * 3 / 4)) {
    fx1[idx] = CRGB::Red;
    // fx1[idx] += CHSV(HUE_RED, 255, intens);
  }

  // Effect 2
  ECG_idx = beat8(heart_rate, fx_timebase);
  intens = round(ECG::wave[ECG_idx] * 100);

  /*
  // Make heart rate depend on IR_dist
  // Is hard to keep beats to start at the start
  if (ECG_idx == 255){
    heart_rate = floor(((uint16_t) IR_dist * 2) / 2); // Ensure even
    fx_timebase = millis();
  }
  */

  /*
  // Make hue depend on IR_dist
  fx_hue =
      200 - ((float)IR_dist - IR_MIN_DIST) / (IR_MAX_DIST - IR_MIN_DIST) * 200;
  */

  for (idx2 = 0; idx2 < s2; idx2++) {
    if (intens > 15) {
      fx2[idx2] += CHSV(fx_hue, 255, intens);
    }
  }

  populate_fx1_strip();
  populate_fx2_strip();
  rotate_strip_90(fx1_strip);
  add_CRGBs(fx1_strip, fx2_strip, fx1_strip, FastLEDConfig::N);

  // Final mix
  add_CRGBs(leds_snapshot, fx1_strip, leds, FastLEDConfig::N);

  EVERY_N_MILLIS(10) {
    fadeToBlackBy(leds_snapshot, FastLEDConfig::N, 5);
    fadeToBlackBy(fx1, s1, 20);
    fadeToBlackBy(fx2, s2, 10);
  }
}

State state__HeartBeat1("HeartBeat1", enter__HeartBeat1, update__HeartBeat1);
State state__HeartBeat2("HeartBeat2", enter__HeartBeat2, update__HeartBeat2);

/*------------------------------------------------------------------------------
  Rainbow

  FastLED's1 built-in rainbow generator
------------------------------------------------------------------------------*/

void enter__Rainbow() {
  segmntr1.set_style(StyleEnum::FULL_STRIP);
  create_leds_snapshot();
  fx_starting = true;
  fx_hue = 0;
  fx_hue_step = 1;
}

void update__Rainbow() {
  s1 = segmntr1.get_base_numel();
  static uint8_t wave_idx;
  static uint8_t fx_blend;

  if (fx_starting) {
    fx_starting = false;
    wave_idx = 0;
    fx_blend = 0;
  }

  fill_rainbow(fx1, s1, fx_hue, 255 / (s1 - 1));
  populate_fx1_strip();

  blend_CRGBs(leds_snapshot, fx1_strip, leds, FastLEDConfig::N, fx_blend);

  EVERY_N_MILLIS(10) {
    fadeToBlackBy(leds_snapshot, FastLEDConfig::N, 5);
  }
  EVERY_N_MILLIS(50) {
    fx_hue_step = round(cubicwave8(wave_idx) / 255. * 15.) + 1;
    fx_hue = fx_hue + fx_hue_step;
    wave_idx++;
  }
  EVERY_N_MILLIS(6) {
    if (fx_blend < 255) {
      fx_blend++;
    }
  }
}

State state__Rainbow("Rainbow", enter__Rainbow, update__Rainbow);

/*------------------------------------------------------------------------------
  Sinelon

  A colored dot sweeping back and forth, with fading trails
------------------------------------------------------------------------------*/

void enter__Sinelon() {
  segmntr1.set_style(StyleEnum::BI_DIR_SIDE2SIDE);
  create_leds_snapshot();
  clear_CRGBs(fx1);
  fx_timebase = millis();
}

void update__Sinelon() {
  s1 = segmntr1.get_base_numel();

  idx = beatsin16(13, 0, s1, fx_timebase, 16384);
  fx_hue = beat8(4, fx_timebase) + 127;
  fx1[idx] = CHSV(fx_hue, 255, 255); // fx_hue, 255, 192
  populate_fx1_strip();

  add_CRGBs(leds_snapshot, fx1_strip, leds, FastLEDConfig::N);

  EVERY_N_MILLIS(10) {
    fadeToBlackBy(leds_snapshot, FastLEDConfig::N, 5);
    fadeToBlackBy(fx1, s1, 5);
  }
}

State state__Sinelon("Sinelon", enter__Sinelon, update__Sinelon);

/*------------------------------------------------------------------------------
  BPM

  Colored stripes pulsing at a defined beats-per-minute
------------------------------------------------------------------------------*/

void enter__BPM() {
  // segmntr1.set_style(StyleEnum::BI_DIR_SIDE2SIDE);
  segmntr1.set_style(StyleEnum::HALFWAY_PERIO_SPLIT_N2);
  create_leds_snapshot();
  fx_timebase = millis();
  fx_hue = 0;
  fx_hue_step = 1;
}

void update__BPM() {
  s1 = segmntr1.get_base_numel();
  CRGBPalette16 palette = PartyColors_p; // RainbowColors_p; // PartyColors_p;
  static uint8_t bpm = 30;
  uint8_t beat;

  beat = beatsin8(bpm, 64, 255, fx_timebase);
  for (idx = 0; idx < s1; idx++) {
    fx1[idx] = ColorFromPalette(palette, fx_hue + 128. / (s1 - 1) * idx,
                                beat + 127. / (s1 - 1) * idx);
  }
  populate_fx1_strip();
  rotate_strip_90(fx1);

  add_CRGBs(leds_snapshot, fx1_strip, leds, FastLEDConfig::N);

  EVERY_N_MILLIS(10) {
    fadeToBlackBy(leds_snapshot, FastLEDConfig::N, 5);
  }
  EVERY_N_MILLISECONDS(30) {
    fx_hue = fx_hue + fx_hue_step;
  }
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

  for (int i = 0; i < 8; i++) {
    fx1[beatsin16(i + 7, 0, s1 - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
  segmntr1.process(leds, fx1);

  EVERY_N_MILLIS(10) {
    fadeToBlackBy(fx1, s1, 24);
  }
}

State state__Juggle("Juggle", enter__Juggle, update__Juggle);

/*------------------------------------------------------------------------------
  FullWhite
------------------------------------------------------------------------------*/

void enter__FullWhite() {
  // segmntr1.set_style(StyleEnum::FULL_STRIP);
}

void update__FullWhite() {
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
  // segmntr1.set_style(StyleEnum::UNI_DIR_SIDE2SIDE);
  segmntr1.set_style(StyleEnum::PERIO_OPP_CORNERS_N2);
  create_leds_snapshot();
  clear_CRGBs(fx1);
  fx_starting = true;
  fx_timebase = millis();
}

void update__Dennis() {
  s1 = segmntr1.get_base_numel();
  static uint8_t fx_blend;

  if (fx_starting) {
    fx_starting = false;
    fx_blend = 0;
  }

  idx = beatsin16(15, 0, s1 - 1, fx_timebase); // 15
  fx1[idx] = CRGB::Red;
  fx1[s1 - idx - 1] = CRGB::OrangeRed;
  populate_fx1_strip();

  blend_CRGBs(leds_snapshot, fx1_strip, leds, FastLEDConfig::N, fx_blend);

  EVERY_N_MILLIS(10) {
    fadeToBlackBy(leds_snapshot, FastLEDConfig::N, 1);
    fadeToBlackBy(fx1, s1, 14);
    if (fx_blend < 255) {
      fx_blend++;
    }
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
    fx1[idx] = (idx % 2 ? CRGB::Blue : CRGB::Yellow);
  }
  fx1[0] = CRGB::Green;
  fx1[s1 - 1] = CRGB::Red;
  segmntr1.process(leds, fx1);
}

State state__TestPattern("TestPattern", enter__TestPattern,
                         update__TestPattern);

#endif