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
16-04-2022
*/
#ifndef DVG_FASTLED_EFFECTS_H
#define DVG_FASTLED_EFFECTS_H

#include <Arduino.h>
#include <algorithm>
//#include <cmath>

#include "FastLED.h"

#include "DvG_ECG_simulation.h"
#include "DvG_FastLED_StripSegmenter.h"
#include "DvG_FastLED_config.h"
#include "DvG_FastLED_functions.h"

using namespace std;

// External variables defined in `main.cpp`
extern FSM fsm_fx;
extern uint8_t IR_dist_cm;
extern float IR_dist_fract;

CRGB leds[FLC::N];          // LED data of the full strip to be send out
CRGB leds_snapshot[FLC::N]; // `leds` snapshot copy
CRGB fx1[FLC::N];           // Will be populated up to length `s1`
CRGB fx2[FLC::N];           // Will be populated up to length `s2`
CRGB fx1_strip[FLC::N];     // Full strip after segmenter on `fx1`
CRGB fx2_strip[FLC::N];     // Full strip after segmenter on `fx2`

FastLED_StripSegmenter segmntr1; // Segmenter operating on `fx1`
static uint16_t s1; // Will hold `s1 = segmntr1.get_base_numel()` for `fx1`

FastLED_StripSegmenter segmntr2; // Segmenter operating on fx2
static uint16_t s2; // Will hold `s2 = segmntr1.get_base_numel()` for `fx2`

// Recurring animation variables
// clang-format off
bool fx_has_finished = false;
static uint16_t idx1; // LED position index used for `fx1`
static uint16_t idx2; // LED position index used for `fx2`
static bool     fx_starting = false;
static uint32_t fx_timebase = 0;
static uint8_t  fx_hue      = 0;
static uint8_t  fx_hue_step = 1;
static uint8_t  fx_intens   = 255;
static uint8_t  fx_blend    = 127;
// static uint8_t  fx_blur     = 0;
// clang-format on

/*------------------------------------------------------------------------------
  https://coolors.co/98ce00-16e0bd-78c3fb-89a6fb-98838f
------------------------------------------------------------------------------*/

CRGBPalette16 custom_palette_1 = {
    0x7400B8, 0x7400B8, 0x6930C3, 0x6930C3,

    0x5E60CE, 0x5390D9, 0x5390D9, 0x4EA8DE,

    0x48BFE3, 0x48BFE3, 0x56CFE1, 0x64DFDF,

    0x64DFDF, 0x72EFDD, 0x80FFDB, 0x80FFDB,
};

/*------------------------------------------------------------------------------
  AllBlack
------------------------------------------------------------------------------*/

void entr__AllBlack() {
  fx_has_finished = false;
  fill_solid(leds, FLC::N, CRGB::Black);
}

State fx__AllBlack("AllBlack", entr__AllBlack, 0);

/*------------------------------------------------------------------------------
  AllWhite
------------------------------------------------------------------------------*/

void entr__AllWhite() {
  fx_has_finished = false;
  fill_solid(leds, FLC::N, CRGB::White);
}

State fx__AllWhite("AllWhite", entr__AllWhite, 0);

/*------------------------------------------------------------------------------
  TestPattern

  First LED : Green
  Last  LED : Red
  In between: Alternating blue/yellow
------------------------------------------------------------------------------*/

void entr__TestPattern() {
  fx_has_finished = false;
  // segmntr1.set_style(StyleEnum::COPIED_SIDES);
}

void upd__TestPattern() {
  s1 = segmntr1.get_base_numel();
  for (idx1 = 0; idx1 < s1; idx1++) {
    fx1[idx1] = (idx1 % 2 ? CRGB::Blue : CRGB::Yellow);
  }
  fx1[0] = CRGB::Green;
  fx1[s1 - 1] = CRGB::Red;
  segmntr1.process(leds, fx1);
}

State fx__TestPattern("TestPattern", entr__TestPattern, upd__TestPattern);

