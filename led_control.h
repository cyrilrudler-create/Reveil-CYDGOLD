#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <Arduino.h>

// =====================================================
//  LED ANIMATION MODES
//  Used by ui_leds.cpp to switch animations.
//  loop_leds() must be called every loop iteration.
// =====================================================

enum LedMode {
    MODE_FIXE    = 0, // Static color from the color wheel
    MODE_RAINBOW = 1, // Full-spectrum hue rotation
    MODE_WAVE    = 2, // Brightness wave across the strip
    MODE_PULSE   = 3  // Breathing effect (color-wheel hue)
};

extern LedMode currentMode;

// Initialize FastLED — call once in setup()
void setup_leds();

// Set the active animation mode
void set_led_mode(LedMode mode);

// Update the stored color and apply it immediately in FIXED mode
void update_leds_color(uint8_t r, uint8_t g, uint8_t b);

// Light one LED in the boot sequence (called during setup steps)
void boot_led_step(int step);

// Run animations — call every loop iteration
void loop_leds();

#endif
