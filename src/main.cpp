/* Infinity mirror

Dennis van Gils
17-04-2022
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
uint8_t bright_idx = 6;
const uint8_t bright_lut[] = {0,   4,   8,   16,  32,  48,  64,  80,  96, 112,
                              128, 144, 160, 176, 192, 208, 224, 240, 255};

const byte PIN_BUTTON = 9;
Switch button = Switch(PIN_BUTTON, INPUT_PULLUP, LOW, 50, 500, 50);

/*------------------------------------------------------------------------------
  Manager to the Finite State Machine which governs calculating the selected
  FastLED effect
------------------------------------------------------------------------------*/
// clang-format off

///*
// Initialize with a preset list of FastLED effects to show consecutively
FastLED_EffectManager fx_mgr = FastLED_EffectManager({
  //        FastLED effect       strip segmentation style           duration [ms]
  //        --------------       ------------------------           -------------
  FX_preset(fx__HeartBeatAwaken, StyleEnum::HALFWAY_PERIO_SPLIT_N2, 9800),
  FX_preset(fx__RainbowSurf    , StyleEnum::FULL_STRIP            , 4000),
  FX_preset(fx__RainbowBarf    , StyleEnum::PERIO_OPP_CORNERS_N2  , 13000),
  FX_preset(fx__Dennis         , StyleEnum::PERIO_OPP_CORNERS_N2  , 13000),
  FX_preset(fx__HeartBeat_2    , StyleEnum::PERIO_OPP_CORNERS_N2  , 13000),
  FX_preset(fx__Rainbow        , StyleEnum::FULL_STRIP            , 13000),
  FX_preset(fx__Sinelon        , StyleEnum::BI_DIR_SIDE2SIDE      , 13000),
  FX_preset(fx__FadeToRed      , 0),
  FX_preset(fx__FadeToBlack    , 0),
});
//*/
/*
FastLED_EffectManager fx_mgr = FastLED_EffectManager({
  FX_preset(fx__HeartBeatAwaken, StyleEnum::HALFWAY_PERIO_SPLIT_N2, 10000),
  FX_preset(fx__HeartBeat, StyleEnum::HALFWAY_PERIO_SPLIT_N2, 10000),
  FX_preset(fx__HeartBeat_2, StyleEnum::PERIO_OPP_CORNERS_N2, 10000),
  FX_preset(fx__RainbowHeartBeat, StyleEnum::FULL_STRIP, 10000),
  FX_preset(fx__FadeToBlack, 0),
});
*/

// clang-format on

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
------------------------------------------------------------------------------*/

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
  Ser.println("Entering MENU");
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
  if (button.singleClick()) {
    Ser.println("single click");
    leds_offset += 13;
    leds_offset %= FLC::N;
  }
  if (button.longPress()) {
    Ser.println("long press");
    fsm_main.transitionTo(show__FastLED);
  }
}

void exit__ShowMenu() {
  Ser.println("Exiting MENU");
  flash_menu(CRGB::Green);
}

/*------------------------------------------------------------------------------
  Show FastLED effect machinery
------------------------------------------------------------------------------*/

void upd__ShowFastLED() {
  uint32_t now = millis();
  static uint32_t tick_audience = now; // Time at audience last detected

  // Audience check: Time-out and go back to sleep when no audience is present
  if (!fx_mgr.fx_override() & (now - tick_audience > FLC::AUDIENCE_TIMEOUT)) {
    fx_mgr.set_fx(FxOverrideEnum::SLEEP_AND_WAIT_FOR_AUDIENCE);

#ifdef USE_ANSI
    ansi.foreground(ANSI::red);
#endif
    Ser.println("Lost interest by audience");
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

  if (fx_has_finished &
      (fx_mgr.fx_override() == FxOverrideEnum::SLEEP_AND_WAIT_FOR_AUDIENCE)) {
    // Check for waking up from sleep because an audience is present
    fx_mgr.set_fx(0);
    tick_audience = now;

#ifdef USE_ANSI
    ansi.foreground(ANSI::green);
#endif
    Ser.println("Audience present");
#ifdef USE_ANSI
    ansi.normal();
#endif

  } else if (ENA_auto_next_fx & fx_has_finished & !fx_mgr.fx_override()) {
    // Check for auto-advancing to the next FastLED effect in the presets list
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
