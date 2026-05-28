#include "ui_themes.h"
#include "ui_config.h"
#include "structures.h"
#include <lvgl.h>
#include "ui_lang.h"

// =====================================================
//  THEME SELECTION SCREEN
//  Displays a 3x2 grid of preview cards, one per theme.
//  Each card shows the theme colors (primary band,
//  mock buttons, simulated text lines).
//  Tapping a card applies the theme instantly and
//  reloads the screen. Confirm saves to config.
// =====================================================

// Theme display names (shown at the bottom of each card)
static const char* theme_names[] = {
    "Orange",
    "Cyberpunk",
    "Matrix",
    "Bleu Pastel",
    "Rose Poudre",
    "Vert Pastel"
};

// Card object references — needed to update the selection border
static lv_obj_t* theme_cards[6] = {NULL};

// --- Save config and return to settings ---
static void back_async(void* p) {
    saveConfig();
    lv_obj_clean(lv_scr_act());
    setup_config_screen();
}

static void btn_back_cb(lv_event_t* e) {
    lv_async_call(back_async, NULL);
}

// --- Highlight the active card with a thick primary-colored border ---
static void update_cards_selection(int selected_id) {
    for (int i = 0; i < NOMBRE_THEMES; i++) {
        if (theme_cards[i] == NULL || !lv_obj_is_valid(theme_cards[i])) continue;
        if (i == selected_id) {
            lv_obj_set_style_border_color(theme_cards[i], currentTheme->primary, 0);
            lv_obj_set_style_border_width(theme_cards[i], 3, 0);
            lv_obj_set_style_shadow_width(theme_cards[i], 10, 0);
            lv_obj_set_style_shadow_color(theme_cards[i], currentTheme->primary, 0);
        } else {
            lv_obj_set_style_border_color(theme_cards[i], lv_color_hex(0x444444), 0);
            lv_obj_set_style_border_width(theme_cards[i], 1, 0);
            lv_obj_set_style_shadow_width(theme_cards[i], 0, 0);
        }
    }
}

// --- Apply selected theme and reload screen ---
static void theme_card_cb(lv_event_t* e) {
    int id = (int)(intptr_t)lv_event_get_user_data(e);
    userConfig.selected_theme = id;
    currentTheme = listeThemes[id];
    Serial.printf("[THEME] Selected: %d (%s)\n", id, theme_names[id]);
    setup_themes_screen();
}

