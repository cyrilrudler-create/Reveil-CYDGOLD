#include "ui_pays.h"
#include "structures.h"
#include "ui_config.h"
#include <lvgl.h>
#include <Arduino.h>
#include "ui_lang.h"

// =====================================================
//  TIMEZONE SELECTION SCREEN
//
//  Displays 33 timezones grouped by region.
//  Region headers use user_data = -1 to be skipped
//  during highlight updates.
//  The selected POSIX string is written to
//  userConfig.timezone and applied on Confirm.
// =====================================================

extern RadioTheme* currentTheme;

// =====================================================
//  TIMEZONE DATA
// =====================================================

struct TimezoneMap {
    const char* name;   // Display name
    const char* posix;  // POSIX TZ string (stored in userConfig)
    const char* region; // Group header label
};

static const TimezoneMap timezone_list[] = {
    // --- Europe ---
    {"Londres / Lisbonne",        "GMT0BST,M3.5.0/1,M10.5.0",         "Europe"},
    {"Paris / Berlin / Rome",     "CET-1CEST,M3.5.0,M10.5.0/3",       "Europe"},
    {"Athenes / Kiev",            "EET-2EEST,M3.5.0/3,M10.5.0/4",     "Europe"},
    {"Moscou / Istanbul",         "MSK-3",                              "Europe"},

    // --- Africa & Middle-East ---
    {"Dubai / Abu Dhabi",         "GST-4",                              "Afrique & M-O"},
    {"Arabie Saoudite",           "AST-3",                              "Afrique & M-O"},
    {"La Reunion / Maurice",      "RET-4",                              "Afrique & M-O"},

    // --- Asia ---
    {"Inde (New Delhi)",          "IST-5:30",                           "Asie"},
    {"Thailande / Vietnam",       "WIB-7",                              "Asie"},
    {"Chine / Singapour",         "CST-8",                              "Asie"},
    {"Japon / Coree du Sud",      "JST-9",                              "Asie"},

    // --- Oceania ---
    {"Perth (Australie Ouest)",   "AWST-8",                             "Oceanie"},
    {"Sydney (NSW)",              "AEST-10AEDT,M10.1.0,M4.1.0/3",      "Oceanie"},
    {"Nouvelle-Zelande",          "NZST-12NZDT,M9.5.0,M4.1.0/3",       "Oceanie"},

    // --- North America ---
    {"New York / Montreal",       "EST5EDT,M3.2.0,M11.1.0",            "Amerique N."},
    {"Chicago / Mexico",          "CST6CDT,M3.2.0,M11.1.0",            "Amerique N."},
    {"Denver",                    "MST7MDT,M3.2.0,M11.1.0",            "Amerique N."},
    {"Los Angeles / Vancouver",   "PST8PDT,M3.2.0,M11.1.0",            "Amerique N."},
    {"Alaska",                    "AKST9AKDT,M3.2.0,M11.1.0",          "Amerique N."},
    {"Hawaii",                    "HST10",                              "Amerique N."},
    {"Acores",                    "AZOT1AZOST,M3.5.0/0,M10.5.0/1",     "Amerique N."},
    {"Groenland (Nuuk)",          "WGST3WGDT,M3.5.0/2,M10.5.0/3",     "Amerique N."},

    // --- South America ---
    {"Guadeloupe / Martinique",   "AST4",                               "Amerique S."},
    {"Bresil (Sao Paulo)",        "BRT3BRST,M10.3.0/0,M2.3.0/0",       "Amerique S."},
    {"Argentine",                 "ART3",                               "Amerique S."},

    // --- Fixed UTC offsets ---
    {"UTC -12",                   "BIT12",                              "UTC fixe"},
    {"UTC -11 (Samoa)",           "SST11",                              "UTC fixe"},
    {"UTC -1",                    "CVT1",                               "UTC fixe"},
    {"UTC +5 (Maldives)",         "MVT-5",                              "UTC fixe"},
    {"UTC +6 (Bangladesh)",       "BST-6",                              "UTC fixe"},
    {"UTC +11 (Nouvelle-Cal.)",   "NCT-11",                             "UTC fixe"},
    {"UTC +13 (Tonga)",           "TOT-13",                             "UTC fixe"},
    {"UTC +14 (Iles de la Ligne)","LINT-14",                            "UTC fixe"},
};
static const int timezone_count = sizeof(timezone_list) / sizeof(timezone_list[0]);

// =====================================================
//  STATE
// =====================================================

static lv_obj_t* ui_list      = NULL;
static lv_obj_t* ui_sel_label = NULL; // Shows current selection in header
static int selected_index     = 0;

// =====================================================
//  CALLBACKS
// =====================================================

// Save config, apply timezone and return to settings
static void back_to_config_async(void* p) {
    saveConfig();
    setenv("TZ", userConfig.timezone, 1);
    tzset();
    lv_obj_clean(lv_scr_act());
    setup_config_screen();
}

