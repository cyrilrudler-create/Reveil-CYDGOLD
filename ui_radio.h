#ifndef UI_RADIO_H
#define UI_RADIO_H

#include <lvgl.h>
#include "ui_home.h"
#include "SD_MMC.h"

// Build and display the web radio screen
void setup_radio_screen();

// Update the clock label in the top bar (called from update_time)
void update_radio_time(const char* time_str);

// Update the scrolling ICY stream title (called from audio_showstreamtitle callback)
void update_radio_stream_title(const char* title);

#endif
