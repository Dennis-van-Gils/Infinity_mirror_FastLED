/* Infinity mirror

Author: Dennis van Gils
Date: 26-09-2020
*/

#include <Arduino.h>

#include "DvG_ECG_simulation.h"
#include "DvG_SerialCommand.h"
#include "FastLED.h"
#include "Streaming.h"

#include "DvG_LED_Segmentor.h"
#include "LEDStripConfig.h"

FASTLED_USING_NAMESPACE

#define Ser Serial
DvG_SerialCommand sc(Ser); // Instantiate serial command listener

uint16_t idx; // LED position index used in many for-loops

// uint16_t s =
//     LEDStripConfig::L;        // Number of LEDs of the current segmenting
//     subset
//
// CRGB leds[LEDStripConfig::N];     // LED data: subset
CRGB leds[LEDStripConfig::N];     // LED data: subset
CRGB leds_all[LEDStripConfig::N]; // LED data: full strip

#define LED_TYPE APA102
#define COLOR_ORDER BGR

#define BRIGHTNESS 64         // 128
#define FRAMES_PER_SECOND 120 // 120

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// ECG simulation
#define ECG_N_SMP 100
float ecg_wave[ECG_N_SMP];

LEDStripSegmentor test;

/*-----------------------------------------------------------------------------
  Patterns
------------------------------------------------------------------------------*/

// List of patterns to cycle through
typedef void (*PatternList[])(uint32_t s);
uint8_t iPattern = 0; // Index number of the current pattern
uint8_t iHue = 0;     // Rotating "base color" used by many of the patterns
uint16_t ecg_wave_idx = 0;

void rainbow(uint32_t s) {
  // FastLED's built-in rainbow generator
  fill_rainbow(leds, s, iHue, 255 / (s - 1));
}

void sinelon(uint32_t s) {
  // A colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, s, 4);
  idx = beatsin16(13, 0, s);
  leds[idx] += CHSV(iHue, 255, 255); // iHue, 255, 192
}

void bpm(uint32_t s) {
  // Colored stripes pulsing at a defined beats-per-minute
  CRGBPalette16 palette = PartyColors_p; // RainbowColors_p; // PartyColors_p;
  uint8_t bpm_ = 30;
  uint8_t beat = beatsin8(bpm_, 64, 255);
  for (idx = 0; idx < s; idx++) {
    leds[idx] = ColorFromPalette(palette, iHue + 128. / (s - 1) * idx,
                                 beat + 127. / (s - 1) * idx);
  }
}

void juggle(uint32_t s) {
  // 8 colored dots, weaving in and out of sync with each other
  byte dothue = 0;
  fadeToBlackBy(leds, s, 20);
  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, s - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void full_white(uint32_t s) { fill_solid(leds, s, CRGB::White); }

void strobe(uint32_t s) {
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

void dennis(uint32_t s) {
  fadeToBlackBy(leds, s, 16);
  idx = beatsin16(15, 0, s - 1);
  // leds[idx] = CRGB::Blue;
  // leds[s - idx - 1] = CRGB::Blue;
  leds[idx] = CRGB::Red;
  leds[s - idx - 1] = CRGB::OrangeRed;
}

void test_pattern(uint32_t s) {
  for (idx = 0; idx < s; idx++) {
    leds[idx] = (idx % 2 ? CRGB::Blue : CRGB::Yellow);
  }
  leds[0] = CRGB::Red;
  leds[s - 1] = CRGB::Green;
}

void heart_beat(uint32_t s) {
  uint8_t iTry = 4;

  switch (iTry) {
    case 1:
      fadeToBlackBy(leds, s, 8);
      // idx = ecg_wave_idx * s / (ECG_N_SMP - 1);
      idx = round((1 - ecg_wave[ecg_wave_idx]) * (s - 1));
      leds[idx] += CHSV(CRGB::Red, 255, uint8_t(ecg_wave[ecg_wave_idx] * 200));
      break;

    case 2:
      fadeToBlackBy(leds, s, 8);
      for (idx = 0; idx < s; idx++) {
        leds[idx] +=
            CHSV(CRGB::Red, 255, 0 + uint8_t(ecg_wave[ecg_wave_idx] * 100));
      }
      break;

    case 3:
      fadeToBlackBy(leds, s, 8);
      for (idx = 0; idx < s; idx++) {
        uint8_t intens = round(ecg_wave[ecg_wave_idx] * 100);
        if (intens > 30) {
          leds[idx] += CHSV(HUE_RED, 255, intens);
        }
      }
      break;

    case 4:
      fadeToBlackBy(leds, s, 8);
      for (idx = 0; idx < s; idx++) {
        uint8_t intens = round(ecg_wave[ecg_wave_idx] * 100);
        if (intens > 15) {
          leds[idx] += CHSV(HUE_RED, 255, intens);
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
  Ser << "pattern: " << iPattern << endl;
}

/*-----------------------------------------------------------------------------
  setup
------------------------------------------------------------------------------*/

void setup() {
  uint32_t tick = millis();
  Ser.begin(9600);

  for (idx = 0; idx < LEDStripConfig::N; idx++) {
    leds[idx] = CRGB::White;
    leds_all[idx] = CRGB::White;
  }

  // ECG simulation
  generate_ECG(ecg_wave, ECG_N_SMP); // data is scaled [0 - 1]
  Ser.println("ECG ready");

  // Ensure a minimum delay for recovery of FastLED
  while (millis() - tick < 3000) {
    delay(10);
  }

  FastLED
      .addLeds<LED_TYPE, LEDStripConfig::DATA_PIN, LEDStripConfig::CLK_PIN,
               COLOR_ORDER, DATA_RATE_MHZ(1)>(leds_all, LEDStripConfig::N)
      .setCorrection(TypicalLEDStrip);

  FastLED.setBrightness(BRIGHTNESS);

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
      test.next_style_in_list();

      char style_descr[64] = {"\0"};
      test.get_style_descr(style_descr);
      Ser.print((int)test.get_style());
      Ser.print(" - ");
      Ser.println(style_descr);

    } else if (strcmp(strCmd, "[") == 0) {
      test.prev_style_in_list();

      char style_descr[64] = {"\0"};
      test.get_style_descr(style_descr);
      Ser.print((int)test.get_style());
      Ser.print(" - ");
      Ser.println(style_descr);

    } else if (strcmp(strCmd, "p") == 0) {
      next_pattern();
    }
  }

  // Call the current pattern function
  pattern_list[iPattern](test.get_s());

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
    full_white(test.get_s());
  }

  test.process(leds, leds_all);

  // Send out the LED data
  FastLED.show();

  // Keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);

  // Periodic updates
  EVERY_N_MILLISECONDS(30) { iHue++; }
  EVERY_N_MILLISECONDS(20) { ecg_wave_idx = (ecg_wave_idx + 1) % ECG_N_SMP; }
  // EVERY_N_SECONDS(24) { next_pattern(); }
  // EVERY_N_SECONDS(10) { next_segment_style(); }
}
