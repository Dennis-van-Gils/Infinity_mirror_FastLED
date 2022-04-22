/* Infinity mirror

Dennis van Gils
22-04-2022
*/

#include <Arduino.h>
#include <vector>

#include "FastLED.h"
#include "FiniteStateMachine.h"
#include "RunningAverage.h"
#include "avdweb_Switch.h"

FASTLED_USING_NAMESPACE

#define Ser Serial
#define USE_ANSI
#ifdef USE_ANSI
#  include "ansi.h"
ANSI ansi(&Serial);
#endif

#include "DvG_FastLED_EffectManager.h"
#include "DvG_FastLED_config.h"
#include "DvG_FastLED_effects.h"

static bool ENA_auto_next_fx = true; // Automatically go to next effect?
static bool ENA_print_FPS = false;   // Print FPS counter to serial?

// Brightness
uint8_t bright_idx = 5;
const uint8_t bright_lut[] = {0,   10,  30,  50,  70,  90,  110,
                              130, 150, 170, 190, 210, 230, 255};

const byte PIN_BUTTON = 9;
Switch button = Switch(PIN_BUTTON, INPUT_PULLUP, LOW, 50, 500, 50);

/*------------------------------------------------------------------------------
  Manager to the Finite State Machine which governs calculating the selected
  FastLED effect
------------------------------------------------------------------------------*/
// clang-format off
// Initialize with a preset list of FastLED effects to show consecutively
FastLED_EffectManager fx_mgr = FastLED_EffectManager({
  //        FastLED effect       strip segmentation style           duration [ms]
  //        --------------       ------------------------           -------------
  FX_preset(fx__HeartBeatAwaken, StyleEnum::HALFWAY_PERIO_SPLIT_N2, 9800),
  FX_preset(fx__RainbowSurf    , StyleEnum::FULL_STRIP            , 8000),
  FX_preset(fx__RainbowBarf    , StyleEnum::PERIO_OPP_CORNERS_N2  , 13000),
  FX_preset(fx__Dennis         , StyleEnum::PERIO_OPP_CORNERS_N2  , 13000),
  FX_preset(fx__HeartBeat_2    , StyleEnum::PERIO_OPP_CORNERS_N2  , 13000),
  FX_preset(fx__DoubleWaveIA   , StyleEnum::COPIED_SIDES          , 13000),
  FX_preset(fx__Sinelon        , StyleEnum::BI_DIR_SIDE2SIDE      , 13000),
  FX_preset(fx__FadeToRed      , 0),
  FX_preset(fx__FadeToBlack    , 0),
});
// clang-format on

/*------------------------------------------------------------------------------
  IR distance sensor
--------------------------------------------------------------------------------
  Sharp 2Y0A02, pin A2
  Fit: distance [cm] = A / bitval ^ C - B, where bitval is at 10-bit
*/
// clang-format off
#define A2_BITS 10         // Calibration has been performed at 10 bits ADC only
uint8_t IR_dist_cm = 0;    // IR distance in [cm]
uint8_t IR_dist_fract = 0; // IR distance as fraction of the full scale [0-255]
const uint8_t IR_DIST_MIN = 16;   // [cm]
const uint8_t IR_DIST_MAX = 150;  // [cm]
const float   IR_CALIB_A = 1512.89;
const uint8_t IR_CALIB_B = 74;
const float   IR_CALIB_C = 0.424;
// clang-format on

