#include "ui_mp3.h"
#include "config.h"
#include "Audio.h"
#include <lvgl.h>
#include "SD_MMC.h"
#include "structures.h"
#include "ui_lang.h"

// =====================================================
//  MP3 PLAYER SCREEN  (320 x 240)
//
//  Scans /mp3 on the SD card and builds a scrollable
//  track list. Supports play/pause, prev/next, shuffle.
//  Progress bar updated every 500ms via LVGL timer.
//  play_next_mp3() is public — called by audio_eof_mp3
//  callback and by the alarm system.
// =====================================================

extern Audio       audio;
extern lv_obj_t*   time_label;
extern String      current_title;
extern RadioTheme* currentTheme;

// --- UI object references ---
lv_obj_t*        ui_mp3_list      = NULL; // exported
static lv_obj_t* ui_btn_play_pause = NULL;
static lv_obj_t* ui_lbl_play_pause = NULL;
static lv_obj_t* ui_bar_progress   = NULL;
static lv_obj_t* ui_lbl_title      = NULL;
static lv_obj_t* ui_lbl_shuffle    = NULL;
static lv_obj_t* ui_btn_shuffle    = NULL;

// --- Playback state ---
bool is_mp3_mode       = false;
bool manual_pause      = false;
bool is_playing        = false;
static bool shuffle_mode       = false;
static int  current_file_index = 0;
static int  total_files_count  = 0;
static String file_names[100]; // max 100 MP3 files

// --- Progress bar LVGL timer ---
static lv_timer_t* progress_timer = NULL;

// =====================================================
//  HELPERS
// =====================================================

