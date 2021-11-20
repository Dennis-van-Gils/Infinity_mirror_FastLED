/* DvG_FastLED_StripSegmenter.h

Manages a FastLED data array making up the full LED strip to be send out. It
takes in another FastLED data array, considered as the base pattern to get
copied/mirrored across by the FastLED_Segmenter to the full LED strip.

There are different styles that can be choosen, in either 1, 2 or 4-fold
symmetry.

    Expects a layout like an infinity mirror with 4 equal sides of length `L`,
    making up the full `out` array of size `N`:

            L
       ┌────<────┐
       │         │
    L  v         ^  L
       │         │
       0────>────┘
            L

Dennis van Gils
20-11-2021
*/
#ifndef DVG_FASTLED_STRIPSEGMENTER_H
#define DVG_FASTLED_STRIPSEGMENTER_H

#include <Arduino.h>

#include "DvG_FastLED_config.h"
#include "FastLED.h"

/*------------------------------------------------------------------------------
  Styles
------------------------------------------------------------------------------*/

#define STYLE_NAME_LEN 64
#define CRGB_SIZE sizeof(CRGB)
#define CRGB_SIZE_L (FastLEDConfig::L * CRGB_SIZE)

enum StyleEnum {
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
                             "EOL"}; // EOL: End-Of-List

/*------------------------------------------------------------------------------
  FastLED_StripSegmenter
-----------------------------------------------------------------------------*/

class FastLED_StripSegmenter {
private:
  int L = FastLEDConfig::L;
  int N = FastLEDConfig::N;

  uint16_t s; // = get_base_numel()
  StyleEnum _style;

  void flip(CRGB *out, const CRGB *in, uint16_t numel) {
    for (uint16_t idx = 0; idx < numel; idx++) {
      out[idx] = in[numel - idx - 1];
    }
  }

public:
  FastLED_StripSegmenter() {
    /* */
    set_style(StyleEnum::FULL_STRIP);
  }

  /*----------------------------------------------------------------------------
    process
  ----------------------------------------------------------------------------*/

