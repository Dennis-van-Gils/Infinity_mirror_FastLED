/* Infinity mirror

Dennis van Gils
16-04-2022
*/

#include <Arduino.h>
#include <vector>

#include "FastLED.h"
#include "FiniteStateMachine.h"
#include "RunningAverage.h"
#include "avdweb_Switch.h"

#include "DvG_FastLED_config.h"
#include "DvG_FastLED_effects.h"

FASTLED_USING_NAMESPACE

/* NOT NEEDED TO DECLARE EXTERN IN `main.cpp`. Using `#include` is sufficient.
// External variables defined in `DvG_FastLED_effects.h`
extern CRGB leds[FLC::N];
extern FastLED_StripSegmenter segmntr1;
*/

// Master switches
static bool ENA_override_AllBlack = false; // Override: Turn off all leds?
static bool ENA_override_AllWhite = false; // Override: Turn all leds to white?
static bool ENA_override_IRDist = false;   // Override: IR distance test?
static bool ENA_override = false;          // Set by any of the above

static bool ENA_auto_next_fx = true; // Automatically go to next effect?
static bool ENA_print_FPS = false;   // Print FPS counter to serial?

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

const byte PIN_BUTTON = 9;
Switch button = Switch(PIN_BUTTON, INPUT_PULLUP, LOW, 50, 500, 50);

/*------------------------------------------------------------------------------
  IR distance sensor
--------------------------------------------------------------------------------
  Sharp 2Y0A02, pin A2
  Fit: distance [cm] = A / bitval ^ C - B, where bitval is at 10-bit
*/
// clang-format off
#define A2_BITS 10       // Calibration has been performed at 10 bits ADC only
uint8_t IR_dist_cm = 0;  // IR distance in [cm]
float IR_dist_fract = 0; // IR distance as fraction of the full scale [0-1]
const uint8_t IR_DIST_MIN = 16;   // [cm]
const uint8_t IR_DIST_MAX = 150;  // [cm]
const float   IR_CALIB_A = 1512.89;
const uint8_t IR_CALIB_B = 74;
const float   IR_CALIB_C = 0.424;
// clang-format on

void update_IR_dist() {
  // Read out the IR distance sensor in [cm] and compute the running average
  uint16_t bitval;
  uint8_t instant_cm;
  static RunningAverage RA(10);

  // Read out instanteneous IR distance
  bitval = analogRead(PIN_A2);
  instant_cm = IR_CALIB_A / pow(bitval, IR_CALIB_C) - IR_CALIB_B; // [cm]
  instant_cm = constrain(instant_cm, IR_DIST_MIN, IR_DIST_MAX);

  // Apply running average
  RA.addValue(instant_cm);
  IR_dist_cm = RA.getAverage();
  IR_dist_fract =
      ((float)IR_dist_cm - IR_DIST_MIN) / (IR_DIST_MAX - IR_DIST_MIN);

  if (ENA_override_IRDist) {
    Ser.print(bitval);
    Ser.print("\t");
    Ser.print(instant_cm);
    Ser.print("\t");
    Ser.print(IR_dist_cm);
    Ser.print("\t");
    Ser.println(IR_dist_fract);
  }
}

/*------------------------------------------------------------------------------
  Finite State Machine: `fsm_fx`
  Governs calculating the selected FastLED effect
------------------------------------------------------------------------------*/

bool fx_has_changed = true;
uint16_t fx_idx = 0;

std::vector<State> fx_list = { // fx__TestPattern,
    fx__HeartBeat,   fx__RainbowSurf, fx__RainbowBarf, fx__Dennis,
    fx__HeartBeat_2, fx__Rainbow,     fx__Sinelon}; // ,fx__RainbowBarf_2};

// Finite State Machine governing the FastLED effect calculation
FSM fsm_fx = FSM(fx_list[fx_idx]);

void set_fx(uint16_t idx) {
  fx_idx = min(idx, fx_list.size() - 1);
  fsm_fx.transitionTo(fx_list[fx_idx]);
  fx_has_changed = true;

  ENA_override_AllBlack = false;
  ENA_override_AllWhite = false;
  ENA_override_IRDist = false;
  ENA_override = false;
}

void next_fx() {
  set_fx((fx_idx + 1) % fx_list.size());
}

void prev_fx() {
  set_fx((fx_idx + fx_list.size() - 1) % fx_list.size());
}

