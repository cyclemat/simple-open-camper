// Victron "Instant Readout" per Bluetooth LE: SmartSolar/BlueSolar MPPT, SmartShunt/BMV, Orion Smart, Orion XS.
// Die Geraete senden ihre Werte AES-CTR-verschluesselt in den Werbepaketen (Hersteller-ID 0x02E1).
// Das Panel lauscht nur mit – keine Verbindung, stoert weder VictronConnect noch einen GX.
// Den Schluessel pro Geraet zeigt VictronConnect: Einstellungen -> Produktinfo -> Instant Readout -> Details.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>

#define VICTRON_MAX_DEVICES 6
#define VICTRON_KEY_PREFIX "victron:"

enum VicKind : uint8_t {
  VIC_UNKNOWN = 0, VIC_SOLAR = 1, VIC_SHUNT = 2, VIC_DCDC = 3,
  // nur ueber den Cerbo GX (cerbo.cpp)
  VIC_GRID = 4,      // Netz/Landstrom am AC-Eingang
  VIC_CHARGER = 5,   // Wechselrichter/Lader (Multi, Quattro) oder Netzladegeraet
  VIC_DCSYS = 6,     // DC-Verbraucher ("DC-System")
  VIC_INVERTER = 7,  // Wechselrichter (Phoenix, Multi RS, Ausgang von MultiPlus/Quattro) – ueber den Cerbo GX
};

struct VictronInfo {
  std::string key;          // "victron:<mac>" – so liegt die Kachel im Layout
  std::string mac, name;
  VicKind kind = VIC_UNKNOWN;
  uint8_t record = 0;       // Victron-Datensatztyp (0x01 Solar, 0x02 Batteriemonitor, 0x04 DC/DC, 0x08 AC-Ladegeraet,
                            // 0x0D DC-Energiezaehler, 0x0F Orion XS)
  int16_t meter = 0;        // DC-Energiezaehler (SmartShunt als DC-Zaehler): Art, z. B. 3 Kuehlschrank, -6 Lichtmaschine
  uint16_t model = 0;
  bool has = false;         // mindestens ein Messwert empfangen
  int32_t age_s = -1;       // Sekunden seit dem letzten Paket
  int rssi = 0;
  std::string status;       // "OK", "Schlüssel passt nicht", ...
  // Gemeinsam: Batterie bzw. Ausgang. Nicht verfuegbare Werte sind NAN bzw. -1.
  int state = -1, error = -1;
  float bat_v = 0, bat_i = 0;
  // Solarregler
  float pv_w = 0, yield_kwh = 0, load_i = 0;
  // Batteriemonitor
  float soc = 0, consumed_ah = 0, aux = 0;
  int remaining_min = -1;
  uint8_t aux_mode = 3;     // 0 Starterbatterie (V), 1 Mittelpunkt (V), 2 Temperatur (°C), 3 keiner
  uint16_t alarm = 0;
  // Ladebooster
  float in_v = 0, in_i = 0;
  uint32_t off_reason = 0;
  // Cerbo GX: Leistungen in W (NAN = unbekannt)
  bool gx = false;          // Wert kommt vom Cerbo GX, nicht per Bluetooth
  float power_w = 0;        // DC-Leistung (Batterie, Lader, DC-System)
  float ac_w = 0, ac_v = 0, ac_i = 0, ac_limit = 0;
  int ac_source = -1;       // aktiver AC-Eingang (0 unbekannt, 1 Netz, 2 Generator, 3 Landstrom, 240 keiner)
  int ac_connected = -1;
  int mode = -1;            // Schalter des Multi: 1 nur Laden, 2 nur Wechselrichter, 3 ein, 4 aus
};

struct VictronFound {
  std::string mac, name;
  uint16_t model = 0;
  uint8_t record = 0;
  int rssi = 0;
  bool configured = false;
};

void victron_begin();
uint32_t victron_version();                        // aendert sich bei neuen Werten/Konfiguration
bool victron_has_devices();
// Auch die Werte des Cerbo GX ("cerbo:...") laufen als Victron-Kacheln
static inline bool victron_is_key(const std::string &k) { return k.rfind(VICTRON_KEY_PREFIX, 0) == 0 || k.rfind("cerbo:", 0) == 0; }

// Vom Bluetooth-Scan (truma.cpp) fuer jedes Paket mit Hersteller-ID 0x02E1 aufgerufen.
// md = komplette Herstellerdaten inkl. der zwei Bytes Hersteller-ID.
void victron_on_adv(const std::string &mac, const uint8_t *md, size_t len, int rssi, const std::string &adv_name);

std::vector<VictronInfo> victron_list();
bool victron_get(const std::string &key, VictronInfo &out);

void victron_discovery_start();                    // 3 Minuten nach Victron-Geraeten suchen
bool victron_discovery_running();
std::vector<VictronFound> victron_found();

std::string victron_add(const std::string &mac, const std::string &key_hex, const std::string &name);  // "" = ok
std::string victron_set_key(const std::string &mac, const std::string &key_hex);
void victron_remove(const std::string &mac);
void victron_rename(const std::string &mac, const std::string &name);

VicKind victron_kind_of_record(uint8_t record);
const char *victron_kind_text(VicKind k);
const char *victron_meter_text(int meter);   // "Kühlschrank", "Lichtmaschine", ... (0 = "")          // "Solarregler", "Batteriemonitor", ...
const char *victron_state_text(int state);         // Betriebszustand, z. B. "Bulk", "Erhaltung"
