/*-----------------------------------------------------------------------------
  Segment styles
------------------------------------------------------------------------------*/
#include <Arduino.h>

#include "FastLED.h"
#include "LEDStripConfig.h"

#define CRGB_SIZE sizeof(CRGB)
#define CRGB_SIZE_L (LEDStripConfig::L * CRGB_SIZE)

enum SegmentStyles {
  FULL_STRIP,
  COPIED_SIDES,
  PERIO_OPP_CORNERS_N4,
  PERIO_OPP_CORNERS_N2,
  UNI_DIR_SIDE2SIDE,
  BI_DIR_SIDE2SIDE,
  HALFWAY_PERIO_SPLIT_N2,
  EOL // End of list
};

const char *segment_names[] = {"Full strip",
                               "Copied sides",
                               "Periodic opposite corners, N=4",
                               "Periodic opposite corners, N=2",
                               "Uni-directional side-to-side",
                               "Bi-directional side-to-side",
                               "Half-way periodic split, N=2",
                               "EOL"};

class LEDStripSegmentor {
private:
  // Mirror and/or repeats an incoming LED pattern, 2 or 4 fold.
  uint16_t idx; // LED position index used in many for-loops
  int L = LEDStripConfig::L;
  int N = LEDStripConfig::N;

  CRGB _input_flipped[LEDStripConfig::N]; // LED data: subset flipped
  SegmentStyles _style;
  uint16_t s; // numel_input

  void calc_leds_flip(const struct CRGB(*input)) {
    for (idx = 0; idx < s; idx++) {
      memmove(&_input_flipped[idx], &input[s - idx - 1], CRGB_SIZE);
    }
  }

public:
  LEDStripSegmentor() {
    /* */
    set_style(SegmentStyles::FULL_STRIP);
  }

