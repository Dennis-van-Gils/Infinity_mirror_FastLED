#include <Arduino.h>
#include <FastLED.h>

#include <algorithm>
using namespace std;

#include "DvG_SerialCommand.h"
#define Ser Serial
DvG_SerialCommand sc(Ser);  // Instantiate serial command listener

FASTLED_USING_NAMESPACE

#define LED_TYPE APA102
#define COLOR_ORDER BGR
#define DATA_PIN PIN_SPI_MOSI
#define CLK_PIN PIN_SPI_SCK

#define NUM_LEDS_ONE_SIDE 13
#define NUM_LEDS_FULL_STRIP 52
uint16_t num_leds_subset = 13;
CRGB leds[NUM_LEDS_FULL_STRIP];      // subset
CRGB leds_rev[NUM_LEDS_FULL_STRIP];  // subset reversed order
CRGB leds_all[NUM_LEDS_FULL_STRIP];  // full strip

#define BRIGHTNESS 128
#define FRAMES_PER_SECOND 120  // 120

// List of patterns to cycle through.  Each is defined as a separate function
// below.
uint8_t gCurrentPatternNumber = 0;  // Index number of which pattern is current
uint8_t gHue = 0;  // rotating "base color" used by many of the patterns

typedef void (*SimplePatternList[])();

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void setup() {
  // Ser.begin(9600);

  delay(3000);  // 3 second delay for recovery FASTLED

  // tell FastLED about the LED strip configuration
  FastLED
      .addLeds<LED_TYPE, DATA_PIN, CLK_PIN, COLOR_ORDER, DATA_RATE_MHZ(1)>(
          leds_all, NUM_LEDS_FULL_STRIP)
      .setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

  // IR distance sensor
  analogReadResolution(16);
}

void rainbow() {
  // FastLED's built-in rainbow generator
  fill_rainbow(leds, num_leds_subset, gHue,
               4);  // leds, num_leds_subset, gHue, 26 or 39
}

void sinelon() {
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, num_leds_subset, 4);
  int pos = beatsin16(13, 0, num_leds_subset);
  leds[pos] += CHSV(gHue, 255, 255);  // gHue, 255, 192
}

void bpm() {
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 26;
  CRGBPalette16 palette = PartyColors_p;  // RainbowColors_p;//PartyColors_p;
  uint8_t beat = beatsin8(0, BeatsPerMinute, 52);  //  BeatsPerMinute, 64, 255
  for (int i = 0; i < num_leds_subset; i++) {
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 1));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy(leds, num_leds_subset, 20);
  byte dothue = 0;
  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, num_leds_subset)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void full_white() { fill_solid(leds, NUM_LEDS_FULL_STRIP, CRGB::White); }

void strobe() {
  if (gHue % 16) {
    fill_solid(leds, num_leds_subset, CRGB::Black);
  } else {
    CRGBPalette16 palette = PartyColors_p;       // PartyColors_p;
    for (int i = 0; i < num_leds_subset; i++) {  // 9948
      leds[i] = ColorFromPalette(palette, gHue + (i * 2), gHue + (i * 10));
    }
    // fill_solid(leds, num_leds_subset, CRGB::White);
  }
}

void dennis() {
  fadeToBlackBy(leds, num_leds_subset, 16);
  int pos = beatsin16(15, 0, num_leds_subset - 1);
  leds[pos] = CRGB::Red;
  leds[num_leds_subset - pos - 1] = CRGB::OrangeRed;
}

/*
SimplePatternList gPatterns = {
    rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm};
SimplePatternList gPatterns = {rainbow, rainbowWithGlitter, confetti, sinelon,
                               juggle};
*/
// SimplePatternList gPatterns = {sinelon, bpm, rainbow};
SimplePatternList gPatterns = {dennis};

void nextPattern() {
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE(gPatterns);
}

/*-----------------------------------------------------------------------------
    loop
  ----------------------------------------------------------------------------*/

void loop() {
  char *strCmd;  // Incoming serial command string

  if (sc.available()) {
    strCmd = sc.getCmd();

    if (strcmp(strCmd, "id?") == 0) {
      Ser.println("Arduino, Infinity mirror");
    }
  }

  // Call the current pattern function once, updating the 'leds' array
  gPatterns[gCurrentPatternNumber]();

  // IR distance sensor
  // Sharp 2Y0A02
  // 20 - 150 cm
  static float A0_V = 0.;
  static bool IR_switch = false;
  EVERY_N_MILLISECONDS(100) {
    A0_V = analogRead(PIN_A0) / 65535. * 3.3;
    IR_switch = (A0_V > 2);
  }
  if (IR_switch) {
    full_white();
  }

  uint16_t num_bytes;
  uint8_t segmenting_style = 2;
  switch (segmenting_style) {
    case 0:
      num_leds_subset = NUM_LEDS_ONE_SIDE;

      num_bytes = NUM_LEDS_ONE_SIDE * sizeof(CRGB);
      memmove(&leds_all[0], &leds[0], num_bytes);
      memmove(&leds_all[NUM_LEDS_ONE_SIDE], &leds[0], num_bytes);
      memmove(&leds_all[NUM_LEDS_ONE_SIDE * 2], &leds[0], num_bytes);
      memmove(&leds_all[NUM_LEDS_ONE_SIDE * 3], &leds[0], num_bytes);
      break;

    case 1:
      num_leds_subset = NUM_LEDS_ONE_SIDE;

      for (uint8_t idx = 0; idx < num_leds_subset; idx++) {
        memmove(&leds_rev[idx], &leds[num_leds_subset - idx - 1], sizeof(CRGB));
      }

      num_bytes = NUM_LEDS_ONE_SIDE * sizeof(CRGB);
      memmove(&leds_all[0], &leds[0], num_bytes);
      memmove(&leds_all[NUM_LEDS_ONE_SIDE], &leds_rev[0], num_bytes);
      memmove(&leds_all[NUM_LEDS_ONE_SIDE * 2], &leds[0], num_bytes);
      memmove(&leds_all[NUM_LEDS_ONE_SIDE * 3], &leds_rev[0], num_bytes);
      break;

    case 2:
      num_leds_subset = NUM_LEDS_ONE_SIDE + 2;

      // Bottom side
      for (uint8_t idx = 0; idx < NUM_LEDS_ONE_SIDE; idx++) {
        memmove(&leds_all[idx], &leds[0], sizeof(CRGB));
      }
      // Right side
      memmove(&leds_all[NUM_LEDS_ONE_SIDE], &leds[1],
              NUM_LEDS_ONE_SIDE * sizeof(CRGB));
      // Top side
      for (uint8_t idx = 0; idx < NUM_LEDS_ONE_SIDE; idx++) {
        memmove(&leds_all[NUM_LEDS_ONE_SIDE * 2 + idx],
                &leds[NUM_LEDS_ONE_SIDE + 1], sizeof(CRGB));
      }
      // Left side
      for (uint8_t idx = 0; idx < NUM_LEDS_ONE_SIDE; idx++) {
        memmove(&leds_all[NUM_LEDS_ONE_SIDE * 3 + idx],
                &leds[NUM_LEDS_ONE_SIDE - idx], sizeof(CRGB));
      }

      break;
  }

  FastLED.show();

  // Keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);

  // Periodic updates
  EVERY_N_MILLISECONDS(20) { gHue++; }
  EVERY_N_SECONDS(24) { nextPattern(); }
}
