/* Infinity mirror

Dennis van Gils
16-11-2021
*/

#include <Arduino.h>

#include "FastLED.h"
#include "FiniteStateMachine.h"

#include "DvG_FastLED_StripSegmenter.h"
#include "DvG_FastLED_config.h"
#include "DvG_FastLED_effects.h"
#include "DvG_SerialCommand.h"

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

FASTLED_USING_NAMESPACE

// LED data of the full strip
CRGB leds_strip[FastLEDConfig::N];

// LED data, base pattern containing the effect to get copied/mirrored across by
// the FastLED_Segmenter in either 1, 2 or 4-fold symmetry.
CRGB leds_effect[FastLEDConfig::N];

FastLED_StripSegmenter segmntr;

#define Ser Serial
DvG_SerialCommand sc(Ser); // Instantiate serial command listener

/*-----------------------------------------------------------------------------
  Effects
------------------------------------------------------------------------------*/

// List of effects to cycle through
typedef void (*Effects[])(struct CRGB(*leds), uint16_t s);
uint8_t effect_idx = 0; // Index number of the current effect

// Effects effects = {rainbow, sinelon, juggle, bpm};
// Effects effects = {sinelon, bpm, rainbow};
// Effects effects = {dennis};
Effects effects = {heart_beat, rainbow, sinelon,     bpm,
                   juggle,     dennis,  test_pattern};

void next_effect() {
  effect_idx = (effect_idx + 1) % ARRAY_SIZE(effects);
  Ser.print("effect: ");
  Ser.println(effect_idx);
}

/*-----------------------------------------------------------------------------
  setup
------------------------------------------------------------------------------*/

void setup() {
  Ser.begin(9600);

  // Ensure a minimum delay for recovery of FastLED
  // Generate heart beat in the mean time
  uint32_t tick = millis();
  generate_heart_beat();
  while (millis() - tick < 3000) {}

  FastLED
      .addLeds<FastLEDConfig::LED_TYPE, FastLEDConfig::PIN_DATA,
               FastLEDConfig::PIN_CLK, FastLEDConfig::COLOR_ORDER,
               DATA_RATE_MHZ(1)>(leds_strip, FastLEDConfig::N)
      .setCorrection(FastLEDConfig::COLOR_CORRECTION);

  FastLED.setBrightness(FastLEDConfig::BRIGHTNESS);

  fill_solid(leds_effect, FastLEDConfig::N, CRGB::Black);
  fill_solid(leds_strip, FastLEDConfig::N, CRGB::Black);

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
      segmntr.next_style();
      segmntr.print_style_name(Ser);

    } else if (strcmp(strCmd, "[") == 0) {
      segmntr.prev_style();
      segmntr.print_style_name(Ser);

    } else if (strcmp(strCmd, "p") == 0) {
      next_effect();
    }
  }

  // Calculate the LED data array of the current effect, i.e. the base pattern.
  // The array is calculated up to a length as dictated by the current
  // StripSegmenter style.
  effects[effect_idx](leds_effect, segmntr.get_base_pattern_numel());

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
    full_white(leds_effect, segmntr.get_base_pattern_numel());
  }

  // Copy/mirror the effect across the full strip
  segmntr.process(leds_effect, leds_strip);

  // Keep the framerate modest and allow for brightness dithering.
  // Will also invoke FASTLED.show() - sending out the LED data - at least once.
  FastLED.delay(FastLEDConfig::DELAY);

  // Periodic updates
  EVERY_N_MILLISECONDS(30) { iHue++; }

  /*
  EVERY_N_SECONDS(24) { next_effect(); }
  EVERY_N_SECONDS(10) {
    segmntr.next_style();
    segmntr.print_style_name(Ser);
  }
  */
}
