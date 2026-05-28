// =====================================================
//  CYD-GOLD - Main sketch
//  ESP32-S3 Internet Radio / MP3 Player / Alarm Clock
//
//  Entry points:
//    setup() - hardware init, WiFi, LVGL, home screen
//    loop()  - audio, LEDs, LVGL, WiFi watchdog, sleep
//
//  All UI screens are in ui_*.cpp files.
//  Config persisted in LittleFS /config.bin.
//  Stations loaded from SD /radios.json.
// =====================================================

#include <Arduino.h>
#include "LittleFS.h"
#include "FS.h"
#include "Audio.h"
#include <WiFi.h>
#include <lvgl.h>
#include <WebServer.h>
#include <Update.h>
#include <FastLED.h>
#include "RTClib.h"

#include "config.h"
#include "codec_es8311.h"
#include "display_gui.h"
#include "touch_helper.h"
#include "ui_home.h"
#include "ui_radio.h"
#include "ui_mp3.h"
#include "structures.h"
#include "fonctions.h"
#include "web_server.h"
#include "led_control.h"
#include "ui_lang.h"

Audio audio(true);
RTC_DS3231 rtc;
String current_title = "No playback";

extern lv_obj_t * now_playing_label;
extern lv_obj_t * ui_visualizer_bars[5];
extern bool is_mp3_mode; 
extern bool is_playing;
extern bool manual_pause;
extern void play_next_mp3();
extern void check_alarme(int rtc_h, int rtc_m);
extern void update_radio_stream_title(const char* title);

int currentStation = 0;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf = nullptr; // alloue en PSRAM dans setup()
extern bool buzzer_actif;

// True while WiFi screen is open  suspends auto-reconnect
bool wifi_ui_active = false;

// =====================================================
//  VEILLE PROFONDE (Deep Sleep)
// =====================================================
void enter_deep_sleep() {
    Serial.println("[SLEEP] Entering deep sleep");
    saveConfig();
    audio.stopSong();
    digitalWrite(AMP_EN, HIGH);      // coupe l'ampli
    FastLED.clear(true);             // LEDs eteintes
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    digitalWrite(TFT_BL, !TFT_BL_ON); // ecran eteint
    // No need to stop lvgl_tick_timer  deep sleep kills everything
    delay(200);
   // Configure pull-up (ESP-IDF API, compatible with ESP32 Arduino Core v3.x)
    // Keeps GPIO HIGH during sleep when switch is OFF
    gpio_set_direction((gpio_num_t)SLEEP_BTN_PIN, GPIO_MODE_INPUT);
    gpio_pullup_en((gpio_num_t)SLEEP_BTN_PIN);
    gpio_pulldown_dis((gpio_num_t)SLEEP_BTN_PIN);

    // Hold GPIO state during deep sleep so pull-up stays active
    gpio_hold_en((gpio_num_t)SLEEP_BTN_PIN);

    // EXT1 wake-up: LOW level = switch flipped back to ON
    uint64_t pin_mask = 1ULL << SLEEP_BTN_PIN;
    esp_sleep_enable_ext1_wakeup(pin_mask, ESP_EXT1_WAKEUP_ALL_LOW);
    Serial.flush();
    esp_deep_sleep_start();
}

// LVGL tick  hardware timer at 1ms, independent of loop load
static esp_timer_handle_t lvgl_tick_timer = nullptr;
static void IRAM_ATTR lvgl_tick_cb(void*) { lv_tick_inc(1); }

lv_obj_t * status_label;
lv_obj_t * radio_img;
lv_obj_t * time_label;
lv_obj_t * date_label;
lv_obj_t * wifi_icon_label;
lv_obj_t * batt_icon_label;

// =====================================================
//  CONFIG PERSISTENCE (LittleFS /config.bin)
// =====================================================

Config userConfig = {80, 21, 0}; // Default values

void saveConfig() {
    File f = LittleFS.open("/config.bin", "w");
    if(f) {
        f.write((const uint8_t*)&userConfig, sizeof(userConfig));
        f.close();
        Serial.println("[CONFIG] Saved");
    }
}

