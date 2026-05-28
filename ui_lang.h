#ifndef UI_LANG_H
#define UI_LANG_H

#include <Arduino.h>

// =====================================================
//  TRANSLATION SYSTEM
//
//  How to add a new language:
//    1. Add an entry to LangID (before LANG_COUNT)
//    2. Fill a new LangStrings table in ui_lang.cpp
//    3. Add it to lang_table[] in ui_lang.cpp
//    4. Add its display name to lang_names[] in ui_lang.cpp
//
//  All UI code uses `lang->field_name` — never
//  hard-coded strings. Changing language = one pointer swap.
// =====================================================

enum LangID {
    LANG_FR = 0,  // Francais
    LANG_EN = 1,  // English
    LANG_ES = 2,  // Espanol
    LANG_IT = 3,  // Italiano
    LANG_DE = 4,  // Deutsch
    LANG_PT = 5,  // Portugais
    LANG_COUNT    // always last — used for array sizing
};

struct LangStrings {
    const char* lang_name;          // Display name in the language picker

    const char* days[7];            // 0=Sun ... 6=Sat
    const char* months[12];         // 0=January ... 11=December

    // --- Radio screen ---
    const char* radio_title;

    // --- MP3 screen ---
    const char* mp3_select_file;
    const char* mp3_no_file;
    const char* mp3_create_folder;
    const char* mp3_err_sd;

    // --- Config screen ---
    const char* config_title;
    const char* config_brightness;
    const char* config_volume;
    const char* config_btn_wifi;
    const char* config_btn_alarm;
    const char* config_btn_leds;
    const char* config_btn_eq;
    const char* config_btn_themes;
    const char* config_btn_tz;
    const char* config_btn_lang;
    const char* config_btn_info;

    // --- WiFi screen ---
    const char* wifi_title;
    const char* wifi_back;
    const char* wifi_scan;
    const char* wifi_saved;
    const char* wifi_connected;
    const char* wifi_lost;
    const char* wifi_scanning;
    const char* wifi_available;
    const char* wifi_no_saved;
    const char* wifi_no_network;
    const char* wifi_pwd_placeholder;

    // --- Alarm screen ---
    const char* alarm_title;
    const char* alarm_mode;
    const char* alarm_validate;

    // --- LED screen ---
    const char* led_title;
    const char* led_fixed;
    const char* led_rainbow;
    const char* led_wave;
    const char* led_pulse;
    const char* led_validate;

    // --- Equalizer screen ---
    const char* eq_title;
    const char* eq_bass;
    const char* eq_mid;
    const char* eq_treble;
    const char* eq_validate;

    // --- Themes screen ---
    const char* theme_title;
    const char* theme_validate;

    // --- Timezone screen ---
    const char* tz_title;
    const char* tz_validate;

    // --- Language screen ---
    const char* lang_title;
    const char* lang_validate;

    // --- Info screen ---
    const char* info_title;
    const char* info_signal;
    const char* info_sd;
    const char* info_temp;

    // --- LED extras (brightness + color preview) ---
    const char* led_brightness;
    const char* led_color_preview;

    // --- Generic error messages ---
    const char* err_no_wifi;
    const char* err_no_sd;
    const char* err_no_rtc;
    const char* err_stream;
};

// Global pointer — always use lang->field_name in UI code
extern const LangStrings* lang;

// Call at boot and when the user changes language
void apply_language(int lang_id);

// Display names for the language picker list
extern const char* lang_names[LANG_COUNT];

#endif
