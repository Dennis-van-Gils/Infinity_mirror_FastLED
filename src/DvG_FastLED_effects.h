/* DvG_LEDStripSegmentor.h

Great backgroun:
https://github.com/FastLED/FastLED/wiki/Pixel-referenced

Dennis van Gils
20-11-2021
*/
#ifndef DVG_FASTLED_EFFECTS_H
#define DVG_FASTLED_EFFECTS_H

#include <Arduino.h>

#include "FastLED.h"
#include "FiniteStateMachine.h"

#include "DvG_ECG_simulation.h"
#include "DvG_FastLED_StripSegmenter.h"

// External variables
extern CRGB leds[FastLEDConfig::N]; // Defined in `main.cpp`
extern FSM fsm;                     // Defined in `main.cpp`

CRGB leds_snapshot[FastLEDConfig::N]; // Snapshot in time
CRGB ledfx[FastLEDConfig::N];         // Will be populated up to length `s`
CRGB ledfx_strip[FastLEDConfig::N];   // Full strip after segmenter on `ledfx`

FastLED_StripSegmenter segmntr;
uint16_t s; // Will hold `s = segmntr.get_base_numel()`

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

void blend_CRGB_arrays(CRGB *out, const CRGB *in_1, const CRGB *in_2,
                       uint16_t numel, fract8 amount_of_2) {
  for (uint16_t idx = 0; idx < numel; idx++) {
    out[idx] = blend(in_1[idx], in_2[idx], amount_of_2);
  }
}

