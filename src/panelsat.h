// Panel: Empfang der Satelliten (eigene ESP32 mit Bluetooth-Sensoren, siehe sat_proto.h)
// Drei Wege, alle landen im selben Speicher:
//   MQTT       panelsat/<id>/... ueber den Zigbee2MQTT-Broker (Haken in z2m.cpp)
//   UDP        Port SAT_UDP_PORT im WLAN, ohne Broker
//   Bluetooth  Werbepakete im normalen Scan (truma.cpp), Protokoll "Satelliten" in den Bluetooth-Einstellungen
// Die Geraete behalten ihre Schluessel ("truma:<mac>", "mopeka:<mac>", "victron:<mac>") – eine Kachel bleibt
// also gueltig, wenn ein Sensor vom Panel-Bluetooth auf einen Satelliten umzieht. Lokale Sensoren haben Vorrang.
// Schalter: [features] satellites in platformio.ini (-DWITH_SATELLITES=0 baut den Empfang aus)
#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "truma.h"
#include "mopeka.h"
#include "victron.h"
#include "sat_proto.h"

#ifndef WITH_SATELLITES
#define WITH_SATELLITES 1
#endif

inline bool panelsat_enabled() { return WITH_SATELLITES; }

// Eigene Anschluesse der Satelliten (Tanks, Temperatur, Analog, Neigung, Eingaenge, Schalter):
// Schluessel "satio:<Satellit>/<Punkt>", z. B. "satio:Satellite-A1B2C3D4E5F6/tank1"
#define SATIO_KEY_PREFIX "satio:"
inline bool panelsat_is_point_key(const std::string &k) { return k.rfind(SATIO_KEY_PREFIX, 0) == 0; }

struct SatNode {
  std::string id, name, ip, fw;
  int rssi = 0;                 // WLAN-Signal des Satelliten
  bool online = false;          // MQTT: Last Will; UDP/Bluetooth: kuerzlich gehoert
  uint8_t via = 0;              // 1 MQTT, 2 UDP, 4 Bluetooth
  int32_t age_s = -1;           // seit der letzten Nachricht
  size_t devices = 0;
};

void panelsat_begin();
uint32_t panelsat_version();

// MQTT (aus z2m.cpp)
std::vector<std::string> panelsat_topics();
bool panelsat_handle(const std::string &topic, const char *payload, size_t len);   // true = Satelliten-Nachricht

std::vector<SatNode> panelsat_nodes();
bool panelsat_truma_get(const std::string &key, TrumaInfo &out);
bool panelsat_mopeka_get(const std::string &key, MopekaInfo &out);
bool panelsat_victron_get(const std::string &key, VictronInfo &out);
std::vector<TrumaInfo> panelsat_truma_list();
std::vector<MopekaInfo> panelsat_mopeka_list();
std::vector<VictronInfo> panelsat_victron_list();
std::string panelsat_sat_of(const std::string &key);     // Kennung des Satelliten, "" = kein Satellitengeraet
std::string panelsat_via_text(const std::string &key);   // "Satellit Heck (MQTT)"
bool panelsat_measure(const std::string &key);           // LevelControl am Satelliten messen lassen (nur MQTT)
void panelsat_forget(const std::string &id);             // Satelliten aus der Liste nehmen
// Bluetooth-Passwort fuer alle Satelliten (leer = Standard "panelsat"); dasselbe am Satelliten eintragen
std::string panelsat_password();
void panelsat_set_password(const std::string &pw);

std::vector<SatPoint> panelsat_point_list();
bool panelsat_point_get(const std::string &key, SatPoint &out);
// Schalter am Satelliten setzen – per MQTT, UDP oder Bluetooth (verschluesselt), je nachdem was erreichbar ist
bool panelsat_switch(const std::string &key, bool on, std::string *err = nullptr);