/*------------------------------------------------------------------------------
  IR distance test
------------------------------------------------------------------------------*/

void entr__IRDist() {
  fx_has_finished = false;
}

void upd__IRDist() {
  CRGB color =
      ColorFromPalette(RainbowColors_p, (uint8_t)(IR_dist_fract * 255));
  fill_solid(leds, FLC::N, color);
}

State fx__IRDist("IRDist", entr__IRDist, upd__IRDist);

/*------------------------------------------------------------------------------
  HeartBeat

  A beating heart. You must call `generate_HeartBeat()` once in `setup()`.
  Author: Dennis van Gils
------------------------------------------------------------------------------*/
#define ECG_N_SMP 256 // 256 so you can use `beat8()` for timing

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

void entr__HeartBeat() {
  fx_has_finished = false;
  // segmntr1.set_style(StyleEnum::BI_DIR_SIDE2SIDE);
  segmntr1.set_style(StyleEnum::HALFWAY_PERIO_SPLIT_N2);
  create_leds_snapshot();
  clear_CRGBs(fx1);
  fx_timebase = millis();
}

void upd__HeartBeat() {
  s1 = segmntr1.get_base_numel();
  uint8_t ECG_idx;
  float ECG_ampl;

  ECG_idx = beat8(30, fx_timebase);
  ECG_ampl = ECG::wave[ECG_idx];
  ECG_ampl = max(ECG_ampl, 0.13); // Suppress ECG depolarization from the wave

  idx1 = round((1 - ECG_ampl) * (s1 - 1));
  fx1[idx1] += CHSV(HUE_RED, 255, uint8_t(ECG::wave[ECG_idx] * 200));
  populate_fx1_strip();

  add_CRGBs(leds_snapshot, fx1_strip, leds, FLC::N);

  EVERY_N_MILLIS(10) {
    fadeToBlackBy(leds_snapshot, FLC::N, 5);
    fadeToBlackBy(fx1, s1, 10);
  }
}

State fx__HeartBeat("HeartBeat", entr__HeartBeat, upd__HeartBeat);

/*------------------------------------------------------------------------------
  HeartBeat_2
  Author: Dennis van Gils
------------------------------------------------------------------------------*/

void entr__HeartBeat_2() {
  fx_has_finished = false;
  segmntr1.set_style(StyleEnum::PERIO_OPP_CORNERS_N2);
  segmntr2.set_style(StyleEnum::FULL_STRIP);
  create_leds_snapshot();
  clear_CRGBs(fx1);
  clear_CRGBs(fx2);
  fx_timebase = millis();
  fx_hue = 0;
}

void upd__HeartBeat_2() {
  s1 = segmntr1.get_base_numel();
  s2 = segmntr2.get_base_numel();
  static uint8_t heart_rate = 30;
  uint8_t ECG_idx;

  // Effect 1
  idx1 = round(beatsin8(heart_rate / 2, 0, 255, fx_timebase) / 255. * (s1 - 1));
  if ((idx1 < s1 / 3) | (idx1 > s1 * 2 / 3)) {
    fx1[idx1] = CRGB::Red;
    // fx1[idx1] += CHSV(HUE_RED, 255, fx_intens);
  }

  // Effect 2
  ECG_idx = beat8(heart_rate, fx_timebase);
  fx_intens = round(ECG::wave[ECG_idx] * 100);

  /*
  // Make heart rate depend on IR_dist_cm
  // Is hard to keep beats to start at the start
  if (ECG_idx == 255){
    heart_rate = floor(((uint16_t) IR_dist_cm * 2) / 2); // Ensure even
    fx_timebase = millis();
  }
  */

  /*
  // Make hue depend on IR_dist_cm
  fx_hue = 200 - IR_dist_fract *  200;
  */

  for (idx2 = 0; idx2 < s2; idx2++) {
    if (fx_intens > 15) {
      fx2[idx2] += CHSV(fx_hue, 255, fx_intens);
    }
  }

  populate_fx1_strip();
  populate_fx2_strip();
  rotate_strip_90(fx1_strip);
  add_CRGBs(fx1_strip, fx2_strip, fx1_strip, FLC::N);

  // Final mix
  add_CRGBs(leds_snapshot, fx1_strip, leds, FLC::N);

  EVERY_N_MILLIS(10) {
    fadeToBlackBy(leds_snapshot, FLC::N, 5);
    fadeToBlackBy(fx1, s1, 20);
    fadeToBlackBy(fx2, s2, 10);
  }
}

