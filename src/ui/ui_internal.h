// Interne Schnittstellen zwischen den UI-Modulen
#pragma once
#include <functional>
#include <string>
#include <lvgl.h>
#include <vector>
#include "../z2m.h"
#include "../truma.h"
#include "../victron.h"
#include "../mopeka.h"
#include "../truma_heat.h"
struct Layout;

enum SettingsSection { SEC_WIFI = 0, SEC_MQTT, SEC_DISPLAY, SEC_DEVICES, SEC_GAS, SEC_VICTRON, SEC_BMS, SEC_PSTATION, SEC_HYMER, SEC_ZIGBEE, SEC_BACKUP, SEC_INFO, SEC_COUNT };

// ui_main.cpp
void ui_main_rebuild();
lv_obj_t *ui_main_screen();

// ui_detail.cpp
void ui_detail_open(const std::string &key);
void ui_detail_update();
void ui_detail_close();
bool ui_detail_is_open();

// ui_settings.cpp
void ui_settings_open(int section);
void ui_settings_close();
bool ui_settings_is_open();
void ui_settings_tick();                 // 1x pro Sekunde
void ui_settings_refresh();
void ui_settings_tick_gas();             // neue Werte/Scanergebnisse der Truma-Gasflaschen
void ui_settings_tick_mopeka();          // neue Werte/Suchergebnisse der Mopeka-Sensoren
void ui_settings_tick_hymer();           // Hymer Connect: Status, Kopplung, Werte
void ui_settings_tick_heat();            // Truma-Heizung: Zustand und Kopplung
void ui_settings_tick_victron();
void ui_settings_tick_sat();              // Satelliten: neue Werte/Geraete         // neue Werte/Suchergebnisse der Victron-Geraete

// ui_gas.cpp (Gasflaschen und Tanks: Truma LevelControl und Mopeka)
// Gemeinsame Sicht auf beide Sensorarten fuer Kachel, Detailansicht und Einstellungen
struct GasView {
  std::string key, mac, name;
  std::string source;          // "Truma LevelControl", "Mopeka Pro Check", ...
  bool mopeka = false;
  bool water = false;          // Wasser-/Tanksensor statt Gasflasche
  bool has = false, busy = false;
  int percent = 0;
  std::string amount;          // "6,8 kg", "40 l" oder leer
  int battery = 0, temperature = 0, quality = 0;
  int level_mm = -1;           // nur Mopeka
  int32_t age_s = -1;
  std::string status;
  std::string sat;             // kommt von einem Satelliten: dessen Name und Weg, sonst leer
};
bool ui_gas_is_key(const std::string &key);
bool ui_gas_get(const std::string &key, GasView &out);
std::vector<GasView> ui_gas_list();
void ui_gas_open(const std::string &key);
void ui_gas_update();
void ui_gas_close();
bool ui_gas_is_open();
lv_color_t ui_gas_color(int pct);
std::string ui_kg_text(float kg);
std::string ui_age_text(int32_t age_s);
std::string ui_gas_label(const Layout &lay, const GasView &g);
std::vector<ControlView> ui_arrange_items();   // Zigbee-Geraete + Gasflaschen fuer "automatisch anordnen"

// ui_heat.cpp (Truma-Heizung: CP Plus per LIN, iNet X per Bluetooth)
bool ui_heat_get(const std::string &key, TrumaHeatInfo &out);
std::vector<TrumaHeatInfo> ui_heat_list();      // nur die in der Firmware eingeschalteten Bedienteile
uint32_t ui_heat_version();
std::string ui_temp_text(float c);              // "21,5 °C", NAN -> "–"
const char *ui_water_text(TrumaWater w);
std::string ui_heat_label(const Layout &lay, const TrumaHeatInfo &h);
std::string ui_heat_sub(const TrumaHeatInfo &h, bool small);
void ui_heat_open(const std::string &key);
void ui_heat_update();
void ui_heat_close();
bool ui_heat_is_open();

