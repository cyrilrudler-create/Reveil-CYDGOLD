#include "ui_leds.h"
#include "ui_config.h"
#include <lvgl.h>
#include "structures.h"
#include "led_control.h"
#include <FastLED.h>
#include "ui_lang.h"

// =====================================================
//  LED SETTINGS SCREEN  (320 x 240)
//
//  Left  : color wheel (hue only, 140x140)
//  Right : 2x2 mode buttons (Fixed/Rainbow/Wave/Pulse)
//          + color preview rectangle
//  Bottom: brightness slider (34px bar)
// =====================================================

extern RadioTheme* currentTheme;

// --- Internal UI references ---
static lv_obj_t*  cw            = NULL; // color wheel widget
static lv_obj_t*  color_preview = NULL; // live color preview rectangle
static lv_obj_t*  slider_bright = NULL;
static lv_obj_t*  mode_btns[4]  = {NULL, NULL, NULL, NULL};
static LedMode    current_mode  = MODE_FIXE;

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

// Color wheel changed — update config and LEDs in real time
static void color_wheel_cb(lv_event_t* e) {
    lv_color_t c = lv_colorwheel_get_rgb(lv_event_get_target(e));

    // LVGL RGB565 to 8-bit per channel
    userConfig.led_r = c.ch.red   << 3;
    userConfig.led_g = c.ch.green << 2;
    userConfig.led_b = c.ch.blue  << 3;

    if (color_preview != NULL && lv_obj_is_valid(color_preview))
        lv_obj_set_style_bg_color(color_preview,
            lv_color_make(userConfig.led_r, userConfig.led_g, userConfig.led_b), 0);

    update_leds_color(userConfig.led_r, userConfig.led_g, userConfig.led_b);
}

// Brightness slider changed — apply immediately to FastLED
static void slider_bright_cb(lv_event_t* e) {
    userConfig.led_bright = lv_slider_get_value(lv_event_get_target(e));
    FastLED.setBrightness(userConfig.led_bright);
    FastLED.show();
}

// Highlight the active mode button, reset others
static void update_mode_buttons(LedMode active) {
    for (int i = 0; i < 4; i++) {
        if (mode_btns[i] == NULL || !lv_obj_is_valid(mode_btns[i])) continue;
        if ((LedMode)i == active) {
            lv_obj_set_style_bg_color(mode_btns[i], currentTheme->primary, 0);
            lv_obj_set_style_border_width(mode_btns[i], 2, 0);
            lv_obj_set_style_border_color(mode_btns[i], currentTheme->text_main, 0);
        } else {
            lv_obj_set_style_bg_color(mode_btns[i], currentTheme->btn_core, 0);
            lv_obj_set_style_border_width(mode_btns[i], 1, 0);
            lv_obj_set_style_border_color(mode_btns[i], currentTheme->border, 0);
        }
    }
}

// Mode button tapped — switch LED animation mode
static void btn_mode_cb(lv_event_t* e) {
    LedMode mode = (LedMode)(uintptr_t)lv_event_get_user_data(e);
    current_mode = mode;
    set_led_mode(mode);
    update_mode_buttons(mode);

    // In fixed mode, re-push the current wheel color to the LEDs
    if (mode == MODE_FIXE && cw != NULL && lv_obj_is_valid(cw)) {
        lv_color_t c   = lv_colorwheel_get_rgb(cw);
        uint32_t rgb24 = lv_color_to32(c);
        update_leds_color((rgb24 >> 16) & 0xFF, (rgb24 >> 8) & 0xFF, rgb24 & 0xFF);
    }
}

