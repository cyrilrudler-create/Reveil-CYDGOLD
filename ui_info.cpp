#include "ui_info.h"
#include "ui_config.h"
#include "structures.h"
#include <lvgl.h>
#include <WiFi.h>
#include "SD_MMC.h"
#include <Arduino.h>
#include "ui_lang.h"

// =====================================================
//  SYSTEM INFO SCREEN  (320 x 240)
//
//  Shows a scrollable table of live system metrics:
//  firmware version, IP, WiFi RSSI, SD card usage,
//  CPU temperature, SRAM / PSRAM / Flash memory.
// =====================================================

extern RadioTheme* currentTheme;

// --- Return to settings screen ---
static void back_async(void* p) {
    lv_obj_clean(lv_scr_act());
    setup_config_screen();
}

static void btn_back_cb(lv_event_t* e) {
    lv_async_call(back_async, NULL);
}

// --- Helper : one info row (icon + label on left, value on right) ---
static void create_info_row(lv_obj_t* parent,
                             const char* symbol,
                             const char* label,
                             const char* value)
{
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), 30);
    lv_obj_set_style_bg_opa(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text_fmt(lbl, "%s  %s", symbol, label);
    lv_obj_set_style_text_color(lbl, currentTheme->text_main, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* val = lv_label_create(row);
    lv_label_set_text(val, value);
    lv_obj_set_style_text_color(val, currentTheme->primary, 0);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);
}

// =====================================================
//  SCREEN LAYOUT  (320 x 240)
//
//  ┌────────────────────────────────────┐
//  │           SYSTEM INFO              │  <- title 30px
//  ├────────────────────────────────────┤
//  │  ⚙  Vers.       2.0-CYDGOLD       │
//  │  Wifi IP        192.168.x.x        │
//  │  Wifi Signal    -55 dBm            │  <- scrollable
//  │  SD  SD         OK                 │  info container
//  │  SD  SD Space   12.3 / 16.0 GB     │  155px
//  │  IMG Temp CPU   42.3 C             │
//  │  SAV SRAM       41/210 KB          │
//  │  DL  PSRAM      6/8 MB             │
//  │  DIR Flash      2/16 MB            │
//  ├────────────────────────────────────┤
//  │  (c) CyrilTech 2026     [Back]     │  <- footer
//  └────────────────────────────────────┘
// =====================================================
void setup_info_screen() {
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), currentTheme->bg_color, 0);

    // --- Title ---
    lv_obj_t* title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, lang->info_title);
    lv_obj_set_style_text_color(title, currentTheme->primary, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // --- Info container (scrollable, 300x155px) ---
    lv_obj_t* cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cont, 300, 155);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cont, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(cont, 1, 0);
    lv_obj_set_style_border_color(cont, currentTheme->border, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cont, 0, 0);
    lv_obj_set_style_pad_top(cont, 0, 0);
    lv_obj_set_style_pad_bottom(cont, 0, 0);

    // Firmware version
    create_info_row(cont, LV_SYMBOL_SETTINGS, "Vers.:", "2.0-CYDGOLD");

    // IP address
    String ip = WiFi.localIP().toString();
    create_info_row(cont, LV_SYMBOL_WIFI, "IP:", ip.c_str());

    // WiFi signal strength
    char rssi_buf[16];
    sprintf(rssi_buf, "%d dBm", WiFi.RSSI());
    create_info_row(cont, LV_SYMBOL_WIFI, lang->info_signal, rssi_buf);

    // SD card status
    bool sd_ok = (SD_MMC.cardType() != CARD_NONE);
    create_info_row(cont, LV_SYMBOL_SD_CARD, "SD:", sd_ok ? "OK" : "MISSING");

    // SD free / total space
    char sd_space_buf[32];
    if (sd_ok) {
        uint64_t total_bytes = SD_MMC.totalBytes();
        uint64_t free_bytes  = total_bytes - SD_MMC.usedBytes();
        float total_gb = (float)total_bytes / (1024.0f * 1024.0f * 1024.0f);
        float free_gb  = (float)free_bytes  / (1024.0f * 1024.0f * 1024.0f);
        sprintf(sd_space_buf, "%.1f / %.1f GB", free_gb, total_gb);
    } else {
        sprintf(sd_space_buf, "MISSING");
    }
    create_info_row(cont, LV_SYMBOL_SD_CARD, lang->info_sd, sd_space_buf);

    // CPU temperature
    char temp_buf[16];
    sprintf(temp_buf, "%.1f C", temperatureRead());
    create_info_row(cont, LV_SYMBOL_IMAGE, lang->info_temp, temp_buf);

    // Internal SRAM (free / total)
    char sram_buf[16];
    sprintf(sram_buf, "%d/%d KB",
        (int)(ESP.getFreeHeap()  / 1024),
        (int)(ESP.getHeapSize()  / 1024));
    create_info_row(cont, LV_SYMBOL_SAVE, "SRAM:", sram_buf);

    // External PSRAM (free / total)
    char psram_buf[16];
    sprintf(psram_buf, "%d/%d MB",
        (int)(ESP.getFreePsram()  / (1024 * 1024)),
        (int)(ESP.getPsramSize()  / (1024 * 1024)));
    create_info_row(cont, LV_SYMBOL_DOWNLOAD, "PSRAM:", psram_buf);

    // Flash storage (sketch / total)
    char flash_buf[16];
    sprintf(flash_buf, "%d/%d MB",
        (int)(ESP.getSketchSize()    / (1024 * 1024)),
        (int)(ESP.getFlashChipSize() / (1024 * 1024)));
    create_info_row(cont, LV_SYMBOL_DIRECTORY, "Flash:", flash_buf);

    // --- Copyright ---
    lv_obj_t* lbl_copy = lv_label_create(lv_scr_act());
    lv_label_set_text(lbl_copy, "(c) C-R Tech 2026");
    lv_obj_set_style_text_font(lbl_copy, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_copy, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(lbl_copy, LV_ALIGN_BOTTOM_LEFT, 15, -20);

    // --- Back button ---
    lv_obj_t* btn_back = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_back, 100, 40);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(btn_back, currentTheme->primary, 0);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, lang->wifi_back);
    lv_obj_center(lbl_back);
}
