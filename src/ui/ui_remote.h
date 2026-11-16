// Zusatzdisplays: kleine Displays (CYD, rundes Panel) zeigen ausgewaehlte Kacheln des Hauptpanels und schalten darueber.
// Thread-sicher (Webserver): Liste lesen, Kachelauswahl setzen, Display entfernen.
#pragma once
#include <string>
#include <vector>

#define RD_UDP_PORT 47812   // Hauptpanel und Zusatzdisplays (eigener Port, Satelliten nutzen 47811)

std::string ui_remote_json();   // {"displays":[{id,name,ip,online,udp,mqtt,age,tiles:[keys]}]}
bool ui_remote_set(const std::string &id, const std::vector<std::string> &keys, const std::string &name);
void ui_remote_forget(const std::string &id);
// MQTT (aus panelsat.cpp, MQTT-Task): <base>/<display>/rd/hello bzw. rd/cmd – wird im UI-Takt verarbeitet
void ui_remote_mqtt_in(const std::string &id, const std::string &sub, const char *payload, size_t len);
