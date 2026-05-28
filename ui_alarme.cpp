#include "ui_config.h"
#include "ui_home.h"
#include "ui_alarme.h"
#include "config.h"
#include <lvgl.h>
#include "Audio.h"
#include <Arduino.h>
#include "structures.h"
#include "ui_lang.h"

// =====================================================
//  ALARM SETTINGS SCREEN  (320 x 240)
//
//  Left column  : hour/minute rollers + ON/OFF switch
//  Right column : mode selector (Radio/MP3/Buzzer)
//                 + station list (visible in Radio mode)
//  Bottom       : Confirm button
//
//  check_alarme() is called every 4s from update_time().
//  Fallback chain: Radio → no WiFi → Buzzer
//                  MP3   → no file → Buzzer
// =====================================================

extern Audio       audio;
extern bool        is_mp3_mode;
extern bool        is_playing;
extern RadioTheme* currentTheme;
extern int         currentStation;

// --- Internal UI references ---
static lv_obj_t* roller_h     = NULL;
static lv_obj_t* roller_m     = NULL;
static lv_obj_t* cb_radio     = NULL;
static lv_obj_t* cb_mp3       = NULL;
static lv_obj_t* cb_buzzer    = NULL;
static lv_obj_t* sw_alarm     = NULL;
static lv_obj_t* lbl_sw_state = NULL;
static lv_obj_t* station_list = NULL; // visible in Radio mode only

// --- State ---
static bool alarm_triggered = false;
bool        buzzer_actif    = false;  // exported — read by the buzzer watchdog

// =====================================================
//  CALLBACKS
// =====================================================

static void back_async(void* p) {
    saveConfig();
    lv_obj_clean(lv_scr_act());
    setup_config_screen();
}

static void btn_back_cb(lv_event_t* e) {
    lv_async_call(back_async, NULL);
}

// Show/hide and highlight the station list depending on alarm mode
static void refresh_station_list_visibility() {
    if (station_list == NULL) return;
    if (userConfig.alarm_mode == 0) {
        lv_obj_clear_flag(station_list, LV_OBJ_FLAG_HIDDEN);
        uint32_t count = lv_obj_get_child_cnt(station_list);
        for (uint32_t i = 0; i < count; i++) {
            lv_obj_t* btn = lv_obj_get_child(station_list, i);
            bool sel = ((int)i == userConfig.alarm_station);
            lv_obj_set_style_bg_color(btn,
                sel ? currentTheme->primary  : currentTheme->btn_core,  0);
            lv_obj_set_style_text_color(btn,
                sel ? currentTheme->bg_color : currentTheme->text_main, 0);
        }
    } else {
        lv_obj_add_flag(station_list, LV_OBJ_FLAG_HIDDEN);
    }
}

// Mode checkbox tapped — Radio / MP3 / Buzzer (mutually exclusive)
static void cb_mode_event_cb(lv_event_t* e) {
    lv_obj_t* target = lv_event_get_target(e);
    lv_obj_clear_state(cb_radio,  LV_STATE_CHECKED);
    lv_obj_clear_state(cb_mp3,    LV_STATE_CHECKED);
    lv_obj_clear_state(cb_buzzer, LV_STATE_CHECKED);
    lv_obj_add_state(target, LV_STATE_CHECKED);

    if (target == cb_radio)  userConfig.alarm_mode = 0;
    if (target == cb_mp3)    userConfig.alarm_mode = 1;
    if (target == cb_buzzer) userConfig.alarm_mode = 2;

    refresh_station_list_visibility();
    Serial.printf("[ALARM] Mode: %d\n", userConfig.alarm_mode);
}

// Hour/minute roller or ON/OFF switch changed
static void alarm_change_cb(lv_event_t* e) {
    userConfig.alarm_h  = lv_roller_get_selected(roller_h);
    userConfig.alarm_m  = lv_roller_get_selected(roller_m) * 5;
    bool is_on          = lv_obj_has_state(sw_alarm, LV_STATE_CHECKED);
    userConfig.alarm_on = is_on;

    lv_label_set_text(lbl_sw_state, is_on ? LV_SYMBOL_BELL " ON" : LV_SYMBOL_BELL " OFF");
    lv_obj_set_style_text_color(lbl_sw_state,
        is_on ? lv_palette_main(LV_PALETTE_GREEN) : currentTheme->text_muted, 0);
}