  void process(const struct CRGB(*input), struct CRGB(*output)) {
    /*
       `input`  contains the LED effect to be copied/mirrored, aka leds_effect
       `output` contains the calculated full LED strip       , aka leds_strip
    */

    // Perform LED-strip segmenting
    switch (_style) {
      case SegmentStyles::COPIED_SIDES:
        /* Copied sides

            D C B A
          A         D
          B         C
          C         B
          D         A
            A B C D

          s = L;
        */
        memmove(&output[0], &input[0], CRGB_SIZE_L);     // bottom
        memmove(&output[L], &input[0], CRGB_SIZE_L);     // right
        memmove(&output[L * 2], &input[0], CRGB_SIZE_L); // top
        memmove(&output[L * 3], &input[0], CRGB_SIZE_L); // left
        break;

      case SegmentStyles::PERIO_OPP_CORNERS_N4:
        /* Periodic opposite corners, N = 4

            D C B A
          D         A
          C         B
          B         C
          A         D
            A B C D

          s = L;
        */
        calc_leds_flip(input);
        memmove(&output[0], &input[0], CRGB_SIZE_L);              // bottom
        memmove(&output[L], &_input_flipped[0], CRGB_SIZE_L);     // right
        memmove(&output[L * 2], &input[0], CRGB_SIZE_L);          // top
        memmove(&output[L * 3], &_input_flipped[0], CRGB_SIZE_L); // left
        break;

      case SegmentStyles::PERIO_OPP_CORNERS_N2:
        /* Periodic opposite corners, N = 2

            E F G H
          D         H
          C         G
          B         F
          A         E
            A B C D

          s = L * 2;
        */
        memmove(&output[0], &input[0], s * CRGB_SIZE); // bottom & right
        for (idx = 0; idx < s; idx++) {                // top & left
          memmove(&output[L * 2 + idx], &input[s - idx - 1], CRGB_SIZE);
        }
        break;

      case SegmentStyles::UNI_DIR_SIDE2SIDE:
        /* Uni-directional side-to-side

            F F F F
          E         E
          D         D
          C         C
          B         B
            A A A A

          s = L + 2;
        */
        for (idx = 0; idx < L; idx++) {
          memmove(&output[idx], &input[0], CRGB_SIZE);               // bottom
          memmove(&output[L * 2 + idx], &input[L + 1], CRGB_SIZE);   // top
          memmove(&output[L * 3 + idx], &input[L - idx], CRGB_SIZE); // left
        }
        memmove(&output[L], &input[1], CRGB_SIZE_L); // right
        break;

      case SegmentStyles::BI_DIR_SIDE2SIDE:
        /* Bi-directional side-to-side

            A A A A
          B         B
          C         C
          C         C
          B         B
            A A A A

          s = (L + 1) / 2 + 1; // Note: Relies on integer math! No residuals.
          L = 4 -> s = 3
          L = 5 -> s = 4
          L = 6 -> s = 4
          L = 7 -> s = 5
        */
        for (idx = 0; idx < L; idx++) {
          memmove(&output[idx], &input[0], CRGB_SIZE); // bottom
          memmove(&output[L + idx], &input[(idx < (L / 2) ? idx + 1 : L - idx)],
                  CRGB_SIZE);                                         // right
          memmove(&output[L * 2 + idx], &input[0], CRGB_SIZE);        // top
          memmove(&output[L * 3 + idx], &output[L + idx], CRGB_SIZE); // left
        }
        break;

      case SegmentStyles::HALFWAY_PERIO_SPLIT_N2:
        /* Half-way periodic split, N = 2

            B A A B
          C         C
          D         D
          D         D
          C         C
            B A A B

          s = ((L + 1) / 2) * 2; // Note: Relies on integer math! No residuals.
          L = 4 -> s = 4
          L = 5 -> s = 6
          L = 6 -> s = 6
          L = 7 -> s = 8
        */
        // clang-format off
        calc_leds_flip(input);
        memmove(&output[0], &_input_flipped[s / 2], L / 2 * CRGB_SIZE); // bottom-left
        memmove(&output[L / 2], &input[0], CRGB_SIZE_L);                // bottom-right & right-bottom
        memmove(&output[L + L / 2], &_input_flipped[0], s / 2 * CRGB_SIZE);                       // right-top
        memmove(&output[L * 2], &output[0], CRGB_SIZE_L); // top
        memmove(&output[L * 3], &output[L], CRGB_SIZE_L); // left
        // clang-format on
        break;

      case SegmentStyles::FULL_STRIP:
      default:
        /* Full strip, no segmenting

            L K J I
          M         H
          N         G
          O         F
          P         E
            A B C D

          s = N;
        */
        memmove(&output[0], &input[0], s * CRGB_SIZE);
        break;
    }
  }

  void set_style(SegmentStyles style) {
    _style = style;
    switch (_style) {
      case SegmentStyles::COPIED_SIDES:
        s = L;
        break;
      case SegmentStyles::PERIO_OPP_CORNERS_N4:
        s = L;
        break;
      case SegmentStyles::PERIO_OPP_CORNERS_N2:
        s = L * 2;
        break;
      case SegmentStyles::UNI_DIR_SIDE2SIDE:
        s = L + 2;
        break;
      case SegmentStyles::BI_DIR_SIDE2SIDE:
        s = (L + 1) / 2 + 1;
        break;
      case SegmentStyles::HALFWAY_PERIO_SPLIT_N2:
        s = ((L + 1) / 2) * 2;
        break;
      case SegmentStyles::FULL_STRIP:
      default:
        s = N;
        break;
    }
  }

  void next_style_in_list() {
    int style_int = _style;
    style_int = (style_int + 1) % int(SegmentStyles::EOL);
    set_style(static_cast<SegmentStyles>(style_int));
  }

  void prev_style_in_list() {
    int style_int = _style;
    style_int =
        (style_int + int(SegmentStyles::EOL) - 1) % int(SegmentStyles::EOL);
    set_style(static_cast<SegmentStyles>(style_int));
  }

  SegmentStyles get_style() { return _style; }

  void get_style_descr(char *buffer) {
    snprintf(buffer, 64, segment_names[int(_style)]);
  }

  uint16_t get_s() { return s; }
};
