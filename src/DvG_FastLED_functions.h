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

#endif