void update_IR_dist() {
  // Read out the IR distance sensor in [cm] and compute the running average
  uint16_t bitval;
  float instant_cm;
  static RunningAverage RA(20);

  // Read out instanteneous IR distance
  bitval = analogRead(PIN_A2);
  if (bitval < 80) { // Cap readings when distance is likely too small
    instant_cm = IR_DIST_MAX;
  } else {
    instant_cm = IR_CALIB_A / pow(bitval, IR_CALIB_C) - IR_CALIB_B;
    instant_cm = constrain(instant_cm, IR_DIST_MIN, IR_DIST_MAX);
  }

  // Apply running average
  RA.addValue(instant_cm);
  IR_dist_cm = RA.getAverage();
  IR_dist_fract = round((RA.getAverage() - IR_DIST_MIN) /
                        (IR_DIST_MAX - IR_DIST_MIN) * 255);

  if (fx_mgr.fx_override() == FxOverrideEnum::IR_DIST) {
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

/*------------------------------------------------------------------------------
  Show Menu machinery

  Menu options are indicated by lighting up one of the four corners of the
  mirror. Always starts with option 1 selected in the menu.

  1) └  Default mode: Show all effects in the preset list consecutively as long
        as an audience is present. Turn the master switch back ON if needed.
  2) ┘  Toggle `auto-next FX` ON/OFF
  3) ┐  Override with IR distance test
  4) ┌  Set master switch to OFF (stay off regardless of audience present)

------------------------------------------------------------------------------*/
static uint8_t menu_idx = 0; // Note: index starts at 0 == menu option 1
static uint32_t menu_tick = millis(); // [ms] Keeps track of time
static bool menu_entered_brightness = false;

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

void show_menu_option() {
  fill_solid(leds, FLC::N, CRGB::Black);
  if (menu_idx < 4) {
    // Show menu option by lighting up the appropiate corner of the mirror
    for (int16_t idx = menu_idx * FLC::L - FLC::MENU_WIDTH;
         idx < menu_idx * FLC::L + FLC::MENU_WIDTH; idx++) {
      int16_t modval = idx % FLC::N;
      if (modval < 0) {
        modval += FLC::N;
      }
      leds[modval] = CRGB::Red;
    }
  } else {
    // Show menu option 5 by lighting up the four centers of the mirror sides
    for (byte side_idx = 0; side_idx < 4; side_idx++) {
      for (int16_t idx = side_idx * FLC::L + FLC::L / 2 - FLC::MENU_WIDTH;
           idx < side_idx * FLC::L + FLC::L / 2 + FLC::MENU_WIDTH; idx++) {
        int16_t modval = idx % FLC::N;
        if (modval < 0) {
          modval += FLC::N;
        }
        leds[modval] = CRGB::Red;
      }
    }
  }
  FastLED.delay(20);
}

void show_menu_brightness() {
  // Show the set brightness as a VU meter on the bottom side
  uint8_t bright = FastLED.getBrightness();
  for (uint16_t idx = 0; idx < FLC::L; idx++) {
    leds[idx] =
        (round(255.f * idx / FLC::L) <= bright ? CRGB::Red : CRGB::Black);
  }
  Ser.print("Brightness ");
  Ser.println(bright_lut[bright_idx]);

  FastLED.delay(20);
}

void entr__ShowMenu() {
  Ser.println("Entering MENU");
  flash_menu(CRGB::Red);
  menu_idx = 0;
  show_menu_option();
  menu_entered_brightness = false;
}

void upd__ShowMenu() {

  if ((menu_idx < 4) | ((menu_idx == 4) & (millis() - menu_tick < 1000))) {
    // Handle menu options 1 to 4 and check time-out of menu option 5 to go into
    // setting the brightness
    if (fsm_main.timeInCurrentState() > 10000) {
      fsm_main.transitionTo(show__FastLED);
    }

    // Check for button presses
    button.poll();
    if (button.singleClick()) {
      Ser.println("single click");
      menu_idx = (menu_idx + 1) % 5;
      if (menu_idx == 4) {
        menu_entered_brightness = true;
        menu_tick = millis();
      }
      show_menu_option();
    }
    if (button.longPress()) {
      Ser.println("long press");
      fsm_main.transitionTo(show__FastLED);
    }
  } else {
    // Setting the brightness
    if (menu_entered_brightness) {
      Ser.println("Entering 'Set brightness'");
      menu_entered_brightness = false;
      show_menu_brightness();
    }

    // Check for button presses
    button.poll();
    if (button.singleClick()) {
      bright_idx = (bright_idx + 1) % sizeof(bright_lut);
      FastLED.setBrightness(bright_lut[bright_idx]);
      show_menu_brightness();
    }
    if (button.longPress()) {
      Ser.println("long press");
      fsm_main.transitionTo(show__FastLED);
    }
  }
}

void exit__ShowMenu() {
  Ser.print("Exiting MENU with chosen option: ");
  Ser.println(menu_idx + 1);

  switch (menu_idx) {
    case 0:
      Ser.println("Default mode is set");
      fx_mgr.set_fx(0);
      ENA_auto_next_fx = true;
      break;
    case 1:
      ENA_auto_next_fx = !ENA_auto_next_fx;
      Ser.print("Auto-next FX: ");
      Ser.println(ENA_auto_next_fx ? "ON" : "OFF");
      break;
    case 2:
      Ser.print("IR distance test: ");
      Ser.println(fx_mgr.toggle_fx_override(FxOverrideEnum::IR_DIST) ? "ON"
                                                                     : "OFF");
      break;
    case 3:
      Ser.print("Output: ");
      Ser.println(fx_mgr.toggle_fx_override(FxOverrideEnum::ALL_BLACK) ? "OFF"
                                                                       : "ON");
      break;
    case 4:
      Ser.println("Brightness is set");
      break;
  }

  flash_menu(CRGB::Green);
}

/*------------------------------------------------------------------------------
  Show FastLED effect machinery
------------------------------------------------------------------------------*/

void upd__ShowFastLED() {
  uint32_t now = millis();
  static uint32_t tick_audience = now; // Time at audience last detected

  // Check if we timed out because no audience is present
  if (
      // We just started but lost audience very quickly?
      (!fx_mgr.fx_override() & (fx_mgr.fx_idx() == 0) &
       (fx_mgr.time_in_current_fx() > 8500) &
       (IR_dist_cm > FLC::AUDIENCE_DISTANCE)) |
      // We ran the default program for an extended time but lost audience?
      (!fx_mgr.fx_override() & (now - tick_audience > FLC::AUDIENCE_TIMEOUT))) {
    // Go back to sleep
    fx_mgr.set_fx(FxOverrideEnum::SLEEP_AND_WAIT_FOR_AUDIENCE);

#ifdef USE_ANSI
    ansi.foreground(ANSI::red);
#endif
    Ser.println("Lost interest from audience");
#ifdef USE_ANSI
    ansi.normal();
#endif
  }

  // CRITICAL: Calculate the current FastLED effect
  fx_mgr.update();

  if (fx_mgr.fx_has_changed()) {
    fx_mgr.print_fx(&Ser);
    fx_mgr.print_style(&Ser);
  }

  // Send out LED data to the strip. `delay()` keeps the framerate modest and
  // allows for brightness dithering. It will invoke FastLED.show() - sending
  // out the LED data - at least once during the delay.
  FastLED.delay(2);

  // Print FPS counter
  EVERY_N_MILLISECONDS(1000) {
    if (ENA_print_FPS) {
      Ser.println(FastLED.getFPS());
    }
  }

  // Check for button presses
  button.poll();
  if (button.singleClick()) {
    Ser.println("single click");
    if (!fx_mgr.fx_override()) {
      fx_mgr.next_fx();
    }
  }
  if (button.longPress()) {
    Ser.println("long press");
    fsm_main.transitionTo(show__Menu);
  }

  // Check for an audience and/or auto-advancing to the next FX
  if (fx_has_finished &
      (fx_mgr.fx_override() == FxOverrideEnum::SLEEP_AND_WAIT_FOR_AUDIENCE)) {
    // Woken up from sleep because an audience is present
    if (ENA_auto_next_fx) {
      // Start from the beginning of the effects preset list
      fx_mgr.set_fx(0);
    } else {
      // Pick up at the last shown effect
      fx_mgr.set_fx(FxOverrideEnum::NONE);
    }
    tick_audience = now;

#ifdef USE_ANSI
    ansi.foreground(ANSI::green);
#endif
    Ser.println("Audience present");
#ifdef USE_ANSI
    ansi.normal();
#endif

  } else if (ENA_auto_next_fx & fx_has_finished & !fx_mgr.fx_override()) {
    // Auto-advance to the next FastLED effect in the presets list
    fx_mgr.next_fx();
  }
}

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
  FastLED.setBrightness(bright_lut[bright_idx]);
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
  char char_cmd; // Incoming serial command

  // Check for incoming serial commands
  if (Ser.available() > 0) {
    char_cmd = Serial.read();

    if (char_cmd == '?') {
      fx_mgr.print_fx(&Ser);
      fx_mgr.print_style(&Ser);

    } else if (char_cmd == '`') {
      Ser.print("Output: ");
      Ser.println(fx_mgr.toggle_fx_override(FxOverrideEnum::ALL_BLACK) ? "OFF"
                                                                       : "ON");

    } else if (char_cmd == 'w') {
      Ser.print("All white: ");
      Ser.println(fx_mgr.toggle_fx_override(FxOverrideEnum::ALL_WHITE) ? "ON"
                                                                       : "OFF");

    } else if (char_cmd == 'i') {
      Ser.print("IR distance test: ");
      Ser.println(fx_mgr.toggle_fx_override(FxOverrideEnum::IR_DIST) ? "ON"
                                                                     : "OFF");

    } else if (char_cmd == 'z') {
      Ser.print("Test pattern: ");
      Ser.println(fx_mgr.toggle_fx_override(FxOverrideEnum::TEST_PATTERN)
                      ? "ON"
                      : "OFF");

    } else if (char_cmd == 'q') {
      ENA_auto_next_fx = !ENA_auto_next_fx;
      Ser.print("Auto-next FX: ");
      Ser.println(ENA_auto_next_fx ? "ON" : "OFF");

    } else if ((char_cmd >= '0') && (char_cmd <= '9')) {
      fx_mgr.set_fx(char_cmd - '0');

    } else if (char_cmd == 'o') {
      fx_mgr.prev_fx();

    } else if (char_cmd == 'p') {
      fx_mgr.next_fx();

    } else if (char_cmd == '[') {
      fx_mgr.prev_style();
      fx_mgr.print_style(&Ser);

    } else if (char_cmd == ']') {
      fx_mgr.next_style();
      fx_mgr.print_style(&Ser);

    } else if (char_cmd == '-') {
      bright_idx = bright_idx > 0 ? bright_idx - 1 : 0;
      FastLED.setBrightness(bright_lut[bright_idx]);
      Ser.print("Brightness ");
      Ser.println(bright_lut[bright_idx]);

    } else if ((char_cmd == '+') || (char_cmd == '=')) {
      bright_idx = bright_idx < sizeof(bright_lut) - 2 ? bright_idx + 1
                                                       : sizeof(bright_lut) - 1;
      FastLED.setBrightness(bright_lut[bright_idx]);
      Ser.print("Brightness ");
      Ser.println(bright_lut[bright_idx]);

    } else if (char_cmd == 'f') {
      ENA_print_FPS = !ENA_print_FPS;

    } else if (char_cmd == 'r') {
      NVIC_SystemReset();

    } else {
      Ser.println("\nInfinity Mirror");
      Ser.println("---------------");
      Ser.println("`  : Output ON/OFF");
      Ser.println("w  : Override FX: Toggle all leds white ON/OFF");
      Ser.println("i  : Override FX: Toggle IR distance test ON/OFF");
      Ser.println("z  : Override FX: Toggle test pattern ON/OFF");
      Ser.println("r  : Reset hardware\n");

      Ser.println("q  : Toggle auto-next FX ON/OFF");
      Ser.println("f  : Toggle FPS counter ON/OFF");
      Ser.println("-  : Decrease brightness");
      Ser.println("+  : Increase brightness\n");

      Ser.println("?  : Print current FX & style");
      Ser.println("0-9: Go to FX preset #");
      Ser.println("o  : Go to previous FX");
      Ser.println("p  : Go to next FX");
      Ser.println("[  : Go to previous style");
      Ser.println("]  : Go to next style\n");
    }
  }

  // CRITICAL: Run the main Finite State Machine
  fsm_main.update();

  // Periodically read out the IR distance sensor
  EVERY_N_MILLISECONDS(25) {
    update_IR_dist();
  }
}
