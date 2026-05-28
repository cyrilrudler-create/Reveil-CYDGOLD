#ifndef DISPLAY_GUI_H
#define DISPLAY_GUI_H

#include <TFT_eSPI.h>
#include "SD_MMC.h"
#include "structures.h"
#include "config.h"
#include "logo_def.h"
#include "buzzer_data.h"

TFT_eSPI tft = TFT_eSPI();


void setup_display() {
    tft.init();
    tft.setRotation(1); // Landscape mode
    tft.fillScreen(TFT_BLACK);
    tft.invertDisplay(true);
    
    // Enable backlight
    pinMode(TFT_BL, OUTPUT);
    analogWrite(TFT_BL, map(userConfig.brightness, 0, 100, 0, 255));
}

void setup_sd_card() {
    // 1. Configure SDMMC pins
    if(!SD_MMC.setPins(SD_SCK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3)){
        Serial.println("[SD] Pin config failed");
        return;
    }

    // 2. Mount SD (mount point: /sd)
    if(!SD_MMC.begin("/sd", false)){
        Serial.println("[SD] Card not found");
        return;
    }
    
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("[SD] Card ready: %llu MB (4-bit mode)\n", cardSize);

    Serial.println("[SD] Checking system folders...");

    // Ensure required folders exist
    const char* folders[] = {"/mp3", "/logos"};
    
    for(const char* folder : folders) {
        if (!SD_MMC.exists(folder)) {
            Serial.printf("[SD] Creating folder: %s\n", folder);
            SD_MMC.mkdir(folder);
        }
    }

    // Restore buzzer.mp3 from PROGMEM if missing
    if (!SD_MMC.exists("/buzzer.mp3")) {
        Serial.println("[SD] Restoring buzzer.mp3 from PROGMEM...");
        File f_buzz = SD_MMC.open("/buzzer.mp3", FILE_WRITE);
        if (f_buzz) {
            uint8_t buf[64];
            unsigned int count = 0;
            
            while (count < default_buzzer_len) {
                unsigned int chunk = default_buzzer_len - count;
                if (chunk > 64) chunk = 64;
                
                // Safe copy from PROGMEM to RAM
                memcpy_P(buf, default_buzzer_mp3 + count, chunk);
                // Write chunk to SD
                f_buzz.write(buf, chunk);
                
                count += chunk;
            }
            f_buzz.flush();
            f_buzz.close();
            delay(200);
            Serial.println("[SD] buzzer.mp3 installed.");
        } else {
            Serial.println("[SD] Failed to create buzzer.mp3");
        }
    }
    // Restore default logo from PROGMEM if missing
    if (!SD_MMC.exists("/logos/def.bin")) {
        Serial.println("[SD] Restoring def.bin from PROGMEM...");
        File f_log = SD_MMC.open("/logos/def.bin", FILE_WRITE);
        if (f_log) {
            uint8_t buf[64];
            unsigned int count = 0;
            
            while (count < def_bin_len) {
                unsigned int chunk = def_bin_len - count;
                if (chunk > 64) chunk = 64;
                
                // Safe copy from PROGMEM to RAM
                memcpy_P(buf, def_bin + count, chunk);
                // Write chunk to SD
                f_log.write(buf, chunk);
                
                count += chunk;
            }
            f_log.flush();
            f_log.close();
            delay(200);
            Serial.println("[SD] def.bin installed.");
        } else {
            Serial.println("[SD] Failed to create def.bin");
        }
    }

    // Create default radios.json if missing
    if (!SD_MMC.exists("/radios.json")) {
        Serial.println("[SD] Creating default radios.json...");
        File file = SD_MMC.open("/radios.json", FILE_WRITE);
        if (file) {
            String defaultRadio = R"([{"name":"FIP","url":"http://icecast.radiofrance.fr/fip-midfi.mp3","logo":"S:/logos/def.bin"}])";
            
            file.print(defaultRadio); 
            file.close();
            Serial.println("[SD] radios.json created with FIP fallback.");
        }
    }
    
    Serial.println("[SD] Ready.");
}

// LVGL file system callbacks for SD card (driver letter S:)
static void * sd_fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
    File * f = new File();
    *f = SD_MMC.open(path, mode == LV_FS_MODE_WR ? FILE_WRITE : FILE_READ);
    if(!*f || f->isDirectory()) {
        delete f;
        return NULL;
    }
    return (void *)f;
}

static lv_fs_res_t sd_fs_close(lv_fs_drv_t * drv, void * file_p) {
    File * f = (File *)file_p;
    f->close();
    delete f;
    return LV_FS_RES_OK;
}

static lv_fs_res_t sd_fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
    File * f = (File *)file_p;
    *br = f->read((uint8_t *)buf, btr);
    return LV_FS_RES_OK;
}

static lv_fs_res_t sd_fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
    File * f = (File *)file_p;
    f->seek(pos, whence == LV_FS_SEEK_SET ? SeekSet : (whence == LV_FS_SEEK_CUR ? SeekCur : SeekEnd));
    return LV_FS_RES_OK;
}

static lv_fs_res_t sd_fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
    File * f = (File *)file_p;
    *pos_p = f->position();
    return LV_FS_RES_OK;
}

// Register the SD LVGL file system driver
void lv_fs_sd_init() {
    static lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);
    
    drv.letter = 'S'; // Use S:/ prefix for SD paths
    drv.ready_cb = NULL;
    drv.open_cb = sd_fs_open;
    drv.close_cb = sd_fs_close;
    drv.read_cb = sd_fs_read;
    drv.seek_cb = sd_fs_seek;
    drv.tell_cb = sd_fs_tell;
    
    lv_fs_drv_register(&drv);
}

#endif