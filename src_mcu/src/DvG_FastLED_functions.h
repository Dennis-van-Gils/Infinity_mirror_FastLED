/* DvG_FastLED_functions.h

To be included inside `DvG_FastLED_effects.h`

Dennis van Gils
16-04-2022
*/
#ifndef DVG_FASTLED_FUCNTIONS_H
#define DVG_FASTLED_FUCNTIONS_H

// External variables defined in `DvG_FastLED_effects.h`
extern CRGB leds[FLC::N];
extern CRGB leds_snapshot[FLC::N];
extern CRGB fx1[FLC::N];
extern CRGB fx2[FLC::N];
extern CRGB fx1_strip[FLC::N];
extern CRGB fx2_strip[FLC::N];
extern FastLED_StripSegmenter segmntr1;
extern FastLED_StripSegmenter segmntr2;

/*------------------------------------------------------------------------------
  CRGB array functions
------------------------------------------------------------------------------*/

void create_leds_snapshot() {
  memcpy8(leds_snapshot, leds, CRGB_SIZE * FLC::N);
}

void populate_fx1_strip() {
  segmntr1.process(fx1_strip, fx1);
}
void populate_fx2_strip() {
  segmntr2.process(fx2_strip, fx2);
}

void copy_strip(const CRGB *in, CRGB *out) {
  memcpy8(out, in, CRGB_SIZE * FLC::N);
}

void flip_strip(CRGB *in) {
  for (uint16_t idx = 0; idx < FLC::N / 2; idx++) {
    CRGB t = in[idx];
    in[idx] = in[FLC::N - idx - 1];
    in[FLC::N - idx - 1] = t;
  }
}

void rotate_strip_90(CRGB *in) {
  std::rotate(in, in + FLC::L, in + FLC::N);
}

void rotate_strip(CRGB *in, uint16_t amount) {
  std::rotate(in, in + amount, in + FLC::N);
}

void clear_CRGBs(CRGB *in) {
  fill_solid(in, FLC::N, CRGB::Black);
}

void add_CRGBs(const CRGB *in_1, const CRGB *in_2, CRGB *out, uint16_t numel) {
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

bool is_all_of_color(CRGB *in, uint32_t numel, const CRGB &target) {
  bool success = true;
  for (uint16_t idx = 0; idx < numel; idx++) {
    success &= (in[idx] == target);
  }
  return success;
}

uint8_t get_avg_luma(CRGB *in, uint32_t numel) {
  uint32_t avg_luma = 0;
  for (uint16_t idx = 0; idx < numel; idx++) {
    avg_luma += in[idx].getLuma();
  }
  return avg_luma / FLC::N;
}

/*------------------------------------------------------------------------------
  Guassian profile functions
------------------------------------------------------------------------------*/

void profile_gauss8strip(uint8_t gauss8[FLC::N], uint16_t mu, float sigma) {
  /* Calculates a Gaussian profile with output range [0 255] over the full strip
  using periodic boundaries, i.e. wrapping around the strip.
  Fast, because `mu` is integer.
  */

  sigma = sigma <= 0 ? 0.01 : sigma;

  // Calculate the left-side (zero included) Gaussian centered at the middle of
  // the strip
  for (uint16_t idx = 0; idx <= FLC::N / 2; idx++) {
    gauss8[idx] = exp(-pow(((idx - FLC::N / 2) / sigma), 2.f) / 2.f) * 255;
  }

  // Exploit mirror symmetry
  for (uint16_t idx = 0; idx < FLC::N / 2 - 1; idx++) {
    gauss8[FLC::N / 2 + idx + 1] = gauss8[FLC::N / 2 - idx - 1];
  }

  // Rotate gaussian array to the correct mu
  std::rotate(gauss8, gauss8 + (FLC::N * 3 / 2 - mu) % FLC::N, gauss8 + FLC::N);
}

void profile_gauss8strip(uint8_t gauss8[FLC::N], float mu, float sigma) {
  /* Calculates a Gaussian profile with output range [0 255] over the full strip
  using periodic boundaries, i.e. wrapping around the strip.
  Slow, but with sub-pixel accuracy on `mu`.
  */

  sigma = sigma <= 0 ? 0.01 : sigma;
  uint16_t mu_rounded = round(mu);
  float mu_remainder = mu - mu_rounded;

  // Calculate the Gaussian centered at the near middle of the strip
  for (int16_t idx = 0; idx < FLC::N; idx++) {
    gauss8[idx] =
        exp(-pow(((idx - FLC::N / 2 - mu_remainder) / sigma), 2.f) / 2.f) * 255;
  }

  // Rotate gaussian array to the correct mu
  std::rotate(gauss8, gauss8 + (FLC::N * 3 / 2 - mu_rounded) % FLC::N,
              gauss8 + FLC::N);
}

/*------------------------------------------------------------------------------
  fadeTowardColor
  Source: https://gist.github.com/kriegsman/d0a5ed3c8f38c64adcb4837dafb6e690

  - Fade one RGB color toward a target RGB color.
  - Fade a whole array of pixels toward a given color.

  Both of these functions _modify_ the existing color, in place.
  All fades are done in RGB color space.

  Mark Kriegsman
  December 2016
------------------------------------------------------------------------------*/

// Helper function that blends one uint8_t toward another by a given amount
void nblendU8TowardU8(uint8_t &cur, const uint8_t target, uint8_t amount) {
  if (cur == target)
    return;

  if (cur < target) {
    uint8_t delta = target - cur;
    delta = scale8_video(delta, amount);
    cur += delta;
  } else {
    uint8_t delta = cur - target;
    delta = scale8_video(delta, amount);
    cur -= delta;
  }
}

// Blend one CRGB color toward another CRGB color by a given amount.
// Blending is linear, and done in the RGB color space.
// This function modifies 'cur' in place.
CRGB fadeTowardColor(CRGB &cur, const CRGB &target, uint8_t amount) {
  nblendU8TowardU8(cur.red, target.red, amount);
  nblendU8TowardU8(cur.green, target.green, amount);
  nblendU8TowardU8(cur.blue, target.blue, amount);
  return cur;
}

// Fade an entire array of CRGBs toward a given background color by a given
// amount This function modifies the pixel array in place.
void fadeTowardColor(CRGB *L, uint16_t N, const CRGB &bgColor,
                     uint8_t fadeAmount) {
  for (uint16_t i = 0; i < N; i++) {
    fadeTowardColor(L[i], bgColor, fadeAmount);
  }
}

#endif