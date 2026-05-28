#include "ui_equalizer.h"
#include "ui_config.h"
#include "structures.h"
#include <lvgl.h>
#include "Audio.h"
#include "ui_lang.h"

// =====================================================
//  EQUALIZER SCREEN  (320 x 240)
//
//  3 vertical sliders : Bass / Mid / Treble
//  Range : -10 dB to +6 dB
//  Applied in real time via audio.setTone()
//  Reset button sets all bands to 0 dB
// =====================================================

extern Audio       audio;
extern RadioTheme* currentTheme;

// --- Internal UI references ---
static lv_obj_t* lbl_values[3] = {NULL};
static lv_obj_t* sliders[3]    = {NULL};

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

// Slider moved — update config, label color and audio tone in real time
static void slider_eq_cb(lv_event_t* e) {
    int val  = (int)lv_slider_get_value(lv_event_get_target(e));
    int type = (int)(uintptr_t)lv_event_get_user_data(e); // 0=bass 1=mid 2=treble

    if (type == 0) userConfig.eq_bass   = val;
    if (type == 1) userConfig.eq_mid    = val;
    if (type == 2) userConfig.eq_treble = val;

    if (lbl_values[type] != NULL && lv_obj_is_valid(lbl_values[type])) {
        lv_label_set_text_fmt(lbl_values[type], "%s%d", val > 0 ? "+" : "", val);

        lv_color_t col;
        if      (val > 0) col = lv_palette_main(LV_PALETTE_GREEN);
        else if (val < 0) col = lv_palette_main(LV_PALETTE_RED);
        else              col = currentTheme->text_muted;
        lv_obj_set_style_text_color(lbl_values[type], col, 0);
    }

    audio.setTone(userConfig.eq_bass, userConfig.eq_mid, userConfig.eq_treble);
}

// Reset all bands to 0 dB
static void btn_reset_cb(lv_event_t* e) {
    userConfig.eq_bass = userConfig.eq_mid = userConfig.eq_treble = 0;

    for (int i = 0; i < 3; i++) {
        if (sliders[i]    != NULL && lv_obj_is_valid(sliders[i]))
            lv_slider_set_value(sliders[i], 0, LV_ANIM_ON);
        if (lbl_values[i] != NULL && lv_obj_is_valid(lbl_values[i])) {
            lv_label_set_text(lbl_values[i], "0");
            lv_obj_set_style_text_color(lbl_values[i], currentTheme->text_muted, 0);
        }
    }
    audio.setTone(0, 0, 0);
}

