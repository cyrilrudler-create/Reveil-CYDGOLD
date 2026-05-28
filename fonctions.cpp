#include "fonctions.h"
#include <ArduinoJson.h>
#include <SD_MMC.h>
#include "config.h"

// =====================================================
//  GLOBAL STATION LIST
// =====================================================

std::vector<Station> STATIONS;

// =====================================================
//  STATION LOADER
//
//  Reads /radios.json from the SD card and fills STATIONS.
//  Falls back to a single hardcoded FIP station if the
//  file is missing or malformed.
// =====================================================
bool loadStationsFromSD() {
    File file = SD_MMC.open("/radios.json");
    if (!file) {
        Serial.println("[SD] radios.json missing — loading fallback station");
        STATIONS.clear();
        Station s;
        s.name      = "FIP (fallback)";
        s.url       = "http://icecast.radiofrance.fr/fip-midfi.mp3";
        s.logo_path = "S:/logos/def.bin";
        STATIONS.push_back(s);
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("[SD] JSON parse error: %s\n", error.c_str());
        return false;
    }

    STATIONS.clear();
    for (JsonObject obj : doc.as<JsonArray>()) {
        Station s;
        s.name      = obj["name"].as<String>();
        s.url       = obj["url"].as<String>();
        s.logo_path = obj["logo"].as<String>();
        STATIONS.push_back(s);
    }

    Serial.printf("[SD] %d station(s) loaded\n", STATIONS.size());
    return true;
}

// =====================================================
//  BATTERY LEVEL
//
//  Averages 10 ADC readings to reduce ESP32 ADC noise.
//  Raw range 1900..2400 mapped to 0..100%.
//  Adjust BAT_ADC_MIN / BAT_ADC_MAX in config.h to
//  calibrate for your specific battery/divider.
// =====================================================
int get_battery_level() {
    int32_t sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += analogRead(BAT_ADC_PIN);
        delay(2); // short delay to let the ADC capacitor recharge
    }
    int raw = (int)(sum / 10);
    return constrain(map(raw, 1900, 2400, 0, 100), 0, 100);
}
