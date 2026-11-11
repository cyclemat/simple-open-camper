// Satellit: eigener ESP32, liest Truma LevelControl, Mopeka und Victron per Bluetooth und schickt die Werte
// per MQTT, UDP und/oder Bluetooth-Werbepaketen an das Panel (siehe sat_proto.h).
// Einrichten: Ohne WLAN-Zugang spannt der Satellit das WLAN "SOC-<id>" auf (Passwort "panelsat"),
// die Weboberflaeche liegt dann unter http://192.168.4.1/
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include "sat_config.h"
#include "../hymer.h"
#include "sat_io.h"
#include "sat_send.h"
#include "sat_web.h"
#include "sat_display.h"
#include "../truma.h"
#include "../mopeka.h"
#include "../victron.h"

static const uint32_t AP_AFTER_MS = 30000;    // so lange ohne WLAN: Einrichtungs-WLAN dazuschalten
static const uint32_t AP_OFF_MS = 120000;     // so lange verbunden und niemand im Einrichtungs-WLAN: abschalten

static uint32_t s_lost_ms = 0, s_conn_ms = 0;
static bool s_mdns = false;

static void ap_on() {
  if (WiFi.getMode() & WIFI_AP) return;
  String ssid = "SOC-" + g_sat.id;   // z. B. "SOC-Satellite-AABBCCDDEEFF"
  WiFi.mode(g_sat.wifi_ssid.isEmpty() ? WIFI_AP : WIFI_AP_STA);
  WiFi.softAP(ssid.c_str(), SAT_AP_PASS);
  log_i("Einrichtungs-WLAN %s an (Passwort %s), http://%s/", ssid.c_str(), SAT_AP_PASS, WiFi.softAPIP().toString().c_str());
}

static void ap_off() {
  if (!(WiFi.getMode() & WIFI_AP)) return;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  log_i("Einrichtungs-WLAN aus");
}

static void wifi_begin() {
  WiFi.persistent(false);
  WiFi.setHostname(g_sat.id.c_str());
  if (g_sat.wifi_ssid.isEmpty()) {
    ap_on();
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(g_sat.wifi_ssid.c_str(), g_sat.wifi_pass.c_str());
  s_lost_ms = millis();
}

static void wifi_loop() {
  if (g_sat.wifi_ssid.isEmpty()) return;
  uint32_t now = millis();
  if (WiFi.isConnected()) {
    if (!s_conn_ms) {
      s_conn_ms = now;
      log_i("WLAN verbunden: %s", WiFi.localIP().toString().c_str());
      if (!s_mdns) s_mdns = MDNS.begin(g_sat.id.c_str());
      if (s_mdns) MDNS.addService("http", "tcp", 80);
    }
    s_lost_ms = 0;
    if ((WiFi.getMode() & WIFI_AP) && now - s_conn_ms > AP_OFF_MS && WiFi.softAPgetStationNum() == 0) ap_off();
  } else {
    s_conn_ms = 0;
    if (!s_lost_ms) s_lost_ms = now;
    if (now - s_lost_ms > AP_AFTER_MS) ap_on();
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  sat_config_load();
  log_i("PanelSat %s (%s) startet", SAT_FW_VERSION, g_sat.id.c_str());
  victron_begin();
  mopeka_begin();
  hymer_begin();       // Hymer Connect (nur mit WITH_HYMER, sonst leer)
  // Bluetooth gleich starten, wenn der Satellit Werbepakete senden soll (sonst erst mit dem ersten Sensor)
  if (g_sat.modes & SAT_M_BLE) truma_ble_init();
  truma_begin();
  sat_io_begin();
  wifi_begin();
  sat_web_begin();
  sat_send_begin();
  sat_display_begin();   // nur mit Display (SAT_DISPLAY), sonst leer
}

void loop() {
  wifi_loop();
  sat_io_loop();
  sat_web_loop();
  sat_send_loop();
  sat_display_loop();
  delay(2);
}