void print_fx() {
  // NOTE: `fsm_fx.update()` must have run to print out the proper effect name
  static char buffer[STATE_NAME_LEN] = {"\0"};
  fsm_fx.getCurrentStateName(buffer);
#ifdef USE_ANSI
  ansi.foreground(ANSI::yellow);
#endif
  Ser.print("Effect: ");
  if (ENA_override) {
    Ser.print("*");
  } else {
    Ser.print(fx_idx);
  }
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
  Finite State Machine: `fsm_main`
  Governs showing the FastLED effect or the menu
------------------------------------------------------------------------------*/

// Declarations
void entr__ShowMenu();
void upd__ShowMenu();
void exit__ShowMenu();
State show__Menu("ShowMenu", entr__ShowMenu, upd__ShowMenu, exit__ShowMenu);

void upd__ShowFastLED();
State show__FastLED("ShowFastLED", upd__ShowFastLED);

// Finite State Machine governing showing the FastLED effect or the menu
FSM fsm_main = FSM(show__FastLED);

void flash_menu(const struct CRGB &color) {
  fill_solid(leds, FLC::N, CRGB::Black);
  FastLED.delay(200);
  fill_solid(leds, FLC::N, color);
  FastLED.delay(200);
  fill_solid(leds, FLC::N, CRGB::Black);
  FastLED.delay(200);
  fill_solid(leds, FLC::N, color);
  FastLED.delay(200);
  fill_solid(leds, FLC::N, CRGB::Black);
  FastLED.delay(200);
}

void entr__ShowMenu() {
  flash_menu(CRGB::Red);
}

void upd__ShowMenu() {
  // In progress: Shows dummy menu for the moment
  static uint8_t leds_offset = 0;

  if (fsm_main.timeInCurrentState() > 10000) {
    fsm_main.transitionTo(show__FastLED);
  }
  fill_solid(leds, FLC::N, CRGB::Black);
  fill_solid(&leds[leds_offset], 13, CRGB::Red);
  FastLED.delay(20);

  // Check for button presses
  button.poll();
  if (button.longPress()) {
    Ser.println("long pressed");
    fsm_main.transitionTo(show__FastLED);
  }
  if (button.singleClick()) {
    Ser.println("single click");
    leds_offset += 13;
    leds_offset %= FLC::N;
  }
}

void exit__ShowMenu() {
  flash_menu(CRGB::Green);
}

void upd__ShowFastLED() {
  fsm_fx.update(); // CRITICAL, calculates current LED effect

  if (fx_has_changed) {
    fx_has_changed = false;
    print_fx();
  }

  // Send out LED data to the strip. `delay()` keeps the framerate modest and
  // allows for brightness dithering. It will invoke FastLED.show() - sending
  // out the LED data - at least once during the delay.
  FastLED.delay(2);

  // FPS counter
  EVERY_N_MILLISECONDS(1000) {
    if (ENA_print_FPS) {
      Ser.println(FastLED.getFPS());
    }
  }

  // Check for button presses
  button.poll();
  if (button.longPress()) {
    Ser.println("long pressed");
    fsm_main.transitionTo(show__Menu);
  }
  if (button.singleClick()) {
    Ser.println("single click");
    if (!ENA_override) {
      next_fx();
    }
  }
}

// FSM fsm_main = FSM(state__ShowFastLED);

/*------------------------------------------------------------------------------
  setup
------------------------------------------------------------------------------*/

void setup() {
  Ser.begin(115200);

  // Ensure a minimum delay for recovery of FastLED
  // Generate `HeartBeat` look-up table in the mean time
  uint32_t tick = millis();
  generate_HeartBeat();
  while (millis() - tick < 3000) {}

  FastLED.addLeds<FLC::LED_TYPE, FLC::PIN_DATA, FLC::PIN_CLK, FLC::COLOR_ORDER,
                  DATA_RATE_MHZ(1)>(leds, FLC::N);
  FastLED.setCorrection(FLC::COLOR_CORRECTION);
  FastLED.setBrightness(brightness_table[brightness_idx]);
  fill_solid(leds, FLC::N, CRGB::Black);

#ifdef ADAFRUIT_ITSYBITSY_M4_EXPRESS
  // This code is needed to turn off the distracting onboard RGB LED
  // NOTE: ItsyBitsy uses a Dotstar (APA102), Feather uses a NeoPixel
  CRGB onboard_led[1];
  FastLED.addLeds<APA102, 8, 6, BGR, DATA_RATE_MHZ(1)>(onboard_led, 1);
  fill_solid(onboard_led, 1, CRGB::Black);
#endif

  FastLED.setMaxRefreshRate(FLC::MAX_REFRESH_RATE);
  FastLED.show();

#ifdef ADAFRUIT_ITSYBITSY_M4_EXPRESS
  // Remove the onboard RGB LED again from the FastLED controllers
  FastLED[1].setLeds(onboard_led, 0);
#endif

  // IR distance sensor
  analogReadResolution(A2_BITS);
  update_IR_dist();
}

/*------------------------------------------------------------------------------
  loop
------------------------------------------------------------------------------*/

void loop() {
  char charCmd; // Incoming serial command

  // Check for incoming serial commands
  if (Ser.available() > 0) {
    charCmd = Serial.read();

    if (charCmd == '?') {
      print_fx();
      print_style();

    } else if (charCmd == '`') {
      ENA_override_AllBlack = !ENA_override_AllBlack;
      ENA_override_AllWhite = false;
      ENA_override_IRDist = false;
      if (ENA_override_AllBlack) {
        ENA_override = true;
        fsm_fx.transitionTo(fx__AllBlack);
        fx_has_changed = true;
      } else {
        ENA_override = false;
        fsm_fx.transitionTo(fx_list[fx_idx]);
        fx_has_changed = true;
      }
      Ser.print("Output ");
      Ser.println(ENA_override_AllBlack ? "OFF" : "ON");

    } else if (charCmd == 'w') {
      ENA_override_AllBlack = false;
      ENA_override_AllWhite = !ENA_override_AllWhite;
      ENA_override_IRDist = false;
      if (ENA_override_AllWhite) {
        ENA_override = true;
        fsm_fx.transitionTo(fx__AllWhite);
        fx_has_changed = true;
      } else {
        ENA_override = false;
        fsm_fx.transitionTo(fx_list[fx_idx]);
        fx_has_changed = true;
      }
      Ser.print("All white ");
      Ser.println(ENA_override_AllWhite ? "ON" : "OFF");

    } else if (charCmd == 'i') {
      ENA_override_AllBlack = false;
      ENA_override_AllWhite = false;
      ENA_override_IRDist = !ENA_override_IRDist;
      if (ENA_override_IRDist) {
        ENA_override = true;
        fsm_fx.transitionTo(fx__IRDist);
        fx_has_changed = true;
      } else {
        ENA_override = false;
        fsm_fx.transitionTo(fx_list[fx_idx]);
        fx_has_changed = true;
      }
      Ser.print("IR distance test ");
      Ser.println(ENA_override_IRDist ? "ON" : "OFF");

    } else if (charCmd == 'f') {
      ENA_print_FPS = !ENA_print_FPS;

    } else if (charCmd == 'q') {
      ENA_auto_next_fx = !ENA_auto_next_fx;
      Ser.print("Auto next effect ");
      Ser.println(ENA_auto_next_fx ? "ON" : "OFF");

    } else if ((charCmd >= '0') && (charCmd <= '9')) {
      set_fx(charCmd - '0');

    } else if (charCmd == 'o') {
      prev_fx();

    } else if (charCmd == 'p') {
      next_fx();

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

    } else if (charCmd == 'r') {
      NVIC_SystemReset();

    } else if (charCmd == 'z') {
      // DEBUG: testing sheit
      fsm_fx.transitionTo(fx__TestPattern);

    } else {
      Ser.println("\nInfinity Mirror");
      Ser.println("---------------");
      Ser.println("`  : Set output ON/OFF");
      Ser.println("w  : Override all white ON/OFF\n");
      Ser.println("i  : Override IR distance test ON/OFF\n");
      Ser.println("q  : Toggle auto next FX ON/OFF");
      Ser.println("f  : Toggle FPS counter ON/OFF");
      Ser.println("-  : Decrease brightness");
      Ser.println("+  : Increase brightness\n");
      Ser.println("?  : Print current FX & style");
      Ser.println("0-9: Go to FX preset #");
      Ser.println("o  : Go to previous FX");
      Ser.println("p  : Go to next FX");
      Ser.println("[  : Go to previous style");
      Ser.println("]  : Go to next style\n");
      Ser.println("r  : Reset hardware");
    }
  }

  fsm_main.update(); // CRITICAL

  // Periodic updates
  EVERY_N_MILLISECONDS(25) {
    update_IR_dist();
  }

  // DEBUG: Working proof of concept for new mechanism `auto next fx`
  if (ENA_auto_next_fx & fx_has_finished & !ENA_override) {
    next_fx();
  }

  // Original approach
  if (ENA_auto_next_fx & !ENA_override) {
    if (fx_idx == 0) {
      if (fsm_fx.timeInCurrentState() > 10000) {
        next_fx();
      }
    } else {
      if (fsm_fx.timeInCurrentState() > 20000) {
        next_fx();
      }
      /*
      EVERY_N_SECONDS(20) {
        next_fx();
      }
      */
    }
  }
}
