#include "ui_config.h"
#include "ui_home.h"
#include "ui_wifi.h"
#include "ui_alarme.h"
#include "ui_leds.h"
#include "ui_equalizer.h"
#include "ui_themes.h"
#include "ui_pays.h"
#include "ui_traduction.h"
#include "ui_info.h"
#include "config.h"
#include <lvgl.h>
#include <WiFi.h>
#include "Audio.h"
#include <Arduino.h>
#include "structures.h"
#include "ui_lang.h"

extern Audio audio;
extern lv_obj_t * time_label;
extern lv_obj_t * date_label;
extern lv_obj_t * wifi_icon_label;
extern lv_obj_t * batt_icon_label;
extern lv_obj_t * ui_wifi_list;
extern RadioTheme* currentTheme;

// =====================================================
//  CALLBACKS NAVIGATION
// =====================================================
#define MAKE_NAV(name, fn) \
    static void name##_async(void*p){lv_obj_clean(lv_scr_act());fn();} \
    static void name##_cb(lv_event_t*e){if(lv_event_get_code(e)==LV_EVENT_CLICKED)lv_async_call(name##_async,NULL);}

MAKE_NAV(go_wifi,       setup_wifi_screen)
MAKE_NAV(go_alarme,     setup_alarme_screen)
MAKE_NAV(go_leds,       setup_leds_screen)
MAKE_NAV(go_equalizer,  setup_equalizer_screen)
MAKE_NAV(go_themes,     setup_themes_screen)
MAKE_NAV(go_pays,       setup_pays_screen)
MAKE_NAV(go_traduction, setup_traduction_screen)
MAKE_NAV(go_info,       setup_info_screen)

// ===== CALLBACK : RETOUR HOME =====
static void switch_to_home_async(void * p) {
    time_label = NULL; date_label = NULL;
    wifi_icon_label = NULL; batt_icon_label = NULL;
    lv_obj_clean(lv_scr_act());
    setup_home_screen();
}
static void back_to_home_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED)
        lv_async_call(switch_to_home_async, NULL);
}

// ===== CALLBACK : LUMINOSITE =====
static void slider_bright_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    int val = (int)lv_slider_get_value(slider);
    analogWrite(TFT_BL, map(val, 0, 100, 0, 255));
    if(code == LV_EVENT_RELEASED) {
        userConfig.brightness = val;
        saveConfig();
    }
}

// ===== CALLBACK : VOLUME =====
static void slider_sound_event_cb(lv_event_t * e) {
    lv_obj_t * slider2 = lv_event_get_target(e);
    lv_event_code_t code2 = lv_event_get_code(e);
    int virtual_val = lv_slider_get_value(slider2);
    int vol = virtual_val / 10;
    if(code2 == LV_EVENT_VALUE_CHANGED) {
        if(vol != userConfig.volsound) {
            audio.setVolume(vol);
            userConfig.volsound = vol;
        }
    }
    if(code2 == LV_EVENT_RELEASED) saveConfig();
}

// =====================================================
//  HELPER : cree un bouton icone + label texte
// =====================================================
static lv_obj_t* make_menu_btn(lv_obj_t* parent,
                                const char* symbol,
                                const char* label_txt,
                                lv_event_cb_t cb,
                                lv_color_t icon_color,
                                bool active = false)
{
    lv_obj_t* btn = lv_obj_create(parent);
    // Size set by caller via lv_obj_set_size()
    lv_obj_set_style_bg_color(btn, currentTheme->btn_core, 0);
    lv_obj_set_style_bg_color(btn, lv_color_mix(currentTheme->primary, currentTheme->btn_core, 40), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_border_width(btn, active ? 2 : 1, 0);
    lv_obj_set_style_border_color(btn, active ? currentTheme->primary : currentTheme->border, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    // Icon
    lv_obj_t* ico = lv_label_create(btn);
    lv_label_set_text(ico, symbol);
    lv_obj_set_style_text_font(ico, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ico, icon_color, 0);
    lv_obj_align(ico, LV_ALIGN_TOP_MID, 0, 8);

    // Label below icon
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label_txt);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl, active ? currentTheme->primary : currentTheme->text_muted, 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -6);

    return btn;
}