// Station row tapped in the alarm station list
static void station_alarm_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    userConfig.alarm_station = idx;
    Serial.printf("[ALARM] Station: %d - %s\n", idx, STATIONS[idx].name.c_str());
    refresh_station_list_visibility();
}

// Apply theme colors to a roller widget
static void style_roller(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, currentTheme->btn_core,  0);
    lv_obj_set_style_text_color(obj, currentTheme->text_main, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, currentTheme->primary, 0);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, 0);
    lv_obj_set_style_radius(obj, 6, 0);
    lv_obj_set_style_bg_color(obj, currentTheme->primary,  LV_PART_SELECTED);
    lv_obj_set_style_text_color(obj, currentTheme->bg_color, LV_PART_SELECTED);
}

// =====================================================
//  CHECK ALARM — called every 4s from update_time()
//
//  Fallback chain:
//    Radio mode : no WiFi  → Buzzer
//    MP3 mode   : no file  → Buzzer
//  alarm_triggered is reset when the minute changes
//  so the alarm only fires once per minute.
// =====================================================
void check_alarme(int rtc_h, int rtc_m) {
    char path_reveil[128] = "";

    // Reset trigger flag when the minute changes
    if (rtc_m != userConfig.alarm_m) alarm_triggered = false;

    if (!userConfig.alarm_on || alarm_triggered) return;

    if (rtc_h != userConfig.alarm_h || rtc_m != userConfig.alarm_m) return;

    Serial.println("[ALARM] Triggered!");
    int mode_final = userConfig.alarm_mode;

    // Radio fallback : no WiFi → Buzzer
    if (mode_final == 0 && WiFi.status() != WL_CONNECTED) {
        Serial.println("[ALARM] No WiFi -> Buzzer");
        mode_final = 2;
    }

    // MP3 fallback : scan /mp3 for the first file
    if (mode_final == 1) {
        File root = SD_MMC.open("/mp3");
        if (!root || !root.isDirectory()) {
            Serial.println("[ALARM] /mp3 not found -> Buzzer");
            mode_final = 2;
        } else {
            File file = root.openNextFile();
            bool found = false;
            while (file) {
                if (!file.isDirectory()) {
                    snprintf(path_reveil, sizeof(path_reveil), "/mp3/%s", file.name());
                    found = true;
                    file.close();
                    break;
                }
                file.close();
                file = root.openNextFile();
            }
            root.close();
            if (!found) {
                Serial.println("[ALARM] /mp3 empty -> Buzzer");
                mode_final = 2;
            }
        }
    }

    // Execute alarm
    if (mode_final == 0) {
        int st = constrain(userConfig.alarm_station, 0, (int)STATIONS.size() - 1);
        Serial.printf("[ALARM] Radio -> %s\n", STATIONS[st].name.c_str());
        is_mp3_mode = false;
        audio.connecttohost(STATIONS[st].url.c_str());
        is_playing  = true;
    } else if (mode_final == 1) {
        Serial.printf("[ALARM] MP3 -> %s\n", path_reveil);
        is_mp3_mode = true;
        audio.connecttoFS(SD_MMC, path_reveil);
        is_playing  = true;
    } else {
        Serial.println("[ALARM] Buzzer");
        buzzer_actif = true;
        audio.connecttoFS(SD_MMC, "/buzzer.mp3");
    }

    alarm_triggered = true;
}

