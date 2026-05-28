#include "ui_radio.h"
#include "config.h"
#include "Audio.h"
#include <lvgl.h>
#include "structures.h"
#include "fonctions.h"
#include "ui_lang.h"

// =====================================================
//  WEB RADIO SCREEN  (320 x 240 landscape)
//
//  Left column  : scrollable station list (192px wide)
//  Right column : station logo + station name label
//  Top bar      : title, clock
//  Stream bar   : scrolling ICY metadata title
// =====================================================

extern Audio       audio;
extern lv_obj_t*   radio_img;
extern lv_obj_t*   time_label;
extern int         currentStation;
extern String      current_title;
extern bool        is_playing;
extern bool        is_mp3_mode;
extern RadioTheme* currentTheme;

// --- Internal UI objects ---
static lv_obj_t* ui_lbl_stream_title = NULL; // scrolling ICY title
static lv_obj_t* ui_station_list     = NULL; // station list widget
static lv_obj_t* ui_lbl_station_name = NULL; // station name below logo

// =====================================================
//  HELPERS
// =====================================================

// Highlight the currently playing station in the list
static void highlight_active_station() {
    if (ui_station_list == NULL) return;
    uint32_t count = lv_obj_get_child_cnt(ui_station_list);
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t* btn = lv_obj_get_child(ui_station_list, i);
        if ((int)i == currentStation) {
            lv_obj_set_style_bg_color(btn, currentTheme->primary,  0);
            lv_obj_set_style_bg_opa(btn,   LV_OPA_COVER,           0);
            lv_obj_set_style_text_color(btn, currentTheme->bg_color, 0);
        } else {
            lv_obj_set_style_bg_color(btn, currentTheme->btn_core, 0);
            lv_obj_set_style_bg_opa(btn,   LV_OPA_COVER,           0);
            lv_obj_set_style_text_color(btn, currentTheme->text_main, 0);
        }
    }
}

// Resolve logo path — falls back to def.bin if file not found on SD
static const char* resolve_logo(int station_id) {
    if (station_id < 0 || station_id >= (int)STATIONS.size())
        return "S:/logos/def.bin";

    String logo    = STATIONS[station_id].logo_path;
    String sd_path = logo;
    if (sd_path.startsWith("S:")) sd_path.remove(0, 2);

    if (SD_MMC.exists(sd_path.c_str())) return STATIONS[station_id].logo_path.c_str();

    Serial.printf("[RADIO] Logo not found (%s), using def.bin\n", sd_path.c_str());
    return "S:/logos/def.bin";
}

// =====================================================
//  PUBLIC — called from audio callbacks
// =====================================================

// Called from audio_showstreamtitle — updates the scrolling ICY title
void update_radio_stream_title(const char* title) {
    if (ui_lbl_stream_title != NULL && lv_obj_is_valid(ui_lbl_stream_title))
        lv_label_set_text(ui_lbl_stream_title, title);
}

// Called from update_time — keeps the clock in sync
void update_radio_time(const char* time_str) {
    if (time_label != NULL && lv_obj_is_valid(time_label))
        lv_label_set_text(time_label, time_str);
}

// =====================================================
//  CALLBACKS
// =====================================================

// Tap a station row — stop current, connect to new station
static void station_click_event_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    if (!btn) return;

    intptr_t id    = (intptr_t)lv_obj_get_user_data(btn);
    currentStation = (int)id;
    current_title  = "Radio: " + STATIONS[id].name;

    audio.stopSong();

    if (radio_img != NULL && lv_obj_is_valid(radio_img))
        lv_img_set_src(radio_img, resolve_logo(id));

    if (ui_lbl_stream_title != NULL && lv_obj_is_valid(ui_lbl_stream_title))
        lv_label_set_text(ui_lbl_stream_title, STATIONS[id].name.c_str());

    audio.connecttohost(STATIONS[id].url.c_str());
    is_playing  = true;
    is_mp3_mode = false;

    if (ui_lbl_station_name != NULL && lv_obj_is_valid(ui_lbl_station_name))
        lv_label_set_text(ui_lbl_station_name, STATIONS[id].name.c_str());

    highlight_active_station();
}

// Return to home screen — reset all shared UI pointers first
static void switch_to_home_async(void* p) {
    time_label          = NULL;
    radio_img           = NULL;
    ui_lbl_stream_title = NULL;
    ui_station_list     = NULL;
    ui_lbl_station_name = NULL;
    lv_obj_clean(lv_scr_act());
    setup_home_screen();
}

static void back_to_home_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        lv_async_call(switch_to_home_async, NULL);
}