bool loadConfig() {
    if (LittleFS.exists("/config.bin")) {
        File f = LittleFS.open("/config.bin", "r");
        if (f) {
            f.read((uint8_t*)&userConfig, sizeof(userConfig));
            f.close();
            Serial.printf("[CONFIG] Loaded  brightness: %d%%\n", userConfig.brightness);
            return true;
        }
        // Fichier existe mais ne s'ouvre pas (FS corrompu ?)
        Serial.println("[CONFIG] File exists but cannot open (FS error?)");
        return false;
    }
    // Fichier absent  premier demarrage normal
    Serial.println("[CONFIG] Not found  using defaults");
    return false;
}

void setDefaultConfig() {
    userConfig.brightness = 70;
    userConfig.volsound = 20;
    userConfig.alarm_h = 7;
    userConfig.alarm_m = 0;
    userConfig.alarm_on = false;
    userConfig.alarm_mode = 0;
    userConfig.alarm_station = 0;
    userConfig.networks_count = 0;
    userConfig.eq_bass = 3;
    userConfig.eq_mid = 1;
    userConfig.eq_treble = 2;
    userConfig.led_r = 255;
    userConfig.led_g = 150;
    userConfig.led_b = 0;
    userConfig.led_bright = 100;
    userConfig.selected_theme = 0;
    userConfig.language = 0;
    // ===== INITIALISATION DU FUSEAU HORAIRE PAR DEFAUT =====
    // On nettoie d'abord la memoire du tableau pour etre super propre
    memset(userConfig.timezone, 0, sizeof(userConfig.timezone));
    // On copie le fuseau de la France par defaut ("CET-1CEST,M3.5.0,M10.5.0/3")
    strncpy(userConfig.timezone, "CET-1CEST,M3.5.0,M10.5.0/3", sizeof(userConfig.timezone) - 1);
    // Tres important : vider les chaines de caracteres
    memset(userConfig.wifi_ssid, 0, sizeof(userConfig.wifi_ssid));
    memset(userConfig.wifi_pass, 0, sizeof(userConfig.wifi_pass));
    // Initialiser le tableau de reseaux connus
    for(int i = 0; i < 10; i++) {
        memset(userConfig.known_networks[i].ssid, 0, 32);
        memset(userConfig.known_networks[i].pass, 0, 64);
    }
    Serial.println("[CONFIG] Defaults applied");
}

// =====================================================
//  WIFI HELPERS
// =====================================================
void add_or_update_network(const char* ssid, const char* pass) {
    int index = -1;
    
    // Check if this SSID is already in the list
    for(int i = 0; i < userConfig.networks_count; i++) {
        if(strcmp(userConfig.known_networks[i].ssid, ssid) == 0) {
            index = i; // Trouve !
            break;
        }
    }

    // Not found and list has room  add it
    if(index == -1 && userConfig.networks_count < 5) {
        index = userConfig.networks_count;
        userConfig.networks_count++;
        Serial.println("[WIFI] New network saved");
    }

    // Write credentials to the slot
    if(index != -1) {
        strncpy(userConfig.known_networks[index].ssid, ssid, 31);
        userConfig.known_networks[index].ssid[31] = '\0';
        
        strncpy(userConfig.known_networks[index].pass, pass, 63);
        userConfig.known_networks[index].pass[63] = '\0';
        
        saveConfig(); 
        Serial.printf("[WIFI] Saved at slot %d\n", index);
    } else {
        Serial.println("[WIFI] Network list full (max 10)");
    }
}

void sync_rtc_from_ntp(); // forward declaration
void start_wifi_connect() {
    // Show connecting status on screen
    lv_obj_clean(ui_wifi_list);
    lv_list_add_text(ui_wifi_list, "Connecting...");
    lv_list_add_text(ui_wifi_list, userConfig.wifi_ssid);
    lv_timer_handler(); 

    Serial.printf("[WIFI] Trying: %s\n", userConfig.wifi_ssid);

    // Attempt connection
    WiFi.disconnect();
    WiFi.begin(userConfig.wifi_ssid, userConfig.wifi_pass);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
        lv_timer_handler();
        delay(500);
        Serial.print(".");
        retry++;
    }

    // Handle result
    lv_obj_clean(ui_wifi_list);
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WIFI] Connected");
        lv_list_add_text(ui_wifi_list, "Connected!");

        sync_rtc_from_ntp(); // sync NTP  DS3231 (meme fonction que le boot)

        add_or_update_network(userConfig.wifi_ssid, userConfig.wifi_pass);

        lv_obj_t * btn = lv_list_add_btn(ui_wifi_list, LV_SYMBOL_OK, userConfig.wifi_ssid);
        lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_GREEN), 0);
    } 

    else {
        Serial.println("\n[WIFI] Failed");
        lv_list_add_text(ui_wifi_list, "Connection failed");
        lv_list_add_text(ui_wifi_list, "Check your password");
    }
}