// =====================================================
//  SCREEN LAYOUT  (320 x 240)
//
//  ┌─────────────────────────────────────────┐
//  │  Bell  ALARM SETTINGS                   │  <- top bar 34px
//  ├──────────────────┬──────────────────────┤
//  │  H        M      │  Mode:               │
//  │ [00]  :  [00]    │  (o) Radio           │
//  │ [01]     [05]    │  ( ) MP3             │  <- rollers + checkboxes
//  │ [02]     [10]    │  ( ) Buzzer          │
//  │                  │  ┌────────────────┐  │
//  │ [ON/OFF] Bell ON │  │ station list   │  │  <- station list (Radio mode)
//  ├──────────────────┴──────────────────────┤
//  │  [OK Confirm]                           │  <- bottom 34px
//  └─────────────────────────────────────────┘
// =====================================================
void setup_alarme_screen() {
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), currentTheme->bg_color, 0);

    // --- Top bar (34px) ---
    lv_obj_t* top_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(top_bar, 320, 34);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(top_bar);
    lv_label_set_text_fmt(title, "%s %s", LV_SYMBOL_BELL, lang->alarm_title);
    lv_obj_set_style_text_color(title, currentTheme->primary, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_center(title);

    lv_obj_t* sep = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sep, 320, 1);
    lv_obj_set_pos(sep, 0, 34);
    lv_obj_set_style_bg_color(sep, currentTheme->border, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);

    // --- Left column : H/M rollers + ON/OFF switch (x=0..133) ---
    lv_obj_t* lbl_h = lv_label_create(lv_scr_act());
    lv_label_set_text(lbl_h, "H");
    lv_obj_set_style_text_color(lbl_h, currentTheme->text_muted, 0);
    lv_obj_set_style_text_font(lbl_h, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_h, 18, 38);

    roller_h = lv_roller_create(lv_scr_act());
    lv_roller_set_options(roller_h,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12"
        "\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23",
        LV_ROLLER_MODE_INFINITE);
    lv_obj_set_size(roller_h, 52, 110);
    lv_obj_set_pos(roller_h, 6, 50);
    style_roller(roller_h);
    lv_roller_set_selected(roller_h, userConfig.alarm_h, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller_h, alarm_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Colon separator
    lv_obj_t* lbl_sep = lv_label_create(lv_scr_act());
    lv_label_set_text(lbl_sep, ":");
    lv_obj_set_style_text_color(lbl_sep, currentTheme->primary, 0);
    lv_obj_set_style_text_font(lbl_sep, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(lbl_sep, 61, 88);

    lv_obj_t* lbl_m = lv_label_create(lv_scr_act());
    lv_label_set_text(lbl_m, "M");
    lv_obj_set_style_text_color(lbl_m, currentTheme->text_muted, 0);
    lv_obj_set_style_text_font(lbl_m, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_m, 76, 38);

    // Minute roller (steps of 5)
    roller_m = lv_roller_create(lv_scr_act());
    char opts[128] = "";
    for (int i = 0; i <= 55; i += 5) {
        char buf[8];
        sprintf(buf, i < 55 ? "%02d\n" : "%02d", i);
        strcat(opts, buf);
    }
    lv_roller_set_options(roller_m, opts, LV_ROLLER_MODE_INFINITE);
    lv_obj_set_size(roller_m, 52, 110);
    lv_obj_set_pos(roller_m, 70, 50);
    style_roller(roller_m);
    lv_roller_set_selected(roller_m, userConfig.alarm_m / 5, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller_m, alarm_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ON/OFF switch
    sw_alarm = lv_switch_create(lv_scr_act());
    lv_obj_set_size(sw_alarm, 46, 22);
    lv_obj_set_pos(sw_alarm, 6, 170);
    lv_obj_set_style_bg_color(sw_alarm, currentTheme->btn_core,   LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw_alarm, currentTheme->text_muted, LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw_alarm, currentTheme->btn_core,   LV_STATE_CHECKED | LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sw_alarm, currentTheme->primary,    LV_STATE_CHECKED | LV_PART_KNOB);
    if (userConfig.alarm_on) lv_obj_add_state(sw_alarm, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_alarm, alarm_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Switch state label
    lbl_sw_state = lv_label_create(lv_scr_act());
    lv_obj_set_pos(lbl_sw_state, 58, 172);
    lv_obj_set_style_text_font(lbl_sw_state, &lv_font_montserrat_12, 0);
    lv_label_set_text(lbl_sw_state, userConfig.alarm_on ? LV_SYMBOL_BELL " ON" : LV_SYMBOL_BELL " OFF");
    lv_obj_set_style_text_color(lbl_sw_state,
        userConfig.alarm_on ? lv_palette_main(LV_PALETTE_GREEN) : currentTheme->text_muted, 0);

    // Vertical separator between left and right columns
    lv_obj_t* vsep = lv_obj_create(lv_scr_act());
    lv_obj_set_size(vsep, 1, 200);
    lv_obj_set_pos(vsep, 133, 36);
    lv_obj_set_style_bg_color(vsep, currentTheme->border, 0);
    lv_obj_set_style_border_width(vsep, 0, 0);
    lv_obj_set_style_radius(vsep, 0, 0);

    // --- Right column : mode selector + station list (x=138..320) ---
    lv_obj_t* lbl_mode = lv_label_create(lv_scr_act());
    lv_label_set_text_fmt(lbl_mode, "%s :", lang->alarm_mode);
    lv_obj_set_style_text_color(lbl_mode, currentTheme->text_muted, 0);
    lv_obj_set_style_text_font(lbl_mode, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_mode, 142, 40);

    // Mode checkboxes (mutually exclusive — managed by cb_mode_event_cb)
    auto make_cb = [&](const char* txt, int y) -> lv_obj_t* {
        lv_obj_t* cb = lv_checkbox_create(lv_scr_act());
        lv_checkbox_set_text(cb, txt);
        lv_obj_set_pos(cb, 142, y);
        lv_obj_set_style_text_color(cb, currentTheme->text_main, 0);
        lv_obj_set_style_text_font(cb, &lv_font_montserrat_12, 0);
        lv_obj_set_style_bg_color(cb, currentTheme->primary, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_event_cb(cb, cb_mode_event_cb, LV_EVENT_CLICKED, NULL);
        return cb;
    };

    cb_radio  = make_cb("Radio",  58);
    cb_mp3    = make_cb("MP3",    82);
    cb_buzzer = make_cb("Buzzer", 106);

    // Set initial checkbox state
    if      (userConfig.alarm_mode == 0) lv_obj_add_state(cb_radio,  LV_STATE_CHECKED);
    else if (userConfig.alarm_mode == 1) lv_obj_add_state(cb_mp3,    LV_STATE_CHECKED);
    else                                  lv_obj_add_state(cb_buzzer, LV_STATE_CHECKED);

    // Station list (Radio mode only)
    station_list = lv_list_create(lv_scr_act());
    lv_obj_set_size(station_list, 172, 92);
    lv_obj_set_pos(station_list, 142, 130);
    lv_obj_set_style_radius(station_list, 6, 0);
    lv_obj_set_style_border_width(station_list, 1, 0);
    lv_obj_set_style_border_color(station_list, currentTheme->border, 0);
    lv_obj_set_style_bg_color(station_list, currentTheme->btn_core, 0);
    lv_obj_set_style_text_font(station_list, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_row(station_list, 1, 0);

    for (int i = 0; i < (int)STATIONS.size(); i++) {
        lv_obj_t* btn = lv_list_add_btn(
            station_list, LV_SYMBOL_AUDIO, STATIONS[i].name.c_str());
        lv_obj_set_style_bg_color(btn, currentTheme->btn_core, 0);
        lv_obj_set_style_text_color(btn, currentTheme->text_main, 0);
        lv_obj_set_style_min_height(btn, 24, 0);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_12, 0);
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, station_alarm_cb, LV_EVENT_CLICKED, NULL);
    }
    refresh_station_list_visibility();

    // --- Confirm button (bottom left, y=200) ---
    lv_obj_t* btn_back = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_back, 110, 34);
    lv_obj_set_pos(btn_back, 6, 200);
    lv_obj_set_style_bg_color(btn_back, currentTheme->primary, 0);
    lv_obj_set_style_bg_color(btn_back, currentTheme->primary, LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_back, 6, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text_fmt(lbl_back, "%s %s", LV_SYMBOL_OK, lang->alarm_validate);
    lv_obj_set_style_text_color(lbl_back, currentTheme->bg_color, 0);
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_back);
}