// =====================================================
//  SCREEN LAYOUT  (320 x 240)
//
//  ┌────────────────────────────────────┐
//  │  THEMES                  [Confirm] │  <- header 38px
//  ├────────────────────────────────────┤
//  │  ┌──────┐  ┌──────┐  ┌──────┐     │
//  │  │██████│  │██████│  │██████│     │  <- primary color band
//  │  │[_][_]│  │[_][_]│  │[_][_]│     │  <- mock buttons
//  │  │ ~~~~ │  │ ~~~~ │  │ ~~~~ │     │  <- simulated text lines
//  │  │Orange│  │Cybrpk│  │Matrx │     │
//  │  └──────┘  └──────┘  └──────┘     │
//  │  ┌──────┐  ┌──────┐  ┌──────┐     │
//  │  │ ...  │  │ ...  │  │ ...  │     │
//  │  └──────┘  └──────┘  └──────┘     │
//  └────────────────────────────────────┘
//
//  Card grid : 3 cols x 2 rows
//  Card size : 95 x 88px  |  Gap H: 8px  Gap V: 8px
//  Origin    : x=7, y=47
// =====================================================
void setup_themes_screen() {
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), currentTheme->bg_color, 0);

    // --- Header ---
    lv_obj_t* header = lv_obj_create(lv_scr_act());
    lv_obj_set_size(header, 320, 38);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(header, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 4, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text_fmt(title, "%s  %s", LV_SYMBOL_IMAGE, lang->theme_title);
    lv_obj_set_style_text_color(title, currentTheme->primary, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t* btn_back = lv_btn_create(header);
    lv_obj_set_size(btn_back, 75, 28);
    lv_obj_align(btn_back, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_back, currentTheme->primary, 0);
    lv_obj_set_style_radius(btn_back, 6, 0);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, lang->theme_validate);
    lv_obj_set_style_text_color(lbl_back, currentTheme->bg_color, 0);
    lv_obj_center(lbl_back);

    // --- Theme card grid ---
    const int COLS     = 3;
    const int CARD_W   = 95;
    const int CARD_H   = 88;
    const int GAP_H    = 8;
    const int GAP_V    = 8;
    const int ORIGIN_X = 7;
    const int ORIGIN_Y = 47;

    for (int i = 0; i < NOMBRE_THEMES; i++) {
        int col = i % COLS;
        int row = i / COLS;
        int cx  = ORIGIN_X + col * (CARD_W + GAP_H);
        int cy  = ORIGIN_Y + row * (CARD_H + GAP_V);

        RadioTheme* th = listeThemes[i];

        // Card background
        theme_cards[i] = lv_obj_create(lv_scr_act());
        lv_obj_set_size(theme_cards[i], CARD_W, CARD_H);
        lv_obj_set_pos(theme_cards[i], cx, cy);
        lv_obj_set_style_bg_color(theme_cards[i], th->bg_color, 0);
        lv_obj_set_style_radius(theme_cards[i], 8, 0);
        lv_obj_set_style_pad_all(theme_cards[i], 5, 0);
        lv_obj_clear_flag(theme_cards[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(theme_cards[i], theme_card_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // Primary color band at the top
        lv_obj_t* band = lv_obj_create(theme_cards[i]);
        lv_obj_set_size(band, CARD_W - 10, 14);
        lv_obj_align(band, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(band, th->primary, 0);
        lv_obj_set_style_border_width(band, 0, 0);
        lv_obj_set_style_radius(band, 4, 0);
        lv_obj_clear_flag(band, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(band, LV_OBJ_FLAG_SCROLLABLE);

        // Two mock buttons (btn_core color with border)
        for (int b = 0; b < 2; b++) {
            lv_obj_t* mock_btn = lv_obj_create(theme_cards[i]);
            lv_obj_set_size(mock_btn, (CARD_W - 18) / 2, 16);
            lv_obj_set_pos(mock_btn, b * ((CARD_W - 18) / 2 + 4), 20);
            lv_obj_set_style_bg_color(mock_btn, th->btn_core, 0);
            lv_obj_set_style_border_color(mock_btn, th->border, 0);
            lv_obj_set_style_border_width(mock_btn, 1, 0);
            lv_obj_set_style_radius(mock_btn, 3, 0);
            lv_obj_clear_flag(mock_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(mock_btn, LV_OBJ_FLAG_SCROLLABLE);

            // Small primary-colored dot on the left button
            if (b == 0) {
                lv_obj_t* dot = lv_obj_create(mock_btn);
                lv_obj_set_size(dot, 6, 6);
                lv_obj_align(dot, LV_ALIGN_LEFT_MID, 3, 0);
                lv_obj_set_style_bg_color(dot, th->primary, 0);
                lv_obj_set_style_border_width(dot, 0, 0);
                lv_obj_set_style_radius(dot, 3, 0);
                lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
            }
        }

        // Simulated text lines (muted color, two widths)
        lv_obj_t* txt_line = lv_obj_create(theme_cards[i]);
        lv_obj_set_size(txt_line, CARD_W - 16, 5);
        lv_obj_set_pos(txt_line, 3, 42);
        lv_obj_set_style_bg_color(txt_line, th->text_muted, 0);
        lv_obj_set_style_bg_opa(txt_line, LV_OPA_60, 0);
        lv_obj_set_style_border_width(txt_line, 0, 0);
        lv_obj_set_style_radius(txt_line, 2, 0);
        lv_obj_clear_flag(txt_line, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(txt_line, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* txt_line2 = lv_obj_create(theme_cards[i]);
        lv_obj_set_size(txt_line2, (CARD_W - 16) * 2 / 3, 5);
        lv_obj_set_pos(txt_line2, 3, 52);
        lv_obj_set_style_bg_color(txt_line2, th->text_muted, 0);
        lv_obj_set_style_bg_opa(txt_line2, LV_OPA_40, 0);
        lv_obj_set_style_border_width(txt_line2, 0, 0);
        lv_obj_set_style_radius(txt_line2, 2, 0);
        lv_obj_clear_flag(txt_line2, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(txt_line2, LV_OBJ_FLAG_SCROLLABLE);

        // Theme name at the bottom of the card
        lv_obj_t* name_lbl = lv_label_create(theme_cards[i]);
        lv_label_set_text(name_lbl, theme_names[i]);
        lv_obj_set_style_text_color(name_lbl, th->text_main, 0);
        lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_10, 0);
        lv_obj_align(name_lbl, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_clear_flag(name_lbl, LV_OBJ_FLAG_CLICKABLE);
    }

    update_cards_selection(userConfig.selected_theme);
}
