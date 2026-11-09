// WLED-Lichtsteuerungen ueber denselben MQTT-Broker wie Zigbee2MQTT.
// In WLED: Einstellungen -> Sync -> MQTT aktivieren, Broker eintragen, Device Topic bei "wled/<name>" lassen.
// WLED meldet <topic>/g (Helligkeit 0..255, 0 = aus), <topic>/c (Farbe #RRGGBB), <topic>/status (online/offline)
// und <topic>/v (Zustand als XML mit Geraetename). Befehle: <topic> "ON"/"OFF"/"T"/0..255, <topic>/col "#RRGGBB".
// Die Geraete erscheinen als ControlView mit Schluessel "wled:<topic-id>" und laufen ueber die z2m_*-Funktionen.
// Schalter: [features] wled in platformio.ini (-DWITH_WLED=0 baut es aus)
#pragma once
#include <string>
#include <vector>
#include "z2m.h"

#ifndef WITH_WLED
#define WITH_WLED 1
#endif

inline bool wled_enabled() { return WITH_WLED; }
inline bool wled_is_key(const std::string &key) { return WITH_WLED && key.compare(0, 5, "wled:") == 0; }

void wled_init();
std::vector<std::string> wled_topics();          // zu abonnierende Topics
void wled_on_connect();
bool wled_handle(const std::string &topic, const char *payload, size_t len);   // true = WLED-Nachricht
void wled_tick();
uint32_t wled_devices_version();
uint32_t wled_state_version();

bool wled_get(const std::string &key, ControlView &out);
void wled_all(std::vector<ControlView> &out);
size_t wled_count();

void wled_toggle(const std::string &key);
void wled_set_on(const std::string &key, bool on);
void wled_set_brightness_pct(const std::string &key, int pct);
void wled_set_color_hs(const std::string &key, float hue, float sat);
