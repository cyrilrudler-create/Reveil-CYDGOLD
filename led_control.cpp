#include "led_control.h"
#include "config.h"
#include <FastLED.h>

// =====================================================
//  LED STRIP CONTROL
//  WS2812B strip, NUM_LEDS LEDs.
//  Call loop_leds() every loop iteration for animations.
// =====================================================

CRGB leds[NUM_LEDS];
LedMode currentMode = MODE_FIXE;
uint8_t hue = 0;
uint8_t currentR = 255, currentG = 89, currentB = 0; // Default: CYD-GOLD orange

void setup_leds() {
    FastLED.addLeds<LED_TYPE, WS_LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(50); // Start at 50% — safe for first boot
    Serial.println("[LED] FastLED initialized");
}

void set_led_mode(LedMode mode) {
    currentMode = mode;
}

// Light one LED during the boot progress sequence
void boot_led_step(int step) {
    if (step >= 0 && step < NUM_LEDS) {
        leds[step] = CRGB(255, 89, 0);
        FastLED.show();
    }
}

// Update stored color — applies immediately in FIXED mode
void update_leds_color(uint8_t r, uint8_t g, uint8_t b) {
    currentR = r;
    currentG = g;
    currentB = b;
    if (currentMode == MODE_FIXE) {
        fill_solid(leds, NUM_LEDS, CRGB(r, g, b));
        FastLED.show();
    }
}

// Run the active animation — call every loop iteration
void loop_leds() {
    CRGB rgbColor = CRGB(currentR, currentG, currentB);
    CHSV hsvColor = rgb2hsv_approximate(rgbColor);

    switch (currentMode) {
        case MODE_RAINBOW:{
            EVERY_N_MILLISECONDS(20) { hue++; }
            fill_rainbow(leds, NUM_LEDS, hue, 20);
            FastLED.show();
        }break;

        case MODE_WAVE:{
            for(int i = 0; i < NUM_LEDS; i++) {
                uint8_t level = beatsin8(15, 30, 255, 0, i * 32);
                leds[i] = CHSV(hsvColor.h, hsvColor.s, level);
            }
            FastLED.show();
        }break;

        case MODE_PULSE:{
            uint8_t breath = beatsin8(10, 20, 255);
            fill_solid(leds, NUM_LEDS, CHSV(hsvColor.h, hsvColor.s, breath));
            FastLED.show();
        }break;

        case MODE_FIXE:
            // Nothing to do — color already applied by update_leds_color()
        break;
    }
}
