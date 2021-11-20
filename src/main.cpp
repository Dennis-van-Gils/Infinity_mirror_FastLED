/* Infinity mirror

Dennis van Gils
20-11-2021
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

// clang-format off

// LED data of the full strip to be send out
CRGB leds[FastLEDConfig::N];  // EXTERNALLY modified by `DvG_FastLED_effects.h`

extern FastLED_StripSegmenter segmntr;    // Defined in `DvG_FastLED_effects.h`
extern uint8_t fx_hue;                    // Defined in `DvG_FastLED_effects.h`
extern float fx_hue_step;                 // Defined in `DvG_FastLED_effects.h`

// IR distance sensor
// Sharp 2Y0A02, pin A0
// Fit: distance [cm] = A / bitval ^ C - B, where bitval is at 10-bit
#define A0_BITS 10
uint8_t IR_dist = 0;      // [cm] // EXTERNALLY read by `DvG_FastLED_effects.h`
const uint8_t IR_MIN_DIST = 16;   // [cm]
const uint8_t IR_MAX_DIST = 150;  // [cm]
const float   IR_CALIB_A = 1512.89;
const uint8_t IR_CALIB_B = 74;
const float   IR_CALIB_C = 0.424;

// clang-format on

#define Ser Serial
DvG_SerialCommand sc(Ser); // Instantiate serial command listener

/*------------------------------------------------------------------------------
  Finite State Machine managing all the LED effects
------------------------------------------------------------------------------*/

std::vector<State> states = {state__HeartBeat,  state__Dennis, state__Rainbow,
                             state__BPM,        state__Juggle, state__Sinelon,
                             state__TestPattern};

char current_state_name[STATE_NAME_LEN] = {"\0"};
bool state_has_changed = true;
uint16_t state_idx = 1;

FSM fsm = FSM(states[state_idx]);

/*------------------------------------------------------------------------------
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
               DATA_RATE_MHZ(1)>(leds, FastLEDConfig::N)
      .setCorrection(FastLEDConfig::COLOR_CORRECTION);

  FastLED.setBrightness(FastLEDConfig::BRIGHTNESS);

  fill_solid(leds, FastLEDConfig::N, CRGB::Black);

  // IR distance sensor
  analogReadResolution(A0_BITS);
}

/*------------------------------------------------------------------------------
  loop
------------------------------------------------------------------------------*/

void loop() {
  char *strCmd; // Incoming serial command string

  // Process serial commands
  if (sc.available()) {
    strCmd = sc.getCmd();

    if (strcmp(strCmd, "id?") == 0) {
      Ser.println("Arduino, Infinity Mirror");

    } else if (strcmp(strCmd, "o") == 0) {
      state_idx = (state_idx + states.size() - 1) % states.size();
      fsm.transitionTo(states[state_idx]);
      state_has_changed = true;

    } else if (strcmp(strCmd, "p") == 0) {
      state_idx = (state_idx + 1) % states.size();
      fsm.transitionTo(states[state_idx]);
      state_has_changed = true;

    } else if (strcmp(strCmd, "]") == 0) {
      segmntr.next_style();
      segmntr.print_style_name(Ser);

    } else if (strcmp(strCmd, "[") == 0) {
      segmntr.prev_style();
      segmntr.print_style_name(Ser);

    } else if (strcmp(strCmd, "?") == 0) {
      fsm.getCurrentStateName(current_state_name);
      Ser.print("Effect: ");
      Ser.println(current_state_name);
      segmntr.print_style_name(Ser);
    }
  }

  fsm.update(); // CRITICAL

  if (state_has_changed) {
    state_has_changed = false;
    fsm.getCurrentStateName(current_state_name);
    Ser.print("Effect: ");
    Ser.println(current_state_name);
  }

  // IR distance sensor
  static bool IR_switch = false;
  EVERY_N_MILLISECONDS(100) {
    uint16_t A0 = analogRead(PIN_A0);
    IR_dist = IR_CALIB_A / pow(A0, IR_CALIB_C) - IR_CALIB_B;
    IR_dist = max(IR_dist, IR_MIN_DIST);
    IR_dist = min(IR_dist, IR_MAX_DIST);
    IR_switch = (IR_dist < 20);

    /*
    Ser.print(A0);
    Ser.print("\t");
    Ser.println(IR_dist);
    */
  }
  if (IR_switch) {
    update__FullWhite();
  }

  // Keep the framerate modest and allow for brightness dithering.
  // Will also invoke FASTLED.show() - sending out the LED data - at least once.
  FastLED.delay(FastLEDConfig::DELAY);

  // Periodic updates
  EVERY_N_MILLISECONDS(30) { fx_hue = fx_hue + fx_hue_step; }

  /*
  EVERY_N_SECONDS(24) { next_effect(); }
  EVERY_N_SECONDS(10) {
    segmntr.next_style();
    segmntr.print_style_name(Ser);
  }
  */
}
