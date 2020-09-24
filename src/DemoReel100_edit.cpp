#include <Arduino.h>
#include "FastLED.h"
#include "DvG_SerialCommand.h"

#define Ser Serial
DvG_SerialCommand sc(Ser); // Instantiate serial command listener

FASTLED_USING_NAMESPACE

// FastLED "100-lines-of-code" demo reel, showing just a few
// of the kinds of animation patterns you can quickly and easily
// compose using FastLED.
//
// This example also shows one easy way to define multiple
// animations patterns and have them automatically rotate.
//
// -Mark Kriegsman, December 2014

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define DATA_PIN 2
#define CLK_PIN 3
#define LED_TYPE APA102
#define COLOR_ORDER BGR
#define NUM_LEDS 13
#define NUM_LEDS_QUAD_COPY 52
CRGB leds[NUM_LEDS];
CRGB leds_quad_copy[NUM_LEDS_QUAD_COPY];

#define BRIGHTNESS 128
#define FRAMES_PER_SECOND 120 // 120

// List of patterns to cycle through.  Each is defined as a separate function below.
uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0;                  // rotating "base color" used by many of the patterns

typedef void (*SimplePatternList[])();

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void addGlitter(fract8 chanceOfGlitter)
{
  if (random8() < chanceOfGlitter)
  {
    leds[random16(NUM_LEDS)] += CRGB::White;
  }
}

void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow(leds, NUM_LEDS, gHue, 4); // leds, NUM_LEDS, gHue, 26 or 39
}

void rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, NUM_LEDS, 4);
  int pos = beatsin16(13, 0, NUM_LEDS);
  leds[pos] += CHSV(gHue, 255, 255); // gHue, 255, 192
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 26;
  CRGBPalette16 palette = PartyColors_p;          //RainbowColors_p;//PartyColors_p;
  uint8_t beat = beatsin8(0, BeatsPerMinute, 52); //  BeatsPerMinute, 64, 255
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 1));
  }
}

void juggle()
{
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy(leds, NUM_LEDS, 20);
  byte dothue = 0;
  for (int i = 0; i < 8; i++)
  {
    leds[beatsin16(i + 7, 0, NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void dennis()
{
  CRGBPalette16 palette = RainbowColors_p;
  fadeToBlackBy(leds, NUM_LEDS, 1);
  /*
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue, 46);
  }
  */
  //int pos = beatsin16(13, 0, NUM_LEDS - 1);
  int pos = beat8(46) / 256.0 * 15;
  leds[pos] = ColorFromPalette(palette, gHue, 46);
  //leds[NUM_LEDS - 1] = CRGB::Red;
}

void full_white()
{
  fill_solid(leds, NUM_LEDS, CRGB::White);
}

void strobe()
{
  if (gHue % 16)
  {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
  }
  else
  {
    CRGBPalette16 palette = PartyColors_p; //PartyColors_p;
    for (int i = 0; i < NUM_LEDS; i++)
    { //9948
      leds[i] = ColorFromPalette(palette, gHue + (i * 2), gHue + (i * 10));
    }
    //fill_solid(leds, NUM_LEDS, CRGB::White);
  }
}

//SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };
//SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle};
SimplePatternList gPatterns = {sinelon, bpm, rainbow};
//SimplePatternList gPatterns = {rainbow};
void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE(gPatterns);
}

void setup()
{
  Ser.begin(9600);

  delay(1000); // 3 second delay for recovery FASTLED

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, DATA_PIN, CLK_PIN, COLOR_ORDER, DATA_RATE_MHZ(1)>(leds_quad_copy, NUM_LEDS_QUAD_COPY).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

  // IR distance sensor
  analogReadResolution(12);
}

void loop()
{
  char *strCmd; // Incoming serial command string

  if (sc.available())
  {
    strCmd = sc.getCmd();

    if (strcmp(strCmd, "id?") == 0)
    {
      Ser.println("Arduino, FASTLED demo");
    }
  }

  // Call the current pattern function once, updating the 'leds' array
  gPatterns[gCurrentPatternNumber]();

  // IR distance sensor
  // Sharp 2Y0A02
  // 20 - 150 cm
  float A0_V;
  //EVERY_N_MILLISECONDS(500)
  //{
    A0_V = analogRead(PIN_A0) / 4095. * 3.3;
    //Ser.println(A0_V, 2);
    if (A0_V > 2)
    {
      full_white();
    }
  //}

  // send the 'leds' array out to the actual LED strip
  memmove(&leds_quad_copy[0], &leds[0], NUM_LEDS * sizeof(CRGB));
  memmove(&leds_quad_copy[NUM_LEDS], &leds[0], NUM_LEDS * sizeof(CRGB));
  memmove(&leds_quad_copy[NUM_LEDS * 2], &leds[0], NUM_LEDS * sizeof(CRGB));
  memmove(&leds_quad_copy[NUM_LEDS * 3], &leds[0], NUM_LEDS * sizeof(CRGB));
  FastLED.show();
  // insert a delay to keep the framerate modest
  //FastLED.delay(1000/FRAMES_PER_SECOND);

  /*
  EVERY_N_MILLISECONDS(1000/FRAMES_PER_SECOND) {
    memmove(&leds_quad_copy[0], &leds[0], NUM_LEDS * sizeof(CRGB));
    memmove(&leds_quad_copy[NUM_LEDS], &leds[0], NUM_LEDS * sizeof(CRGB));
    memmove(&leds_quad_copy[NUM_LEDS * 2], &leds[0], NUM_LEDS * sizeof(CRGB));
    memmove(&leds_quad_copy[NUM_LEDS * 3], &leds[0], NUM_LEDS * sizeof(CRGB));
    FastLED.show();
  }
  */

  // do some periodic updates
  EVERY_N_MILLISECONDS(20) { gHue++; }   // slowly cycle the "base color" through the rainbow
  EVERY_N_SECONDS(24) { nextPattern(); } // change patterns periodically
}
