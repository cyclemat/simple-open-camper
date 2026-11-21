// Verbindung zu Home Assistant ueber die Integration "simple-open-camper-HA-Integration" (Beta).
// HA verbindet sich per WebSocket mit dem Panel (ws://<panel>/api/ha – LAN, Tailscale …), meldet sich mit einem
// Zugangscode an und bekommt nur, was es abonniert; umgekehrt stellt HA ausgewaehlte Entitaeten bereit, die auf dem
// Panel wie die Anschluesse eines Satelliten "HA" erscheinen (Schluessel "satio:HA/<punkt>", Gruppe = Bereich in HA).
//
// Nachrichten (JSON-Text, je Rahmen eine):
//   HA -> Panel  {"t":"hello","tok":"…","name":"Home"}      Anmeldung; Antwort {"t":"ok","name","ver"} bzw. {"t":"err"}
//                {"t":"list"}                                Katalog anfordern -> {"t":"list","items":[{id,n,g,k,u,c,b}]}
//                {"t":"sub","ids":[…]}                       nur diese Werte senden -> {"t":"st","v":{id:{…}}} bei Aenderung
//                {"t":"cmd","id":"…","on":true,"bri":50}     schalten/dimmen
//                {"t":"ents","list":[{id,n,dom,u,c,area}]}   bereitgestellte HA-Entitaeten (ersetzt die Liste)
//                {"t":"hst","v":{id:{s,bri,av}}}             deren Zustaende
//                {"t":"ping"}                                -> {"t":"pong"}
//   Panel -> HA  {"t":"call","id":"light.x","on":true}       HA-Kachel am Panel angetippt
// Kein Kreislauf: Was aus HA kommt (und Geraete aus MQTT-Discovery) bietet das Panel HA nicht an.
// Ab Werk AUS (NVS "halink": on, tok, map).
#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "sat_proto.h"

#ifndef WITH_HA_LINK
#define WITH_HA_LINK 0
#endif
#define HA_SAT_ID "HA"

struct HaLinkInfo {
  bool on = false, connected = false;
  std::string token, ha_name;
  uint16_t subs = 0, imports = 0;
};

#if WITH_HA_LINK
void ha_link_begin();
bool ha_link_enabled();
void ha_link_set_enabled(bool on);
std::string ha_link_new_token();
HaLinkInfo ha_link_info();
void ha_link_tick();                       // UI-Task (ui_loop): Katalog, Werte, Befehle
// webserver.cpp
void ha_link_server(void *httpd_handle);   // Webserver gestartet (nullptr = gestoppt)
int ha_link_ws(void *httpd_req);           // Handler fuer /api/ha (WebSocket), liefert esp_err_t
// panelsat.cpp: bereitgestellte HA-Entitaeten wie Satelliten-Punkte
std::vector<SatPoint> ha_link_points();
bool ha_link_switch(uint8_t idx, bool on, std::string *err);
uint32_t ha_link_version();
std::string ha_link_device_name(uint8_t idx);
#else
inline void ha_link_begin() {}
inline bool ha_link_enabled() { return false; }
inline void ha_link_set_enabled(bool) {}
inline std::string ha_link_new_token() { return std::string(); }
inline HaLinkInfo ha_link_info() { return {}; }
inline void ha_link_tick() {}
inline void ha_link_server(void *) {}
inline int ha_link_ws(void *) { return -1; }
inline std::vector<SatPoint> ha_link_points() { return {}; }
inline bool ha_link_switch(uint8_t, bool, std::string *) { return false; }
inline uint32_t ha_link_version() { return 0; }
inline std::string ha_link_device_name(uint8_t) { return std::string(); }
#endif