State fx__HeartBeat_2("HeartBeat_2", entr__HeartBeat_2, upd__HeartBeat_2);

/*------------------------------------------------------------------------------
  Rainbow

  FastLED's built-in rainbow generator
------------------------------------------------------------------------------*/

void entr__Rainbow() {
  fx_has_finished = false;
  segmntr1.set_style(StyleEnum::FULL_STRIP);
  create_leds_snapshot();
  fx_starting = true;
  fx_hue = 0;
  fx_hue_step = 1;
  fx_blend = 0;
}

void upd__Rainbow() {
  s1 = segmntr1.get_base_numel();
  static uint8_t wave_idx;

  if (fx_starting) {
    fx_starting = false;
    wave_idx = 0;
  }

  fill_rainbow(fx1, s1, fx_hue, 255 / (s1 - 1));
  populate_fx1_strip();

  blend_CRGBs(leds_snapshot, fx1_strip, leds, FLC::N, fx_blend);

  EVERY_N_MILLIS(10) {
    fadeToBlackBy(leds_snapshot, FLC::N, 5);
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

State fx__Rainbow("Rainbow", entr__Rainbow, upd__Rainbow);

/*------------------------------------------------------------------------------
  Sinelon

  A colored dot sweeping back and forth, with fading trails
------------------------------------------------------------------------------*/

void entr__Sinelon() {
  fx_has_finished = false;
  segmntr1.set_style(StyleEnum::BI_DIR_SIDE2SIDE);
  create_leds_snapshot();
  clear_CRGBs(fx1);
  fx_timebase = millis();
}

void upd__Sinelon() {
  s1 = segmntr1.get_base_numel();

  idx1 = beatsin16(13, 0, s1, fx_timebase, 16384);
  fx_hue = beat8(4, fx_timebase) + 127;
  fx1[idx1] = CHSV(fx_hue, 255, 255); // fx_hue, 255, 192
  populate_fx1_strip();

  add_CRGBs(leds_snapshot, fx1_strip, leds, FLC::N);

  EVERY_N_MILLIS(10) {
    fadeToBlackBy(leds_snapshot, FLC::N, 5);
    fadeToBlackBy(fx1, s1, 5);
  }
}

State fx__Sinelon("Sinelon", entr__Sinelon, upd__Sinelon);

/*------------------------------------------------------------------------------
  BPM

  Colored stripes pulsing at a defined beats-per-minute
------------------------------------------------------------------------------*/

void entr__BPM() {
  fx_has_finished = false;
  // segmntr1.set_style(StyleEnum::BI_DIR_SIDE2SIDE);
  segmntr1.set_style(StyleEnum::HALFWAY_PERIO_SPLIT_N2);
  create_leds_snapshot();
  fx_timebase = millis();
  fx_hue = 0;
  fx_hue_step = 1;
}

void upd__BPM() {
  s1 = segmntr1.get_base_numel();
  CRGBPalette16 palette = PartyColors_p; // RainbowColors_p; // PartyColors_p;
  static uint8_t bpm = 30;
  uint8_t beat;

  beat = beatsin8(bpm, 64, 255, fx_timebase);
  for (idx1 = 0; idx1 < s1; idx1++) {
    fx1[idx1] = ColorFromPalette(palette, fx_hue + 128. / (s1 - 1) * idx1,
                                 beat + 127. / (s1 - 1) * idx1);
  }
  populate_fx1_strip();
  rotate_strip_90(fx1);

  add_CRGBs(leds_snapshot, fx1_strip, leds, FLC::N);

  EVERY_N_MILLIS(10) {
    fadeToBlackBy(leds_snapshot, FLC::N, 5);
  }
  EVERY_N_MILLISECONDS(30) {
    fx_hue = fx_hue + fx_hue_step;
  }
}

State fx__BPM("BPM", entr__BPM, upd__BPM);

/*------------------------------------------------------------------------------
  Juggle

  8 colored dots, weaving in and out of sync with each other
------------------------------------------------------------------------------*/

void entr__Juggle() {
  fx_has_finished = false;
  // segmntr1.set_style(StyleEnum::PERIO_OPP_CORNERS_N4);
}

void upd__Juggle() {
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

State fx__Juggle("Juggle", entr__Juggle, upd__Juggle);

/*------------------------------------------------------------------------------
  Stobe

  Needs Work. Bad design.
------------------------------------------------------------------------------*/

/*
namespace Strobe {
  float FPS = 10;                              // flashes per second [Hz]
  uint16_t T_flash_delay = round(1000. / FPS); // [ms]
  uint16_t T_flash_length = 20;                // [ms]
} // namespace Strobe

void entr__Strobe() {
  fx_has_finished = false;
  // segmntr1.set_style(StyleEnum::FULL_STRIP);
}

void upd__Strobe() {
  s1 = segmntr1.get_base_numel();
  EVERY_N_MILLISECONDS(Strobe::T_flash_delay) {
    FastLED.showColor(CRGB::White);
    FastLED.show();
    delay(Strobe::T_flash_length);
    FastLED.showColor(CRGB::Black);
    FastLED.show();
  }
}

State fx__Strobe("Strobe", entr__Strobe, upd__Strobe);
*/

/*------------------------------------------------------------------------------
  Dennis
------------------------------------------------------------------------------*/

void entr__Dennis() {
  fx_has_finished = false;
  // segmntr1.set_style(StyleEnum::HALFWAY_PERIO_SPLIT_N2);
  // segmntr1.set_style(StyleEnum::UNI_DIR_SIDE2SIDE);
  segmntr1.set_style(StyleEnum::PERIO_OPP_CORNERS_N2);
  create_leds_snapshot();
  clear_CRGBs(fx1);
  fx_timebase = millis();
  fx_blend = 0;
}

void upd__Dennis() {
  s1 = segmntr1.get_base_numel();

  idx1 = beatsin16(15, 0, s1 - 1, fx_timebase); // 15
  fx1[idx1] = CRGB::Red;
  fx1[s1 - idx1 - 1] = CRGB::OrangeRed;
  populate_fx1_strip();

  blend_CRGBs(leds_snapshot, fx1_strip, leds, FLC::N, fx_blend);

  EVERY_N_MILLIS(10) {
    fadeToBlackBy(leds_snapshot, FLC::N, 1);
    fadeToBlackBy(fx1, s1, 14);
    if (fx_blend < 255) {
      fx_blend++;
    }
  }
}

State fx__Dennis("Dennis", entr__Dennis, upd__Dennis);

/*------------------------------------------------------------------------------
  Try
------------------------------------------------------------------------------*/

void entr__Try() {
  fx_has_finished = false;
  // segmntr1.set_style(StyleEnum::PERIO_OPP_CORNERS_N4);
  // segmntr1.set_style(StyleEnum::BI_DIR_SIDE2SIDE);
  segmntr1.set_style(StyleEnum::HALFWAY_PERIO_SPLIT_N2);
  create_leds_snapshot();
  clear_CRGBs(fx1);
  clear_CRGBs(fx2);
  fx_timebase = millis();
  fx_blend = 0;
  fx_hue = 0;
}

void upd__Try() {
  s1 = segmntr1.get_base_numel();

  idx1 = beatsin16(15, 0, s1 - 1, fx_timebase); // 15
  // fx1[idx1] = CRGB::Red;
  fx1[idx1] = ColorFromPalette(custom_palette_1, fx_hue);
  // fx1[s1 - idx1 - 1] = CRGB::OrangeRed;
  populate_fx1_strip();

  copy_strip(fx1_strip, fx2_strip);
  flip_strip(fx2_strip);
  add_CRGBs(fx1_strip, fx2_strip, fx1_strip, FLC::N);

  /*
  // Boost blue
  for (uint16_t i = 0; i < FLC::N; i++) {
    fx1_strip[i].blue = scale8(fx1_strip[i].blue, 255);
    //fx1_strip[i].green = scale8(fx1_strip[i].green, 200);
    //fx1_strip[i] |= CRGB(2, 5, 7);
  }
  */

  // blur1d(fx1_strip, FLC::N, 128);
  blend_CRGBs(leds_snapshot, fx1_strip, leds, FLC::N, fx_blend);

  EVERY_N_MILLIS(10) {
    fadeToBlackBy(leds_snapshot, FLC::N, 1);
    fadeToBlackBy(fx1, s1, 4);
    if (fx_blend < 255) {
      fx_blend++;
    }
    fx_hue += 1;
  }
}

State fx__Try("Try", entr__Try, upd__Try);

/*------------------------------------------------------------------------------
  RainbowBarf

  Demonstrates gaussian with sub-pixel `mu`
  Author: Dennis van Gils
------------------------------------------------------------------------------*/

void entr__RainbowBarf() {
  fx_has_finished = false;
  segmntr1.set_style(StyleEnum::PERIO_OPP_CORNERS_N2);
  create_leds_snapshot();
  fx_starting = true;
  fx_blend = 0;
}

void upd__RainbowBarf() {
  s1 = segmntr1.get_base_numel();
  uint8_t gauss8[FLC::N]; // Will hold the Gaussian profile
  static float mu;
  float sigma = 6;

  if (fx_starting) {
    fx_starting = false;
    mu = 0;
  }
  profile_gauss8strip(gauss8, mu, sigma);

  for (idx1 = 0; idx1 < s1; idx1++) {
    // fx1[idx1] = CRGB(gauss8[idx1], 0, 0);
    fx1[idx1] = ColorFromPalette(RainbowColors_p, gauss8[idx1], gauss8[idx1]);
  }
  populate_fx1_strip();

  blend_CRGBs(leds_snapshot, fx1_strip, leds, FLC::N, fx_blend);

  EVERY_N_MILLIS(20) {
    mu += .4;
    while (mu >= FLC::N) {
      mu -= FLC::N;
    }
    fadeToBlackBy(leds_snapshot, FLC::N, 10);
    if (fx_blend < 255) {
      fx_blend++;
    }
  }
}

State fx__RainbowBarf("RainbowBarf", entr__RainbowBarf, upd__RainbowBarf);

/*------------------------------------------------------------------------------
  RainbowBarf_2

  Demonstrates gaussian with integer-pixel `mu`
  Author: Dennis van Gils
------------------------------------------------------------------------------*/

void entr__RainbowBarf_2() {
  segmntr1.set_style(StyleEnum::FULL_STRIP);
  fx_starting = true;
}

void upd__RainbowBarf_2() {
  fx_has_finished = false;
  s1 = segmntr1.get_base_numel();
  uint8_t gauss8[FLC::N];   // Will hold the Gaussian profile
  static uint16_t wave_idx; // `triwave8`-index driving `sigma`
  static uint16_t mu;
  float sigma;

  if (fx_starting) {
    fx_starting = false;
    wave_idx = 0;
    mu = 6;
  }

  sigma = ((float)triwave8(wave_idx) / 255) * 24;
  profile_gauss8strip(gauss8, mu, sigma);

  for (idx1 = 0; idx1 < s1; idx1++) {
    fx1[idx1] = ColorFromPalette(RainbowColors_p, gauss8[idx1], gauss8[idx1]);
  }
  populate_fx1_strip();
  copy_strip(fx1_strip, leds);

  EVERY_N_MILLIS(20) {
    wave_idx += 1;
    if (wave_idx >= 255) {
      wave_idx -= 255;

      mu += FLC::N / 2;
      while (mu >= FLC::N) {
        mu -= FLC::N;
      }
    }
  }
}

State fx__RainbowBarf_2("RainbowBarf_2", entr__RainbowBarf_2,
                        upd__RainbowBarf_2);

/*------------------------------------------------------------------------------
  RainbowHeartBeat
  Still ugly staccato
  Author: Dennis van Gils
------------------------------------------------------------------------------*/

void entr__RainbowHeartBeat() {
  fx_has_finished = false;
  segmntr1.set_style(StyleEnum::FULL_STRIP);
  fx_timebase = millis();
}

void upd__RainbowHeartBeat() {
  s1 = segmntr1.get_base_numel();
  uint8_t gauss8[FLC::N]; // Will hold the Gaussian profile
  uint8_t ECG_idx;        // Driving `sigma`
  uint16_t mu = 6;
  float sigma;
  static uint8_t heart_rate = 30;

  ECG_idx = beat8(heart_rate, fx_timebase);
  sigma = ECG::wave[ECG_idx] - 0.13; // -0.13 removes ECG depolarization
  profile_gauss8strip(gauss8, mu, sigma * 6);

  for (idx1 = 0; idx1 < s1; idx1++) {
    fx1[idx1] = ColorFromPalette(RainbowColors_p, gauss8[idx1], gauss8[idx1]);
  }
  populate_fx1_strip();

  copy_strip(fx1_strip, leds);

  EVERY_N_MILLIS(20) {
    // fadeToBlackBy(fx1, FLC::N, 60);
  }
}

State fx__RainbowHeartBeat("RainbowHeartBeat", entr__RainbowHeartBeat,
                           upd__RainbowHeartBeat);

/*------------------------------------------------------------------------------
  RainbowSurf

  A slowly shifting rainbow over the full strip with a faster smaller rainbow
  wave surfing on top.
  Author: Dennis van Gils
------------------------------------------------------------------------------*/

void entr__RainbowSurf() {
  fx_has_finished = false;
  segmntr1.set_style(StyleEnum::FULL_STRIP);
  create_leds_snapshot();
  fx_starting = true;
  fx_blend = 0;
  fx_hue = 0;
}

void upd__RainbowSurf() {
  s1 = segmntr1.get_base_numel();
  CHSV fx3[FLC::N];       // CHSV instead of CRGB
  uint8_t gauss8[FLC::N]; // Will hold the Gaussian profile
  static float mu;
  float sigma = 12;

  fill_rainbow(fx3, s1, fx_hue, 255 / (s1 - 1));

  if (fx_starting) {
    fx_starting = false;
    mu = 6.;
  }
  profile_gauss8strip(gauss8, mu, sigma);

  for (idx1 = 0; idx1 < s1; idx1++) {
    fx1[idx1] = CHSV(fx3[idx1].hue + gauss8[idx1], 255, 255);
  }
  populate_fx1_strip();

  blend_CRGBs(leds_snapshot, fx1_strip, leds, FLC::N, fx_blend);

  EVERY_N_MILLIS(20) {
    mu += .4;
    while (mu >= FLC::N) {
      mu -= FLC::N;
    }
    fadeToBlackBy(leds_snapshot, FLC::N, 10);
    if (fx_blend < 255) {
      fx_blend++;
    }
  }
  EVERY_N_MILLIS(50) {
    fx_hue += 1;
  }

  // DEBUG: Working proof of concept for new mechanism `auto next fx`
  if (fsm_fx.timeInCurrentState() > 20000) {
    fx_has_finished = true;
  }
}

State fx__RainbowSurf("RainbowSurf", entr__RainbowSurf, upd__RainbowSurf);

#endif