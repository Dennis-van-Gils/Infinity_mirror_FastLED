/* DvG_FastLED_EffectManager.h

Manages the Finite State Machine resonsible for calculating the FastLED effect

Dennis van Gils
17-04-2022
*/
#ifndef DVG_FASTLED_EFFECTMANAGER_H
#define DVG_FASTLED_EFFECTMANAGER_H

#include <Arduino.h>
#include <vector>

#include "FastLED.h"
#include "FiniteStateMachine.h"

#include "DvG_FastLED_StripSegmenter.h"
#include "DvG_FastLED_config.h"
#include "DvG_FastLED_effects.h"

using namespace std;

enum FxOverrideEnum {
  NONE,                       // No override
  ALL_BLACK,                  // Override: Turn all leds off
  ALL_WHITE,                  // Override: Turn all leds to white
  IR_DIST,                    // Override: Show IR distance test
  TEST_PATTERN,               // Override: Show test pattern
  SLEEP_AND_WAIT_FOR_AUDIENCE // Override
};

/*------------------------------------------------------------------------------
  FX preset
------------------------------------------------------------------------------*/

struct FX_preset {
  // Constructors
  FX_preset(State _fx) : fx{_fx} {}
  FX_preset(State _fx, uint32_t _duration) : fx{_fx}, duration{_duration} {}
  FX_preset(State _fx, StyleEnum _style) : fx{_fx}, style{_style} {}
  FX_preset(State _fx, StyleEnum _style, uint32_t _duration)
      : fx{_fx}, style{_style}, duration{_duration} {}

  // Members and defaults
  State fx{fx__FadeToBlack};
  StyleEnum style{StyleEnum::FULL_STRIP};
  uint32_t duration{
      0}; // 0 indicates infinite duration or until effect is done otherwise
};

/*------------------------------------------------------------------------------
  FastLED_EffectManager
  NOTE: Handle this class as a singleton
  TODO: Enforce singleton
-----------------------------------------------------------------------------*/

class FastLED_EffectManager {
private:
  uint16_t _fx_idx = 0;
  std::vector<FX_preset> _fx_list;
  bool _fx_has_changed = true;
  FxOverrideEnum _fx_override = FxOverrideEnum::NONE;

  // Finite State Machine governing the FastLED effect calculation
  FSM _fsm_fx = FSM(fx__FadeToBlack);

public:
  FastLED_EffectManager(std::vector<FX_preset> fx_list) {
    /* Constructor, initialized with a presets list of FastLED effects to run
     */
    _fx_list = fx_list;
    _fsm_fx.immediateTransitionTo(_fx_list[_fx_idx].fx);
    fx_style = _fx_list[_fx_idx].style;
    fx_duration = _fx_list[_fx_idx].duration;
  }

  void set_fx_list(std::vector<FX_preset> fx_list) {
    /* Dynamically change the presets list of FastLED effects to run
     */
    _fx_list = fx_list;
    set_fx(_fx_idx); // Assume we are already running, hence play it safe
  }

  void update() {
    /* Calculate the current FastLED effect
     */
    _fsm_fx.update();
  }

  /* Redundant
  uint32_t time_in_current_fx() {
    return _fsm_fx.timeInCurrentState();
  }
  */

  /* Redundant
  uint16_t fx_idx() {
    return _fx_idx;
  }
  */

  FxOverrideEnum fx_override() {
    return _fx_override;
  }

  bool fx_has_changed() {
    if (_fx_has_changed) {
      _fx_has_changed = false;
      return true;
    } else {
      return false;
    }
  }

  bool toggle_fx_override(FxOverrideEnum fx_override) {
    if (_fx_override != fx_override) {
      set_fx(fx_override);
      return true;
    } else {
      set_fx(FxOverrideEnum::NONE);
      return false;
    }
  }

  void set_fx(FxOverrideEnum fx_override) {
    _fx_override = fx_override;
    switch (fx_override) {
      case FxOverrideEnum::ALL_BLACK:
        _fsm_fx.transitionTo(fx__FadeToBlack);
        fx_duration = 0;
        break;
      case FxOverrideEnum::ALL_WHITE:
        _fsm_fx.transitionTo(fx__FadeToWhite);
        fx_duration = 0;
        break;
      case FxOverrideEnum::IR_DIST:
        _fsm_fx.transitionTo(fx__IRDist);
        fx_duration = 0;
        break;
      case FxOverrideEnum::TEST_PATTERN:
        _fsm_fx.transitionTo(fx__TestPattern);
        fx_duration = 0;
        break;
      case FxOverrideEnum::SLEEP_AND_WAIT_FOR_AUDIENCE:
        _fsm_fx.transitionTo(fx__SleepAndWaitForAudience);
        fx_duration = 0;
        break;
      case FxOverrideEnum::NONE:
        set_fx(_fx_idx);
        fx_duration = _fx_list[_fx_idx].duration;
        break;
    }
    _fx_has_changed = true;
  }

  void set_fx(uint16_t idx) {
    _fx_override = FxOverrideEnum::NONE;
    _fx_idx = min(idx, _fx_list.size() - 1);
    _fsm_fx.transitionTo(_fx_list[_fx_idx].fx);
    _fx_has_changed = true;

    fx_style = _fx_list[_fx_idx].style;
    fx_duration = _fx_list[_fx_idx].duration;
  }

  void prev_fx() {
    set_fx((_fx_idx + _fx_list.size() - 1) % _fx_list.size());
  }

  void next_fx() {
    set_fx((_fx_idx + 1) % _fx_list.size());
  }

  void prev_style() {
    segmntr1.prev_style();
  }

  void next_style() {
    segmntr1.next_style();
  }

  /*----------------------------------------------------------------------------
    Prints
  ----------------------------------------------------------------------------*/

  void print_fx(Stream *stream) {
    // NOTE: `_fsm_fx.update()` must have run to print out the proper name
    static char buffer[STATE_NAME_LEN] = {"\0"};
    _fsm_fx.getCurrentStateName(buffer);
#ifdef USE_ANSI
    ansi.foreground(ANSI::yellow);
#endif
    stream->print("Effect: ");
    if (_fx_override == FxOverrideEnum::NONE) {
      stream->print(_fx_idx);
    } else {
      stream->print("*");
    }
    stream->print(" - \"");
    stream->print(buffer);
    stream->println("\"");
#ifdef USE_ANSI
    ansi.normal();
#endif
  }

  void print_style(Stream *stream) {
    static char buffer[STYLE_NAME_LEN] = {"\0"};
    segmntr1.get_style_name(buffer);
#ifdef USE_ANSI
    ansi.foreground(ANSI::white | ANSI::bright);
#endif
    stream->print("Style : ");
    stream->print((int)segmntr1.get_style());
    stream->print(" - ");
    stream->println(buffer);
#ifdef USE_ANSI
    ansi.normal();
#endif
  }
};

#endif
