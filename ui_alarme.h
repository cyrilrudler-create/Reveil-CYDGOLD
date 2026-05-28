#ifndef UI_ALARME_H
#define UI_ALARME_H

#include <lvgl.h>
#include "ui_config.h"

// Alarm settings screen
void setup_alarme_screen();

// Check if the alarm should trigger — called every 4s from update_time()
// rtc_h / rtc_m : current hour and minute from the DS3231
void check_alarme(int rtc_h, int rtc_m);

#endif
