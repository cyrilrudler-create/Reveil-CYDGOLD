#include "ui_wifi.h"
#include "ui_config.h"
#include "ui_home.h"
#include "config.h"
#include <lvgl.h>
#include <WiFi.h>
#include <Arduino.h>
#include "structures.h"
#include "ui_lang.h"

// =====================================================
//  WIFI MANAGEMENT SCREEN
//
//  Three-panel architecture on a single screen:
//    Panel 1 (main)  — saved networks list
//    Panel 2 (scan)  — scan results
//    Panel 3 (pwd)   — full-screen password keyboard
//
//  Only one panel is visible at a time.
//  wifi_ui_active flag suspends the auto-reconnect
//  loop while this screen is open.
// =====================================================

extern RadioTheme* currentTheme;

// --- UI object references ---
lv_obj_t* ui_wifi_list    = NULL; // exported — used in esp32s3audio.ino

static lv_obj_t* ui_main_panel   = NULL; // Panel 1 : saved networks
static lv_obj_t* ui_scan_panel   = NULL; // Panel 2 : scan results
static lv_obj_t* ui_pwd_panel    = NULL; // Panel 3 : password entry
static lv_obj_t* ui_status_bar   = NULL; // Bottom status bar (IP / state)
static lv_obj_t* ui_kb           = NULL; // LVGL keyboard widget
static lv_obj_t* ui_pwd_ta       = NULL; // Password text area
static lv_obj_t* ui_pwd_ssid_lbl = NULL; // SSID label in password panel
static lv_obj_t* ui_scan_list    = NULL; // Scrollable scan results list
static lv_obj_t* ui_pwd_eye_lbl  = NULL; // Eye icon for show/hide password

// =====================================================
//  HELPERS
// =====================================================

// Show one panel, hide the other two
static void show_panel(lv_obj_t* panel) {
    if (ui_main_panel != NULL && lv_obj_is_valid(ui_main_panel))
        lv_obj_add_flag(ui_main_panel, LV_OBJ_FLAG_HIDDEN);
    if (ui_scan_panel != NULL && lv_obj_is_valid(ui_scan_panel))
        lv_obj_add_flag(ui_scan_panel, LV_OBJ_FLAG_HIDDEN);
    if (ui_pwd_panel  != NULL && lv_obj_is_valid(ui_pwd_panel))
        lv_obj_add_flag(ui_pwd_panel,  LV_OBJ_FLAG_HIDDEN);
    if (panel != NULL && lv_obj_is_valid(panel))
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

// Refresh the bottom status bar with current WiFi state and IP
static void update_status_bar() {
    if (ui_status_bar == NULL || !lv_obj_is_valid(ui_status_bar)) return;
    lv_obj_clean(ui_status_bar);

    lv_obj_t* lbl = lv_label_create(ui_status_bar);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);

    if (WiFi.status() == WL_CONNECTED) {
        String txt = LV_SYMBOL_WIFI " S-WEB IP: " + WiFi.localIP().toString();
        lv_label_set_text(lbl, txt.c_str());
        lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_GREEN), 0);
    } else {
        lv_label_set_text(lbl, LV_SYMBOL_CLOSE "  Not connected");
        lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_RED), 0);
    }
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);
}

// =====================================================
//  NAVIGATION CALLBACKS
// =====================================================

extern bool wifi_ui_active;

// Leave WiFi screen — re-enables auto-reconnect
static void back_to_config_async(void* p) {
    wifi_ui_active = false;
    lv_obj_clean(lv_scr_act());
    setup_config_screen();
}

static void btn_back_cb(lv_event_t* e) {
    lv_async_call(back_to_config_async, NULL);
}

static void btn_retour_main_cb(lv_event_t* e) {
    show_panel(ui_main_panel);
}

// =====================================================
//  PANEL 1 — SAVED NETWORKS
// =====================================================

