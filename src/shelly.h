// Shelly-Geraete ueber denselben MQTT-Broker wie Zigbee2MQTT (Gen1 und Gen2+)
// Die Geraete erscheinen als ControlView mit Schluessel "shelly:<id>#<komponente>" und werden
// ueber die z2m_*-Funktionen mit abgefragt und gesteuert.
#pragma once
#include <string>
#include <vector>
#include "z2m.h"

inline bool shelly_is_key(const std::string &key) { return key.compare(0, 7, "shelly:") == 0; }

void shelly_init();
std::vector<std::string> shelly_topics();       // zu abonnierende Topics
void shelly_on_connect();                       // nach (Wieder-)Verbindung: Geraete abfragen
bool shelly_handle(const std::string &topic, const char *payload, size_t len);   // true = Shelly-Nachricht
void shelly_tick();                             // aus dem MQTT-Worker, oft aufrufen
void shelly_refresh();
uint32_t shelly_devices_version();
uint32_t shelly_state_version();

bool shelly_get(const std::string &key, ControlView &out);
void shelly_all(std::vector<ControlView> &out);
size_t shelly_count();

void shelly_toggle(const std::string &key);
void shelly_set_on(const std::string &key, bool on);
void shelly_set_brightness_pct(const std::string &key, int pct);
void shelly_set_ct(const std::string &key, int mired);
void shelly_set_color_hs(const std::string &key, float hue, float sat);
