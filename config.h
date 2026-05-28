#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =====================================================
//  HARDWARE PIN CONFIGURATION — CYD-GOLD
//  All GPIO assignments for the ESP32-S3 board.
//  Edit here only — never hard-code pins elsewhere.
// =====================================================

// --- TFT display (ILI9341, SPI, landscape 320x240) ---
#define TFT_W        320  // Width  in landscape mode (used by LVGL)
#define TFT_H        240  // Height in landscape mode
#define TFT_MOSI     11
#define TFT_SCLK     12
#define TFT_CS       10
#define TFT_DC       46
#define TFT_RST      -1   // Tied to global reset
#define TFT_MISO     13
#define TFT_BL       45   // Backlight PWM
#define TFT_BL_ON    HIGH

// --- Capacitive touch controller ---
#define TP_INT       GPIO_NUM_17
#define TP_RST       GPIO_NUM_18

// --- I2C bus (DS3231 RTC + ES8311 codec) ---
#define I2C_SDA      GPIO_NUM_16
#define I2C_SCL      GPIO_NUM_15

// --- SD card (SDMMC 4-bit mode) ---
#define SD_SCK       GPIO_NUM_38
#define SD_CMD       GPIO_NUM_40
#define SD_D0        GPIO_NUM_39
#define SD_D1        GPIO_NUM_41
#define SD_D2        GPIO_NUM_48
#define SD_D3        GPIO_NUM_47

// --- Audio I2S (ES8311 codec) ---
#define I2S_BCK      GPIO_NUM_5
#define I2S_WS       GPIO_NUM_7
#define I2S_DOUT     GPIO_NUM_8
#define I2S_MCK      GPIO_NUM_4
#define AMP_EN       GPIO_NUM_1  // LOW = amplifier active

// --- Battery ADC ---
#define BAT_ADC_PIN  9
#define BAT_ADC_MIN  1900   // Raw ADC value at 0%  — calibrate for your divider
#define BAT_ADC_MAX  2400   // Raw ADC value at 100%

// --- WS2812B LED strip ---
#define WS_LED_PIN   14
#define NUM_LEDS     7
#define LED_TYPE     WS2812B
#define COLOR_ORDER  GRB
#define MAX_BRIGHTNESS 180

// --- Deep-sleep slide switch ---
// Wired between GPIO and GND with INPUT_PULLUP.
// Switch OFF → GPIO HIGH → deep sleep.
// Switch ON  → GPIO LOW  → normal operation.
// Wake-up: EXT1 on LOW level (switch flipped back to ON).
#define SLEEP_BTN_PIN    GPIO_NUM_2
#define SLEEP_BTN_LEVEL  LOW

#endif
