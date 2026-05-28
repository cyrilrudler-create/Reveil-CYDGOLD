#include "ui_traduction.h"
#include "ui_lang.h"
#include "structures.h"
#include "ui_config.h"
#include <lvgl.h>
#include <Arduino.h>

extern RadioTheme* currentTheme;

// =====================================================
//  LANGUAGE SELECTION SCREEN
//  Allows the user to pick a display language.
//  The change is applied immediately (live preview)
//  and saved when the user taps Confirm.
// =====================================================

// Currently highlighted language index (not yet confirmed)
static int preview_lang = 0;

// --- Save config and return to settings ---
static void back_async(void* p) {
    saveConfig();
    lv_obj_clean(lv_scr_act());
    setup_config_screen();
}

static void btn_back_cb(lv_event_t* e) {
    lv_async_call(back_async, NULL);
}

// --- Called when user taps a language button ---
// Applies the language immediately so all labels update live
static void lang_item_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    preview_lang = idx;

    apply_language(idx);
    userConfig.language = idx;

    // Highlight selected button, reset all others
    lv_obj_t* btn_tapped = lv_event_get_current_target(e);
    lv_obj_t* parent     = lv_obj_get_parent(btn_tapped);
    uint32_t  cnt        = lv_obj_get_child_cnt(parent);

    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(parent, i);
        if (!lv_obj_check_type(child, &lv_btn_class)) continue;

        bool selected = ((int)i == idx);
        lv_obj_set_style_bg_color(child,
            selected ? currentTheme->primary : currentTheme->btn_core, 0);

        lv_obj_t* lbl = lv_obj_get_child(child, 0);
        if (lbl) lv_obj_set_style_text_color(lbl,
            selected ? currentTheme->bg_color : currentTheme->text_main, 0);
    }
}

// =====================================================
//  SCREEN LAYOUT  (320 x 240)
//
//  ┌──────────────────────────────────┐
//  │  LANGUE          lang name       │  <- header 52px
//  │                       [Valider]  │
//  ├──────────────────────────────────┤
//  │  ✓  Francais                     │  <- scrollable list
//  │     English                      │
//  │     Espanol                      │
//  │     ...                          │
//  └──────────────────────────────────┘
// =====================================================
void setup_traduction_screen() {
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), currentTheme->bg_color, 0);

    // --- Header ---
    lv_obj_t* header = lv_obj_create(lv_scr_act());
    lv_obj_set_size(header, 320, 52);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(header, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 4, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, lang->lang_title);
    lv_obj_set_style_text_color(title, currentTheme->primary, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 2);

    // Current language name shown below the title
    lv_obj_t* sub = lv_label_create(header);
    lv_label_set_text(sub, lang->lang_name);
    lv_obj_set_style_text_color(sub, currentTheme->text_muted, 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_10, 0);
    lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, 6, -2);

    // Confirm button
    lv_obj_t* btn_back = lv_btn_create(header);
    lv_obj_set_size(btn_back, 75, 28);
    lv_obj_align(btn_back, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_back, currentTheme->primary, 0);
    lv_obj_set_style_radius(btn_back, 6, 0);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, lang->lang_validate);
    lv_obj_set_style_text_color(lbl_back, currentTheme->bg_color, 0);
    lv_obj_center(lbl_back);

    // --- Scrollable language list ---
    lv_obj_t* scroll = lv_obj_create(lv_scr_act());
    lv_obj_set_size(scroll, 320, 240 - 52);
    lv_obj_align(scroll, LV_ALIGN_TOP_LEFT, 0, 52);
    lv_obj_set_style_bg_color(scroll, currentTheme->bg_color, 0);
    lv_obj_set_style_border_width(scroll, 0, 0);
    lv_obj_set_style_pad_all(scroll, 8, 0);
    lv_obj_set_style_pad_row(scroll, 6, 0);
    lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);

    for (int i = 0; i < LANG_COUNT; i++) {
        bool selected = (i == userConfig.language);

        lv_obj_t* btn = lv_btn_create(scroll);
        lv_obj_set_size(btn, 290, 44);
        lv_obj_set_style_bg_color(btn,
            selected ? currentTheme->primary : currentTheme->btn_core, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, currentTheme->border, 0);
        lv_obj_add_event_cb(btn, lang_item_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // Checkmark icon on selected language
        lv_obj_t* ico = lv_label_create(btn);
        lv_label_set_text(ico, selected ? LV_SYMBOL_OK : " ");
        lv_obj_set_style_text_color(ico,
            selected ? currentTheme->bg_color : currentTheme->text_muted, 0);
        lv_obj_align(ico, LV_ALIGN_LEFT_MID, 8, 0);

        // Language name
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, lang_names[i]);
        lv_obj_set_style_text_color(lbl,
            selected ? currentTheme->bg_color : currentTheme->text_main, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 36, 0);
    }

    preview_lang = userConfig.language;
}
