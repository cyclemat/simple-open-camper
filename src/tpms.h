// Reifendruck (TPMS) per Bluetooth LE – SuperSense (comworks) und baugleiche Sensoren. BETA, UNGETESTET:
// Protokoll aus der Android-App "SuperSense" 2.13.121 rekonstruiert (docs/protokolle/supersense_ble.md).
//
// Die Sensoren senden ihre Werte nur in den Werbepaketen (keine Verbindung, kein Schluessel):
//   Typ A: Service Data 0xEEFD oder 0xFAF0, 11 Byte: [0..5] MAC rueckwaerts, [6..7] kPa int16 LE,
//          [8..9] Batterie int16 LE (vermutlich mV), [10] Temperatur = Wert - 50 °C (0xFF = ungueltig)
//   Typ B: Herstellerdaten Firma 0x0100, 16 Byte: [0..5] Kennung, [6..9] Pa int32 LE, [10..13] 0,01 °C int32 LE,
//          [14] Batterie-Code (Formel der App -> mV); Name "TPMS<Rad>_…" bzw. "SSPS-<FL|FR|RL|RR|PS>-…"
// Ob der Druck absolut oder relativ ist, verraet die App nicht – angezeigt wird der Wert wie gesendet.
//
// ALPHA (Wunsch des Nutzers 2026-11-19): liegt im Repo, wird aber nirgends kompiliert. Ohne WITH_TPMS gelten die
// leeren Fassungen unten, tpms.cpp ist dann leer und muss nicht gebaut werden. Einschalten: -DWITH_TPMS=1 und
// tpms.cpp in den build_src_filter. Zur Laufzeit zusaetzlich BLE_P_TPMS (ble_proto_on).
#pragma once
#include <math.h>
#include <stdint.h>
#include <string>
#include <vector>

#ifndef WITH_TPMS
#define WITH_TPMS 0
#endif

#define TPMS_MAX 6
#define TPMS_KEY_PREFIX "tpms:"

class NimBLEAdvertisedDevice;

struct TpmsInfo {
  std::string key;          // "tpms:<kennung>"
  std::string id, name;     // Kennung = MAC des Sensors aus den Daten (aendert sich nicht mit der Funkadresse)
  uint8_t pos = 0;          // 0 keine, 1 vorne links, 2 vorne rechts, 3 hinten links, 4 hinten rechts
  uint8_t type = 0;         // 1 = Typ A (Service Data), 2 = Typ B (Herstellerdaten)
  bool has = false;
  float bar = NAN;          // Druck wie gesendet
  float temp = NAN;         // °C
  int batt_mv = -1;
  int rssi = 0;
  int32_t age_s = -1;
  std::string status;       // "OK", "Warte auf Sensor …", "Kein Empfang – …"
};

struct TpmsFound {
  std::string id, name;
  uint8_t pos = 0, type = 0;
  float bar = NAN, temp = NAN;
  int rssi = 0;
  bool configured = false;
};

static inline bool tpms_is_key(const std::string &k) { return k.rfind(TPMS_KEY_PREFIX, 0) == 0; }

#if WITH_TPMS
void tpms_begin();
uint32_t tpms_version();
bool tpms_has_devices();
bool tpms_on_adv(const NimBLEAdvertisedDevice *dev);   // true = war ein Reifensensor

std::vector<TpmsInfo> tpms_list();
void tpms_discovery_start();          // 3 Minuten suchen
bool tpms_discovery_running();
std::vector<TpmsFound> tpms_found();
std::string tpms_add(const std::string &id, const std::string &name, int pos);   // "" = ok
void tpms_remove(const std::string &id);
void tpms_rename(const std::string &id, const std::string &name);
void tpms_set_pos(const std::string &id, int pos);
const char *tpms_pos_text(uint8_t pos);   // "vorne links" …
#else   // Alpha, nicht eingebaut
static inline void tpms_begin() {}
static inline uint32_t tpms_version() { return 0; }
static inline bool tpms_has_devices() { return false; }
static inline bool tpms_on_adv(const NimBLEAdvertisedDevice *) { return false; }
static inline std::vector<TpmsInfo> tpms_list() { return {}; }
static inline void tpms_discovery_start() {}
static inline bool tpms_discovery_running() { return false; }
static inline std::vector<TpmsFound> tpms_found() { return {}; }
static inline const char *tpms_pos_text(uint8_t) { return ""; }
#endif