// ui_victron.cpp (Victron Instant Readout)
struct VicTileText {
  std::string value, sub;   // grosser Wert oben rechts, Zeile unter dem Namen
  lv_color_t accent;        // Icon-/Pegelfarbe
  int fill = -1;            // Pegel 0..100 (Batteriemonitor), -1 = keiner
  bool stale = false;       // lange kein Paket empfangen
};
void ui_vic_open(const std::string &key);
bool ui_vic_get(const std::string &key, VictronInfo &out);   // Panel-Bluetooth, Cerbo GX oder Satellit
void ui_vic_update();
void ui_vic_close();
bool ui_vic_is_open();
const char *ui_vic_icon(VicKind k);
std::string ui_vic_label(const Layout &lay, const VictronInfo &v);
VicTileText ui_vic_tile_text(const VictronInfo &v, bool small, bool big);
std::string ui_vic_summary(const VictronInfo &v);   // eine Zeile fuer Listen
std::vector<std::pair<std::string, std::string>> ui_vic_rows(const VictronInfo &v);   // Zeilen des Detailfensters
std::string ui_num(float v, int decimals, const char *unit);   // "12,84 V", NAN -> "–"

// ui_main.cpp: fertige Kachel-Daten (Zusatzdisplays bekommen sie vom Hauptpanel, ui_remote.cpp)
struct TileModel {
  std::string name, icon, value, sub;   // icon = Zeichen aus der Icon-Schrift (UTF-8)
  uint32_t accent = 0x9AA5B1;           // Farbe von Icon/Pegel (RGB)
  int fill = -1;                        // Pegel 0..100, -1 = keiner
  bool lit = false;                     // eingeschaltet (Licht/Schalter)
  uint32_t lit_color = 0;               // Lichtfarbe bei lit (RGB)
  bool dim = false;                     // nicht erreichbar / veraltet
  uint8_t act = 0;                      // 1 = Antippen schaltet um
  // Licht (Zigbee): dimmbar, Farbe, Farbtemperatur – fuer das Dimm-Fenster der Zusatzdisplays
  int bri = -1;                         // Helligkeit 0..100, -1 = nicht dimmbar
  bool color = false;                   // Farbrad moeglich
  int hue = -1, sat = -1;               // aktuelle Farbe 0..360 / 0..100
  int ct = -1, ct_min = 0, ct_max = 0;  // Farbtemperatur in Mired (-1 = keine)
  // MultiPlus/Ladegeraet ueber den GX – fuer das Bedienfenster der Zusatzdisplays
  uint8_t vset = 0;                     // 0 nicht schaltbar, 1 MultiPlus (1/2/3/4), 2 Ladegeraet (1 ein, 4 aus)
  int vmode = -1;                       // aktuelle Schalterstellung
  int vlim = -1;                        // Netzstrom-Begrenzung in 0,1 A, -1 = nicht einstellbar
};
bool ui_tile_model(const std::string &key, TileModel &m);   // false = unbekannte Kachel
bool ui_tile_action(const std::string &key, std::string *err);   // wie Antippen am Panel
// Licht: 'b' Helligkeit %, 'h' hue*1000+sat, 'c' Mired; MultiPlus/Ladegeraet: 'm' Schalter, 'l' Begrenzung in 0,1 A
bool ui_tile_set(const std::string &key, char what, int value);
std::vector<std::string> ui_tile_keys();   // alle Kacheln aller Seiten (Reihenfolge der Seiten)

// ui_remote.cpp (Zusatzdisplays)
void ui_remote_tick();

// ui_saver.cpp
void ui_saver_start();
void ui_saver_stop();
void ui_saver_tick();                    // in jeder ui_loop-Runde
bool ui_saver_active();
void ui_saver_logo_changed();

// ui_input.cpp
struct InputOptions {
  std::string title;
  std::string hint;
  std::string initial;
  bool password = false;
  bool numeric = false;
  int max_len = 64;
  std::string check_label;              // optionales Kontrollkaestchen
  bool check_value = false;
  std::function<void(const std::string &text, bool checked)> on_ok;
};
void ui_input_open(const InputOptions &opt);
void ui_confirm(const char *title, const char *text, const char *ok_label, bool danger, std::function<void()> on_ok);
bool ui_input_is_open();

// Gemeinsame Hilfen
void ui_rename_device(const std::string &key);   // Dialog zum Umbenennen
