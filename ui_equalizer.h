#ifndef UI_EQUALIZER_H
#define UI_EQUALIZER_H

#include <lvgl.h>

// 3-band equalizer screen (Bass / Mid / Treble)
// Vertical sliders, range -10dB to +6dB, applied in real time
void setup_equalizer_screen();

#endif