void refresh_saved_wifi_list() {
    if (ui_wifi_list == NULL || !lv_obj_is_valid(ui_wifi_list)) return;
    lv_obj_clean(ui_wifi_list);

    if (userConfig.networks_count == 0) {
        lv_obj_t* lbl = lv_label_create(ui_wifi_list);
        lv_label_set_text(lbl, lang->wifi_no_saved);
        lv_obj_set_style_text_color(lbl, currentTheme->text_muted, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    for (int i = 0; i < userConfig.networks_count; i++) {
        bool is_connected = (WiFi.status() == WL_CONNECTED &&
                             String(WiFi.SSID()) == String(userConfig.known_networks[i].ssid));

        lv_obj_t* row = lv_obj_create(ui_wifi_list);
        lv_obj_set_size(row, LV_PCT(100), 36);
        lv_obj_set_style_bg_color(row, currentTheme->btn_core, 0);
        lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row, currentTheme->border, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 4, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_user_data(row, (void*)(intptr_t)i);
        lv_obj_set_style_bg_color(row, currentTheme->primary, LV_STATE_PRESSED);

        // WiFi icon (green if connected)
        lv_obj_t* ico = lv_label_create(row);
        lv_label_set_text(ico, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(ico,
            is_connected ? lv_palette_main(LV_PALETTE_GREEN) : currentTheme->text_muted, 0);
        lv_obj_align(ico, LV_ALIGN_LEFT_MID, 0, 0);

        // SSID label
        lv_obj_t* ssid_lbl = lv_label_create(row);
        lv_label_set_text(ssid_lbl, userConfig.known_networks[i].ssid);
        lv_obj_set_style_text_color(ssid_lbl,
            is_connected ? lv_palette_main(LV_PALETTE_GREEN) : currentTheme->text_main, 0);
        lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(ssid_lbl, LV_ALIGN_LEFT_MID, 22, 0);

        // "Connected" badge
        if (is_connected) {
            lv_obj_t* badge = lv_label_create(row);
            lv_label_set_text_fmt(badge, "%s %s", LV_SYMBOL_OK, lang->wifi_connected);
            lv_obj_set_style_text_color(badge, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_font(badge, &lv_font_montserrat_10, 0);
            lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -4, 0);
        }

        // Tap row → connect to this saved network
        lv_obj_add_event_cb(row, [](lv_event_t* e) {
            int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            strncpy(userConfig.wifi_ssid, userConfig.known_networks[idx].ssid, 31);
            strncpy(userConfig.wifi_pass, userConfig.known_networks[idx].pass, 63);
            Serial.printf("[WIFI] Connecting to saved: %s\n", userConfig.wifi_ssid);
            start_wifi_connect();
            update_status_bar();
            refresh_saved_wifi_list();
        }, LV_EVENT_CLICKED, NULL);
    }
}

// =====================================================
//  PANEL 2 — SCAN & RESULTS
// =====================================================

void start_wifi_scan() {
    if (ui_scan_list == NULL || !lv_obj_is_valid(ui_scan_list)) return;

    show_panel(ui_scan_panel);
    lv_obj_clean(ui_scan_list);

    // Show "Scanning..." while scan runs
    lv_obj_t* scanning = lv_label_create(ui_scan_list);
    lv_label_set_text_fmt(scanning, "%s  %s", LV_SYMBOL_REFRESH, lang->wifi_scanning);
    lv_obj_set_style_text_color(scanning, currentTheme->text_muted, 0);
    lv_obj_align(scanning, LV_ALIGN_CENTER, 0, 0);
    lv_timer_handler();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    lv_obj_clean(ui_scan_list);

    if (n == 0) {
        lv_obj_t* lbl = lv_label_create(ui_scan_list);
        lv_label_set_text(lbl, lang->wifi_no_network);
        lv_obj_set_style_text_color(lbl, currentTheme->text_muted, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    } else {
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            int    rssi = WiFi.RSSI(i);
            if (ssid.length() == 0) continue;

            lv_obj_t* row = lv_obj_create(ui_scan_list);
            lv_obj_set_size(row, LV_PCT(100), 36);
            lv_obj_set_style_bg_color(row, currentTheme->btn_core, 0);
            lv_obj_set_style_border_width(row, 1, 0);
            lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
            lv_obj_set_style_border_color(row, currentTheme->border, 0);
            lv_obj_set_style_radius(row, 0, 0);
            lv_obj_set_style_pad_all(row, 4, 0);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(row, currentTheme->primary, LV_STATE_PRESSED);

            // Signal icon color : green > -55dBm, yellow > -70dBm, red otherwise
            lv_color_t sig_color;
            if      (rssi > -55) sig_color = lv_palette_main(LV_PALETTE_GREEN);
            else if (rssi > -70) sig_color = lv_palette_main(LV_PALETTE_YELLOW);
            else                 sig_color = lv_palette_main(LV_PALETTE_RED);

            lv_obj_t* ico = lv_label_create(row);
            lv_label_set_text(ico, LV_SYMBOL_WIFI);
            lv_obj_set_style_text_color(ico, sig_color, 0);
            lv_obj_align(ico, LV_ALIGN_LEFT_MID, 0, 0);

            lv_obj_t* ssid_lbl = lv_label_create(row);
            lv_label_set_text(ssid_lbl, ssid.c_str());
            lv_label_set_long_mode(ssid_lbl, LV_LABEL_LONG_CLIP);
            lv_obj_set_width(ssid_lbl, 160);
            lv_obj_set_style_text_color(ssid_lbl, currentTheme->text_main, 0);
            lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_14, 0);
            lv_obj_align(ssid_lbl, LV_ALIGN_LEFT_MID, 22, 0);

            lv_obj_t* rssi_lbl = lv_label_create(row);
            lv_label_set_text_fmt(rssi_lbl, "%d dBm", rssi);
            lv_obj_set_style_text_color(rssi_lbl, sig_color, 0);
            lv_obj_set_style_text_font(rssi_lbl, &lv_font_montserrat_10, 0);
            lv_obj_align(rssi_lbl, LV_ALIGN_RIGHT_MID, -4, 0);

            // Store SSID pointer for the tap callback
            String* ssid_copy = new String(ssid);
            lv_obj_set_user_data(row, (void*)ssid_copy);

            // Tap row → open password panel for this network
            lv_obj_add_event_cb(row, [](lv_event_t* e) {
                String* s = (String*)lv_obj_get_user_data(lv_event_get_target(e));
                if (!s) return;

                strncpy(userConfig.wifi_ssid, s->c_str(), sizeof(userConfig.wifi_ssid) - 1);
                userConfig.wifi_ssid[sizeof(userConfig.wifi_ssid) - 1] = '\0';

                if (ui_pwd_ssid_lbl != NULL && lv_obj_is_valid(ui_pwd_ssid_lbl))
                    lv_label_set_text_fmt(ui_pwd_ssid_lbl, LV_SYMBOL_WIFI "  %s", s->c_str());

                if (ui_pwd_ta != NULL && lv_obj_is_valid(ui_pwd_ta)) {
                    lv_textarea_set_text(ui_pwd_ta, "");
                    lv_textarea_set_password_mode(ui_pwd_ta, true);
                }
                if (ui_pwd_eye_lbl != NULL && lv_obj_is_valid(ui_pwd_eye_lbl)) {
                    lv_label_set_text(ui_pwd_eye_lbl, LV_SYMBOL_EYE_CLOSE);
                    lv_obj_set_style_text_color(ui_pwd_eye_lbl, currentTheme->text_muted, 0);
                }
                show_panel(ui_pwd_panel);
            }, LV_EVENT_CLICKED, NULL);
        }
    }
    WiFi.scanDelete();
}

// =====================================================
//  PANEL 3 — PASSWORD KEYBOARD
// =====================================================

// Called by the LVGL keyboard on READY (Enter) or CANCEL (Esc)
static void kb_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_READY) {
        const char* pwd = lv_textarea_get_text(ui_pwd_ta);
        strncpy(userConfig.wifi_pass, pwd, sizeof(userConfig.wifi_pass) - 1);
        userConfig.wifi_pass[sizeof(userConfig.wifi_pass) - 1] = '\0';
        Serial.printf("[WIFI] Connecting to: %s\n", userConfig.wifi_ssid);
        start_wifi_connect();
        update_status_bar();
        refresh_saved_wifi_list();
        show_panel(ui_main_panel);
    }

    if (code == LV_EVENT_CANCEL) {
        show_panel(ui_scan_panel);
    }
}