// =====================================================
//  AUDIO LIBRARY CALLBACKS
// =====================================================
void audio_showstreamtitle(const char *info) {
    Serial.printf("[RADIO] Stream title : %s\n", info);
    current_title = "[RADIO] " + String(info);
    update_radio_stream_title(info);
}

void audio_showstation(const char *info) {
    Serial.printf("[RADIO] Station : %s\n", info);
}

// Stream error  called by ESP32-audioI2S on connection failure
void audio_error_mp3(const char *info) {
    Serial.printf("[RADIO] Erreur flux : %s\n", info);
    // Mark as stopped so the watchdog does not retry on a dead stream
    is_playing = false;
    current_title = String(lang->err_stream) + " : " + String(info);
    update_radio_stream_title(current_title.c_str());
}

// Stream connected  logged for debug
void audio_connected(const char *info) {
    Serial.printf("[RADIO] Connecte : %s\n", info);
}
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    ts.read();
    if (ts.isTouched) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = ts.points[0].x;
        data->point.y = ts.points[0].y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// Indique si le RTC DS3231 a ete trouve au boot (evite d'interroger un RTC absent)
static bool rtc_ok = false;

// Applique le fuseau horaire POSIX  appele une seule fois au boot et si l'user
// change le fuseau dans les reglages. Jamais dans le loop.
void apply_timezone() {
    setenv("TZ", userConfig.timezone, 1);
    tzset();
    Serial.printf("[TZ] Applied: %s\n", userConfig.timezone);
}

// Resynchronise le DS3231 via NTP si le WiFi est disponible.
// Appele au boot apres connexion WiFi, et periodiquement (1x/jour).
void sync_rtc_from_ntp() {
    if (WiFi.status() != WL_CONNECTED) return;

    configTzTime(userConfig.timezone, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 10) {
        delay(500);
        retry++;
    }
    if (retry < 10) {
        time_t now_utc;
        time(&now_utc);
        struct tm utc_tm;
        gmtime_r(&now_utc, &utc_tm);
        rtc.adjust(DateTime(utc_tm.tm_year + 1900, utc_tm.tm_mon + 1, utc_tm.tm_mday,
                            utc_tm.tm_hour, utc_tm.tm_min, utc_tm.tm_sec));
        Serial.println("[NTP] DS3231 synced via NTP (UTC)");
    } else {
        Serial.println("[NTP] Timeout  RTC keeps its time");
    }
}

