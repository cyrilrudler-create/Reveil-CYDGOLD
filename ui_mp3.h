#ifndef UI_MP3_H
#define UI_MP3_H

#include <lvgl.h>
#include "ui_home.h"

// Exported state flags — read externally (esp32s3audio.ino, ui_alarme.cpp)
extern lv_obj_t* ui_mp3_list;
extern bool is_mp3_mode;
extern bool manual_pause;
extern bool is_playing;

// Build and display the MP3 player screen
void setup_mp3_screen();

// Play the next track — called on track end (audio_eof_mp3) or from alarm
void play_next_mp3();

#endif