// =====================================================
//  SCREEN LAYOUT  (320 x 240)
//
//  ┌────────────────────────────────────┐
//  │  Vol  EQUALIZER     [Reset]  [OK]  │  <- header 38px
//  ├────────────────────────────────────┤
//  │  +6  ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄     │  <- +6dB reference line
//  │                                    │
//  │       [B]      [M]      [A]        │  <- band initials
//  │        |        |        |         │
//  │       ███      ███      ███        │  <- vertical sliders
//  │        |        |        |         │  range -10..+6
//  │   0  ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄     │  <- 0dB reference (primary color)
//  │        |        |        |         │
//  │ -10  ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄     │  <- -10dB reference line
//  │                                    │
//  │      +3       0       -2           │  <- dB values
//  │     Bass     Mid    Treble         │  <- band names
//  └────────────────────────────────────┘
//
//  Slider positions : x = 80, 160, 240
//  Slider zone      : y = 52..188  (h = 136px)
//  0dB position     : y = 52 + (6 * 136 / 16) = 103px
// =====================================================
void setup_equalizer_screen() {
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), currentTheme->bg_color, 0);

    int*        eq_vals[]  = {&userConfig.eq_bass, &userConfig.eq_mid, &userConfig.eq_treble};
    const char* names[]    = {lang->eq_bass, lang->eq_mid, lang->eq_treble};
    const char* initials[] = {"B", "M", "A"};

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
    lv_label_set_text_fmt(title, "%s  %s", LV_SYMBOL_VOLUME_MAX, lang->eq_title);
    lv_obj_set_style_text_color(title, currentTheme->primary, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

    // OK button
    lv_obj_t* btn_ok = lv_btn_create(header);
    lv_obj_set_size(btn_ok, 55, 28);
    lv_obj_align(btn_ok, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_ok, currentTheme->primary, 0);
    lv_obj_set_style_radius(btn_ok, 6, 0);
    lv_obj_add_event_cb(btn_ok, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, lang->eq_validate);
    lv_obj_set_style_text_color(lbl_ok, currentTheme->bg_color, 0);
    lv_obj_center(lbl_ok);

    // Reset button
    lv_obj_t* btn_reset = lv_btn_create(header);
    lv_obj_set_size(btn_reset, 60, 28);
    lv_obj_align(btn_reset, LV_ALIGN_RIGHT_MID, -62, 0);
    lv_obj_set_style_bg_color(btn_reset, currentTheme->btn_core, 0);
    lv_obj_set_style_border_color(btn_reset, currentTheme->border, 0);
    lv_obj_set_style_border_width(btn_reset, 1, 0);
    lv_obj_set_style_radius(btn_reset, 6, 0);
    lv_obj_add_event_cb(btn_reset, btn_reset_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, "Reset");
    lv_obj_set_style_text_color(lbl_reset, currentTheme->text_muted, 0);
    lv_obj_set_style_text_font(lbl_reset, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_reset);

    // --- dB reference lines ---
    const int SL_TOP    = 52;
    const int SL_HEIGHT = 136;
    const int SL_BOTTOM = SL_TOP + SL_HEIGHT; // = 188
    const int y_zero    = SL_TOP + (6 * SL_HEIGHT / 16); // 0dB position ≈ 103

    auto make_ref_line = [&](int y, lv_color_t color, lv_opa_t opa) {
        lv_obj_t* line = lv_obj_create(lv_scr_act());
        lv_obj_set_size(line, 240, 1);
        lv_obj_set_pos(line, 40, y);
        lv_obj_set_style_bg_color(line, color, 0);
        lv_obj_set_style_bg_opa(line, opa, 0);
        lv_obj_set_style_border_width(line, 0, 0);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    };

    make_ref_line(SL_TOP,    currentTheme->border,  LV_OPA_COVER);
    make_ref_line(y_zero,    currentTheme->primary,  LV_OPA_50);
    make_ref_line(SL_BOTTOM, currentTheme->border,  LV_OPA_COVER);

    auto make_ref_label = [&](int x, int y, const char* txt, lv_color_t color) {
        lv_obj_t* lbl = lv_label_create(lv_scr_act());
        lv_label_set_text(lbl, txt);
        lv_obj_set_style_text_color(lbl, color, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(lbl, x, y);
    };

    make_ref_label(4,  SL_TOP    - 6, "+6",  currentTheme->text_muted);
    make_ref_label(10, y_zero    - 7, "0",   currentTheme->primary);
    make_ref_label(2,  SL_BOTTOM - 6, "-10", currentTheme->text_muted);

    // --- 3 vertical sliders ---
    const int sl_x[3]  = {80, 160, 240};
    const int SL_WIDTH = 22;

    for (int i = 0; i < 3; i++) {
        // Column background
        lv_obj_t* col = lv_obj_create(lv_scr_act());
        lv_obj_set_size(col, 60, SL_HEIGHT + 46);
        lv_obj_set_pos(col, sl_x[i] - 30, SL_TOP - 4);
        lv_obj_set_style_bg_color(col, currentTheme->btn_core, 0);
        lv_obj_set_style_bg_opa(col, LV_OPA_30, 0);
        lv_obj_set_style_border_width(col, 0, 0);
        lv_obj_set_style_radius(col, 8, 0);
        lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(col, LV_OBJ_FLAG_CLICKABLE);

        // Vertical slider
        sliders[i] = lv_slider_create(lv_scr_act());
        lv_obj_set_size(sliders[i], SL_WIDTH, SL_HEIGHT);
        lv_obj_set_pos(sliders[i], sl_x[i] - SL_WIDTH / 2, SL_TOP);
        lv_slider_set_range(sliders[i], -10, 6);
        lv_slider_set_value(sliders[i], *eq_vals[i], LV_ANIM_OFF);
        lv_obj_set_style_bg_color(sliders[i], currentTheme->btn_core,  LV_PART_MAIN);
        lv_obj_set_style_bg_color(sliders[i], currentTheme->primary,   LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(sliders[i], currentTheme->text_main, LV_PART_KNOB);
        lv_obj_set_style_radius(sliders[i], 4,                LV_PART_MAIN);
        lv_obj_set_style_radius(sliders[i], LV_RADIUS_CIRCLE, LV_PART_KNOB);
        lv_obj_set_style_border_color(sliders[i], currentTheme->primary, LV_PART_KNOB);
        lv_obj_set_style_border_width(sliders[i], 2, LV_PART_KNOB);
        lv_obj_set_style_pad_all(sliders[i], 7, LV_PART_KNOB);
        lv_obj_add_event_cb(sliders[i], slider_eq_cb, LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)i);

        // Band initial above slider
        lv_obj_t* lbl_init = lv_label_create(lv_scr_act());
        lv_label_set_text(lbl_init, initials[i]);
        lv_obj_set_style_text_color(lbl_init, currentTheme->text_muted, 0);
        lv_obj_set_style_text_font(lbl_init, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(lbl_init, sl_x[i] - 4, SL_TOP - 14);

        // dB value below slider (green/red/muted)
        lbl_values[i] = lv_label_create(lv_scr_act());
        int val = *eq_vals[i];
        lv_label_set_text_fmt(lbl_values[i], "%s%d", val > 0 ? "+" : "", val);
        lv_color_t col_v = (val > 0) ? lv_palette_main(LV_PALETTE_GREEN) :
                           (val < 0) ? lv_palette_main(LV_PALETTE_RED)   :
                                        currentTheme->text_muted;
        lv_obj_set_style_text_color(lbl_values[i], col_v, 0);
        lv_obj_set_style_text_font(lbl_values[i], &lv_font_montserrat_12, 0);
        lv_obj_set_pos(lbl_values[i], sl_x[i] - 8, SL_BOTTOM + 6);

        // Band name below value
        lv_obj_t* lbl_name = lv_label_create(lv_scr_act());
        lv_label_set_text(lbl_name, names[i]);
        lv_obj_set_style_text_color(lbl_name, currentTheme->text_muted, 0);
        lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(lbl_name, sl_x[i] - 22, SL_BOTTOM + 22);
    }
}
