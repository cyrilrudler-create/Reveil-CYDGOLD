#ifndef FONCTIONS_H
#define FONCTIONS_H

#include <vector>
#include "structures.h"

// Global station list — populated by loadStationsFromSD()
extern std::vector<Station> STATIONS;

// Load /radios.json from SD into STATIONS.
// Falls back to a hardcoded FIP station if the file is missing.
bool loadStationsFromSD();

// Read battery level as a percentage (0..100).
// Averages 10 ADC samples to reduce ESP32 ADC noise.
int get_battery_level();

#endif
