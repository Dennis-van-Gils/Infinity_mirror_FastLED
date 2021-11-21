/* Infinity mirror

Dennis van Gils
21-11-2021
*/

#include <Arduino.h>
#include <vector>

#include "FastLED.h"
#include "FiniteStateMachine.h"
#include "RunningAverage.h"

#include "DvG_FastLED_StripSegmenter.h"
#include "DvG_FastLED_config.h"
#include "DvG_FastLED_effects.h"

FASTLED_USING_NAMESPACE

// LED data of the full strip to be send out
CRGB leds[FastLEDConfig::N]; // EXTERNALLY modified by `DvG_FastLED_effects.h`

// clang-format off
extern FastLED_StripSegmenter segmntr1;  // Defined in `DvG_FastLED_effects.h`
extern uint8_t fx_hue;                   // Defined in `DvG_FastLED_effects.h`
extern uint8_t fx_hue_step;              // Defined in `DvG_FastLED_effects.h`
// clang-format on

// Master switch to turn leds on/off
bool led_output_is_on = true;

#define Ser Serial

#define USE_ANSI
#ifdef USE_ANSI
#  include "ansi.h"
ANSI ansi(&Serial);
#endif

/*------------------------------------------------------------------------------
  IR distance sensor
--------------------------------------------------------------------------------
  Sharp 2Y0A02, pin A0
  Fit: distance [cm] = A / bitval ^ C - B, where bitval is at 10-bit
*/
// clang-format off
#define A0_BITS 10
uint8_t IR_dist = 0;     // [cm] // EXTERNALLY read by `DvG_FastLED_effects.h`
const uint8_t IR_MIN_DIST = 16;  // [cm]
const uint8_t IR_MAX_DIST = 150; // [cm]
const float   IR_CALIB_A = 1512.89;
const uint8_t IR_CALIB_B = 74;
const float   IR_CALIB_C = 0.424;
// clang-format on

void update_IR_dist() {
  // Reads out the IR distance sensor in [cm] and computes the running average
  static RunningAverage IR_dist_RA(10);
  uint8_t val;

  val = IR_CALIB_A / pow(analogRead(PIN_A0), IR_CALIB_C) - IR_CALIB_B; // [cm]
  val = max(val, IR_MIN_DIST);
  val = min(val, IR_MAX_DIST);

  IR_dist_RA.addValue(val);
  IR_dist = IR_dist_RA.getAverage();

  if (0) {
    Ser.print(A0);
    Ser.print("\t");
    Ser.print(val);
    Ser.print("\t");
    Ser.println(IR_dist);
  }
}

/*------------------------------------------------------------------------------
  Finite State Machine managing all the LED effects
------------------------------------------------------------------------------*/

std::vector<State> states = {
    // state__TestPattern,
    state__HeartBeat1, state__HeartBeat2, state__Dennis,
    state__Rainbow,    state__BPM,        state__Sinelon};

bool state_has_changed = true;
uint16_t state_idx = 1;

FSM fsm = FSM(states[state_idx]);

void set_state(uint16_t idx) {
  state_idx = min(idx, states.size() - 1);
  fsm.transitionTo(states[state_idx]);
  state_has_changed = true;
}

void next_state() { set_state((state_idx + 1) % states.size()); }

void prev_state() {
  set_state((state_idx + states.size() - 1) % states.size());
}

void print_state() {
  static char buffer[STATE_NAME_LEN] = {"\0"};
  fsm.getCurrentStateName(buffer);
#ifdef USE_ANSI
  ansi.foreground(ANSI::yellow);
#endif
  Ser.print("Effect: ");
  Ser.print(state_idx);
  Ser.print(" - \"");
  Ser.print(buffer);
  Ser.println("\"");
#ifdef USE_ANSI
  ansi.normal();
#endif
}

void print_style() {
  static char buffer[STYLE_NAME_LEN] = {"\0"};
  segmntr1.get_style_name(buffer);
#ifdef USE_ANSI
  ansi.foreground(ANSI::white | ANSI::bright);
#endif
  Ser.print("Style : ");
  Ser.print((int)segmntr1.get_style());
  Ser.print(" - ");
  Ser.println(buffer);
#ifdef USE_ANSI
  ansi.normal();
#endif
}

/*------------------------------------------------------------------------------
  setup
------------------------------------------------------------------------------*/

void setup() {
  Ser.begin(115200);

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
  update_IR_dist();
}

/*------------------------------------------------------------------------------
  loop
------------------------------------------------------------------------------*/

void loop() {
  char charCmd;                    // Incoming serial command
  static uint32_t tick = millis(); // Keep track of frame rate

  if (Ser.available() > 0) {
    charCmd = Serial.read();

    if (charCmd == '?') {
      print_state();
      print_style();

    } else if (charCmd == '`') {
      led_output_is_on = !led_output_is_on;
      Ser.print("Output ");
      Ser.println(led_output_is_on ? "ON" : "OFF");

    } else if ((charCmd >= '0') & (charCmd <= '9')) {
      set_state(charCmd - '0');

    } else if (charCmd == 'o') {
      prev_state();

    } else if (charCmd == 'p') {
      next_state();

    } else if (charCmd == '[') {
      segmntr1.prev_style();
      print_style();

    } else if (charCmd == ']') {
      segmntr1.next_style();
      print_style();
    }
  }

  fsm.update(); // CRITICAL

  // Note: Printing state name can only happen after the call to `fsm.update()`
  if (state_has_changed) {
    state_has_changed = false;
    print_state();
  }

  // Overrule any LED effect
  if (!led_output_is_on) {
    fill_solid(leds, FastLEDConfig::N, CRGB::Black);
  }

  // Send out LED data to the strip. `delay()` keeps the framerate modest and
  // allows for brightness dithering. It will invoke FastLED.show() - sending
  // out the LED data - at least once during the delay.
  FastLED.delay(FastLEDConfig::DELAY);

  // DEBUG frame rate
  if (led_output_is_on & 0) {
    Ser.println(millis() - tick);
    tick = millis();
  }

  // Periodic updates
  EVERY_N_MILLISECONDS(100) { update_IR_dist(); }
  EVERY_N_MILLISECONDS(30) { fx_hue = fx_hue + fx_hue_step; }
  EVERY_N_SECONDS(20) { next_state(); }
  /*
  EVERY_N_SECONDS(10) {
    segmntr1.next_style();
    print_style();
  }
  */
}
