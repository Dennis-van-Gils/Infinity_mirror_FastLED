#include <Arduino.h>

#include "FastLED.h"
#include "LEDStripConfig.h"

/*-----------------------------------------------------------------------------
  Segment styles
------------------------------------------------------------------------------*/

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

const char *style_names[] = {"Full strip",
                             "Copied sides",
                             "Periodic opposite corners, N=4",
                             "Periodic opposite corners, N=2",
                             "Uni-directional side-to-side",
                             "Bi-directional side-to-side",
                             "Half-way periodic split, N=2",
                             "EOL"};

/*-----------------------------------------------------------------------------
  LEDStripSegmentor
--------------------------------------------------------------------------------
  Mirror and/or repeats an incoming LED pattern, 2 or 4 fold
*/

class LEDStripSegmentor {
private:
  int L = LEDStripConfig::L;
  int N = LEDStripConfig::N;
  uint32_t CRGB_SIZE = sizeof(CRGB);
  uint32_t CRGB_SIZE_L = LEDStripConfig::L * CRGB_SIZE;

  SegmentStyles _style;
  uint16_t s; // numel_input

  CRGB in_flip[LEDStripConfig::N]; // Flipped `in` LED effect data

  void calc_in_flip(const struct CRGB(*_in)) {
    for (uint16_t idx = 0; idx < s; idx++) {
      memmove(&in_flip[idx], &_in[s - idx - 1], CRGB_SIZE);
    }
  }

public:
  LEDStripSegmentor() {
    /* */
    set_style(SegmentStyles::FULL_STRIP);
  }

  /*---------------------------------------------------------------------------
    process
  ----------------------------------------------------------------------------*/

  void process(const struct CRGB(*in), struct CRGB(*out)) {
    /*
       `in`  contains the LED effect data to be copied/mirrored, aka leds_effect
       `out` contains the full LED strip data to be send out   , aka leds_strip
    */
    uint16_t idx; // LED position index used in many for-loops

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
        // clang-format off
        memmove(&out[0]    , &in[0], CRGB_SIZE_L); // bottom
        memmove(&out[L]    , &in[0], CRGB_SIZE_L); // right
        memmove(&out[L * 2], &in[0], CRGB_SIZE_L); // top
        memmove(&out[L * 3], &in[0], CRGB_SIZE_L); // left
        // clang-format off
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
        // clang-format off
        calc_in_flip(in);
        memmove(&out[0]    , &in[0]     , CRGB_SIZE_L); // bottom
        memmove(&out[L]    , &in_flip[0], CRGB_SIZE_L); // right
        memmove(&out[L * 2], &in[0]     , CRGB_SIZE_L); // top
        memmove(&out[L * 3], &in_flip[0], CRGB_SIZE_L); // left
        // clang-format on
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
        memmove(&out[0], &in[0], s * CRGB_SIZE); // bottom & right
        for (idx = 0; idx < s; idx++) {          // top & left
          memmove(&out[L * 2 + idx], &in[s - idx - 1], CRGB_SIZE);
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
        // clang-format off
        for (idx = 0; idx < L; idx++) {
          memmove(&out[idx]        , &in[0]      , CRGB_SIZE); // bottom
          memmove(&out[L * 2 + idx], &in[L + 1]  , CRGB_SIZE); // top
          memmove(&out[L * 3 + idx], &in[L - idx], CRGB_SIZE); // left
        }
        memmove(&out[L], &in[1], CRGB_SIZE_L);                 // right
        // clang-format on
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
          // clang-format off
          memmove(&out[idx]        , &in[0]       , CRGB_SIZE); // bottom
          memmove(&out[L + idx]    , &in[(idx < (L / 2) ? idx + 1 : L - idx)]
                                                  , CRGB_SIZE); // right
          memmove(&out[L * 2 + idx], &in[0]       , CRGB_SIZE); // top
          memmove(&out[L * 3 + idx], &out[L + idx], CRGB_SIZE); // left
          // clang-format on
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
        calc_in_flip(in);
        memmove(&out[0]        , &in_flip[s / 2], L / 2 * CRGB_SIZE); // bottom-left
        memmove(&out[L / 2]    , &in[0]         , CRGB_SIZE_L);       // bottom-right & right-bottom
        memmove(&out[L + L / 2], &in_flip[0]    , s / 2 * CRGB_SIZE); // right-top
        memmove(&out[L * 2]    , &out[0]        , CRGB_SIZE_L);       // top
        memmove(&out[L * 3]    , &out[L]        , CRGB_SIZE_L);       // left
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
        memmove(&out[0], &in[0], s * CRGB_SIZE);
        break;
    }
  }

  /*---------------------------------------------------------------------------
    style
  ----------------------------------------------------------------------------*/

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

  SegmentStyles get_style() { return _style; }

  SegmentStyles next_style() {
    int style_int = _style;
    style_int = (style_int + 1) % int(SegmentStyles::EOL);
    set_style(static_cast<SegmentStyles>(style_int));
    return _style;
  }

  SegmentStyles prev_style() {
    int style_int = _style;
    style_int =
        (style_int + int(SegmentStyles::EOL) - 1) % int(SegmentStyles::EOL);
    set_style(static_cast<SegmentStyles>(style_int));
    return _style;
  }

  void get_style_name(char buffer[64]) {
    snprintf(buffer, 64, style_names[int(_style)]);
  }

  void print_style_name(Stream &port) {
    port.print("Style ");
    port.print((int)_style);
    port.print(" - ");
    port.println(style_names[int(_style)]);
  }

  uint16_t get_s() { return s; }
};