// --- fonction mise a jour donnees ---
void update_time(bool force) {
    static uint32_t last_check    = 0;
    static uint32_t last_ntp_sync = 0; 

    if (!force && (millis() - last_check < 4000)) return;
    last_check = millis();

    // ===== BUZZER DE SECOURS =====
    if (buzzer_actif && !audio.isRunning()) {
        Serial.println("[Securite] Relance du buzzer...");
        audio.connecttoFS(SD_MMC, "/buzzer.mp3");
    }

    // ===== WATCHDOG FLUX RADIO =====
    // Si on est en mode radio, que is_playing=true mais que l'audio
    // ne tourne plus (WiFi coupe, flux mort, timeout)  tentative de
    // reconnexion automatique toutes les 10s maximum.
    // Evite le crash silencieux quand la connexion 5G du telephone flanche.
    static uint32_t last_radio_retry = 0;
    if (!is_mp3_mode && is_playing && !buzzer_actif && !audio.isRunning()) {
        if (millis() - last_radio_retry > 10000) {
            last_radio_retry = millis();
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[RADIO] Flux perdu, reconnexion station %d...\n", currentStation);
                current_title = "Reconnexion...";
                update_radio_stream_title("Reconnexion...");
                audio.connecttohost(STATIONS[currentStation].url.c_str());
            } else {
                // No WiFi  stop cleanly without infinite retry
                Serial.println("[RADIO] Flux perdu + pas de WiFi, arret.");
                is_playing = false;
                current_title = String(lang->wifi_lost);
                update_radio_stream_title(lang->wifi_lost);
            }
        }
    }

    // ===== RESYNC NTP QUOTIDIENNE (toutes les 24h si WiFi dispo) =====
    if (WiFi.status() == WL_CONNECTED &&
        (last_ntp_sync == 0 || millis() - last_ntp_sync > 86400000UL)) {
        sync_rtc_from_ntp();
        last_ntp_sync = millis();
    }

    // ===== LECTURE RTC =====
    // Si le DS3231 n'a pas ete detecte au boot, on n'essaie pas de le lire
    if (!rtc_ok) {
        if (time_label != NULL && lv_obj_is_valid(time_label))
            lv_label_set_text(time_label, "--:--");
        return;
    }

    DateTime now = rtc.now();

    // Sanite basique : l'annee doit etre plausible (DS3231 non initialise = 2000)
    if (now.year() < 2020) {
        Serial.println("[RTC] Not initialized (year < 2020)  waiting for NTP sync");
        if (time_label != NULL && lv_obj_is_valid(time_label))
            lv_label_set_text(time_label, "--:--");
        return;
    }

    // Conversion UTC  heure locale (fuseau deja applique une fois au boot)
    time_t utc_epoch = now.unixtime();
    struct tm local_tm;
    localtime_r(&utc_epoch, &local_tm);

    int heures       = local_tm.tm_hour;
    int minutes      = local_tm.tm_min;
    int jour_semaine = local_tm.tm_wday;
    int jour_mois    = local_tm.tm_mday;
    int num_mois     = local_tm.tm_mon + 1;

    // ===== AFFICHAGE HEURE =====
    if (time_label != NULL && lv_obj_is_valid(time_label))
        lv_label_set_text_fmt(time_label, "%02d:%02d", heures, minutes);

    check_alarme(heures, minutes);

    // ===== AFFICHAGE DATE =====
    if (date_label != NULL && lv_obj_is_valid(date_label)) {
        int im = constrain(num_mois - 1, 0, 11);
        int ij = constrain(jour_semaine, 0, 6);
        lv_label_set_text_fmt(date_label, "%s %d %s",
            lang->days[ij], jour_mois, lang->months[im]);
    }

    // ===== ICNE WIFI =====
    if (wifi_icon_label != NULL && lv_obj_is_valid(wifi_icon_label)) {
        if (WiFi.status() == WL_CONNECTED) {
            lv_label_set_text(wifi_icon_label, LV_SYMBOL_WIFI);
            lv_obj_set_style_text_color(wifi_icon_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else {
            lv_label_set_text(wifi_icon_label, LV_SYMBOL_CLOSE);
            lv_obj_set_style_text_color(wifi_icon_label, lv_palette_main(LV_PALETTE_RED), 0);
        }
    }

    // ===== ICNE BATTERIE =====
    if (batt_icon_label != NULL && lv_obj_is_valid(batt_icon_label)) {
        int bat = get_battery_level();
        if      (bat > 80) lv_label_set_text(batt_icon_label, LV_SYMBOL_BATTERY_FULL);
        else if (bat > 50) lv_label_set_text(batt_icon_label, LV_SYMBOL_BATTERY_3);
        else if (bat > 20) lv_label_set_text(batt_icon_label, LV_SYMBOL_BATTERY_1);
        else               lv_label_set_text(batt_icon_label, LV_SYMBOL_BATTERY_EMPTY);
        lv_obj_set_style_text_color(batt_icon_label,
            bat < 20 ? lv_palette_main(LV_PALETTE_RED) : currentTheme->text_muted, 0);
    }
}

void audio_eof_mp3(const char *info) {
    Serial.printf("[MP3] Track ended: %s\n", info);
    if (buzzer_actif && strcmp(info, "/buzzer.mp3") == 0) {
        Serial.println("[ALARM] Buzzer looping");
        audio.connecttoFS(SD_MMC, "/buzzer.mp3"); 
    }
}

void setup() {
    gpio_hold_dis((gpio_num_t)2);
    Serial.begin(115200);
    pinMode(BAT_ADC_PIN, INPUT);

    setDefaultConfig();

    setup_leds(); 
    boot_led_step(0);

    // --- Hardware and file system init ---
    setup_display();
    if (setup_i2c_and_codec() == ESP_OK) {
        Serial.println("[CODEC] OK");
        codec_volume(77);
    } else {
        Serial.println("[CODEC] ERROR");
    }

    pinMode(AMP_EN, OUTPUT);
    digitalWrite(AMP_EN, HIGH);

    // Deep-sleep switch  INPUT_PULLUP: rest=HIGH, on=LOW
    pinMode(SLEEP_BTN_PIN, INPUT_PULLUP);
    setup_touch();

    boot_led_step(1);

    if(!LittleFS.begin(true)){ 
        Serial.println("[FS] LittleFS mount failed");
    } else {
        // First boot or formatted FS  write defaults and restart
            if (!LittleFS.exists("/config.bin")) {
                Serial.println("[FS] First boot  writing defaults");
                setDefaultConfig();
                saveConfig();
                ESP.restart(); 
                }
    loadConfig();
    }
    

    // Apply saved brightness and LED settings immediately 
    analogWrite(TFT_BL, map(userConfig.brightness, 0, 100, 0, 255));
    FastLED.setBrightness(userConfig.led_bright);
    set_led_mode(MODE_FIXE);
    FastLED.show();
    apply_language(userConfig.language);

    boot_led_step(2);
    
    setup_sd_card();
    loadStationsFromSD();

    boot_led_step(3);
    
    // --- RTC DS3231 ---
    rtc_ok = rtc.begin();
    if (!rtc_ok) {
        Serial.println("[RTC] DS3231 not found  clock disabled");
    } else {
        Serial.println("[RTC] DS3231 found");
    }

    // Fuseau horaire applique
    apply_timezone();

    // --- WiFi AUTOMATIQUE ---
    if (strlen(userConfig.wifi_ssid) > 0) {
        Serial.printf("[WIFI] Auto-connect: %s\n", userConfig.wifi_ssid);
        WiFi.disconnect();
        WiFi.begin(userConfig.wifi_ssid, userConfig.wifi_pass);

        int retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < 20) {
            delay(500);
            Serial.print(".");
            retry++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n[WIFI] Connected");
            Serial.println(WiFi.localIP());
            sync_rtc_from_ntp(); // sync NTP  DS3231 (fonction commune)
        } else {
            Serial.println("\n[WIFI] Failed  RTC keeps its time");
        }
    } else {
        Serial.println("[WIFI] No saved network  offline mode");
    }

    boot_led_step(4);
    
    if (WiFi.status() == WL_CONNECTED) {
        setup_web_server();
    }
    
    boot_led_step(5);

    lv_init();

    // Hardware timer  1ms LVGL tick (independent of loop)
    const esp_timer_create_args_t tick_args = {
        .callback        = lvgl_tick_cb,
        .arg             = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "lvgl_tick"
    };
    esp_timer_create(&tick_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, 1000); // 1000 s = 1 ms
    Serial.println("[LVGL] 1ms hardware tick started");
    
    // --- Configuration Audio ---
    audio.setPinout(I2S_BCK, I2S_WS, I2S_DOUT, I2S_MCK);
    audio.setVolume(userConfig.volsound);
    audio.setTone(userConfig.eq_bass, userConfig.eq_mid, userConfig.eq_treble);

    boot_led_step(6);

    // --- LVGL display and touch drivers ---
    buf = (lv_color_t*) heap_caps_malloc(320 * 40 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!buf) {
        Serial.println("[LVGL] PSRAM unavailable  fallback to SRAM");
        buf = (lv_color_t*) heap_caps_malloc(320 * 10 * sizeof(lv_color_t), MALLOC_CAP_DEFAULT);
    } else {
        Serial.println("[LVGL] Buffer allocated in PSRAM (40 rows)");
    }
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, 320 * 40);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_t * my_indev = lv_indev_drv_register(&indev_drv);
    lv_fs_sd_init();

    // --- Apply theme and launch home screen ---
    if (userConfig.selected_theme < 0 || userConfig.selected_theme >= NOMBRE_THEMES) {
        userConfig.selected_theme = 0; 
    }
    currentTheme = listeThemes[userConfig.selected_theme];
    Serial.print("[THEME] Applied: ");
    Serial.println(userConfig.selected_theme);
    update_leds_color(userConfig.led_r, userConfig.led_g, userConfig.led_b);
    digitalWrite(AMP_EN, LOW);
    setup_home_screen();
    lv_timer_handler();
}

