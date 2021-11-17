/* Infinity mirror

Dennis van Gils
17-11-2021
*/

#include <Arduino.h>
#include <vector>

#include "FastLED.h"
#include "FiniteStateMachine.h"

#include "DvG_FastLED_StripSegmenter.h"
#include "DvG_FastLED_config.h"
#include "DvG_FastLED_effects.h"
#include "DvG_SerialCommand.h"

FASTLED_USING_NAMESPACE

// LED data of the full strip
CRGB leds_strip[FastLEDConfig::N];

// LED data, base pattern containing the effect to get copied/mirrored across by
// the FastLED_Segmenter in either 1, 2 or 4-fold symmetry.
CRGB leds[FastLEDConfig::N];
uint16_t s = FastLEDConfig::N;

FastLED_StripSegmenter segmntr;

#define Ser Serial
DvG_SerialCommand sc(Ser); // Instantiate serial command listener

/*-----------------------------------------------------------------------------
  Finite State Machine managing all the effects
------------------------------------------------------------------------------*/

std::vector<State> states = {state__HeartBeat,  state__Dennis, state__Rainbow,
                             state__BPM,        state__Juggle, state__Sinelon,
                             state__TestPattern};

char current_state_name[STATE_NAME_LEN] = {"\0"};
bool state_has_changed = true;
uint16_t state_idx = 0;

FSM fsm = FSM(states[0]);

/*-----------------------------------------------------------------------------
  setup
------------------------------------------------------------------------------*/

void setup() {
  Ser.begin(9600);

  // Ensure a minimum delay for recovery of FastLED
  // Generate heart beat in the mean time
  uint32_t tick = millis();
  generate_HeartBeat();
  while (millis() - tick < 3000) {}

  FastLED
      .addLeds<FastLEDConfig::LED_TYPE, FastLEDConfig::PIN_DATA,
               FastLEDConfig::PIN_CLK, FastLEDConfig::COLOR_ORDER,
               DATA_RATE_MHZ(1)>(leds_strip, FastLEDConfig::N)
      .setCorrection(FastLEDConfig::COLOR_CORRECTION);

  FastLED.setBrightness(FastLEDConfig::BRIGHTNESS);

  fill_solid(leds, FastLEDConfig::N, CRGB::Black);
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
      state_idx = (state_idx + 1) % states.size();
      fsm.transitionTo(states[state_idx]);
      state_has_changed = true;
    }
  }

  // Calculate the LED data array of the current effect, i.e. the base pattern.
  // The array is calculated up to length `s` as dictated by the current
  // StripSegmenter style.
  s = segmntr.get_base_pattern_numel();
  fsm.update();

  if (state_has_changed) {
    state_has_changed = false;
    fsm.getCurrentStateName(current_state_name);
    Ser.print("Effect: ");
    Ser.println(current_state_name);
  }

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
    update__FullWhite();
  }

  // Copy/mirror the effect across the full strip
  segmntr.process(leds, leds_strip);

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
