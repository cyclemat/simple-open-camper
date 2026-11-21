// Satellit: eigener ESP32, liest Truma LevelControl, Mopeka und Victron per Bluetooth und schickt die Werte
// per MQTT, UDP und/oder Bluetooth-Werbepaketen an das Panel (siehe sat_proto.h).
// Einrichten: Ohne WLAN-Zugang spannt der Satellit das WLAN "SOC-<id>" auf (Standardpasswort "panelsat", beim
// ersten Oeffnen muss ein eigenes vergeben werden), die Einstellungsseite oeffnet sich von selbst (http://4.3.2.1/).
// Notfall: 3x schnell aus/ein = Einrichtungs-WLAN sofort, 5x = WLAN und Passwort zuruecksetzen (devsetup.h).
// Einrichten vom Panel aus: BOOT 3 s halten (oder 3x schnell aus/ein) -> das Panel uebertraegt WLAN und Passwort
// per Bluetooth (sat_pair.h).
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include "sat_config.h"
#include "../devsetup.h"
#include "../hymer.h"
#include "sat_io.h"
#include "sat_send.h"
#include "sat_web.h"
#include "sat_display.h"
#include "sat_oled.h"
#include "sat_sens.h"
#include "sat_shunt.h"
#ifndef SAT_BLE_SENS
#define SAT_BLE_SENS 1   // 0 = keine Bluetooth-Sensoren (Truma, Mopeka, Victron, Hymer); Bluetooth nur zum Einrichten
#endif
#include "sat_pair.h"
#include "rd_client.h"
#include "../truma.h"
#include "../mopeka.h"
#include "../victron.h"
#include "../dbglog.h"

static const uint32_t AP_AFTER_MS = 30000;    // so lange ohne WLAN: Einrichtungs-WLAN dazuschalten
static const uint32_t AP_OFF_MS = 120000;     // so lange verbunden und niemand im Einrichtungs-WLAN: abschalten

static uint32_t s_lost_ms = 0, s_conn_ms = 0;
static bool s_mdns = false;

static void ap_on() {
  if (WiFi.getMode() & WIFI_AP) return;
  String ssid = "SOC-" + g_sat.id;   // z. B. "SOC-Satellite-AABBCCDDEEFF"
  WiFi.mode(g_sat.wifi_ssid.isEmpty() ? WIFI_AP : WIFI_AP_STA);
  devsetup::ap_start(ssid);   // 4.3.2.1, Captive Portal, eigenes Passwort
}

static void ap_off() {
  if (!(WiFi.getMode() & WIFI_AP)) return;
  devsetup::ap_stop();
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
  if (devsetup::force_ap()) ap_on();   // Mehrfach-Reset: Einrichtungs-WLAN sofort
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
    if (!devsetup::force_ap() && (WiFi.getMode() & WIFI_AP) && now - s_conn_ms > AP_OFF_MS && WiFi.softAPgetStationNum() == 0) ap_off();
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
  if (devsetup::boot_check().factory) {   // 5x schnell neu gestartet: WLAN-Daten loeschen
    g_sat.wifi_ssid = "";
    g_sat.wifi_pass = "";
    sat_config_save();
  }
  log_i("PanelSat %s (%s) startet", SAT_FW_VERSION, g_sat.id.c_str());
  // Debug-Log (ab Werk aus, Weboberflaeche: Fehlersuche): mit PSRAM 32 KB, sonst 8 KB; CYD mit Kartenslot: Dateien
#if DBGLOG_CYD_SD
  dbglog_begin(psramFound() ? 32 * 1024 : 8 * 1024, dbglog_cyd_sd);
#else
  dbglog_begin(psramFound() ? 32 * 1024 : 8 * 1024);
#endif
#if SAT_BLE_SENS
  victron_begin();
  mopeka_begin();
  hymer_begin();       // Hymer Connect (nur mit WITH_HYMER, sonst leer)
#endif
  // Bluetooth gleich starten, wenn der Satellit Werbepakete senden soll (sonst erst mit dem ersten Sensor)
  if (g_sat.modes & SAT_M_BLE) truma_ble_init();
  truma_begin();
  sat_io_begin();
  sat_shunt_begin();    // SmartShunt (nur mit SAT_SHUNT)
  wifi_begin();
  sat_web_begin();
  sat_send_begin();
  sat_pair_begin();     // Einrichten per Knopf (BOOT 3 s) bzw. nach 3x Neustart
#if SAT_REMOTE
  rd_begin();           // Zusatzdisplay: Kacheln vom Hauptpanel
#endif
  sat_display_begin();   // nur mit Display (SAT_DISPLAY), sonst leer
  sat_oled_begin();      // nur mit kleinem OLED (SAT_OLED), sonst leer
  if (sat_display_reset_requested()) {   // beim Einschalten 10 s aufs Display gedrueckt: Passwort zuruecksetzen
    devsetup::password_reset();
    if (WiFi.getMode() & WIFI_AP) {
      ap_off();
      ap_on();
    }
  }
}

void loop() {
  wifi_loop();
  sat_io_loop();
  sat_sens_loop();      // Zusatzsensoren (nur mit SAT_SENS)
  sat_shunt_loop();     // SmartShunt (nur mit SAT_SHUNT)
  sat_web_loop();
  sat_send_loop();
  sat_pair_loop();
#if SAT_REMOTE
  rd_loop();
#endif
  sat_display_loop();
  sat_oled_loop();
  delay(2);
}