// =====================================================
//  SCREEN LAYOUT  (320 x 240)
//
//  ┌────────────────────────────────────┐
//  │  WiFi  WIFI MANAGER      [Back]    │  <- header 38px
//  ├────────────────────────────────────┤
//  │                                    │
//  │  PANEL 1 — saved networks          │  <- 162px, visible by default
//  │  PANEL 2 — scan results            │  <- hidden by default
//  │  PANEL 3 — password keyboard       │  <- hidden by default, fullscreen
//  │                                    │
//  ├────────────────────────────────────┤
//  │  [Scan]              S-WEB IP:...  │  <- footer 40px
//  └────────────────────────────────────┘
// =====================================================
void setup_wifi_screen() {
    wifi_ui_active = true; // suspend auto-reconnect while this screen is open
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
    lv_label_set_text_fmt(title, "%s %s", LV_SYMBOL_WIFI, lang->wifi_title);
    lv_obj_set_style_text_color(title, currentTheme->primary, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t* btn_back = lv_btn_create(header);
    lv_obj_set_size(btn_back, 75, 28);
    lv_obj_align(btn_back, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_back, currentTheme->primary, 0);
    lv_obj_set_style_radius(btn_back, 6, 0);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, lang->wifi_back);
    lv_obj_set_style_text_color(lbl_back, currentTheme->bg_color, 0);
    lv_obj_center(lbl_back);

    // --- Footer (scan button + status bar) ---
    lv_obj_t* footer = lv_obj_create(lv_scr_act());
    lv_obj_set_size(footer, 320, 40);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(footer, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_radius(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 4, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn_scan = lv_btn_create(footer);
    lv_obj_set_size(btn_scan, 100, 30);
    lv_obj_align(btn_scan, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(btn_scan, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_radius(btn_scan, 6, 0);
    lv_obj_add_event_cb(btn_scan, [](lv_event_t* e) {
        start_wifi_scan();
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_scan = lv_label_create(btn_scan);
    lv_label_set_text_fmt(lbl_scan, "%s  %s", LV_SYMBOL_REFRESH, lang->wifi_scan);
    lv_obj_set_style_text_font(lbl_scan, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_scan);

    ui_status_bar = lv_obj_create(footer);
    lv_obj_set_size(ui_status_bar, 200, 30);
    lv_obj_align(ui_status_bar, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(ui_status_bar, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(ui_status_bar, 0, 0);
    lv_obj_set_style_radius(ui_status_bar, 0, 0);
    lv_obj_set_style_pad_all(ui_status_bar, 2, 0);
    lv_obj_clear_flag(ui_status_bar, LV_OBJ_FLAG_SCROLLABLE);
    update_status_bar();

    // --- Central zone : y=38, h=162px (between header and footer) ---
    const int ZONE_Y = 38;
    const int ZONE_H = 162;

    // --- Panel 1 : saved networks ---
    ui_main_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ui_main_panel, 320, ZONE_H);
    lv_obj_set_pos(ui_main_panel, 0, ZONE_Y);
    lv_obj_set_style_bg_color(ui_main_panel, currentTheme->bg_color, 0);
    lv_obj_set_style_border_width(ui_main_panel, 0, 0);
    lv_obj_set_style_radius(ui_main_panel, 0, 0);
    lv_obj_set_style_pad_all(ui_main_panel, 0, 0);
    lv_obj_set_flex_flow(ui_main_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(ui_main_panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui_main_panel, LV_SCROLLBAR_MODE_ACTIVE);

    lv_obj_t* sec_title = lv_label_create(ui_main_panel);
    lv_label_set_text(sec_title, lang->wifi_saved);
    lv_obj_set_style_text_color(sec_title, currentTheme->text_muted, 0);
    lv_obj_set_style_text_font(sec_title, &lv_font_montserrat_10, 0);
    lv_obj_set_style_pad_left(sec_title, 8, 0);
    lv_obj_set_style_pad_top(sec_title, 4, 0);

    ui_wifi_list = lv_obj_create(ui_main_panel);
    lv_obj_set_size(ui_wifi_list, 320, ZONE_H - 20);
    lv_obj_set_style_bg_color(ui_wifi_list, currentTheme->bg_color, 0);
    lv_obj_set_style_border_width(ui_wifi_list, 0, 0);
    lv_obj_set_style_radius(ui_wifi_list, 0, 0);
    lv_obj_set_style_pad_all(ui_wifi_list, 0, 0);
    lv_obj_set_flex_flow(ui_wifi_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(ui_wifi_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui_wifi_list, LV_SCROLLBAR_MODE_ACTIVE);

    // --- Panel 2 : scan results ---
    ui_scan_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ui_scan_panel, 320, ZONE_H);
    lv_obj_set_pos(ui_scan_panel, 0, ZONE_Y);
    lv_obj_set_style_bg_color(ui_scan_panel, currentTheme->bg_color, 0);
    lv_obj_set_style_border_width(ui_scan_panel, 0, 0);
    lv_obj_set_style_radius(ui_scan_panel, 0, 0);
    lv_obj_set_style_pad_all(ui_scan_panel, 0, 0);
    lv_obj_add_flag(ui_scan_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* scan_hdr = lv_obj_create(ui_scan_panel);
    lv_obj_set_size(scan_hdr, 320, 28);
    lv_obj_align(scan_hdr, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(scan_hdr, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(scan_hdr, 0, 0);
    lv_obj_set_style_radius(scan_hdr, 0, 0);
    lv_obj_set_style_pad_all(scan_hdr, 4, 0);
    lv_obj_clear_flag(scan_hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* scan_title = lv_label_create(scan_hdr);
    lv_label_set_text(scan_title, lang->wifi_available);
    lv_obj_set_style_text_color(scan_title, currentTheme->text_muted, 0);
    lv_obj_set_style_text_font(scan_title, &lv_font_montserrat_10, 0);
    lv_obj_align(scan_title, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t* btn_scan_back = lv_btn_create(scan_hdr);
    lv_obj_set_size(btn_scan_back, 65, 22);
    lv_obj_align(btn_scan_back, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_scan_back, currentTheme->btn_core, 0);
    lv_obj_set_style_border_color(btn_scan_back, currentTheme->border, 0);
    lv_obj_set_style_border_width(btn_scan_back, 1, 0);
    lv_obj_set_style_radius(btn_scan_back, 4, 0);
    lv_obj_add_event_cb(btn_scan_back, btn_retour_main_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_sb = lv_label_create(btn_scan_back);
    lv_label_set_text_fmt(lbl_sb, "%s %s", LV_SYMBOL_LEFT, lang->wifi_back);
    lv_obj_set_style_text_color(lbl_sb, currentTheme->text_muted, 0);
    lv_obj_set_style_text_font(lbl_sb, &lv_font_montserrat_10, 0);
    lv_obj_center(lbl_sb);

    ui_scan_list = lv_obj_create(ui_scan_panel);
    lv_obj_set_size(ui_scan_list, 320, ZONE_H - 30);
    lv_obj_set_pos(ui_scan_list, 0, 28);
    lv_obj_set_style_bg_color(ui_scan_list, currentTheme->bg_color, 0);
    lv_obj_set_style_border_width(ui_scan_list, 0, 0);
    lv_obj_set_style_radius(ui_scan_list, 0, 0);
    lv_obj_set_style_pad_all(ui_scan_list, 0, 0);
    lv_obj_set_flex_flow(ui_scan_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(ui_scan_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui_scan_list, LV_SCROLLBAR_MODE_ACTIVE);

    // --- Panel 3 : password entry (fullscreen) ---
    ui_pwd_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ui_pwd_panel, 320, 240);
    lv_obj_set_pos(ui_pwd_panel, 0, 0);
    lv_obj_set_style_bg_color(ui_pwd_panel, currentTheme->bg_color, 0);
    lv_obj_set_style_border_width(ui_pwd_panel, 0, 0);
    lv_obj_set_style_radius(ui_pwd_panel, 0, 0);
    lv_obj_set_style_pad_all(ui_pwd_panel, 0, 0);
    lv_obj_add_flag(ui_pwd_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pwd_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Mini-header : network name + Cancel button
    lv_obj_t* pwd_hdr = lv_obj_create(ui_pwd_panel);
    lv_obj_set_size(pwd_hdr, 320, 38);
    lv_obj_set_pos(pwd_hdr, 0, 0);
    lv_obj_set_style_bg_color(pwd_hdr, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(pwd_hdr, 0, 0);
    lv_obj_set_style_radius(pwd_hdr, 0, 0);
    lv_obj_set_style_pad_all(pwd_hdr, 4, 0);
    lv_obj_clear_flag(pwd_hdr, LV_OBJ_FLAG_SCROLLABLE);

    ui_pwd_ssid_lbl = lv_label_create(pwd_hdr);
    lv_label_set_text(ui_pwd_ssid_lbl, LV_SYMBOL_WIFI "  ...");
    lv_obj_set_style_text_color(ui_pwd_ssid_lbl, currentTheme->primary, 0);
    lv_obj_set_style_text_font(ui_pwd_ssid_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(ui_pwd_ssid_lbl, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t* btn_pwd_cancel = lv_btn_create(pwd_hdr);
    lv_obj_set_size(btn_pwd_cancel, 75, 28);
    lv_obj_align(btn_pwd_cancel, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_pwd_cancel, currentTheme->btn_core, 0);
    lv_obj_set_style_border_color(btn_pwd_cancel, currentTheme->border, 0);
    lv_obj_set_style_border_width(btn_pwd_cancel, 1, 0);
    lv_obj_set_style_radius(btn_pwd_cancel, 6, 0);
    lv_obj_add_event_cb(btn_pwd_cancel, [](lv_event_t* e) {
        show_panel(ui_scan_panel);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_cancel = lv_label_create(btn_pwd_cancel);
    lv_label_set_text_fmt(lbl_cancel, "%s %s", LV_SYMBOL_LEFT, lang->wifi_back);
    lv_obj_set_style_text_color(lbl_cancel, currentTheme->text_muted, 0);
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_10, 0);
    lv_obj_center(lbl_cancel);

    // Password text area (y=42, h=34)
    ui_pwd_ta = lv_textarea_create(ui_pwd_panel);
    lv_textarea_set_password_mode(ui_pwd_ta, true);
    lv_textarea_set_one_line(ui_pwd_ta, true);
    lv_textarea_set_placeholder_text(ui_pwd_ta, lang->wifi_pwd_placeholder);
    lv_obj_set_size(ui_pwd_ta, 264, 34);
    lv_obj_set_pos(ui_pwd_ta, 6, 42);
    lv_obj_set_style_bg_color(ui_pwd_ta, currentTheme->btn_core, 0);
    lv_obj_set_style_text_color(ui_pwd_ta, currentTheme->text_main, 0);
    lv_obj_set_style_border_color(ui_pwd_ta, currentTheme->primary, 0);
    lv_obj_set_style_border_width(ui_pwd_ta, 2, 0);
    lv_obj_set_style_radius(ui_pwd_ta, 6, 0);
    lv_obj_set_style_pad_ver(ui_pwd_ta, 6, 0);
    lv_obj_set_style_pad_hor(ui_pwd_ta, 8, 0);
    lv_obj_set_style_text_font(ui_pwd_ta, &lv_font_montserrat_14, 0);

    // Show/hide password toggle button (eye icon)
    lv_obj_t* btn_eye = lv_btn_create(ui_pwd_panel);
    lv_obj_set_size(btn_eye, 38, 34);
    lv_obj_set_pos(btn_eye, 276, 42);
    lv_obj_set_style_bg_color(btn_eye, currentTheme->btn_core, 0);
    lv_obj_set_style_border_color(btn_eye, currentTheme->border, 0);
    lv_obj_set_style_border_width(btn_eye, 1, 0);
    lv_obj_set_style_radius(btn_eye, 6, 0);
    lv_obj_clear_flag(btn_eye, LV_OBJ_FLAG_SCROLLABLE);

    ui_pwd_eye_lbl = lv_label_create(btn_eye);
    lv_label_set_text(ui_pwd_eye_lbl, LV_SYMBOL_EYE_CLOSE);
    lv_obj_set_style_text_color(ui_pwd_eye_lbl, currentTheme->text_muted, 0);
    lv_obj_center(ui_pwd_eye_lbl);

    lv_obj_add_event_cb(btn_eye, [](lv_event_t* e) {
        lv_obj_t* label = (lv_obj_t*)lv_event_get_user_data(e);
        if (label == NULL) return;
        bool pwd_mode = lv_textarea_get_password_mode(ui_pwd_ta);
        lv_textarea_set_password_mode(ui_pwd_ta, !pwd_mode);
        if (!pwd_mode) {
            lv_label_set_text(label, LV_SYMBOL_EYE_CLOSE);
            lv_obj_set_style_text_color(label, currentTheme->text_muted, 0);
        } else {
            lv_label_set_text(label, LV_SYMBOL_EYE_OPEN);
            lv_obj_set_style_text_color(label, currentTheme->primary, 0);
        }
    }, LV_EVENT_CLICKED, ui_pwd_eye_lbl);

    // LVGL keyboard (bottom of password panel, h=162)
    ui_kb = lv_keyboard_create(ui_pwd_panel);
    lv_obj_set_size(ui_kb, 320, 162);
    lv_obj_align(ui_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(ui_kb, ui_pwd_ta);
    lv_obj_set_style_bg_color(ui_kb, currentTheme->btn_core, 0);
    lv_obj_add_event_cb(ui_kb, kb_event_cb, LV_EVENT_ALL, NULL);

    // Show the saved networks panel on entry
    refresh_saved_wifi_list();
    show_panel(ui_main_panel);
}