// Tap a timezone button — save POSIX string and update highlight
static void timezone_btn_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    selected_index = idx;

    strncpy(userConfig.timezone, timezone_list[idx].posix, sizeof(userConfig.timezone) - 1);
    userConfig.timezone[sizeof(userConfig.timezone) - 1] = '\0';

    Serial.printf("[TZ] Selected: %s (%s)\n", timezone_list[idx].name, userConfig.timezone);

    // Update the header selection label
    if (ui_sel_label != NULL && lv_obj_is_valid(ui_sel_label)) {
        lv_label_set_text_fmt(ui_sel_label, ">> %s", timezone_list[idx].name);
        lv_obj_set_style_text_color(ui_sel_label, currentTheme->primary, 0);
    }

    // Highlight selected button — region headers (user_data = -1) are skipped
    uint32_t child_count = lv_obj_get_child_cnt(ui_list);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* child = lv_obj_get_child(ui_list, i);
        if (lv_obj_get_user_data(child) == (void*)(intptr_t)-1) continue;
        int child_idx = (int)(intptr_t)lv_obj_get_user_data(child);
        lv_obj_set_style_bg_color(child,
            child_idx == idx ? currentTheme->primary : currentTheme->btn_core, 0);
        lv_obj_set_style_bg_opa(child, LV_OPA_COVER, 0);
    }
}

static void btn_confirm_cb(lv_event_t* e) {
    lv_async_call(back_to_config_async, NULL);
}

// =====================================================
//  SCREEN LAYOUT  (320 x 240)
//
//  ┌──────────────────────────────────────┐
//  │  FUSEAU HORAIRE                      │  <- header 52px
//  │  >> Paris / Berlin / Rome [Confirm]  │
//  ├──────────────────────────────────────┤
//  │  Europe                              │  <- region header
//  │  [ Londres / Lisbonne              ] │
//  │  [ Paris / Berlin / Rome           ] │  <- highlighted
//  │  ...                                 │  <- scrollable 188px
//  └──────────────────────────────────────┘
// =====================================================
void setup_pays_screen() {
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), currentTheme->bg_color, 0);

    // --- Header (52px) ---
    lv_obj_t* header = lv_obj_create(lv_scr_act());
    lv_obj_set_size(header, 320, 52);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(header, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 4, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, lang->tz_title);
    lv_obj_set_style_text_color(title, currentTheme->primary, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 2);

    // Find the index matching the currently saved POSIX string
    int current_idx = 0;
    for (int i = 0; i < timezone_count; i++) {
        if (strcmp(userConfig.timezone, timezone_list[i].posix) == 0) {
            current_idx = i;
            break;
        }
    }
    selected_index = current_idx;

    // Current selection label (scrolls if long)
    ui_sel_label = lv_label_create(header);
    lv_label_set_text_fmt(ui_sel_label, ">> %s", timezone_list[current_idx].name);
    lv_obj_set_style_text_color(ui_sel_label, currentTheme->primary, 0);
    lv_obj_set_style_text_font(ui_sel_label, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(ui_sel_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(ui_sel_label, 220);
    lv_obj_align(ui_sel_label, LV_ALIGN_BOTTOM_LEFT, 4, -2);

    // Confirm button
    lv_obj_t* btn_ok = lv_btn_create(header);
    lv_obj_set_size(btn_ok, 75, 38);
    lv_obj_align(btn_ok, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_ok, currentTheme->primary, 0);
    lv_obj_set_style_radius(btn_ok, 6, 0);
    lv_obj_add_event_cb(btn_ok, btn_confirm_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, lang->tz_validate);
    lv_obj_set_style_text_color(lbl_ok, currentTheme->bg_color, 0);
    lv_obj_center(lbl_ok);

    // --- Scrollable timezone list (188px) ---
    ui_list = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ui_list, 320, 188);
    lv_obj_align(ui_list, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(ui_list, currentTheme->bg_color, 0);
    lv_obj_set_style_border_width(ui_list, 0, 0);
    lv_obj_set_style_radius(ui_list, 0, 0);
    lv_obj_set_style_pad_all(ui_list, 4, 0);
    lv_obj_set_style_pad_row(ui_list, 3, 0);
    lv_obj_set_flex_flow(ui_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(ui_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui_list, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_add_flag(ui_list, LV_OBJ_FLAG_SCROLL_MOMENTUM);

    // Populate list — insert region headers when the group changes
    const char* last_region = "";
    for (int i = 0; i < timezone_count; i++) {

        // Region header (user_data = -1 marks it as non-selectable)
        if (strcmp(timezone_list[i].region, last_region) != 0) {
            last_region = timezone_list[i].region;
            lv_obj_t* region_lbl = lv_label_create(ui_list);
            lv_label_set_text(region_lbl, last_region);
            lv_obj_set_width(region_lbl, 310);
            lv_obj_set_style_text_color(region_lbl, currentTheme->text_muted, 0);
            lv_obj_set_style_text_font(region_lbl, &lv_font_montserrat_12, 0);
            lv_obj_set_style_pad_top(region_lbl, 4, 0);
            lv_obj_set_style_pad_left(region_lbl, 6, 0);
            lv_obj_set_user_data(region_lbl, (void*)(intptr_t)-1);
        }

        // Timezone button
        lv_obj_t* btn = lv_btn_create(ui_list);
        lv_obj_set_size(btn, 308, 36);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_pad_left(btn, 10, 0);
        lv_obj_set_style_pad_right(btn, 6, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn,
            i == current_idx ? currentTheme->primary : currentTheme->btn_core, 0);
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, timezone_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, timezone_list[i].name);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(lbl, 290);
        lv_obj_set_style_text_color(lbl, currentTheme->text_main, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    }

    // Auto-scroll to the currently selected entry
    uint32_t child_count = lv_obj_get_child_cnt(ui_list);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* child = lv_obj_get_child(ui_list, i);
        if (lv_obj_get_user_data(child) == (void*)(intptr_t)current_idx) {
            lv_obj_scroll_to_view(child, LV_ANIM_ON);
            break;
        }
    }
}
