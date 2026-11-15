// Satellit: Einrichten per Knopf – siehe ../sat_pair_proto.h
// BOOT-Taste 3 s halten (ESP32/S3: GPIO0, C3/C6: GPIO9; -DSAT_PAIR_BTN=-1 schaltet die Taste ab) oder 3x schnell neu
// starten -> 3 Minuten Einrichtungsmodus. Abschaltbar mit -DSAT_PAIR=0 (spart Flash, z. B. beim C6).
#pragma once

#ifndef SAT_PAIR
#define SAT_PAIR 1
#endif

void sat_pair_begin();
void sat_pair_loop();
bool sat_pair_active();   // Einrichtungsmodus laeuft (die normalen Bluetooth-Werbepakete pausieren dann)
