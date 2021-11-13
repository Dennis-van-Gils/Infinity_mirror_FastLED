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

FASTLED_USING_NAMESPACE

#define Ser Serial
DvG_SerialCommand sc(Ser);  // Instantiate serial command listener

#define LED_TYPE APA102
#define COLOR_ORDER BGR
#define DATA_PIN PIN_SPI_MOSI
#define CLK_PIN PIN_SPI_SCK

#define L 13        // Number of LEDs of one side
#define N (4 * L)   // Number of LEDs of the full strip (4 * 13 = 52)
uint16_t s = N;     // Number of LEDs of the current segmenting subset
CRGB leds[N];       // LED data: subset
CRGB leds_flip[N];  // LED data: subset flipped
CRGB leds_all[N];   // LED data: full strip
uint16_t idx;       // LED position index used in many for-loops

#define CRGB_SIZE   sizeof(CRGB)
#define CRGB_SIZE_L (L * CRGB_SIZE)

#define BRIGHTNESS 64         // 128
#define FRAMES_PER_SECOND 120  // 120

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// ECG simulation
#define ECG_N_SMP 100
float ecg_wave[ECG_N_SMP];

LEDStripSegmentor test;

/*-----------------------------------------------------------------------------
  Patterns
------------------------------------------------------------------------------*/

// List of patterns to cycle through
typedef void (*PatternList[])();
uint8_t iPattern = 0;  // Index number of the current pattern
uint8_t iHue = 0;      // Rotating "base color" used by many of the patterns
uint16_t ecg_wave_idx = 0;

void rainbow() {
  // FastLED's built-in rainbow generator
  fill_rainbow(leds, s, iHue, 255 / (s - 1));
}

void sinelon() {
  // A colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, s, 4);
  idx = beatsin16(13, 0, s);
  leds[idx] += CHSV(iHue, 255, 255);  // iHue, 255, 192
}

void bpm() {
  // Colored stripes pulsing at a defined beats-per-minute
  CRGBPalette16 palette = PartyColors_p;  // RainbowColors_p; // PartyColors_p;
  uint8_t bpm_ = 30;
  uint8_t beat = beatsin8(bpm_, 64, 255);
  for (idx = 0; idx < s; idx++) {
    leds[idx] = ColorFromPalette(
      palette, iHue + 128. / (s - 1) * idx, beat + 127. / (s - 1) * idx
    );
  }
}