  void process(CRGB *out, const CRGB *in) {
    /* Copy/mirror the base array `in` across the full output array `out` using
    1, 2 or 4-fold symmetry as dictated by the currently selected style.

    Expects a layout like an infinity mirror with 4 equal sides of length `L`,
    making up the full `out` array of size `N`:

            L
       ┌────<────┐
       │         │
    L  v         ^  L
       │         │
       0────>────┘
            L

    TODO: Mention `s`
    Calculate the LED data array of the current effect, i.e. the base pattern.
    The array is calculated up to length `s` as dictated by the current
    StripSegmenter style.
    s = segmntr.get_base_numel(); // CRITICAL
    */
    uint16_t idx; // LED position index reused in the for-loops

    switch (_style) {
      case StyleEnum::COPIED_SIDES:
        /* Copied sides

            0 1 2 3                           s = L
            A B C D
               ↓
            D C B A
          A         D
          B         C
          C         B
          D         A       0 1 2 3 / 4 5 6 7 / 8 9 0 1 / 2 3 4 5
            A B C D      →  A B C D / A B C D / A B C D / A B C D
        */
        // clang-format off
        memcpy8(&out[0    ], &in[0], CRGB_SIZE_L); // bottom
        memcpy8(&out[L    ], &in[0], CRGB_SIZE_L); // right
        memcpy8(&out[L * 2], &in[0], CRGB_SIZE_L); // top
        memcpy8(&out[L * 3], &in[0], CRGB_SIZE_L); // left
        // clang-format on
        break;

      case StyleEnum::PERIO_OPP_CORNERS_N4:
        /* Periodic opposite corners, N = 4

            0 1 2 3                           s = L
            A B C D
               ↓
            D C B A
          D         A
          C         B
          B         C
          A         D       0 1 2 3 / 4 5 6 7 / 8 9 0 1 / 2 3 4 5
            A B C D      →  A B C D / D C B A / A B C D / D C B A

            0 1 2 3 4
            A B C D E
                ↓
            E D C B A
          E           A
          D           B
          C           C
          B           D
          A           E     0 1 2 3 4 / 5 6 7 8 9 / 0 1 2 3 4 / 5 6 7 8 9
            A B C D E    →  A	B	C	D	E	/ E	D	C	B	A	/ A	B	C	D	E	/ E	D	C	B	A
        */
        // clang-format off
        memcpy8(&out[0    ], &in[0] , CRGB_SIZE_L    ); // bottom
        flip   (&out[L    ], &in[0] , L              ); // right
        memcpy8(&out[L * 2], &out[0], CRGB_SIZE_L * 2); // top & left
        // clang-format on
        break;

      case StyleEnum::PERIO_OPP_CORNERS_N2:
        /* Periodic opposite corners, N = 2

            0 1 2 3 4 5 6 7                   s = L * 2
            A B C D E F G H
               ↓
            E F G H
          D         H
          C         G
          B         F
          A         E       0 1 2 3 / 4 5 6 7 / 8 9 0 1 / 2 3 4 5
            A B C D      →  A B C D / E F G H / H G F E / D C B A

            0 1 2 3 4 5 6
            A B C D E F G
                ↓
            F G H I J
          E           J
          D           I
          C           H
          B           G
          A           F     0 1 2 3 4 / 5 6 7 8 9 / 0 1 2 3 4 / 5 6 7 8 9
            A B C D E    →  A	B	C	D	E	/ F	G	H	I	J	/ J	I	H	G	F	/ E	D	C	B	A
        */
        // clang-format off
        memcpy8(&out[0    ], &in[0], CRGB_SIZE_L * 2); // bottom & right
        flip   (&out[L * 2], &in[0], L * 2          ); // top & left
        // clang-format on
        break;

      case StyleEnum::UNI_DIR_SIDE2SIDE:
        /* Uni-directional side-to-side

            0 1 2 3 4 5                       s = L + 2
            A B C D E F
               ↓
            F F F F
          E         E
          D         D
          C         C
          B         B       0 1 2 3 / 4 5 6 7 / 8 9 0 1 / 2 3 4 5
            A A A A      →  A A A A / B C D E / F F F F / E D C B

            0 1 2 3 4 5 6
            A B C D E F G
                ↓
            G G G G G
          F           F
          E           E
          D           D
          C           C
          B           B     0 1 2 3 4 / 5 6 7 8 9 / 0 1 2 3 4 / 5 6 7 8 9
            A A A A A    →  A	A	A	A	A	/ B	C	D	E	F	/ G	G	G	G	G	/ F	E	D	C	B
        */
        // clang-format off
        for (idx = 0; idx < L; idx++) {
          out[idx        ] = in[0      ];      // bottom
          out[idx + L * 2] = in[L + 1  ];      // top
          out[idx + L * 3] = in[L - idx];      // left
        }
        memcpy8(&out[L], &in[1], CRGB_SIZE_L); // right
        // clang-format on
        break;

      case StyleEnum::BI_DIR_SIDE2SIDE:
        /* Bi-directional side-to-side

            0 1 2                             s = (L + 1) / 2 + 1
            A B C
               ↓
            A A A A
          B         B
          C         C
          C         C
          B         B       0 1 2 3 / 4 5 6 7 / 8 9 0 1 / 2 3 4 5
            A A A A      →  A A A A / B C C B / A A A A / B C C B

            0 1 2 3
            A B C D
                ↓
            A A A A A
          B           B
          C           C
          D           D
          C           C
          B           B     0 1 2 3 4 / 5 6 7 8 9 / 0 1 2 3 4 / 5 6 7 8 9
            A A A A A    →  A	A	A	A	A	/ B	C	D	C	B / A	A	A	A	A	/ B	C	D	C	B

          Note: Relies on integer math! No residuals.
          L = 4 -> s = 3
          L = 5 -> s = 4
          L = 6 -> s = 4
          L = 7 -> s = 5
        */
        // clang-format off
        for (idx = 0; idx < L; idx++) {
          out[idx    ] = in[0];                                   // bottom
          out[idx + L] = in[(idx < (L / 2) ? idx + 1 : L - idx)]; // right
        }
        memcpy8(&out[L * 2], &out[0], CRGB_SIZE_L * 2);           // top & left
        // clang-format on
        break;

      case StyleEnum::HALFWAY_PERIO_SPLIT_N2:
        /* Half-way periodic split, N = 2

            0 1 2 3                           s = ((L + 1) / 2) * 2
            A B C D
               ↓
            B A A B
          C         C
          D         D
          D         D
          C         C       0 1 2 3 / 4 5 6 7 / 8 9 0 1 / 2 3 4 5
            B A A B      →  B A A B / C D D C / B A A B / C D D C

            0 1 2 3 4 5
            A B C D E F
                ↓
            C B A B C
          D           D
          E           E
          F           F
          E           E
          D           D     0 1 2 3 4 / 5 6 7 8 9 / 0 1 2 3 4 / 5 6 7 8 9
            C B A B C    →  C B A B C / D E F E D / C B A B C / D E F E D

          Note: Relies on integer math! No residuals.
          L = 4 -> s = 4
          L = 5 -> s = 6
          L = 6 -> s = 6
          L = 7 -> s = 8
        */

        // clang-format off
        memcpy8(&out[L / 2], &in[0], CRGB_SIZE_L);      // corner bottom-right
        for (idx = 0; idx < s / 2; idx++) {
          out[idx + L / 2 + L] = in[s - idx - 1];       // right-top
          if (idx == L / 2) {continue;}
          out[idx] = in[s / 2 - idx - 1];               // bottom-left
        }
        memcpy8(&out[L * 2], &out[0], CRGB_SIZE_L * 2); // top & left
        // clang-format on
        break;

      case StyleEnum::FULL_STRIP:
      default:
        /* Full strip, no segments

            L K J I                           s = N
          M         H
          N         G
          O         F
          P         E
            A B C D
        */
        memcpy8(out, in, CRGB_SIZE * N);
        break;
    }
  }