void loop() {
    audio.loop();
    loop_leds();
    lv_timer_handler();
    
    // ===== RECONNEXION WIFI AUTOMATIQUE =====
    static uint32_t last_wifi_check = 0;
    static bool     was_connected   = false;
    if (millis() - last_wifi_check > 30000) {
        last_wifi_check = millis();
        if (WiFi.status() != WL_CONNECTED) {
            if (was_connected) {
                // On vient de perdre la connexion  arret propre du flux radio
                Serial.println("[WIFI] Lost  stopping radio stream");
                if (!is_mp3_mode && is_playing) {
                    audio.stopSong();
                    current_title = String(lang->wifi_lost);
                }
                was_connected = false;
            }
            // Try all known networks in order
            bool reconnected = false;
            for (int i = 0; i < userConfig.networks_count && !reconnected; i++) {
                if (strlen(userConfig.known_networks[i].ssid) == 0) continue;
                Serial.printf("[WIFI] Retrying: %s\n", userConfig.known_networks[i].ssid);
                WiFi.disconnect();
                WiFi.begin(userConfig.known_networks[i].ssid,
                           userConfig.known_networks[i].pass);
                int retry = 0;
                while (WiFi.status() != WL_CONNECTED && retry < 10) {
                    delay(500);
                    retry++;
                }
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.printf("[WIFI] Reconnected: %s\n", userConfig.known_networks[i].ssid);
                    // Update last-used network
                    strncpy(userConfig.wifi_ssid, userConfig.known_networks[i].ssid, 31);
                    strncpy(userConfig.wifi_pass, userConfig.known_networks[i].pass, 63);
                    sync_rtc_from_ntp();
                    reconnected  = true;
                    was_connected = true;
                }
            }
            if (!reconnected) {
                Serial.println("[WIFI] No known network available");
            }
        } else {
            was_connected = true;
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        server.handleClient();
    }

    // ===== INTERRUPTEUR VEILLE PROFONDE (glissiere) =====
    {
        static uint32_t last_sleep_check = 0;
        if (millis() - last_sleep_check > 200) {
            last_sleep_check = millis();
            if (digitalRead(SLEEP_BTN_PIN) == HIGH) {
                enter_deep_sleep(); // ne revient jamais ici
            }
        }
    }

    // Update now-playing label on home screen
    static uint32_t last_ui_update = 0;
    static String last_title = "";
    if (millis() - last_ui_update > 2000) {
        last_ui_update = millis();
        if (now_playing_label != NULL && current_title != last_title) {
            lv_label_set_text(now_playing_label, current_title.c_str());
            last_title = current_title;
        }
        update_time(false);
    }

    // Auto-advance to next track when MP3 ends
    if (is_playing && !manual_pause) {
        if (!audio.isRunning()) {
            // MP3 mode  advance to next track
            if (is_mp3_mode) {
                Serial.println("[MP3] Track ended  next");
                play_next_mp3();
            } else {
                // Radio stopped  handled by watchdog in update_time()
            }
        }
    }
  
     // Audio visualizer bars animation
    static uint32_t last_anim = 0;
    static bool bars_reset = false;
    if (is_playing && !manual_pause) {
        bars_reset = false;
        if (millis() - last_anim > 120) { 
            last_anim = millis();
            for (int i = 0; i < 5; i++) {
                if (ui_visualizer_bars != NULL && ui_visualizer_bars[i] != NULL) {
                    int h = random(5, 35);
                    lv_obj_set_height(ui_visualizer_bars[i], h);
                    lv_obj_set_y(ui_visualizer_bars[i], 40 - h); 
                }
            }
        }
    } else {

    // Reset bars to minimum when stopped (once)
        if (!bars_reset) {
            for (int i = 0; i < 5; i++) {
                if (ui_visualizer_bars != NULL && ui_visualizer_bars[i] != NULL) {
                    lv_obj_set_height(ui_visualizer_bars[i], 5);
                    lv_obj_set_y(ui_visualizer_bars[i], 35);
                }
            }
            bars_reset = true;
        }
    }
}