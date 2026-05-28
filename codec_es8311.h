#ifndef CODEC_ES8311_H
#define CODEC_ES8311_H

#include <Arduino.h>
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "config.h"

// =====================================================
//  ES8311 AUDIO CODEC DRIVER
//
//  Communicates via I2C (ESP-IDF master API).
//  I2C address : 0x18
//  Bus         : I2C_NUM_1  (SDA=GPIO16, SCL=GPIO15)
//  Call setup_i2c_and_codec() once in setup().
//  Call codec_volume(0..100) to set hardware volume.
// =====================================================

// ES8311 register addresses
#define R00 0x00
#define R01 0x01
#define R02 0x02
#define R03 0x03
#define R04 0x04
#define R05 0x05
#define R06 0x06
#define R07 0x07
#define R08 0x08
#define R09 0x09
#define R0A 0x0A
#define R0D 0x0D
#define R0E 0x0E
#define R12 0x12
#define R13 0x13
#define R14 0x14
#define R17 0x17
#define R1C 0x1C
#define R32 0x32  // Volume register
#define R37 0x37

static i2c_master_dev_handle_t codec;
static i2c_master_bus_handle_t i2c_bus;

// Write one byte to a codec register
static esp_err_t cw(uint8_t r, uint8_t d) {
    const uint8_t b[2] = {r, d};
    return i2c_master_transmit(codec, b, 2, pdMS_TO_TICKS(1000));
}

// Read one byte from a codec register
static esp_err_t cr(uint8_t r, uint8_t* v) {
    return i2c_master_transmit_receive(codec, &r, 1, v, 1, pdMS_TO_TICKS(1000));
}

// Initialize I2C bus and ES8311 codec
// Returns ESP_OK on success, ESP_FAIL if the codec is not found
static esp_err_t setup_i2c_and_codec() {
    // I2C bus configuration
    i2c_master_bus_config_t bc = {};
    bc.i2c_port    = I2C_NUM_1;
    bc.sda_io_num  = (gpio_num_t)I2C_SDA;
    bc.scl_io_num  = (gpio_num_t)I2C_SCL;
    bc.clk_source  = I2C_CLK_SRC_DEFAULT;
    if (i2c_new_master_bus(&bc, &i2c_bus) != ESP_OK) return ESP_FAIL;

    // ES8311 device at 0x18, 400kHz
    i2c_device_config_t dc = {};
    dc.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dc.device_address  = 0x18;
    dc.scl_speed_hz    = 400000;
    if (i2c_master_bus_add_device(i2c_bus, &dc, &codec) != ESP_OK) return ESP_FAIL;

    // ES8311 initialization sequence
    cw(R00, 0x1F);
    vTaskDelay(pdMS_TO_TICKS(20));
    cw(R00, 0x00);
    cw(R00, 0x80);
    cw(R01, 0x3F);
    cw(R02, 0x08);
    cw(R03, 0x10);
    cw(R04, 0x10);
    cw(R05, 0x00);
    cw(R06, 0x03);
    cw(R07, 0x00);
    cw(R08, 0xFF);

    uint8_t v;
    cr(R00, &v);
    cw(R00, v & 0xBF);

    cw(R09, 0x0C);
    cw(R0A, 0x0C);
    cw(R0D, 0x01);
    cw(R0E, 0x02);
    cw(R12, 0x00);
    cw(R13, 0x10);
    cw(R1C, 0x6A);
    cw(R37, 0x08);
    cw(R17, 0xA8);
    cw(R14, 0x1A);

    return ESP_OK;
}

// Set codec hardware volume (0..100 → mapped to 0..255 in R32)
static void codec_volume(int vol) {
    vol = constrain(vol, 0, 100);
    cw(R32, (uint8_t)((vol * 255) / 100));
}

#endif
