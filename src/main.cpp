/* Infinity mirror

Dennis van Gils
16-11-2021
*/

#include <Arduino.h>

#include "FastLED.h"
#include "FiniteStateMachine.h"

#include "DvG_ECG_simulation.h"
#include "DvG_FastLED_StripSegmentor.h"
#include "DvG_FastLED_config.h"
#include "DvG_SerialCommand.h"

FASTLED_USING_NAMESPACE

#define Ser Serial
DvG_SerialCommand sc(Ser); // Instantiate serial command listener

uint16_t idx; /* LED position index used in many for-loops */
CRGB leds_effect[FastLEDConfig::N]; /* LED data, subset of the strip containing
                                        the effects */
CRGB leds_strip[FastLEDConfig::N];  /* LED data of the full strip */

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// ECG simulation
#define ECG_N_SMP 100
float ecg_wave[ECG_N_SMP];

FastLED_StripSegmentor segmentor;

/*-----------------------------------------------------------------------------
  Patterns
------------------------------------------------------------------------------*/

// List of patterns to cycle through
typedef void (*PatternList[])(uint16_t s);
uint8_t iPattern = 0; // Index number of the current pattern
uint8_t iHue = 0;     // Rotating "base color" used by many of the patterns
uint16_t ecg_wave_idx = 0;

void rainbow(uint16_t s) {
  // FastLED's built-in rainbow generator
  fill_rainbow(leds_effect, s, iHue, 255 / (s - 1));
}

void sinelon(uint16_t s) {
  // A colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds_effect, s, 4);
  idx = beatsin16(13, 0, s);
  leds_effect[idx] += CHSV(iHue, 255, 255); // iHue, 255, 192
}

void bpm(uint16_t s) {
  // Colored stripes pulsing at a defined beats-per-minute
  CRGBPalette16 palette = PartyColors_p; // RainbowColors_p; // PartyColors_p;
  uint8_t bpm_ = 30;
  uint8_t beat = beatsin8(bpm_, 64, 255);
  for (idx = 0; idx < s; idx++) {
    leds_effect[idx] = ColorFromPalette(palette, iHue + 128. / (s - 1) * idx,
                                        beat + 127. / (s - 1) * idx);
  }
}

