// Satellit mit eigener Anzeige – z. B. ESP32-2432S028R ("Cheap Yellow Display": 2,8" 320x240 ILI9341, resistiver
// Touch XPT2046). Zeigt alle Werte des Satelliten als Liste (eigene Sensoren/Schalter, Truma, Mopeka, Victron, Hymer);
// Schalter per Antippen, Blaettern ueber die Pfeile oben/unten. Kein Bildspeicher (Board ohne PSRAM): es wird direkt
// gezeichnet, und nur Zeilen, die sich geaendert haben.
// Nur mit -DSAT_DISPLAY=1 (Umgebung esp32-cyd in satellite/platformio.ini), sonst leer.
#pragma once

void sat_display_begin();
void sat_display_loop();