void add_CRGB_arrays(CRGB *out, const CRGB *in_1, const CRGB *in_2,
                     uint16_t numel) {
  for (uint16_t idx = 0; idx < numel; idx++) {
    out[idx] = in_1[idx] + in_2[idx];
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
  `idx_delay` determines the heart rate and can be changed at runtime.
------------------------------------------------------------------------------*/
#define ECG_N_SMP 255 // max uint8_t

namespace ECG {
  static float wave[ECG_N_SMP] = {0};
  static uint8_t idx = 0;
} // namespace ECG

void generate_HeartBeat() {
  // Generate ECG wave data, output range [0 - 1]. Note that the `resting`
  // state of the heart is somewhere above 0. 0 is simply the minimum of the
  // ECG action potential.
  generate_ECG(ECG::wave, ECG_N_SMP);
}

void enter__HeartBeat() {
  // segmntr.set_style(StyleEnum::FULL_STRIP);
  create_leds_snapshot();
  fx_timebase = millis();
}

void update__HeartBeat() {
  s = segmntr.get_base_numel();

  switch (2) {
    case 1:
      // Nice with segment style 5
      fadeToBlackBy(ledfx, s, 8);
      ECG::idx = beat8(30, fx_timebase);
      idx = round((1 - ECG::wave[ECG::idx]) * (s - 1));
      ledfx[idx] += CHSV(HUE_RED, 255, uint8_t(ECG::wave[ECG::idx] * 200));
      segmntr.process(ledfx_strip, ledfx);

      fadeToBlackBy(leds_snapshot, FastLEDConfig::N, 4);
      add_CRGB_arrays(leds, leds_snapshot, ledfx_strip, FastLEDConfig::N);
      break;

    case 2:
      fadeToBlackBy(ledfx, s, 8);
      ECG::idx = beat8(30, fx_timebase);
      for (idx = 0; idx < s; idx++) {
        uint8_t intens = round(ECG::wave[ECG::idx] * 100);
        if (intens > 15) {
          ledfx[idx] += CHSV(HUE_RED, 255, intens);
        }
      }
      segmntr.process(ledfx_strip, ledfx);

      fadeToBlackBy(leds_snapshot, FastLEDConfig::N, 4);
      add_CRGB_arrays(leds, leds_snapshot, ledfx_strip, FastLEDConfig::N);
      break;
  }
}

State state__HeartBeat("HeartBeat", enter__HeartBeat, update__HeartBeat);

/*------------------------------------------------------------------------------
  Rainbow

  FastLED's built-in rainbow generator
------------------------------------------------------------------------------*/

void enter__Rainbow() {
  // segmntr.set_style(StyleEnum::PERIO_OPP_CORNERS_N4);
  // segmntr.set_style(StyleEnum::COPIED_SIDES);
}

void update__Rainbow() {
  s = segmntr.get_base_numel();
  fill_rainbow(ledfx, s, fx_hue, 255 / (s - 1));
  fx_hue_step = beatsin16(10, 1, 20);
  segmntr.process(leds, ledfx);
}

State state__Rainbow("Rainbow", enter__Rainbow, update__Rainbow);

/*------------------------------------------------------------------------------
  Sinelon

  A colored dot sweeping back and forth, with fading trails
------------------------------------------------------------------------------*/

void enter__Sinelon() {
  // segmntr.set_style(StyleEnum::BI_DIR_SIDE2SIDE);
}

void update__Sinelon() {
  s = segmntr.get_base_numel();
  fadeToBlackBy(ledfx, s, 4);
  idx = beatsin16(13, 0, s);
  ledfx[idx] += CHSV(fx_hue, 255, 255); // fx_hue, 255, 192
  segmntr.process(leds, ledfx);
}

State state__Sinelon("Sinelon", enter__Sinelon, update__Sinelon);

/*------------------------------------------------------------------------------
  BPM

  Colored stripes pulsing at a defined beats-per-minute
------------------------------------------------------------------------------*/

void enter__BPM() {
  // segmntr.set_style(StyleEnum::HALFWAY_PERIO_SPLIT_N2);
}

void update__BPM() {
  s = segmntr.get_base_numel();
  CRGBPalette16 palette = PartyColors_p; // RainbowColors_p; // PartyColors_p;
  uint8_t bpm_ = 30;
  uint8_t beat = beatsin8(bpm_, 64, 255);
  for (idx = 0; idx < s; idx++) {
    ledfx[idx] = ColorFromPalette(palette, fx_hue + 128. / (s - 1) * idx,
                                  beat + 127. / (s - 1) * idx);
  }
  segmntr.process(leds, ledfx);
}

State state__BPM("BPM", enter__BPM, update__BPM);

/*------------------------------------------------------------------------------
  Juggle

  8 colored dots, weaving in and out of sync with each other
------------------------------------------------------------------------------*/

void enter__Juggle() {
  // segmntr.set_style(StyleEnum::PERIO_OPP_CORNERS_N4);
}

void update__Juggle() {
  s = segmntr.get_base_numel();
  byte dothue = 0;
  fadeToBlackBy(ledfx, s, 20);
  for (int i = 0; i < 8; i++) {
    ledfx[beatsin16(i + 7, 0, s - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
  segmntr.process(leds, ledfx);
}

State state__Juggle("Juggle", enter__Juggle, update__Juggle);

/*------------------------------------------------------------------------------
  FullWhite
------------------------------------------------------------------------------*/

void enter__FullWhite() {
  // segmntr.set_style(StyleEnum::FULL_STRIP);
}

void update__FullWhite() {
  s = segmntr.get_base_numel();
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
  // segmntr.set_style(StyleEnum::FULL_STRIP);
}

void update__Strobe() {
  s = segmntr.get_base_numel();
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
  // segmntr.set_style(StyleEnum::HALFWAY_PERIO_SPLIT_N2);
  // fill_solid(ledfx, FastLEDConfig::N, CRGB::Black);
  fill_solid(ledfx, FastLEDConfig::N, CRGB::Black);
  fill_rainbow(leds_snapshot, FastLEDConfig::N, fx_hue,
               255 / (FastLEDConfig::N - 1));
  segmntr.set_style(StyleEnum::UNI_DIR_SIDE2SIDE);

  fx_timebase = millis();
  effect_is_at_startup = true;
}

void update__Dennis() {
  s = segmntr.get_base_numel();
  // static uint8_t blend_amount = 0;

  if (0) {
    // fill_solid(ledfx, FastLEDConfig::N, CRGB::Black);
    fadeToBlackBy(ledfx, FastLEDConfig::N, 16);

    idx = beatsin16(15, 0, s - 1);
    ledfx[idx] = CRGB::Red;
    ledfx[s - idx - 1] = CRGB::OrangeRed;
    segmntr.process(leds, ledfx);

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

    fadeToBlackBy(ledfx, s, 12); // 16

    idx = beatsin16(15, 0, s - 1, fx_timebase); // 15
    ledfx[idx] = CRGB::Red;
    ledfx[s - idx - 1] = CRGB::OrangeRed;
    segmntr.process(ledfx_strip, ledfx);

    fadeToBlackBy(leds_snapshot, FastLEDConfig::N, 1);
    blend_CRGB_arrays(leds, leds_snapshot, ledfx_strip, FastLEDConfig::N, 127);
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
  // segmntr.set_style(StyleEnum::COPIED_SIDES);
}

void update__TestPattern() {
  s = segmntr.get_base_numel();
  for (idx = 0; idx < s; idx++) {
    ledfx[idx] = (idx % 2 ? CRGB::Blue : CRGB::Yellow);
  }
  ledfx[0] = CRGB::Green;
  ledfx[s - 1] = CRGB::Red;
  segmntr.process(leds, ledfx);
}

State state__TestPattern("TestPattern", enter__TestPattern,
                         update__TestPattern);

#endif