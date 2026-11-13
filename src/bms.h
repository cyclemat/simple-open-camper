// Akkus mit eigenem BMS per Bluetooth (Verbindung, nicht nur Werbepakete):
//   Super B Epsilon (App "Be in Charge" 2.1.1): meldet seine Werte selbst (Notify), Entsperren mit "SuperB".
//   Power Quarry VBxxx / CBT-BMS (App "Power Quarry" 3.5.2): Spannung/Strom alle 20 s, Ladezustand und Rest jede Minute, binaer (AA 55 …) oder
//   ASCII (~…\r, Pylontech-artig, Herstellerdaten "CBT^" Version 2).
//   Bulltron (App "BullTron" 1.1.36.2): Modbus-RTU (Adresse 0xD2, Funktion 0x03, CRC-16), Dienst 0xFFF0
//   (0xFFF1 Antworten, 0xFFF2 Anfragen), alle 20 s 62 Laufdaten-Register.
//   JBD / Xiaoxiang (Dienst 0xFF00, DD A5 03 … 77), JK-BMS (JK02, 0xFFE1, 300-Byte-Rahmen 55 AA EB 90),
//   LiTime / Redodo / PowerQueen (0xFFE1/0xFFE2) – Protokolle nach der Beschreibung in mr-manuel/
//   venus-os_dbus-serialbattery (dort AGPL-3.0; hier eigene Umsetzung, kein uebernommener Code).
// Alle ausser Super B werden nur kurz verbunden: verbinden, abfragen, trennen (alle 20 s).
// Haengt am Bluetooth-Ablauf von truma.cpp (Scan-Task): bms_on_adv / bms_scan_wanted / bms_want_connect /
// bms_connect_now – wie Hymer und iNet X. Ein eingerichteter Akku bleibt dauerhaft verbunden und meldet seine Werte
// selbst (Notify); bei Abbruch wird nach kurzer Pause neu verbunden.
// SICHERHEIT: Es wird nur entsperrt und gelesen. Die Akku-Konsole kennt auch Befehle, die die Leistungsschalter
// schalten oder Einstellungen aendern – solche Befehle werden nie gesendet.
// Schalter: -DWITH_BMS=1 (ab Werk aus). Zur Laufzeit zusaetzlich BLE_P_BMS (ble_proto_on).
// NVS-Namensraum "bms": n, m<i> MAC, t<i> Adresstyp, k<i> Modell, a<i> Name, c<i> Kapazitaet Ah, z<i> Zellen.
#pragma once
#include <math.h>
#include <stdint.h>
#include <string>
#include <vector>

#ifndef WITH_BMS
#define WITH_BMS 0
#endif

#define BMS_MAX 4
#define BMS_KEY_PREFIX "bms:"

enum BmsModel : uint8_t {
  BMS_UNKNOWN = 0, BMS_SUPERB = 1, BMS_PQ_HEX = 2, BMS_PQ_ASCII = 3, BMS_BULLTRON = 4, BMS_JBD = 5, BMS_JK = 6, BMS_LITIME = 7
};

struct BmsInfo {
  std::string key, mac, name;
  BmsModel model = BMS_UNKNOWN;
  std::string model_text;             // "Super B", ergaenzt um das Modell aus der Geraete-Info
  std::string serial, firmware;
  bool connected = false;
  bool has = false;                   // mindestens ein Statuspaket empfangen
  int32_t age_s = -1;                 // Sekunden seit dem letzten Statuspaket
  int soc = -1, soh = -1;             // %
  float volt = NAN, amp = NAN;        // Strom: + laden, - entladen
  int32_t ttg_s = -1;                 // Restzeit bis leer, -1 = unbekannt
  bool ok = true, charging = false, discharging = false, low = false;
  bool charge_allowed = true, discharge_allowed = true;
  float temp = NAN;                   // Akku-Temperatur °C (Power Quarry)
  int cycles = -1;                    // Ladezyklen (Power Quarry)
  int capacity_ah = 0;                // Nennkapazitaet (Power Quarry, fuer die Restzeit)
  std::vector<uint16_t> cells_mv;     // Zellspannungen (Power Quarry)
  uint32_t alarms = 0;                // Alarmbits (Power Quarry binaer), 0 = keine
  std::string status;                 // "Verbunden", "Verbinde …", "Nicht erreichbar" …
};

struct BmsFound {
  std::string mac, name;
  uint8_t addr_type = 0;
  int rssi = 0;
  BmsModel model = BMS_UNKNOWN;
  bool service = false;               // wirbt mit dem Dienst des Herstellers (sicherer Treffer)
  std::string product;                // z. B. "VB101" (Power Quarry)
  int capacity_ah = 0, cells = 0;     // aus der Produkttabelle (Power Quarry)
  bool configured = false;
};

const char *bms_model_text(BmsModel m);

#if WITH_BMS
class NimBLEAdvertisedDevice;
void bms_begin();
uint32_t bms_version();
bool bms_has_devices();
// Bluetooth-Ablauf (nur aus dem Scan-Task von truma.cpp)
bool bms_on_adv(const NimBLEAdvertisedDevice *dev);   // true = Akku-Kandidat (waehrend der Suche)
void bms_scan_wanted(bool &passive, bool &active);
bool bms_want_connect();
void bms_connect_now();                               // Scan ist gestoppt, blockiert einige Sekunden
// Abfragen und Einrichten
std::vector<BmsInfo> bms_list();
bool bms_get(const std::string &key, BmsInfo &out);
void bms_discovery_start();                           // 60 s aktiv suchen
bool bms_discovery_running();
std::vector<BmsFound> bms_found();
std::string bms_add(const std::string &mac, uint8_t addr_type, const std::string &name, BmsModel model,
                    int capacity_ah = 0, int cells = 0);   // "" = ok
bool bms_want_poll();                                 // Power Quarry: Abfrage faellig (Scan-Task)
void bms_poll_now();                                  // Abfrage ausfuehren, blockiert 1–3 s (Scan laeuft weiter)
void bms_remove(const std::string &mac);
void bms_rename(const std::string &mac, const std::string &name);
std::vector<std::string> bms_log();                   // letzte Meldungen (Fehlersuche ueber die Weboberflaeche)
#else
inline void bms_begin() {}
inline uint32_t bms_version() { return 0; }
inline bool bms_has_devices() { return false; }
inline std::vector<BmsInfo> bms_list() { return {}; }
inline bool bms_get(const std::string &, BmsInfo &) { return false; }
#endif