// =====================================================
//  HELPER : cree un slider avec icone et label
// =====================================================
static void make_slider(lv_obj_t* parent,
                         const char* icon, const char* lbl_txt,
                         int x, int y, int w,
                         int range_min, int range_max, int val,
                         lv_event_cb_t cb)
{
    // Row background — 300px wide, centered on 320px
    int row_w = 300;
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, row_w, 28);
    lv_obj_set_pos(row, (320 - row_w) / 2, y); // = 10px margin on each side
    lv_obj_set_style_bg_color(row, currentTheme->btn_core, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_40, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // Icon
    lv_obj_t* ico = lv_label_create(row);
    lv_label_set_text(ico, icon);
    lv_obj_set_style_text_color(ico, currentTheme->primary, 0);
    lv_obj_set_style_text_font(ico, &lv_font_montserrat_14, 0);
    lv_obj_align(ico, LV_ALIGN_LEFT_MID, 6, 0);

    // Label
    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, lbl_txt);
    lv_obj_set_style_text_color(lbl, currentTheme->text_muted, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 24, 0);

    // Slider — slightly shorter to avoid the right edge of the enclosure
    lv_obj_t* sl = lv_slider_create(row);
    lv_obj_set_size(sl, row_w - 115, 8);
    lv_obj_align(sl, LV_ALIGN_RIGHT_MID, -18, 0); // right margin + gap from label
    lv_slider_set_range(sl, range_min, range_max);
    lv_slider_set_value(sl, val, LV_ANIM_OFF);
    lv_obj_add_event_cb(sl, cb, LV_EVENT_ALL, NULL);

    lv_obj_set_style_bg_color(sl, currentTheme->border,   LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, currentTheme->primary,  LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, currentTheme->primary,  LV_PART_KNOB);
    lv_obj_set_style_radius(sl,   LV_RADIUS_CIRCLE,       LV_PART_KNOB);
    lv_obj_set_style_pad_all(sl,  5,                      LV_PART_KNOB);
}

// =====================================================
//  SETUP ECRAN CONFIG
//
//  ┌──────────────────────────────────────────┐
//  │ ⚙ CONFIGURATION                    [🏠] │  ← header 36px
//  ├──────────────────────────────────────────┤
//  │  [WiFi] [Alarme] [LEDs] [EQ]            │  ← ligne 1 boutons
//  │  [Theme][Fuseau] [Lang] [Info]           │  ← ligne 2 boutons
//  ├──────────────────────────────────────────┤
//  │  ☀ Luminosite  ────────────────          │  ← slider 1
//  │  🔊 Volume     ────────────────          │  ← slider 2
//  └──────────────────────────────────────────┘
// =====================================================

