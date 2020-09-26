/* Infinity mirror

L: number of LEDs on one side (CONSTANT, 13)
N: number of LEDs on the full strip (CONSTANT, 4 * 13 = 52)
s: number of LEDs of the current segmenting subset (variable)

Author: Dennis van Gils
Date: 26-09-2020
*/

#include <Arduino.h>
#include <FastLED.h>

#include "DvG_SerialCommand.h"
#define Ser Serial
DvG_SerialCommand sc(Ser);  // Instantiate serial command listener

FASTLED_USING_NAMESPACE

#define LED_TYPE APA102
#define COLOR_ORDER BGR
#define DATA_PIN PIN_SPI_MOSI
#define CLK_PIN PIN_SPI_SCK

#define L 13
#define N (4 * L)
uint16_t s = L;
CRGB leds[N];       // subset
CRGB leds_flip[N];  // subset flipped
CRGB leds_all[N];   // full strip

#define BRIGHTNESS 64          // 128
#define FRAMES_PER_SECOND 120  // 120

// List of patterns to cycle through.  Each is defined as a separate function
// below.
uint8_t gCurrentPatternNumber = 0;  // Index number of which pattern is current
uint8_t gHue = 0;  // rotating "base color" used by many of the patterns

typedef void (*SimplePatternList[])();

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

/*-----------------------------------------------------------------------------
  setup
------------------------------------------------------------------------------*/

void setup() {
  Ser.begin(9600);
  delay(3000);  // Delay for recovery FASTLED

  FastLED
      .addLeds<LED_TYPE, DATA_PIN, CLK_PIN, COLOR_ORDER, DATA_RATE_MHZ(1)>(
          leds_all, N)
      .setCorrection(TypicalLEDStrip);

  FastLED.setBrightness(BRIGHTNESS);

  // IR distance sensor
  analogReadResolution(16);
}

/*-----------------------------------------------------------------------------
  Patterns
------------------------------------------------------------------------------*/

void rainbow() {
  // FastLED's built-in rainbow generator
  fill_rainbow(leds, s, gHue,
               255 / s);  // leds, s, gHue, 4 or 26 or 39
}

void sinelon() {
  // A colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, s, 4);
  int pos = beatsin16(13, 0, s);
  leds[pos] += CHSV(gHue, 255, 255);  // gHue, 255, 192
}

void bpm() {
  // Colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 26;
  CRGBPalette16 palette = PartyColors_p;  // RainbowColors_p;//PartyColors_p;
  uint8_t beat = beatsin8(0, BeatsPerMinute, 52);  //  BeatsPerMinute, 64, 255
  for (int i = 0; i < s; i++) {
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 1));
  }
}

void juggle() {
  // 8 colored dots, weaving in and out of sync with each other
  fadeToBlackBy(leds, s, 20);
  byte dothue = 0;
  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, s)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void full_white() { fill_solid(leds, N, CRGB::White); }

void strobe() {
  if (gHue % 16) {
    fill_solid(leds, s, CRGB::Black);
  } else {
    CRGBPalette16 palette = PartyColors_p;  // PartyColors_p;
    for (int i = 0; i < s; i++) {           // 9948
      leds[i] = ColorFromPalette(palette, gHue + (i * 2), gHue + (i * 10));
    }
    // fill_solid(leds, s, CRGB::White);
  }
}

void dennis() {
  fadeToBlackBy(leds, s, 16);
  int pos = beatsin16(15, 0, s - 1);
  // leds[pos] = CRGB::Blue;
  // leds[s - pos - 1] = CRGB::Blue;
  leds[pos] = CRGB::Red;
  leds[s - pos - 1] = CRGB::OrangeRed;
}

/*
SimplePatternList gPatterns = {
    rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm};
SimplePatternList gPatterns = {rainbow, rainbowWithGlitter, confetti, sinelon,
                               juggle};
*/
// SimplePatternList gPatterns = {sinelon, bpm, rainbow};
// SimplePatternList gPatterns = {dennis};
SimplePatternList gPatterns = {sinelon};

void nextPattern() {
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE(gPatterns);
}

/*-----------------------------------------------------------------------------
  loop
------------------------------------------------------------------------------*/

