// Funkchip des CrowPanel P4 (ESP32-C6, ESP-Hosted): Firmware-Stand pruefen und auf Knopfdruck aktualisieren – aus dem
// Internet (espressif.github.io, dieselbe Datei wie bei Arduino) oder aus /sd/c6.bin. KEINE Automatik (Wunsch des
// Nutzers): nur Editor > Info, Panel > Info oder serielle Befehle c6update/c6datei. Das Schreiben laeuft in einem eigenen
// Task (etwa eine Minute), danach startet das Panel neu. Der Online-Flasher kann den C6 nicht erreichen (SDIO hinter dem
// P4, kein USB) – darum macht es das Panel selbst.
#pragma once
#include <string>
#include "app_config.h"

#if defined(BOARD_P4) && BOARD_P4
#define WITH_C6_UPDATE 1
struct C6Info {
  std::string fw, host;   // Firmware im Funkchip / die zur Panel-Firmware passende
  bool ready = false;     // Funkchip antwortet
  bool update = false;    // Stand passt nicht zum Panel
  bool file = false;      // /sd/c6.bin vorhanden
  int state = 0;          // 0 nichts, 1 laeuft, 2 fertig (Neustart folgt), 3 Fehler
  std::string msg;        // Fehlertext
};
C6Info c6_info();
void c6_info_json(std::string &out);                     // JSON-Objekt fuer /api/info "c6"
bool c6_update_start(bool from_file, std::string &err);  // false = laeuft schon / nicht bereit / keine Datei / kein WLAN
bool c6_update_file(const char *path, std::string &msg); // Datei direkt schreiben (blockiert; serieller Befehl)
#else
#define WITH_C6_UPDATE 0
#endif
