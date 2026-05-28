#include "structures.h"
#include <lvgl.h>

// =====================================================
//  VISUAL THEMES
//
//  Each RadioTheme defines a complete color palette.
//  Add a new theme by declaring a RadioTheme, adding it
//  to listeThemes[], and giving it a name in ui_themes.cpp.
//  The active theme pointer is stored in currentTheme.
// =====================================================

// Theme 0 — Orange & Black (default)
RadioTheme themeOrange = {
    .bg_color   = lv_color_hex(0x000000),
    .primary    = lv_palette_main(LV_PALETTE_ORANGE),
    .text_main  = lv_color_hex(0xFFFFFF),
    .text_muted = lv_color_hex(0xAAAAAA),
    .btn_core   = lv_color_hex(0x1A1A1A),
    .border     = lv_palette_main(LV_PALETTE_ORANGE)
};

// Theme 1 — Cyberpunk Blue
RadioTheme themeBleu = {
    .bg_color   = lv_color_hex(0x0B0F19),
    .primary    = lv_palette_main(LV_PALETTE_BLUE),
    .text_main  = lv_color_hex(0xFFFFFF),
    .text_muted = lv_color_hex(0x8892B0),
    .btn_core   = lv_color_hex(0x1F2937),
    .border     = lv_palette_main(LV_PALETTE_BLUE)
};

// Theme 2 — Matrix Green
RadioTheme themeVert = {
    .bg_color   = lv_color_hex(0x050A05),
    .primary    = lv_palette_main(LV_PALETTE_GREEN),
    .text_main  = lv_color_hex(0x00FF00),
    .text_muted = lv_color_hex(0x008800),
    .btn_core   = lv_color_hex(0x0D1A0D),
    .border     = lv_palette_main(LV_PALETTE_GREEN)
};

// Theme 3 — Soft Blue Pastel
RadioTheme themePastelbleu = {
    .bg_color   = lv_color_hex(0xB9C9DC), // Periwinkle mist
    .primary    = lv_color_hex(0x4A6FA5), // Denim blue
    .text_main  = lv_color_hex(0x0F1E36), // Deep navy (max contrast)
    .text_muted = lv_color_hex(0x4B5E78), // Blue-grey
    .btn_core   = lv_color_hex(0xE3EDF7), // Frosted sky
    .border     = lv_color_hex(0x9AB0CC)
};

// Theme 4 — Dusty Rose Pastel
RadioTheme themeRosePoudre = {
    .bg_color   = lv_color_hex(0xE8C5C1), // Powder rose
    .primary    = lv_color_hex(0xB35C64), // Raspberry
    .text_main  = lv_color_hex(0x2B0F1A), // Deep plum (max contrast)
    .text_muted = lv_color_hex(0x6E4A58), // Muted wine
    .btn_core   = lv_color_hex(0xFBF0EF), // Dragee pink
    .border     = lv_color_hex(0xD1A39E)
};

// Theme 5 — Sage Green Pastel
RadioTheme themeVertPastel = {
    .bg_color   = lv_color_hex(0xC2D3C4), // Sage
    .primary    = lv_color_hex(0x4A7856), // Forest green
    .text_main  = lv_color_hex(0x132A18), // Deep emerald (max contrast)
    .text_muted = lv_color_hex(0x556B5A), // Grey-green
    .btn_core   = lv_color_hex(0xE2EFE4), // Mint
    .border     = lv_color_hex(0xA3B8A6)
};

// Theme registry — order must match ui_themes.cpp theme_names[]
RadioTheme* listeThemes[] = {
    &themeOrange,     // 0
    &themeBleu,       // 1
    &themeVert,       // 2
    &themePastelbleu, // 3
    &themeRosePoudre, // 4
    &themeVertPastel  // 5
};

const int NOMBRE_THEMES = sizeof(listeThemes) / sizeof(listeThemes[0]);

// Active theme pointer — starts on Orange, updated by ui_themes.cpp
RadioTheme* currentTheme = &themeOrange;
