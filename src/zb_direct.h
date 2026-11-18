// Zigbee direkt: Panel <-> Zigbee-Funkmodul (ESP32-H2) per UART – Protokoll siehe zb_link.h.
// Grundgeruest (2026-11-18): Geraeteliste, letzte Werte, Anlernen, Schalten. Kacheln/Treiber folgen.
#pragma once
#include <ArduinoJson.h>
#include <string>

#ifndef WITH_ZB_DIRECT
#define WITH_ZB_DIRECT 0
#endif
#ifndef ZBD_TX
#define ZBD_TX 27   // P4 TXD2 -> Steckplatz J9 Pin 1 (CrowPanel Advanced 7" V1.2)
#endif
#ifndef ZBD_RX
#define ZBD_RX 28   // P4 RXD2 <- Steckplatz J11 Pin 1
#endif

void zbd_begin();                                  // startet nur, wenn eingeschaltet (NVS "zbd" on, ab Werk an)
bool zbd_enabled();
void zbd_set_enabled(bool on);
bool zbd_present();                                // Modul hat sich in den letzten 70 s gemeldet
void zbd_permit(int seconds);                      // Anlernen oeffnen (0 = schliessen)
bool zbd_send_cmd(JsonObjectConst cmd);            // beliebiger Befehl {"c":…} ans Modul
void zbd_status_json(JsonObject o);                // fuer /api/zbd
void zbd_debug(const char *args);                  // serieller Befehl "zb …"

// Fuer die Panel-Einstellungen
#include <vector>
struct ZbdInfo {
  bool started = false, present = false, ready = false;
  std::string fw, coord;
  uint16_t pan = 0;
  uint8_t ch = 0;
  int open = 0;   // Anlernen offen: Restsekunden
};
struct ZbdDevice {
  std::string ieee, mf, model, desc;   // mf/model: aus der Treiberliste, sonst wie das Geraet sich meldet
  std::string name;                    // eigener Name ("" = keiner)
  bool done = false;
  int age = -1;            // Sekunden seit der letzten Meldung (-1 = noch keine)
  std::string summary;     // lesbare Werte, z. B. "An · 21,4 °C · Batterie 87 %"
};
ZbdInfo zbd_info();
std::vector<ZbdDevice> zbd_devices();
std::vector<std::string> zbd_log();
uint32_t zbd_structure_sig();                      // aendert sich, wenn Geraete/Zustand neu aufgebaut werden muessen
void zbd_remove(const std::string &ieee);          // Geraet abmelden und vergessen
void zbd_rename(const std::string &ieee, const std::string &name);   // eigener Name (im Modul gespeichert)

// Einstellungen eines Geraets: werden erst beim Oeffnen der Seite vom Modul geholt und nur so lange gehalten
// (zbd_config_close gibt den Speicher frei). Speichern schickt nur die geaenderten Werte ans Modul.
struct ZbdCfgItem {
  std::string name, unit, cur;     // cur: aktueller Wert als JSON ("" = unbekannt)
  uint8_t type = 0, group = 0;     // ZbdType (1 Ein/Aus, 2 Zahl, 3 Auswahl, 4 Text)
  float min = 0, max = 0, step = 0;
  std::vector<std::string> values; // Auswahlwerte bzw. [Ein, Aus]
};
void zbd_config_open(const std::string &ieee);     // Abfrage starten
bool zbd_config_get(std::vector<ZbdCfgItem> &out); // true = vollstaendig empfangen
void zbd_config_close();
bool zbd_set(const std::string &ieee, const std::string &json_values);   // {"name":wert,…}
std::vector<std::pair<std::string, std::string>> zbd_values(const std::string &ieee);   // Name -> Wert (Text)
void zbd_reset_network();                          // neues Netz (alle Geraete neu anlernen!)