  /*----------------------------------------------------------------------------
    style
  ----------------------------------------------------------------------------*/

  void set_style(StyleEnum style) {
    _style = style;
    switch (_style) {
      case StyleEnum::COPIED_SIDES:
        s = L;
        break;
      case StyleEnum::PERIO_OPP_CORNERS_N4:
        s = L;
        break;
      case StyleEnum::PERIO_OPP_CORNERS_N2:
        s = L * 2;
        break;
      case StyleEnum::UNI_DIR_SIDE2SIDE:
        s = L + 2;
        break;
      case StyleEnum::BI_DIR_SIDE2SIDE:
        s = (L + 1) / 2 + 1;
        break;
      case StyleEnum::HALFWAY_PERIO_SPLIT_N2:
        s = ((L + 1) / 2) * 2;
        break;
      case StyleEnum::FULL_STRIP:
      default:
        s = N;
        break;
    }
  }

  StyleEnum next_style() {
    int style_int = _style;
    style_int = (style_int + 1) % int(StyleEnum::EOL);
    set_style(static_cast<StyleEnum>(style_int));
    return _style;
  }

  StyleEnum prev_style() {
    int style_int = _style;
    style_int = (style_int + int(StyleEnum::EOL) - 1) % int(StyleEnum::EOL);
    set_style(static_cast<StyleEnum>(style_int));
    return _style;
  }

  StyleEnum get_style() { return _style; }

  void get_style_name(char buffer[STYLE_NAME_LEN]) {
    snprintf(buffer, STYLE_NAME_LEN, style_names[int(_style)]);
  }

  void print_style_name(Stream &port) {
    port.print("Style : ");
    port.print((int)_style);
    port.print(" - ");
    port.println(style_names[int(_style)]);
  }

  /*----------------------------------------------------------------------------
    get_base_numel
  ----------------------------------------------------------------------------*/

  uint16_t get_base_numel() {
    /* Return the number of elements making up the base pattern befitting the
    current style.
     */
    return s;
  }
};

#endif