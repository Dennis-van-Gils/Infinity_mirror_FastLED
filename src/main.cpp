/* Infinity mirror

Dennis van Gils
04-12-2021
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
CRGB leds[FLC::N]; // EXTERNALLY modified by `DvG_FastLED_effects.h`

extern FastLED_StripSegmenter segmntr1; // Defined in `DvG_FastLED_effects.h`

// Master switches
static bool ENA_leds = true;            // Enable leds?
static bool ENA_full_white = false;     // Override with full white?
static bool ENA_IRDistTest = false;     // Override with IR distance test?
static bool ENA_auto_next_state = true; // Automatically go to next state?

// Brightness
uint8_t brightness_idx = 5;
const uint8_t brightness_table[] = {0,   1,   16,  32,  48,  64,
                                    80,  96,  112, 128, 144, 160,
                                    176, 192, 208, 224, 240, 255};

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
uint8_t IR_dist = 0;     // [cm]  // EXTERNALLY read by `DvG_FastLED_effects.h`
float IR_dist_fract = 0; // [0-1] // EXTERNALLY read by `DvG_FastLED_effects.h`
const uint8_t IR_MIN_DIST = 16;   // [cm]
const uint8_t IR_MAX_DIST = 150;  // [cm]
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
  IR_dist_fract = (IR_dist - IR_MIN_DIST) / (float)(IR_MAX_DIST - IR_MIN_DIST);

  if (0) {
    Ser.print(A0);
    Ser.print("\t");
    Ser.print(val);
    Ser.print("\t");
    Ser.print(IR_dist);
    Ser.print("\t");
    Ser.println(IR_dist_fract);
  }
}

/*------------------------------------------------------------------------------
  Finite State Machine managing all the LED effects
------------------------------------------------------------------------------*/

std::vector<State> states =
    {
        // state__TestPattern,
        state__HeartBeat1, state__RainbowSurf, state__RainbowBarf,
        state__Dennis,     state__HeartBeat2,  state__Rainbow,
        state__Sinelon}; // ,state__RainbowBarf2};

bool state_has_changed = true;
uint16_t state_idx = 0;

FSM fsm = FSM(states[state_idx]);

void set_state(uint16_t idx) {
  state_idx = min(idx, states.size() - 1);
  fsm.transitionTo(states[state_idx]);
  state_has_changed = true;
}

void next_state() {
  set_state((state_idx + 1) % states.size());
}

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
      .addLeds<FLC::LED_TYPE, FLC::PIN_DATA, FLC::PIN_CLK, FLC::COLOR_ORDER,
               DATA_RATE_MHZ(1)>(leds, FLC::N)
      .setCorrection(FLC::COLOR_CORRECTION);

  // FastLED.setBrightness(FLC::BRIGHTNESS);
  FastLED.setBrightness(brightness_table[brightness_idx]);

  fill_solid(leds, FLC::N, CRGB::Black);

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
      ENA_leds = !ENA_leds;
      Ser.print("Output ");
      Ser.println(ENA_leds ? "ON" : "OFF");

    } else if (charCmd == 'q') {
      ENA_auto_next_state = !ENA_auto_next_state;
      Ser.print("Auto next state ");
      Ser.println(ENA_auto_next_state ? "ON" : "OFF");

    } else if ((charCmd >= '0') && (charCmd <= '9')) {
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

    } else if (charCmd == '-') {
      brightness_idx = brightness_idx > 0 ? brightness_idx - 1 : 0;
      FastLED.setBrightness(brightness_table[brightness_idx]);
      Ser.print("Brightness ");
      Ser.println(brightness_table[brightness_idx]);

    } else if ((charCmd == '+') || (charCmd == '=')) {
      brightness_idx = brightness_idx < sizeof(brightness_table) - 2
                           ? brightness_idx + 1
                           : sizeof(brightness_table) - 1;
      FastLED.setBrightness(brightness_table[brightness_idx]);
      Ser.print("Brightness ");
      Ser.println(brightness_table[brightness_idx]);

    } else if (charCmd == 'w') {
      ENA_full_white = !ENA_full_white;
      Ser.print("Full white ");
      Ser.println(ENA_full_white ? "ON" : "OFF");

    } else if (charCmd == 'i') {
      ENA_IRDistTest = !ENA_IRDistTest;
      ENA_auto_next_state = !ENA_IRDistTest;
      Ser.print("IR distance test ");
      Ser.println(ENA_IRDistTest ? "ON" : "OFF");

    } else {
      Ser.println("\nInfinity Mirror");
      Ser.println("---------------");
      Ser.println("`  : Toggle output ON/OFF");
      Ser.println("q  : Toggle auto next state ON/OFF");
      Ser.println("w  : Toggle full white ON/OFF\n");
      Ser.println("-  : Decrease brightness");
      Ser.println("+  : Increase brightness\n");
      Ser.println("?  : Print current state & style");
      Ser.println("0-9: Go to state preset #");
      Ser.println("o  : Go to previous state");
      Ser.println("p  : Go to next state");
      Ser.println("[  : Go to previous style");
      Ser.println("]  : Go to next style\n");
    }
  }

  // Master switch
  if (ENA_IRDistTest) {
    fsm.transitionTo(state__IRDistTest);
  }

  fsm.update(); // CRITICAL, calculates current LED effect

  // Note: Printing state name can only happen after the call to `fsm.update()`
  if (state_has_changed) {
    state_has_changed = false;
    print_state();
  }

  // Master switches
  if (!ENA_leds) {
    fill_solid(leds, FLC::N, CRGB::Black);
  } else {
    if (ENA_full_white) {
      fill_solid(leds, FLC::N, CRGB::White);
    }
  }

  // Send out LED data to the strip. `delay()` keeps the framerate modest and
  // allows for brightness dithering. It will invoke FastLED.show() - sending
  // out the LED data - at least once during the delay.
  FastLED.delay(FLC::DELAY);

  // DEBUG frame rate
  if (ENA_leds & 0) {
    Ser.println(millis() - tick);
    tick = millis();
  }

  // Periodic updates
  EVERY_N_MILLISECONDS(100) {
    update_IR_dist();
  }
  if (ENA_auto_next_state) {
    if (state_idx == 0) {
      if (fsm.timeInCurrentState() > 10000) {
        next_state();
      }
    } else {
      if (fsm.timeInCurrentState() > 20000) {
        next_state();
      }
      /*
      EVERY_N_SECONDS(20) {
        next_state();
      }
      */
    }
  }
  /*
  EVERY_N_SECONDS(10) {
    segmntr1.next_style();
    print_style();
  }
  */
}