void juggle(uint16_t s) {
  // 8 colored dots, weaving in and out of sync with each other
  byte dothue = 0;
  fadeToBlackBy(leds_effect, s, 20);
  for (int i = 0; i < 8; i++) {
    leds_effect[beatsin16(i + 7, 0, s - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void full_white(uint16_t s) { fill_solid(leds_effect, s, CRGB::White); }

void strobe(uint16_t s) {
  if (iHue % 16) {
    fill_solid(leds_effect, s, CRGB::Black);
  } else {
    CRGBPalette16 palette = PartyColors_p; // PartyColors_p;
    for (idx = 0; idx < s; idx++) {        // 9948
      leds_effect[idx] =
          ColorFromPalette(palette, iHue + (idx * 2), iHue + (idx * 10));
    }
    // fill_solid(leds_effect, s, CRGB::White);
  }
}

void dennis(uint16_t s) {
  fadeToBlackBy(leds_effect, s, 16);
  idx = beatsin16(15, 0, s - 1);
  // leds_effect[idx] = CRGB::Blue;
  // leds_effect[s - idx - 1] = CRGB::Blue;
  leds_effect[idx] = CRGB::Red;
  leds_effect[s - idx - 1] = CRGB::OrangeRed;
}

void test_pattern(uint16_t s) {
  for (idx = 0; idx < s; idx++) {
    leds_effect[idx] = (idx % 2 ? CRGB::Blue : CRGB::Yellow);
  }
  leds_effect[0] = CRGB::Red;
  leds_effect[s - 1] = CRGB::Green;
}

void heart_beat(uint16_t s) {
  uint8_t iTry = 4;

  switch (iTry) {
    case 1:
      // Nice with segment style 5
      fadeToBlackBy(leds_effect, s, 8);
      // idx = ecg_wave_idx * s / (ECG_N_SMP - 1);
      idx = round((1 - ecg_wave[ecg_wave_idx]) * (s - 1));
      leds_effect[idx] +=
          CHSV(HUE_RED, 255, uint8_t(ecg_wave[ecg_wave_idx] * 200));
      break;

    case 2:
      fadeToBlackBy(leds_effect, s, 8);
      for (idx = 0; idx < s; idx++) {
        uint8_t intens = round(ecg_wave[ecg_wave_idx] * 100);
        leds_effect[idx] += CHSV(HUE_RED, 255, 0 + intens);
      }
      break;

    case 3:
      fadeToBlackBy(leds_effect, s, 8);
      for (idx = 0; idx < s; idx++) {
        uint8_t intens = round(ecg_wave[ecg_wave_idx] * 100);
        if (intens > 30) {
          leds_effect[idx] += CHSV(HUE_RED, 255, intens);
        }
      }
      break;

    case 4:
      fadeToBlackBy(leds_effect, s, 8);
      for (idx = 0; idx < s; idx++) {
        uint8_t intens = round(ecg_wave[ecg_wave_idx] * 100);
        if (intens > 15) {
          leds_effect[idx] += CHSV(HUE_RED, 255, intens);
        }
      }
      break;
  }
}

// PatternList pattern_list = {rainbow, sinelon, juggle, bpm};
// PatternList pattern_list = {sinelon, bpm, rainbow};
// PatternList pattern_list = {dennis};
PatternList pattern_list = {heart_beat, rainbow, sinelon,     bpm,
                            juggle,     dennis,  test_pattern};

void next_pattern() {
  iPattern = (iPattern + 1) % ARRAY_SIZE(pattern_list);
  Ser.print("pattern: ");
  Ser.println(iPattern);
}

/*-----------------------------------------------------------------------------
  setup
------------------------------------------------------------------------------*/

void setup() {
  uint32_t tick = millis();
  Ser.begin(9600);

  // ECG simulation
  generate_ECG(ecg_wave, ECG_N_SMP); // data is scaled [0 - 1]
  Ser.println("ECG ready");

  // Ensure a minimum delay for recovery of FastLED
  while (millis() - tick < 3000) {
    delay(10);
  }

  fill_solid(leds_effect, FastLEDConfig::N, CRGB::Black);
  fill_solid(leds_strip, FastLEDConfig::N, CRGB::Black);

  FastLED
      .addLeds<FastLEDConfig::LED_TYPE, FastLEDConfig::PIN_DATA,
               FastLEDConfig::PIN_CLK, FastLEDConfig::COLOR_ORDER,
               DATA_RATE_MHZ(1)>(leds_strip, FastLEDConfig::N)
      .setCorrection(FastLEDConfig::COLOR_CORRECTION);

  FastLED.setBrightness(FastLEDConfig::BRIGHTNESS);

  // IR distance sensor
  analogReadResolution(16);
}

/*-----------------------------------------------------------------------------
  loop
------------------------------------------------------------------------------*/

void loop() {
  char *strCmd; // Incoming serial command string

  // Process serial commands
  if (sc.available()) {
    strCmd = sc.getCmd();

    if (strcmp(strCmd, "id?") == 0) {
      Ser.println("Arduino, Infinity mirror");

    } else if (strcmp(strCmd, "]") == 0) {
      segmentor.next_style();
      segmentor.print_style_name(Ser);

    } else if (strcmp(strCmd, "[") == 0) {
      segmentor.prev_style();
      segmentor.print_style_name(Ser);

    } else if (strcmp(strCmd, "p") == 0) {
      next_pattern();
    }
  }

  // Calculate the current LED effect
  pattern_list[iPattern](segmentor.get_base_pattern_numel());

  /* IR distance sensor
    Sharp 2Y0A02
    20 - 150 cm --> ~2.5 - ~0.4 V
  */
  static bool IR_switch = false;
  EVERY_N_MILLISECONDS(100) {
    float A0_V = analogRead(PIN_A0) / 65535. * 3.3;
    IR_switch = (A0_V > 2);
  }
  if (IR_switch) {
    full_white(segmentor.get_base_pattern_numel());
  }

  segmentor.process(leds_effect, leds_strip);

  // Keep the framerate modest and allow for brightness dithering.
  // Will also invoke FASTLED.show() - sending out the LED data - at least once.
  FastLED.delay(FastLEDConfig::DELAY);

  // Periodic updates
  EVERY_N_MILLISECONDS(30) { iHue++; }
  EVERY_N_MILLISECONDS(20) { ecg_wave_idx = (ecg_wave_idx + 1) % ECG_N_SMP; }
  // EVERY_N_SECONDS(24) { next_pattern(); }
  /*
  EVERY_N_SECONDS(10) {
    segmentor.next_style();
    segmentor.print_style_name(Ser);
  }
  */
}