void setup_config_screen() {
    lv_obj_clean(lv_scr_act());
    time_label = NULL; date_label = NULL;
    wifi_icon_label = NULL; batt_icon_label = NULL;

    lv_obj_set_style_bg_color(lv_scr_act(), currentTheme->bg_color, 0);

    // --- Header (36px) ---
    lv_obj_t* header = lv_obj_create(lv_scr_act());
    lv_obj_set_size(header, 320, 36);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(header, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 4, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Accent separator below header
    lv_obj_t* sep = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sep, 320, 2);
    lv_obj_set_pos(sep, 0, 36);
    lv_obj_set_style_bg_color(sep, currentTheme->primary, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_60, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text_fmt(title, "%s  %s", LV_SYMBOL_SETTINGS, lang->config_title);
    lv_obj_set_style_text_color(title, currentTheme->primary, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

    // Home button (top right)
    lv_obj_t* btn_home = lv_btn_create(header);
    lv_obj_set_size(btn_home, 32, 26);
    lv_obj_align(btn_home, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_home, currentTheme->primary, 0);
    lv_obj_set_style_radius(btn_home, 6, 0);
    lv_obj_set_style_shadow_width(btn_home, 0, 0);
    lv_obj_add_event_cb(btn_home, back_to_home_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_home = lv_label_create(btn_home);
    lv_label_set_text(lbl_home, LV_SYMBOL_HOME);
    lv_obj_set_style_text_color(lbl_home, currentTheme->bg_color, 0);
    lv_obj_center(lbl_home);

    // --- Button grid ---
    // Ecran : 320x240, header : 36px → zone dispo : 204px
    // Sliders : 2 x 30px + separateur 8px + marge bas 6px = 74px
    // Zone boutons : 204 - 74 = 130px
    // 2 lignes de boutons : BH = (130 - GAP_V - marge_haut) / 2
    //
    // Bouton : 70 x 52px
    // Gap H : 8px  →  4*70 + 3*8 = 304px  →  marge = (320-304)/2 = 8px
    // Gap V : 8px
    // ROW1_Y = 44 (juste sous le header+sep)
    // ROW2_Y = 44 + 52 + 8 = 104
    // slider_y_start = 104 + 52 + 10 = 166
    // Reste : 240 - 166 = 74px pour 2 sliders → OK confortable

    const int BW       = 70;
    const int BH       = 52;
    const int BGAP_H   = 8;
    const int BGAP_V   = 8;
    const int BSTART_X = (320 - 4 * BW - 3 * BGAP_H) / 2; // = 8px marge
    const int ROW1_Y   = 44;
    const int ROW2_Y   = ROW1_Y + BH + BGAP_V;             // = 104

    // State colors (WiFi connected, alarm active)
    bool wifi_ok  = (WiFi.status() == WL_CONNECTED);
    bool alarm_ok = userConfig.alarm_on;

    // Button definitions
    struct BtnDef {
        const char* symbol;
        const char* label;
        lv_event_cb_t cb;
        lv_color_t color;
        bool active;
        int row, col;
    };

    BtnDef btns[] = {
        { LV_SYMBOL_WIFI,       lang->config_btn_wifi,    go_wifi_cb,
          wifi_ok ? lv_palette_main(LV_PALETTE_GREEN) : currentTheme->text_main,
          wifi_ok, 0, 0 },
        { LV_SYMBOL_BELL,       lang->config_btn_alarm,   go_alarme_cb,
          alarm_ok ? currentTheme->primary : currentTheme->text_main,
          alarm_ok, 0, 1 },
        { LV_SYMBOL_EYE_OPEN,   lang->config_btn_leds,    go_leds_cb,
          currentTheme->text_main, false, 0, 2 },
        { LV_SYMBOL_VOLUME_MAX, lang->config_btn_eq,      go_equalizer_cb,
          currentTheme->text_main, false, 0, 3 },
        { LV_SYMBOL_IMAGE,      lang->config_btn_themes,  go_themes_cb,
          currentTheme->text_main, false, 1, 0 },
        { LV_SYMBOL_LOOP,       lang->config_btn_tz,      go_pays_cb,
          currentTheme->text_main, false, 1, 1 },
        { LV_SYMBOL_KEYBOARD,   lang->config_btn_lang,    go_traduction_cb,
          currentTheme->text_main, false, 1, 2 },
        { LV_SYMBOL_LIST,       lang->config_btn_info,    go_info_cb,
          currentTheme->text_main, false, 1, 3 },
    };

    for (auto& b : btns) {
        int x = BSTART_X + b.col * (BW + BGAP_H);
        int y = (b.row == 0) ? ROW1_Y : ROW2_Y;
        lv_obj_t* btn = make_menu_btn(lv_scr_act(),
                                       b.symbol, b.label,
                                       b.cb, b.color, b.active);
        lv_obj_set_size(btn, BW, BH);
        lv_obj_set_pos(btn, x, y);
    }

    // --- Separator before sliders ---
    int slider_y_start = ROW2_Y + BH + 10;   // = 166px

    lv_obj_t* sep2 = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sep2, 300, 1);
    lv_obj_set_pos(sep2, 10, slider_y_start - 5);
    lv_obj_set_style_bg_color(sep2, currentTheme->border, 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_set_style_radius(sep2, 0, 0);

    // --- Sliders ---
    // Available zone: 166..240px = 74px
    // Slider 1 at y=166, slider 2 at y=198 — 12px bottom margin
    make_slider(lv_scr_act(),
                LV_SYMBOL_IMAGE, lang->config_brightness,
                10, slider_y_start, 220,
                10, 100, userConfig.brightness,
                slider_bright_event_cb);

    make_slider(lv_scr_act(),
                LV_SYMBOL_VOLUME_MAX, lang->config_volume,
                10, slider_y_start + 34, 220,
                70, 210, userConfig.volsound * 10,
                slider_sound_event_cb);
}



