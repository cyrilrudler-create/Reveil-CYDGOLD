#include "ui_home.h"
#include "ui_radio.h"
#include "ui_config.h"
#include "structures.h"
#include "fonctions.h"
#include "Audio.h"
#include <lvgl.h>
#include <WiFi.h>
#include "ui_lang.h"

// =====================================================
//  HOME SCREEN  (320 x 240)
//
//  Top-left : date, WiFi icon, battery icon
//  Center   : large 7-segment clock + alarm badge
//  Center   : now-playing label + audio visualizer
//  Bottom   : 4 navigation buttons (Radio/MP3/Conf/Stop)
// =====================================================

LV_FONT_DECLARE(font_7seg_90);

extern lv_obj_t*   time_label;
extern lv_obj_t*   radio_img;
extern lv_obj_t*   date_label;
extern lv_obj_t*   wifi_icon_label;
extern lv_obj_t*   batt_icon_label;
extern Audio       audio;
extern bool        is_playing;
extern String      current_title;
extern RadioTheme* currentTheme;
extern void        update_time(bool force);

// Exported — updated by the audio loop and update_time
lv_obj_t* now_playing_label  = NULL;
lv_obj_t* ui_visualizer_bars[5];

static lv_obj_t* ui_alarm_icon = NULL;

// =====================================================
//  NAVIGATION — reset shared UI pointers before
//  switching screens so dangling refs don't crash
// =====================================================

static void reset_home_pointers() {
    time_label        = NULL;
    radio_img         = NULL;
    date_label        = NULL;
    wifi_icon_label   = NULL;
    batt_icon_label   = NULL;
    now_playing_label = NULL;
    for (int i = 0; i < 5; i++) ui_visualizer_bars[i] = NULL;
}

static void switch_to_radio_async(void* p) {
    reset_home_pointers();
    lv_obj_clean(lv_scr_act());
    setup_radio_screen();
}

static void switch_to_mp3_async(void* p) {
    reset_home_pointers();
    lv_obj_clean(lv_scr_act());
    setup_mp3_screen();
}

static void switch_to_config_async(void* p) {
    reset_home_pointers();
    lv_obj_clean(lv_scr_act());
    setup_config_screen();
}

// =====================================================
//  BUTTON CALLBACKS
// =====================================================

static void btn_radio_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        Serial.println("[HOME] -> Radio");
        lv_async_call(switch_to_radio_async, NULL);
    }
}

static void btn_mp3_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        Serial.println("[HOME] -> MP3");
        lv_async_call(switch_to_mp3_async, NULL);
    }
}

static void btn_config_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        Serial.println("[HOME] -> Config");
        lv_async_call(switch_to_config_async, NULL);
    }
}

static void btn_stop_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    extern bool buzzer_actif;
    buzzer_actif  = false;
    audio.stopSong();
    is_playing    = false;
    current_title = "No playback";
    Serial.println("[HOME] Global stop");
    if (now_playing_label != NULL)
        lv_label_set_text(now_playing_label, current_title.c_str());
}

// =====================================================
//  HELPER — create a 70x70 navigation button
// =====================================================

static lv_obj_t* make_nav_btn(const char* icon, const char* label,
                               lv_color_t bg, lv_color_t border,
                               lv_event_cb_t cb)
{
    lv_obj_t* btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 70, 70);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, border, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* ico = lv_label_create(btn);
    lv_label_set_text(ico, icon);
    lv_obj_set_style_text_font(ico, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(ico, currentTheme->text_main, 0);
    lv_obj_align(ico, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, currentTheme->text_main, 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -2);

    return btn;
}

