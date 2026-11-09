// Persistente Einstellungen (NVS)
#pragma once
#include <Arduino.h>

enum NetMode : uint8_t {
  NET_STA = 0,    // Netzwerk-Modus: Panel im Heim-WLAN
  NET_AP = 1,     // Router-Modus: Panel spannt eigenes WLAN mit DHCP auf
  NET_APSTA = 2,  // Kombi: eigenes WLAN + Heim-WLAN mit Weiterleitung ins Internet
};

enum SaverMode : uint8_t {
  SAVER_DIM = 0,     // Dashboard abgedunkelt
  SAVER_BLACK = 1,   // schwarzes Bild, Beleuchtung aus (schont das LCD am meisten)
  SAVER_CLOCK = 2,   // dunkle Uhr, wechselt jede Minute die Position
  SAVER_BOUNCE = 3,  // huepfendes Logo mit Farbwechsel
};

// Bluetooth-Protokolle, die das Panel auswertet (in den Einstellungen waehlbar)
enum BleProto : uint8_t {
  BLE_P_TRUMA = 0x01,    // Truma LevelControl (Gasflaschen)
  BLE_P_MOPEKA = 0x02,   // Mopeka-Tanksensoren
  BLE_P_VICTRON = 0x04,  // Victron Instant Readout
  BLE_P_INETX = 0x08,    // Truma iNet X (Heizung)
  BLE_P_SAT = 0x10,      // Satelliten, die ihre Werte per Bluetooth senden
};
#define BLE_P_DEFAULT (BLE_P_TRUMA | BLE_P_MOPEKA | BLE_P_VICTRON)

struct Settings {
  uint8_t ble_protos = BLE_P_DEFAULT;
  uint8_t net_mode = NET_STA;
  String ap_ssid, ap_pass;     // Panel-WLAN (Router/Kombi)
  uint8_t ap_channel = 13;
  bool mqtt_auto = true;       // Broker im Panel-WLAN automatisch suchen (Router/Kombi)
  String wifi_ssid, wifi_pass;
  String hostname;
  String mqtt_host, mqtt_user, mqtt_pass, mqtt_base;
  uint16_t mqtt_port = 1883;
  uint8_t brightness = 80;     // %
  uint16_t dim_after = 120;    // Sekunden, 0 = nie
  uint8_t dim_level = 10;      // %, 0 = Display aus
  uint8_t saver = 0;           // SaverMode: Ruhezustand
  bool saver_recolor = true;   // Huepfendes Logo: Farbe beim Abprallen wechseln
  String tz;
};

extern Settings g_settings;

void settings_load();
void settings_save();
bool settings_mqtt_configured();            // Host gesetzt oder automatische Suche aktiv
bool settings_mqtt_auto_active();           // automatische Broker-Suche im Panel-WLAN
Settings settings_copy();                    // thread-sicher (fuer den Web-Task)
void settings_apply(const Settings &s);      // uebernimmt + speichert (nur aus dem Haupt-Task)
bool ble_proto_on(uint8_t p);                // thread-sicher, z. B. aus dem Bluetooth-Scan
void ble_proto_set(uint8_t p, bool on);      // speichert sofort