// =====================================================
//  SCREEN LAYOUT  (320 x 240)
//
//  ┌────────────────────────────────────┐
//  │ WiFi  RADIO WEB          --:--     │  <- top bar 36px
//  ├────────────────────────────────────┤
//  │ scrolling ICY stream title...      │  <- stream bar 22px
//  ├─────────────────┬──────────────────┤
//  │                 │   [logo 100x100] │
//  │  station list   │   station name   │  <- content 178px
//  │  (192px wide)   │                  │
//  │                 │          [Home]  │
//  └─────────────────┴──────────────────┘
// =====================================================
void setup_radio_screen() {
    lv_obj_clean(lv_scr_act());
    radio_img           = NULL;
    time_label          = NULL;
    ui_lbl_stream_title = NULL;
    ui_station_list     = NULL;

    lv_obj_set_style_bg_color(lv_scr_act(), currentTheme->bg_color, 0);

    // --- Top bar (36px) ---
    lv_obj_t* top_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(top_bar, 320, 36);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    // 1px separator below top bar
    lv_obj_t* sep_top = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sep_top, 320, 1);
    lv_obj_set_pos(sep_top, 0, 36);
    lv_obj_set_style_bg_color(sep_top, currentTheme->border, 0);
    lv_obj_set_style_border_width(sep_top, 0, 0);
    lv_obj_set_style_radius(sep_top, 0, 0);

    // WiFi icon
    lv_obj_t* ico = lv_label_create(top_bar);
    lv_label_set_text(ico, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(ico, currentTheme->primary, 0);
    lv_obj_set_style_text_font(ico, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(ico, 8, 10);

    // Screen title
    lv_obj_t* lbl_radio = lv_label_create(top_bar);
    lv_label_set_text(lbl_radio, lang->radio_title);
    lv_obj_set_style_text_color(lbl_radio, currentTheme->primary, 0);
    lv_obj_set_style_text_font(lbl_radio, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_radio, 26, 10);

    // Clock (updated externally via update_radio_time)
    time_label = lv_label_create(top_bar);
    lv_label_set_text(time_label, "--:--");
    lv_obj_set_style_text_color(time_label, currentTheme->primary, 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(time_label, 270, 10);

    // --- Stream title bar (22px, scrolling) ---
    lv_obj_t* stream_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(stream_bar, 320, 22);
    lv_obj_set_pos(stream_bar, 0, 37);
    lv_obj_set_style_bg_color(stream_bar, currentTheme->bg_color, 0);
    lv_obj_set_style_border_width(stream_bar, 0, 0);
    lv_obj_set_style_radius(stream_bar, 0, 0);
    lv_obj_set_style_pad_all(stream_bar, 0, 0);
    lv_obj_clear_flag(stream_bar, LV_OBJ_FLAG_SCROLLABLE);

    ui_lbl_stream_title = lv_label_create(stream_bar);
    lv_label_set_text(ui_lbl_stream_title,
        (!STATIONS.empty()) ? STATIONS[currentStation].name.c_str() : "...");
    lv_obj_set_style_text_color(ui_lbl_stream_title, currentTheme->text_muted, 0);
    lv_obj_set_style_text_font(ui_lbl_stream_title, &lv_font_montserrat_12, 0);
    lv_obj_set_width(ui_lbl_stream_title, 310);
    lv_label_set_long_mode(ui_lbl_stream_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_pos(ui_lbl_stream_title, 5, 4);

    // 1px separator below stream bar
    lv_obj_t* sep_stream = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sep_stream, 320, 1);
    lv_obj_set_pos(sep_stream, 0, 59);
    lv_obj_set_style_bg_color(sep_stream, currentTheme->border, 0);
    lv_obj_set_style_border_width(sep_stream, 0, 0);
    lv_obj_set_style_radius(sep_stream, 0, 0);

    // --- Station list (left column, 192 x 178px, y=61) ---
    ui_station_list = lv_list_create(lv_scr_act());
    lv_obj_set_size(ui_station_list, 192, 178);
    lv_obj_set_pos(ui_station_list, 4, 61);
    lv_obj_set_style_radius(ui_station_list, 8, 0);
    lv_obj_set_style_border_width(ui_station_list, 1, 0);
    lv_obj_set_style_border_color(ui_station_list, currentTheme->border, 0);
    lv_obj_set_style_bg_color(ui_station_list, currentTheme->btn_core, 0);
    lv_obj_set_style_text_color(ui_station_list, currentTheme->text_main, 0);
    lv_obj_set_style_text_font(ui_station_list, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_row(ui_station_list, 2, 0);

    for (int i = 0; i < (int)STATIONS.size(); i++) {
        lv_obj_t* btn = lv_list_add_btn(
            ui_station_list, LV_SYMBOL_AUDIO, STATIONS[i].name.c_str());
        lv_obj_set_style_bg_color(btn, currentTheme->btn_core, 0);
        lv_obj_set_style_text_color(btn, currentTheme->text_main, 0);
        lv_obj_set_style_bg_color(btn, currentTheme->primary, LV_STATE_PRESSED);
        lv_obj_set_style_min_height(btn, 26, 0);
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, station_click_event_cb, LV_EVENT_CLICKED, NULL);
    }

    // --- Right column : logo (100x100) + station name + Home button ---
    radio_img = lv_img_create(lv_scr_act());
    lv_obj_set_size(radio_img, 100, 100);
    lv_obj_set_pos(radio_img, 206, 65);
    lv_obj_set_style_bg_opa(radio_img, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(radio_img, currentTheme->btn_core, 0);
    lv_obj_set_style_radius(radio_img, 8, 0);
    lv_obj_set_style_clip_corner(radio_img, true, 0);
    lv_img_set_src(radio_img, resolve_logo(currentStation));

    ui_lbl_station_name = lv_label_create(lv_scr_act());
    lv_label_set_text(ui_lbl_station_name,
        (!STATIONS.empty()) ? STATIONS[currentStation].name.c_str() : "");
    lv_obj_set_style_text_color(ui_lbl_station_name, currentTheme->text_main, 0);
    lv_obj_set_style_text_font(ui_lbl_station_name, &lv_font_montserrat_12, 0);
    lv_obj_set_width(ui_lbl_station_name, 112);
    lv_label_set_long_mode(ui_lbl_station_name, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(ui_lbl_station_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(ui_lbl_station_name, 200, 177);

    // Home button (bottom right)
    lv_obj_t* btn_back = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_back, 40, 32);
    lv_obj_set_pos(btn_back, 274, 204);
    lv_obj_set_style_bg_color(btn_back, currentTheme->primary, 0);
    lv_obj_set_style_bg_color(btn_back, currentTheme->primary, LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_back, 6, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_add_event_cb(btn_back, back_to_home_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_HOME);
    lv_obj_set_style_text_color(lbl_back, currentTheme->text_main, 0);
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_back);

    audio.setVolume(userConfig.volsound);
    highlight_active_station();
}
