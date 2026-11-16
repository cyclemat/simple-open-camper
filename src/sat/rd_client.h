// Zusatzdisplay (Satellit im Modus SAT_REMOTE): zeigt ausgewaehlte Kacheln des Hauptpanels und schaltet darueber.
// Gegenstueck im Panel: src/ui/ui_remote.cpp (Ablauf und Paketformat dort beschrieben).
#pragma once
#include <stdint.h>
#include <string>
#include <vector>

#ifndef SAT_REMOTE
#define SAT_REMOTE 0
#endif

struct RdTile {
  std::string key, name, icon, value, sub;
  uint32_t accent = 0x9AA5B1, lit_color = 0;
  int fill = -1;
  bool lit = false, dim = false;
  uint8_t act = 0;
  uint32_t ms = 0;   // zuletzt empfangen
  // Licht: Helligkeit 0..100 (-1 = nicht dimmbar), Farbe, Farbtemperatur (Mired, -1 = keine)
  int bri = -1, hue = -1, sat = -1, ct = -1, ct_min = 0, ct_max = 0;
  bool color = false;
};

#if SAT_REMOTE
void rd_begin();
void rd_loop();
std::vector<RdTile> rd_tiles();          // in der Reihenfolge des Hauptpanels (nur schon empfangene)
bool rd_action(const std::string &key);  // Antippen: Befehl ans Hauptpanel
bool rd_set(const std::string &key, char what, int value);   // Dimm-Fenster: 'b' Helligkeit, 'h' hue*1000+sat, 'c' Mired
bool rd_tile(const std::string &key, RdTile &out);
uint32_t rd_version();
bool rd_panel_seen();                    // Hauptpanel meldet sich (in den letzten 60 s)
std::string rd_status();                 // fuer die Weboberflaeche
void rd_mqtt_in(const std::string &sub, const char *data, size_t len);   // aus sat_send.cpp (MQTT-Task)
#endif