void juggle() {
  // 8 colored dots, weaving in and out of sync with each other
  byte dothue = 0;
  fadeToBlackBy(leds, s, 20);
  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, s - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void full_white() {
  fill_solid(leds, N, CRGB::White);
}

void strobe() {
  if (iHue % 16) {
    fill_solid(leds, s, CRGB::Black);
  } else {
    CRGBPalette16 palette = PartyColors_p;  // PartyColors_p;
    for (idx = 0; idx < s; idx++) {           // 9948
      leds[idx] = ColorFromPalette(
        palette, iHue + (idx * 2), iHue + (idx * 10)
      );
    }
    // fill_solid(leds, s, CRGB::White);
  }
}

void dennis() {
  fadeToBlackBy(leds, s, 16);
  idx = beatsin16(15, 0, s - 1);
  // leds[idx] = CRGB::Blue;
  // leds[s - idx - 1] = CRGB::Blue;
  leds[idx] = CRGB::Red;
  leds[s - idx - 1] = CRGB::OrangeRed;
}

void test_pattern() {
  for (idx = 0; idx < s; idx++) {
    leds[idx] = (idx % 2 ? CRGB::Blue : CRGB::Yellow);
  }
  leds[0] = CRGB::Red;
  leds[s - 1] = CRGB::Green;
}

void heart_beat() {
  uint8_t iTry = 4;

  switch (iTry) {
    case 1:
      fadeToBlackBy(leds, s, 8);
      //idx = ecg_wave_idx * s / (ECG_N_SMP - 1);
      idx = round((1 - ecg_wave[ecg_wave_idx]) * (s - 1));
      leds[idx] += CHSV(CRGB::Red, 255, uint8_t(ecg_wave[ecg_wave_idx] * 200));
      break;

    case 2:
      fadeToBlackBy(leds, s, 8);
      for (idx = 0; idx < s; idx++) {
          leds[idx] += CHSV(CRGB::Red, 255, 0 + uint8_t(ecg_wave[ecg_wave_idx] * 100));
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

//PatternList pattern_list = {rainbow, sinelon, juggle, bpm};
//PatternList pattern_list = {sinelon, bpm, rainbow};
//PatternList pattern_list = {dennis};
PatternList pattern_list = {
  heart_beat, rainbow, sinelon, bpm, juggle, dennis, test_pattern
};

void next_pattern() {
  iPattern = (iPattern + 1) % ARRAY_SIZE(pattern_list);
  Ser << "pattern: " << iPattern << endl;
}

/*-----------------------------------------------------------------------------
  Segment styles
------------------------------------------------------------------------------*/

enum SegmentStyles {
  FULL_STRIP,
  COPIED_SIDES,
  PERIO_OPP_CORNERS_N4,
  PERIO_OPP_CORNERS_N2,
  UNI_DIR_SIDE2SIDE,
  BI_DIR_SIDE2SIDE,
  HALFWAY_PERIO_SPLIT_N2,
  EOL  // End of list
};

const char *segment_names[] = {"Full strip",
                               "Copied sides",
                               "Periodic opposite corners, N=4",
                               "Periodic opposite corners, N=2",
                               "Uni-directional side-to-side",
                               "Bi-directional side-to-side",
                               "Half-way periodic split, N=2",
                               "EOL"};

SegmentStyles segment_style = SegmentStyles::BI_DIR_SIDE2SIDE;

void next_segment_style() {
  int style_int = segment_style;
  style_int = (style_int + 1) % int(SegmentStyles::EOL);
  segment_style = static_cast<SegmentStyles>(style_int);

  Ser << int(segment_style) << ": " << segment_names[int(segment_style)]
      << endl;
}

void calc_leds_flip() {
  for (idx = 0; idx < s; idx++) {
    memmove(&leds_flip[idx], &leds[s - idx - 1], CRGB_SIZE);
  }
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

  FastLED
      .addLeds<LED_TYPE, DATA_PIN, CLK_PIN, COLOR_ORDER, DATA_RATE_MHZ(1)>(
          leds_all, N)
      .setCorrection(TypicalLEDStrip);

  FastLED.setBrightness(BRIGHTNESS);

  // IR distance sensor
  analogReadResolution(16);
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

    } else if (strcmp(strCmd, "s") == 0) {
      next_segment_style();

    } else if (strcmp(strCmd, "p") == 0) {
      next_pattern();

    } else {
      Ser.println(CRGB_SIZE);
    }
  }

  // Call the current pattern function
  pattern_list[iPattern]();

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
    full_white();
  }

  // Perform LED-strip segmenting
  switch (segment_style) {
    case SegmentStyles::FULL_STRIP:
      /* Full strip, no segmenting

          L K J I
        M         H
        N         G
        O         F
        P         E
          A B C D
      */
      s = N;
      memmove(&leds_all[0], &leds[0], N * CRGB_SIZE);
      break;

    case SegmentStyles::COPIED_SIDES:
      /* Copied sides

          D C B A
        A         D
        B         C
        C         B
        D         A
          A B C D
      */
      s = L;
      memmove(&leds_all[0]    , &leds[0], CRGB_SIZE_L);  // bottom
      memmove(&leds_all[L]    , &leds[0], CRGB_SIZE_L);  // right
      memmove(&leds_all[L * 2], &leds[0], CRGB_SIZE_L);  // top
      memmove(&leds_all[L * 3], &leds[0], CRGB_SIZE_L);  // left
      break;

    case SegmentStyles::PERIO_OPP_CORNERS_N4:
      /* Periodic opposite corners, N = 4

          D C B A
        D         A
        C         B
        B         C
        A         D
          A B C D
      */
      s = L;
      calc_leds_flip();
      memmove(&leds_all[0]    , &leds[0]     , CRGB_SIZE_L);  // bottom
      memmove(&leds_all[L]    , &leds_flip[0], CRGB_SIZE_L);  // right
      memmove(&leds_all[L * 2], &leds[0]     , CRGB_SIZE_L);  // top
      memmove(&leds_all[L * 3], &leds_flip[0], CRGB_SIZE_L);  // left
      break;

    case SegmentStyles::PERIO_OPP_CORNERS_N2:
      /* Periodic opposite corners, N = 2

          E F G H
        D         H
        C         G
        B         F
        A         E
          A B C D
      */
      s = L * 2;
      memmove(&leds_all[0], &leds[0], s * CRGB_SIZE);  // bottom & right
      for (idx = 0; idx < s; idx++) {                  // top & left
        memmove(&leds_all[L * 2 + idx], &leds[s - idx - 1], CRGB_SIZE);
      }
      break;

    case SegmentStyles::UNI_DIR_SIDE2SIDE:
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
        memmove(&leds_all[idx]        , &leds[0]      , CRGB_SIZE);  // bottom
        memmove(&leds_all[L * 2 + idx], &leds[L + 1]  , CRGB_SIZE);  // top
        memmove(&leds_all[L * 3 + idx], &leds[L - idx], CRGB_SIZE);  // left
      }
      memmove(&leds_all[L], &leds[1], CRGB_SIZE_L);  // right
      break;

    case SegmentStyles::BI_DIR_SIDE2SIDE:
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
        memmove(&leds_all[idx]    , &leds[0], CRGB_SIZE);                // bottom
        memmove(&leds_all[L + idx], &leds[(idx < (L / 2) ? idx + 1 : L - idx)],
                CRGB_SIZE);                                              // right
        memmove(&leds_all[L * 2 + idx], &leds[0]          , CRGB_SIZE);  // top
        memmove(&leds_all[L * 3 + idx], &leds_all[L + idx], CRGB_SIZE);  // left
      }
      break;

    case SegmentStyles::HALFWAY_PERIO_SPLIT_N2:
      /* Half-way periodic split, N = 2

          B A A B
        C         C
        D         D
        D         D
        C         C
          B A A B
      */
      s = ((L + 1) / 2) * 2;  // Note: Relies on integer math! No residuals.
      // L = 4 -> s = 4
      // L = 5 -> s = 6
      // L = 6 -> s = 6
      // L = 7 -> s = 8
      calc_leds_flip();
      memmove(&leds_all[0]        , &leds_flip[s / 2], L / 2 * CRGB_SIZE);  // bottom-left
      memmove(&leds_all[L / 2]    , &leds[0]         , CRGB_SIZE_L);        // bottom-right & right-bottom
      memmove(&leds_all[L + L / 2], &leds_flip[0]    , s / 2 * CRGB_SIZE);  // right-top
      memmove(&leds_all[L * 2]    , &leds_all[0]     , CRGB_SIZE_L);        // top
      memmove(&leds_all[L * 3]    , &leds_all[L]     , CRGB_SIZE_L);        // left
      break;

    default:
      /* Full strip, no segmenting */
      s = N;
      memmove(&leds_all[0], &leds[0], N * CRGB_SIZE);
      break;
  }

  test.process(&leds, &leds_all);

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
