// Geraete aus MQTT-Discovery (Home-Assistant-Format) auf dem Panel: ESPHome (mit "mqtt:"), Tasmota, OpenMQTTGateway …
// Das Panel liest die Konfigurationen unter <praefix>/<art>/[<knoten>/]<objekt>/config und zeigt die Geraete wie die
// Anschluesse eines Satelliten "MQTT" (Schluessel "satio:MQTT/<punkt>"): Sensoren (Wert + Einheit), Binaersensoren,
// Schalter und Lampen (ein/aus). Damit gehen Kacheln, Regeln, Live-Ansicht, App und Zusatzdisplays ohne Zusatzarbeit.
//
// Kein Kreislauf: Geraete von Simple Open Camper selbst (Satelliten, spaeter das Panel) und von Zigbee2MQTT (kennt das
// Panel direkt) werden uebergangen; das Panel meldet hieraus nichts weiter.
// Ab Werk AUS (NVS "mqdisco": on, pre). Punktnummern bleiben ueber Neustarts gleich (NVS "map": Hash der unique_id -> Nr).
#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "sat_proto.h"

#ifndef WITH_MQTT_DISCO
#define WITH_MQTT_DISCO 0
#endif
#define MQD_SAT_ID "MQTT"

struct MqdInfo {
  uint16_t entities = 0;   // uebernommene Entitaeten
  uint16_t devices = 0;    // davon Geraete
  uint16_t skipped = 0;    // uebergangen (eigene, Zigbee2MQTT, nicht unterstuetzte Art)
};

#if WITH_MQTT_DISCO
void mqd_begin();
bool mqd_enabled();
std::string mqd_prefix();
void mqd_set(bool on, const std::string &prefix);   // speichert und verbindet MQTT neu (z2m_restart)
MqdInfo mqd_info();
// fuer z2m.cpp
std::vector<std::string> mqd_topics();                // beim (Neu-)Verbinden abonnieren
std::vector<std::string> mqd_take_new_topics();       // neu entdeckte Zustands-Topics (Arbeits-Task)
bool mqd_handle(const std::string &topic, const char *payload, size_t len);   // MQTT-Task; true = erledigt
// fuer panelsat.cpp (Punkte wie ein Satellit)
std::vector<SatPoint> mqd_points();
bool mqd_switch(uint8_t idx, bool on, std::string *err);
uint32_t mqd_version();
std::string mqd_device_name(uint8_t idx);   // Geraet, zu dem der Punkt gehoert (Gruppe in der Geraeteliste)
#else
inline void mqd_begin() {}
inline bool mqd_enabled() { return false; }
inline std::string mqd_prefix() { return "homeassistant"; }
inline void mqd_set(bool, const std::string &) {}
inline MqdInfo mqd_info() { return {}; }
inline std::vector<std::string> mqd_topics() { return {}; }
inline std::vector<std::string> mqd_take_new_topics() { return {}; }
inline bool mqd_handle(const std::string &, const char *, size_t) { return false; }
inline std::vector<SatPoint> mqd_points() { return {}; }
inline bool mqd_switch(uint8_t, bool, std::string *) { return false; }
inline uint32_t mqd_version() { return 0; }
inline std::string mqd_device_name(uint8_t) { return std::string(); }
#endif