// Highlight the currently playing row in the track list
static void highlight_current_track() {
    if (ui_mp3_list == NULL) return;
    uint32_t count = lv_obj_get_child_cnt(ui_mp3_list);
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t* btn = lv_obj_get_child(ui_mp3_list, i);
        if ((int)i == current_file_index) {
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

// Update the title label — truncate to 28 chars to fit 320px
static void update_title_label(const String& name) {
    if (ui_lbl_title == NULL) return;
    String display = (name.length() > 28) ? name.substring(0, 26) + ".." : name;
    lv_label_set_text(ui_lbl_title, display.c_str());
}

// Start playing the file at the given index
static void play_file_by_index(int index) {
    if (total_files_count == 0) return;
    current_file_index = index;

    String filename = file_names[index];
    String nameOnly = filename;
    int dot = nameOnly.lastIndexOf('.');
    if (dot != -1) nameOnly = nameOnly.substring(0, dot);
    current_title = "MP3: " + nameOnly;

    char path[128];
    snprintf(path, sizeof(path), "/mp3/%s", filename.c_str());
    Serial.printf("[MP3] Playing: %s\n", path);
    audio.connecttoFS(SD_MMC, path);

    is_playing   = true;
    is_mp3_mode  = true;
    manual_pause = false;

    if (ui_lbl_play_pause != NULL && lv_obj_is_valid(ui_btn_play_pause)) {
        lv_label_set_text(ui_lbl_play_pause, LV_SYMBOL_PAUSE);
        lv_obj_set_style_bg_color(ui_btn_play_pause, lv_palette_main(LV_PALETTE_YELLOW), 0);
    }
    if (ui_bar_progress != NULL && lv_obj_is_valid(ui_bar_progress))
        lv_bar_set_value(ui_bar_progress, 0, LV_ANIM_OFF);

    update_title_label(nameOnly);
    highlight_current_track();
}

// =====================================================
//  TIMER — progress bar update (every 500ms)
// =====================================================

static void progress_timer_cb(lv_timer_t* t) {
    if (!is_playing || manual_pause) return;
    if (ui_bar_progress == NULL || !lv_obj_is_valid(ui_bar_progress)) return;

    uint32_t current_sec  = audio.getAudioCurrentTime();
    uint32_t duration_sec = audio.getAudioFileDuration();

    if (duration_sec > 0) {
        int pct = (int)((current_sec * 100UL) / duration_sec);
        lv_bar_set_value(ui_bar_progress, constrain(pct, 0, 100), LV_ANIM_ON);
    }
}

// =====================================================
//  CALLBACKS
// =====================================================

// Return to home — stop timer and reset all UI pointers
static void switch_to_home_async(void* p) {
    if (progress_timer != NULL) {
        lv_timer_del(progress_timer);
        progress_timer = NULL;
    }
    time_label        = NULL;
    ui_mp3_list       = NULL;
    ui_btn_play_pause = NULL;
    ui_lbl_play_pause = NULL;
    ui_bar_progress   = NULL;
    ui_lbl_title      = NULL;
    ui_btn_shuffle    = NULL;
    ui_lbl_shuffle    = NULL;
    lv_obj_clean(lv_scr_act());
    setup_home_screen();
}

static void back_to_home_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        lv_async_call(switch_to_home_async, NULL);
}

// Play/Pause toggle
static void btn_play_pause_cb(lv_event_t* e) {
    if (!is_playing) return;
    audio.pauseResume();
    manual_pause = !manual_pause;
    if (manual_pause) {
        lv_label_set_text(ui_lbl_play_pause, LV_SYMBOL_PLAY);
        lv_obj_set_style_bg_color(lv_event_get_target(e), currentTheme->primary, 0);
    } else {
        lv_label_set_text(ui_lbl_play_pause, LV_SYMBOL_PAUSE);
        lv_obj_set_style_bg_color(lv_event_get_target(e), lv_palette_main(LV_PALETTE_YELLOW), 0);
    }
}

// Shuffle toggle
static void btn_shuffle_cb(lv_event_t* e) {
    shuffle_mode = !shuffle_mode;
    if (ui_lbl_shuffle != NULL) {
        if (shuffle_mode) {
            lv_obj_set_style_bg_color(lv_event_get_target(e), currentTheme->primary, 0);
            lv_obj_set_style_text_color(ui_lbl_shuffle, currentTheme->bg_color, 0);
        } else {
            lv_obj_set_style_bg_color(lv_event_get_target(e), currentTheme->btn_core, 0);
            lv_obj_set_style_text_color(ui_lbl_shuffle, currentTheme->text_muted, 0);
        }
    }
    Serial.printf("[MP3] Shuffle: %s\n", shuffle_mode ? "ON" : "OFF");
}

// Tap a track row
static void play_file_event_cb(lv_event_t* e) {
    play_file_by_index(lv_obj_get_index(lv_event_get_target(e)));
}

// =====================================================
//  PUBLIC — next/prev track
// =====================================================

// Play next track (sequential or random).
// Called by audio_eof_mp3 callback and alarm system.
void play_next_mp3() {
    if (total_files_count == 0) return;
    int next;
    if (shuffle_mode) {
        do { next = random(0, total_files_count); }
        while (total_files_count > 1 && next == current_file_index);
    } else {
        next = (current_file_index + 1) % total_files_count;
    }
    play_file_by_index(next);
}

static void play_prev_mp3() {
    if (total_files_count == 0) return;
    play_file_by_index((current_file_index - 1 + total_files_count) % total_files_count);
}

// =====================================================
//  SCREEN LAYOUT  (320 x 240)
//
//  ┌────────────────────────────────────┐
//  │ ♪  -- track title --               │  <- title bar 40px
//  ├────────────────────────────────────┤  <- progress bar 6px
//  │                                    │
//  │  track list (up to 100 MP3 files)  │  <- list 132px, y=52
//  │                                    │
//  ├────────────────────────────────────┤
//  │ [Shuf] [Prev] [ Play ] [Next] [Hom]│  <- control bar 50px
//  └────────────────────────────────────┘
// =====================================================
void setup_mp3_screen() {
    lv_obj_clean(lv_scr_act());
    time_label  = NULL;
    is_mp3_mode = true;
    lv_obj_set_style_bg_color(lv_scr_act(), currentTheme->bg_color, 0);

    // --- Title bar (40px) ---
    lv_obj_t* title_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(title_bar, 320, 40);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 0, 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* ico = lv_label_create(title_bar);
    lv_label_set_text(ico, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_color(ico, currentTheme->primary, 0);
    lv_obj_set_style_text_font(ico, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(ico, 8, 12);

    ui_lbl_title = lv_label_create(title_bar);
    lv_label_set_text(ui_lbl_title, lang->mp3_select_file);
    lv_obj_set_style_text_font(ui_lbl_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_lbl_title, currentTheme->text_main, 0);
    lv_obj_set_width(ui_lbl_title, 270);
    lv_label_set_long_mode(ui_lbl_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_pos(ui_lbl_title, 28, 12);

    // --- Progress bar (6px, y=40) ---
    ui_bar_progress = lv_bar_create(lv_scr_act());
    lv_obj_set_size(ui_bar_progress, 300, 6);
    lv_obj_set_pos(ui_bar_progress, 10, 40);
    lv_bar_set_range(ui_bar_progress, 0, 100);
    lv_bar_set_value(ui_bar_progress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui_bar_progress, currentTheme->btn_core, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_bar_progress, currentTheme->primary, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ui_bar_progress, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_bar_progress, 3, LV_PART_INDICATOR);

    // --- Track list (132px, y=52) ---
    ui_mp3_list = lv_list_create(lv_scr_act());
    lv_obj_set_size(ui_mp3_list, 300, 132);
    lv_obj_set_pos(ui_mp3_list, 10, 52);
    lv_obj_set_style_radius(ui_mp3_list, 8, 0);
    lv_obj_set_style_border_width(ui_mp3_list, 1, 0);
    lv_obj_set_style_border_color(ui_mp3_list, currentTheme->border, 0);
    lv_obj_set_style_bg_color(ui_mp3_list, currentTheme->btn_core, 0);
    lv_obj_set_style_text_color(ui_mp3_list, currentTheme->text_main, 0);
    lv_obj_set_style_text_font(ui_mp3_list, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_row(ui_mp3_list, 2, 0);

    // Scan /mp3 on SD
    total_files_count = 0;
    File root = SD_MMC.open("/mp3");

    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file && total_files_count < 100) {
            if (!file.isDirectory()) {
                String fname   = String(file.name());
                String display = fname;
                int dot = display.lastIndexOf('.');
                if (dot != -1) display = display.substring(0, dot);

                file_names[total_files_count] = fname;
                lv_obj_t* btn = lv_list_add_btn(ui_mp3_list, LV_SYMBOL_AUDIO, display.c_str());
                lv_obj_set_style_bg_color(btn, currentTheme->btn_core, 0);
                lv_obj_set_style_text_color(btn, currentTheme->text_main, 0);
                lv_obj_set_style_bg_color(btn, currentTheme->primary, LV_STATE_PRESSED);
                lv_obj_set_style_min_height(btn, 28, 0);
                lv_obj_add_event_cb(btn, play_file_event_cb, LV_EVENT_CLICKED, NULL);
                total_files_count++;
            }
            file = root.openNextFile();
        }
        if (total_files_count == 0) {
            lv_obj_t* t1 = lv_list_add_text(ui_mp3_list, lang->mp3_no_file);
            lv_obj_set_style_text_color(t1, currentTheme->text_muted, 0);
            lv_obj_t* t2 = lv_list_add_text(ui_mp3_list, lang->mp3_create_folder);
            lv_obj_set_style_text_color(t2, currentTheme->text_muted, 0);
        }
    } else {
        lv_obj_t* t1 = lv_list_add_text(ui_mp3_list,
            (String(LV_SYMBOL_WARNING " ") + lang->mp3_err_sd).c_str());
        lv_obj_set_style_text_color(t1, lv_palette_main(LV_PALETTE_RED), 0);
        lv_list_add_text(ui_mp3_list, lang->mp3_create_folder);
    }

    // --- Control bar (50px, y=190) ---
    lv_obj_t* ctrl_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ctrl_bar, 320, 50);
    lv_obj_set_pos(ctrl_bar, 0, 190);
    lv_obj_set_style_bg_color(ctrl_bar, currentTheme->btn_core, 0);
    lv_obj_set_style_border_width(ctrl_bar, 0, 0);
    lv_obj_set_style_radius(ctrl_bar, 0, 0);
    lv_obj_set_style_pad_all(ctrl_bar, 0, 0);
    lv_obj_clear_flag(ctrl_bar, LV_OBJ_FLAG_SCROLLABLE);

    // 1px separator above control bar
    lv_obj_t* sep = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sep, 320, 1);
    lv_obj_set_pos(sep, 0, 189);
    lv_obj_set_style_bg_color(sep, currentTheme->border, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);

    // Button layout : [Shuf] [Prev] [Play] [Next] [Home]
    // Total width : 5*44 + 4*10 = 260px → x start = (320-260)/2 = 30
    const int BTN_W   = 44;
    const int BTN_H   = 36;
    const int BTN_GAP = 10;
    const int BAR_Y   = 197;
    const int start_x = (320 - (5 * BTN_W + 4 * BTN_GAP)) / 2;

    // Helper : create an icon button at position x
    auto make_btn = [&](int x, const char* icon, lv_color_t bg) -> lv_obj_t* {
        lv_obj_t* btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(btn, BTN_W, BTN_H);
        lv_obj_set_pos(btn, x, BAR_Y);
        lv_obj_set_style_bg_color(btn, bg, 0);
        lv_obj_set_style_bg_color(btn, currentTheme->primary, LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, icon);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, currentTheme->text_main, 0);
        lv_obj_center(lbl);
        return btn;
    };

    int bx = start_x;

    // Shuffle
    ui_btn_shuffle = make_btn(bx, LV_SYMBOL_SHUFFLE, currentTheme->btn_core);
    ui_lbl_shuffle = lv_obj_get_child(ui_btn_shuffle, 0);
    lv_obj_set_style_text_color(ui_lbl_shuffle, currentTheme->text_muted, 0);
    lv_obj_add_event_cb(ui_btn_shuffle, btn_shuffle_cb, LV_EVENT_CLICKED, NULL);
    bx += BTN_W + BTN_GAP;

    // Previous
    lv_obj_t* btn_prev = make_btn(bx, LV_SYMBOL_PREV, currentTheme->primary);
    lv_obj_add_event_cb(btn_prev, [](lv_event_t* e){ play_prev_mp3(); }, LV_EVENT_CLICKED, NULL);
    bx += BTN_W + BTN_GAP;

    // Play/Pause (slightly larger to stand out)
    ui_btn_play_pause = make_btn(bx, LV_SYMBOL_PLAY, currentTheme->primary);
    lv_obj_set_size(ui_btn_play_pause, BTN_W + 4, BTN_H + 2);
    lv_obj_set_pos(ui_btn_play_pause, bx - 2, BAR_Y - 1);
    ui_lbl_play_pause = lv_obj_get_child(ui_btn_play_pause, 0);
    lv_obj_add_event_cb(ui_btn_play_pause, btn_play_pause_cb, LV_EVENT_CLICKED, NULL);
    bx += BTN_W + BTN_GAP;

    // Next
    lv_obj_t* btn_next = make_btn(bx, LV_SYMBOL_NEXT, currentTheme->primary);
    lv_obj_add_event_cb(btn_next, [](lv_event_t* e){ play_next_mp3(); }, LV_EVENT_CLICKED, NULL);
    bx += BTN_W + BTN_GAP;

    // Home
    lv_obj_t* btn_back = make_btn(bx, LV_SYMBOL_HOME, currentTheme->primary);
    lv_obj_add_event_cb(btn_back, back_to_home_cb, LV_EVENT_CLICKED, NULL);

    // Start progress timer
    if (progress_timer != NULL) lv_timer_del(progress_timer);
    progress_timer = lv_timer_create(progress_timer_cb, 500, NULL);
}
