#include "sat_config.h"
#include "../hymer.h"
#include <Preferences.h>
#include "esp_mac.h"
#include "../sat_proto.h"

SatConfig g_sat;
static volatile uint8_t s_ble = BLE_P_DEFAULT;   // Spiegel fuer den Bluetooth-Task
static volatile bool s_listen = false;            // Schaltbefehle des Panels per Bluetooth hoeren

String sat_ble_mac() {
  uint8_t m[6];
  esp_read_mac(m, ESP_MAC_BT);
  char b[18];
  snprintf(b, sizeof(b), "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
  return b;
}

#if SAT_REMOTE
// Zusatzdisplay: "Display-<MAC>" – so erkennt das Panel es (und fuehrt es nicht als Satellit)
String sat_default_id() {
  String s = sat_ble_name(sat_ble_mac().c_str()).c_str();
  s.replace("Satellite-", "Display-");
  return s;
}
#else
String sat_default_id() { return sat_ble_name(sat_ble_mac().c_str()).c_str(); }
#endif

void sat_config_load() {
  Preferences p;
  p.begin("sat", true);
  g_sat.id = p.getString("id", sat_default_id());
  g_sat.name = p.getString("name", "Satellit");
  g_sat.wifi_ssid = p.getString("ssid", "");
  g_sat.wifi_pass = p.getString("wpass", "");
  g_sat.mqtt_host = p.getString("mhost", "");
  g_sat.mqtt_port = p.getUShort("mport", 1883);
  g_sat.mqtt_user = p.getString("muser", "");
  g_sat.mqtt_pass = p.getString("mpass", "");
  g_sat.base = p.getString("base", SAT_DEFAULT_BASE);
  g_sat.ha = p.getBool("ha", true);
  g_sat.ha_prefix = p.getString("hapre", "homeassistant");
  // iNet X gibt es im Satelliten (noch) nicht; Hymer nur mit WITH_HYMER
  // ab Werk alles aus (Wunsch 2026-11-15): nur einschalten, was am Satelliten gebraucht wird
  g_sat.ble = p.getUChar("ble", 0) & (BLE_P_TRUMA | BLE_P_MOPEKA | BLE_P_VICTRON | (WITH_HYMER ? BLE_P_HYMER : 0));
  // Sendewege: MQTT und UDP (ab Werk aus; die Einrichtung per Knopf schaltet UDP ein). Bluetooth als Datenweg ist
  // entfallen (zu langsam) – Bluetooth dient nur noch zum Einrichten (sat_pair.cpp)
  g_sat.modes = p.getUChar("modes", 0) & (SAT_M_MQTT | SAT_M_UDP);
  g_sat.udp_target = p.getString("udpip", "");
  g_sat.ble_pw = p.getString("bpw", SAT_DEFAULT_PASSWORD);
  p.end();
  if (g_sat.id.isEmpty()) g_sat.id = sat_default_id();
  if (g_sat.base.isEmpty()) g_sat.base = SAT_DEFAULT_BASE;
  s_ble = g_sat.ble;
}

void sat_config_save() {
  Preferences p;
  p.begin("sat", false);
  p.putString("id", g_sat.id);
  p.putString("name", g_sat.name);
  p.putString("ssid", g_sat.wifi_ssid);
  p.putString("wpass", g_sat.wifi_pass);
  p.putString("mhost", g_sat.mqtt_host);
  p.putUShort("mport", g_sat.mqtt_port);
  p.putString("muser", g_sat.mqtt_user);
  p.putString("mpass", g_sat.mqtt_pass);
  p.putString("base", g_sat.base);
  p.putBool("ha", g_sat.ha);
  p.putString("hapre", g_sat.ha_prefix);
  p.putUChar("ble", g_sat.ble);
  p.putUChar("modes", g_sat.modes);
  p.putString("udpip", g_sat.udp_target);
  p.putString("bpw", g_sat.ble_pw);
  p.end();
  s_ble = g_sat.ble;
}

String sat_topic(const char *sub) { return g_sat.base + "/" + g_sat.id + "/" + sub; }

// ---------------------------------------------------------------- fuer truma.cpp (Bluetooth-Scan)
bool ble_proto_on(uint8_t p) {
  if (p == BLE_P_SAT) return s_listen;   // im Satelliten: Befehle des Panels (scannen, solange Schalter da sind)
  return (s_ble & p) != 0;
}

void sat_ble_listen(bool on) { s_listen = on; }

void ble_proto_set(uint8_t p, bool on) {
  g_sat.ble = on ? (g_sat.ble | p) : (g_sat.ble & ~p);
  sat_config_save();
}
