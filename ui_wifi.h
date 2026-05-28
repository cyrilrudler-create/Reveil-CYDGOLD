#ifndef UI_WIFI_H
#define UI_WIFI_H

#include <lvgl.h>
#include "ui_config.h"

// WiFi management screen
void setup_wifi_screen();

// Start a WiFi scan and populate the scan panel
void start_wifi_scan();

// Refresh the saved-networks list (called after connect/disconnect)
void refresh_saved_wifi_list();

#endif