// =====================================================
//  SCREEN LAYOUT  (320 x 240)
//
//  ┌──────────────────────────────────────┐
//  │ Mon 12 mai          [Wifi] [Battery] │  <- top bar 48px
//  ├──────────────────────────────────────┤
//  │                                      │
//  │         18:47  [Bell 18:30]          │  <- 7-seg clock
//  │                                      │
//  │         Radio: FIP          [||||]   │  <- now playing + visualizer
//  │                                      │
//  │  [RADIO] [MP3]  [CONF]  [STOP]       │  <- nav buttons 70x70
//  └──────────────────────────────────────┘
// =====================================================
void setup_home_screen() {
    lv_obj_clean(lv_scr_act());
    reset_home_pointers();

    lv_obj_set_style_bg_color(lv_scr_act(), currentTheme->bg_color, 0);

    // --- 7-segment clock (center) ---
    time_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(time_label, &font_7seg_90, 0);
    lv_obj_set_style_text_color(time_label, currentTheme->primary, 0);
    lv_label_set_text(time_label, "00:00");
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -25);

    // --- Alarm badge (shown only when alarm is active) ---
    if (userConfig.alarm_on) {
        ui_alarm_icon = lv_label_create(lv_scr_act());
        lv_label_set_text(ui_alarm_icon, LV_SYMBOL_BELL);
        lv_obj_set_style_text_color(ui_alarm_icon, currentTheme->primary, 0);
        lv_obj_set_style_text_font(ui_alarm_icon, &lv_font_montserrat_32, 0);
        lv_obj_align_to(ui_alarm_icon, time_label, LV_ALIGN_OUT_RIGHT_MID, 10, -15);
        lv_obj_set_style_text_opa(ui_alarm_icon, LV_OPA_80, 0);

        lv_obj_t* alarm_time = lv_label_create(lv_scr_act());
        lv_label_set_text_fmt(alarm_time, "%02d:%02d", userConfig.alarm_h, userConfig.alarm_m);
        lv_obj_set_style_text_color(alarm_time, currentTheme->text_muted, 0);
        lv_obj_set_style_text_font(alarm_time, &lv_font_montserrat_10, 0);
        lv_obj_align_to(alarm_time, ui_alarm_icon, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    } else {
        ui_alarm_icon = NULL;
    }

    // --- Date label (top left) ---
    date_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(date_label, currentTheme->text_main, 0);
    lv_label_set_text(date_label, "...");
    lv_obj_align(date_label, LV_ALIGN_TOP_LEFT, 7, 7);

    // --- WiFi icon (top left, below date) ---
    wifi_icon_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(wifi_icon_label, &lv_font_montserrat_14, 0);
    lv_obj_align(wifi_icon_label, LV_ALIGN_TOP_LEFT, 7, 30);
    if (WiFi.status() == WL_CONNECTED) {
        lv_label_set_text(wifi_icon_label, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(wifi_icon_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    } else {
        lv_label_set_text(wifi_icon_label, LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_color(wifi_icon_label, lv_palette_main(LV_PALETTE_RED), 0);
    }

    // --- Battery icon ---
    batt_icon_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(batt_icon_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(batt_icon_label, currentTheme->text_main, 0);
    lv_obj_align(batt_icon_label, LV_ALIGN_TOP_LEFT, 35, 30);
    lv_label_set_text(batt_icon_label, LV_SYMBOL_BATTERY_FULL);

    // --- Navigation buttons (4 x 70x70, centered at bottom) ---
    // Positions : x offsets from center = -120, -40, +40, +120
    struct { const char* icon; const char* lbl; lv_event_cb_t cb; lv_color_t bg; lv_color_t border; int ox; } btns[] = {
        { LV_SYMBOL_AUDIO,    "RADIO", btn_radio_cb,  currentTheme->btn_core,  currentTheme->primary,              -120 },
        { LV_SYMBOL_LIST,     "MP3",   btn_mp3_cb,    currentTheme->btn_core,  currentTheme->primary,              -40  },
        { LV_SYMBOL_SETTINGS, "CONF",  btn_config_cb, currentTheme->btn_core,  currentTheme->primary,              +40  },
    };

    for (auto& b : btns) {
        lv_obj_t* btn = make_nav_btn(b.icon, b.lbl, b.bg, b.border, b.cb);
        lv_obj_align(btn, LV_ALIGN_CENTER, b.ox, 75);
    }

    // Stop button — custom colors (dark red bg, red border, white text)
    lv_obj_t* btn_stop = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_stop, 70, 70);
    lv_obj_align(btn_stop, LV_ALIGN_CENTER, 120, 75);
    lv_obj_set_style_bg_color(btn_stop, lv_color_hex(0x400000), 0);
    lv_obj_set_style_radius(btn_stop, 8, 0);
    lv_obj_set_style_border_width(btn_stop, 2, 0);
    lv_obj_set_style_border_color(btn_stop, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_add_event_cb(btn_stop, btn_stop_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* icn_stop = lv_label_create(btn_stop);
    lv_label_set_text(icn_stop, LV_SYMBOL_STOP);
    lv_obj_set_style_text_font(icn_stop, &lv_font_montserrat_24, 0);
    lv_obj_align(icn_stop, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t* txt_stop = lv_label_create(btn_stop);
    lv_label_set_text(txt_stop, "STOP");
    lv_obj_set_style_text_font(txt_stop, &lv_font_montserrat_14, 0);
    lv_obj_align(txt_stop, LV_ALIGN_BOTTOM_MID, 0, -2);

    // --- Now playing label ---
    now_playing_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(now_playing_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(now_playing_label, currentTheme->text_muted, 0);
    lv_label_set_text(now_playing_label,
        (is_playing && current_title != "") ? current_title.c_str() : "No playback");
    lv_obj_align(now_playing_label, LV_ALIGN_CENTER, 0, 20);

    // --- Audio visualizer (5 vertical bars, top-right area) ---
    lv_obj_t* visualizer_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(visualizer_cont, 70, 40);
    lv_obj_align(visualizer_cont, LV_ALIGN_CENTER, 115, -105);
    lv_obj_set_style_bg_opa(visualizer_cont, 0, 0);
    lv_obj_set_style_border_width(visualizer_cont, 0, 0);
    lv_obj_clear_flag(visualizer_cont, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 5; i++) {
        ui_visualizer_bars[i] = lv_obj_create(visualizer_cont);
        lv_obj_set_size(ui_visualizer_bars[i], 8, 5);
        lv_obj_set_pos(ui_visualizer_bars[i], i * 12, 35);
        lv_obj_set_style_bg_color(ui_visualizer_bars[i], currentTheme->primary, 0);
        lv_obj_set_style_radius(ui_visualizer_bars[i], 2, 0);
        lv_obj_set_style_border_width(ui_visualizer_bars[i], 0, 0);
        lv_obj_clear_flag(ui_visualizer_bars[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    update_time(true);
}