// =====================================================
//  SCREEN LAYOUT  (320 x 240)
//
//  ┌─────────────────────────────────────────┐
//  │  LED SETTINGS                 [Confirm] │  <- header 38px
//  ├───────────────────┬─────────────────────┤
//  │                   │  [Fixed] [Rainbow]  │
//  │  Color Wheel      │  [Wave]  [Pulse]    │  <- mode grid 2x2
//  │  (hue, 140x140)   │                     │
//  │                   │  ████ Color preview │
//  ├───────────────────┴─────────────────────┤
//  │  ☀  Brightness  ────────────────────    │  <- slider bar 34px
//  └─────────────────────────────────────────┘
// =====================================================
void setup_leds_screen() {
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), currentTheme->bg_color, 0);

    // --- Header (38px) ---
    lv_obj_t* header = lv_obj_create(lv_scr_act());
    lv_obj_set_size(header, 320, 38);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(header, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 4, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text_fmt(title, "%s  %s", LV_SYMBOL_EYE_OPEN, lang->led_title);
    lv_obj_set_style_text_color(title, currentTheme->primary, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t* btn_back = lv_btn_create(header);
    lv_obj_set_size(btn_back, 75, 28);
    lv_obj_align(btn_back, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_back, currentTheme->primary, 0);
    lv_obj_set_style_radius(btn_back, 6, 0);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, lang->led_validate);
    lv_obj_set_style_text_color(lbl_back, currentTheme->bg_color, 0);
    lv_obj_center(lbl_back);

    // --- Color wheel (left, 140x140, hue only) ---
    // Available height : 240 - 38 (header) - 34 (slider) = 168px
    cw = lv_colorwheel_create(lv_scr_act(), true);
    lv_obj_set_size(cw, 140, 140);
    lv_obj_align(cw, LV_ALIGN_LEFT_MID, 10, 5);
    lv_obj_set_style_arc_width(cw, 14, LV_PART_MAIN);
    lv_colorwheel_set_mode(cw, LV_COLORWHEEL_MODE_HUE);
    lv_colorwheel_set_mode_fixed(cw, true);

    lv_color_t saved = lv_color_make(userConfig.led_r, userConfig.led_g, userConfig.led_b);
    lv_colorwheel_set_rgb(cw, saved);
    lv_obj_add_event_cb(cw, color_wheel_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // --- Mode buttons (right column, 2x2 grid) ---
    // Each button : 72x34px  |  positions: col0=165, col1=243, row0=44, row1=84
    const char* mode_names[] = {
        lang->led_fixed, lang->led_rainbow, lang->led_wave, lang->led_pulse
    };
    const int bx[4] = {165, 243, 165, 243};
    const int by[4] = { 44,  44,  84,  84};

    for (int i = 0; i < 4; i++) {
        mode_btns[i] = lv_btn_create(lv_scr_act());
        lv_obj_set_size(mode_btns[i], 72, 34);
        lv_obj_set_pos(mode_btns[i], bx[i], by[i]);
        lv_obj_set_style_radius(mode_btns[i], 6, 0);
        lv_obj_add_event_cb(mode_btns[i], btn_mode_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);

        lv_obj_t* lbl = lv_label_create(mode_btns[i]);
        lv_label_set_text(lbl, mode_names[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, currentTheme->text_main, 0);
        lv_obj_center(lbl);
    }
    update_mode_buttons(current_mode);

    // --- Color preview (below mode buttons, y=126) ---
    lv_obj_t* preview_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(preview_container, 150, 60);
    lv_obj_set_pos(preview_container, 163, 126);
    lv_obj_set_style_bg_color(preview_container, currentTheme->btn_core, 0);
    lv_obj_set_style_border_color(preview_container, currentTheme->border, 0);
    lv_obj_set_style_border_width(preview_container, 1, 0);
    lv_obj_set_style_radius(preview_container, 6, 0);
    lv_obj_set_style_pad_all(preview_container, 4, 0);
    lv_obj_clear_flag(preview_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_prev = lv_label_create(preview_container);
    lv_label_set_text(lbl_prev, lang->led_color_preview);
    lv_obj_set_style_text_color(lbl_prev, currentTheme->text_muted, 0);
    lv_obj_set_style_text_font(lbl_prev, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_prev, LV_ALIGN_TOP_LEFT, 2, 0);

    color_preview = lv_obj_create(preview_container);
    lv_obj_set_size(color_preview, 130, 32);
    lv_obj_align(color_preview, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(color_preview, saved, 0);
    lv_obj_set_style_border_width(color_preview, 0, 0);
    lv_obj_set_style_radius(color_preview, 4, 0);
    lv_obj_clear_flag(color_preview, LV_OBJ_FLAG_SCROLLABLE);

    // --- Brightness slider bar (34px, bottom) ---
    lv_obj_t* slider_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(slider_bar, 320, 34);
    lv_obj_align(slider_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(slider_bar, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(slider_bar, 0, 0);
    lv_obj_set_style_radius(slider_bar, 0, 0);
    lv_obj_set_style_pad_all(slider_bar, 4, 0);
    lv_obj_clear_flag(slider_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_sun = lv_label_create(slider_bar);
    lv_label_set_text(lbl_sun, LV_SYMBOL_IMAGE);
    lv_obj_set_style_text_color(lbl_sun, currentTheme->text_muted, 0);
    lv_obj_align(lbl_sun, LV_ALIGN_LEFT_MID, 2, 0);

    lv_obj_t* lbl_bright = lv_label_create(slider_bar);
    lv_label_set_text(lbl_bright, lang->led_brightness);
    lv_obj_set_style_text_color(lbl_bright, currentTheme->text_muted, 0);
    lv_obj_set_style_text_font(lbl_bright, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_bright, LV_ALIGN_LEFT_MID, 22, 0);

    slider_bright = lv_slider_create(slider_bar);
    lv_obj_set_size(slider_bright, 160, 8);
    lv_obj_align(slider_bright, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_slider_set_range(slider_bright, 0, 180);
    lv_slider_set_value(slider_bright, userConfig.led_bright, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_bright, slider_bright_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_bg_color(slider_bright, currentTheme->border,    LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_bright, currentTheme->primary,   LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_bright, currentTheme->text_main, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_bright, 5, LV_PART_KNOB);
}