void loop() {
  char *strCmd;  // Incoming serial command string

  // Process serial commands
  if (sc.available()) {
    strCmd = sc.getCmd();

    if (strcmp(strCmd, "id?") == 0) {
      Ser.println("Arduino, Infinity mirror");
    }
  }

  // Call the current pattern function
  gPatterns[gCurrentPatternNumber]();

  /* IR distance sensor
    Sharp 2Y0A02
    20 - 150 cm --> ~2.5 - ~0.4 V
  */
  static float A0_V = 0.;
  static bool IR_switch = false;
  EVERY_N_MILLISECONDS(100) {
    A0_V = analogRead(PIN_A0) / 65535. * 3.3;
    IR_switch = (A0_V > 2);
  }
  if (IR_switch) {
    full_white();
  }

  // Perform LED-strip segmenting
  uint16_t idx;
  uint16_t num_bytes;
  static uint8_t segmenting_style = 5;
  switch (segmenting_style) {
    case 0:
      /* Copied sides

          D C B A
        A         D
        B         C
        C         B
        D         A
          A B C D
      */
      s = L;
      num_bytes = L * sizeof(CRGB);
      memmove(&leds_all[0], &leds[0], num_bytes);      // bottom
      memmove(&leds_all[L], &leds[0], num_bytes);      // right
      memmove(&leds_all[L * 2], &leds[0], num_bytes);  // top
      memmove(&leds_all[L * 3], &leds[0], num_bytes);  // left
      break;

    case 1:
      /* Periodic opposite corners, N = 4

          D C B A
        D         A
        C         B
        B         C
        A         D
          A B C D
      */
      s = L;
      for (idx = 0; idx < s; idx++) {
        memmove(&leds_flip[idx], &leds[s - idx - 1], sizeof(CRGB));
      }

      num_bytes = L * sizeof(CRGB);
      memmove(&leds_all[0], &leds[0], num_bytes);           // bottom
      memmove(&leds_all[L], &leds_flip[0], num_bytes);      // right
      memmove(&leds_all[L * 2], &leds[0], num_bytes);       // top
      memmove(&leds_all[L * 3], &leds_flip[0], num_bytes);  // left
      break;

    case 2:
      /* Periodic opposite corners, N = 2

          E F G H
        D         H
        C         G
        B         F
        A         E
          A B C D
      */
      s = L * 2;
      memmove(&leds_all[0], &leds[0], s * sizeof(CRGB));  // bottom & right
      for (idx = 0; idx < s; idx++) {                     // top & left
        memmove(&leds_all[L * 2 + idx], &leds[s - idx - 1], sizeof(CRGB));
      }
      break;

    case 3:
      /* Uni-directional side-to-side

          F F F F
        E         E
        D         D
        C         C
        B         B
          A A A A
      */
      s = L + 2;
      for (idx = 0; idx < L; idx++) {
        memmove(&leds_all[idx], &leds[0], sizeof(CRGB));              // bottom
        memmove(&leds_all[L * 2 + idx], &leds[L + 1], sizeof(CRGB));  // top
        memmove(&leds_all[L * 3 + idx], &leds[L - idx], sizeof(CRGB));  // left
      }
      memmove(&leds_all[L], &leds[1], L * sizeof(CRGB));  // right
      break;

    case 4:
      /* Bi-directional side-to-side

          A A A A
        B         B
        C         C
        C         C
        B         B
          A A A A
      */
      s = (L + 1) / 2 + 1;  // Note: Relies on integer math! No residuals.
      // L = 4 -> s = 3
      // L = 5 -> s = 4
      // L = 6 -> s = 4
      // L = 7 -> s = 5
      for (idx = 0; idx < L; idx++) {
        memmove(&leds_all[idx], &leds[0], sizeof(CRGB));  // bottom
        memmove(&leds_all[L + idx], &leds[(idx < (L / 2) ? idx + 1 : L - idx)],
                sizeof(CRGB));                                    // right
        memmove(&leds_all[L * 2 + idx], &leds[0], sizeof(CRGB));  // top
        memmove(&leds_all[L * 3 + idx], &leds_all[L + idx],
                sizeof(CRGB));  // left
      }
      break;

    case 5:
      /* Half-way periodic split, N = 2

          B A A B
        C         C
        D         D
        D         D
        C         C
          B A A B

        2 3 4 5    6 7 8 9   10111213   1415 0 1
        A B C D    D C B A    A B C D    D C B A

        2 3 4 5 6  7 8 91011 1213141516 171819 0 1
        A B C D E  F E D C B  A B C D E  F E D C B
      */
      s = ((L + 1) / 2) * 2;  // Note: Relies on integer math! No residuals.
      // L = 4 -> s = 4
      // L = 5 -> s = 6
      // L = 6 -> s = 6
      // L = 7 -> s = 8
      for (idx = 0; idx < s; idx++) {
        memmove(&leds_flip[idx], &leds[s - idx - 1], sizeof(CRGB));
      }

      memmove(&leds_all[0], &leds_flip[s / 2],
              L / 2 * sizeof(CRGB));  // bottom-left
      memmove(&leds_all[L / 2], &leds[0],
              L * sizeof(CRGB));  // bottom-right & right-bottom
      memmove(&leds_all[L + L / 2], &leds_flip[0],
              s / 2 * sizeof(CRGB));                              // right-top
      memmove(&leds_all[L * 2], &leds_all[0], L * sizeof(CRGB));  // top
      memmove(&leds_all[L * 3], &leds_all[L], L * sizeof(CRGB));  // left
      break;
  }

  // Send out the LED data
  FastLED.show();

  // Keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);

  // Periodic updates
  EVERY_N_MILLISECONDS(30) { gHue++; }
  // EVERY_N_SECONDS(24) { nextPattern(); }

  //*
  EVERY_N_SECONDS(10) {
    segmenting_style++;
    if (segmenting_style > 5) {
      segmenting_style = 0;
    }
  }
  //*/
}
