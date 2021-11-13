/*-----------------------------------------------------------------------------
  Segment styles
------------------------------------------------------------------------------*/
#include "FastLED.h"
#include <Arduino.h>

#define L 13      // Number of LEDs of one side
#define N (4 * L) // Number of LEDs of the full strip (4 * 13 = 52)
/*
uint16_t s = N;    // Number of LEDs of the current segmenting subset
CRGB leds[N];      // LED data: subset
CRGB leds_flip[N]; // LED data: subset flipped
CRGB leds_all[N];  // LED data: full strip
uint16_t idx;      // LED position index used in many for-loops
*/

#define CRGB_SIZE sizeof(CRGB)
#define CRGB_SIZE_L (L * CRGB_SIZE)

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

/*
SegmentStyles segment_style = SegmentStyles::BI_DIR_SIDE2SIDE;

void next_segment_style() {
  int style_int = segment_style;
  style_int = (style_int + 1) % int(SegmentStyles::EOL);
  segment_style = static_cast<SegmentStyles>(style_int);

  // Ser << int(segment_style) << ": " << segment_names[int(segment_style)]
  //     << endl;
}

void calc_leds_flip() {
  for (idx = 0; idx < s; idx++) {
    memmove(&leds_flip[idx], &leds[s - idx - 1], CRGB_SIZE);
  }
}
*/

class LEDStripSegmentor {
private:
  // Mirror and/or repeats an incoming LED pattern, 2 or 4 fold.
  CRGB _input_flipped[N]; // LED data: subset flipped
  SegmentStyles _style = SegmentStyles::BI_DIR_SIDE2SIDE;

  // clang-format off
  //
  // Effects operate on   `leds_effect[]`
  // Strip contents is on `leds_strip[]`
  //
  // This class will operate on `leds_effect[]` and updated to `leds_strip[]`
  //
  // clang-format on
public:
  // LEDStripSegmentor(CRGB (*output)[N]) { _output = output; }
  LEDStripSegmentor() { ; }

  void process(const CRGB (*input)[N], CRGB (*output)[N]) {
    output[0]->setRGB(0, 255, 0);
  }

  void set_style(SegmentStyles style) { _style = style; }

  void next_style_in_list() {
    int style_int = _style;
    style_int = (style_int + 1) % int(SegmentStyles::EOL);
    _style = static_cast<SegmentStyles>(style_int);
  }

  void prev_style_in_list() {
    int style_int = _style;
    style_int =
        (style_int + int(SegmentStyles::EOL) - 1) % int(SegmentStyles::EOL);
    _style = static_cast<SegmentStyles>(style_int);
  }

  SegmentStyles get_style() { return _style; }

  void get_style_descr(char *buffer) {
    snprintf(buffer, 64, segment_names[int(_style)]);
  }
};

/*
int main() {
  CRGB leds_strip[N]; // LED data: full strip
  // LEDStripSegmentor test(&leds_strip);

  LEDStripSegmentor test;
  test.process(&leds_strip);
}
*/