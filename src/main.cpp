// Simply Open Camper (S.O.C.) fuer Waveshare ESP32-S3-Touch-LCD-7B
// Steuert Zigbee2MQTT-Geraete direkt per MQTT; Layout per Web-Editor (http://<panel>/)
#include <Arduino.h>
#include <LittleFS.h>
#include <lvgl.h>

#include "app.h"
#include "app_config.h"
#include "board.h"
#include "layout.h"
#include "net.h"
#include "settings.h"
#include "ui/ui.h"
#include "web_server.h"
#include "z2m.h"
#include "truma.h"
#include "victron.h"
#include "cerbo.h"
#include "panelsat.h"
#include "mopeka.h"
#include "truma_cpplus.h"
#include "truma_inetx.h"
#include "alde.h"
#include "sdcard.h"

static void apply_settings_event(Settings *st) {
  Settings old = settings_copy();
  bool mqtt_changed = old.mqtt_host != st->mqtt_host || old.mqtt_port != st->mqtt_port || old.mqtt_user != st->mqtt_user ||
                      old.mqtt_pass != st->mqtt_pass || old.mqtt_base != st->mqtt_base;
  bool tz_changed = old.tz != st->tz;
  bool restart = old.net_mode != st->net_mode || old.ap_ssid != st->ap_ssid || old.ap_pass != st->ap_pass ||
                 old.ap_channel != st->ap_channel || old.hostname != st->hostname;
  mqtt_changed |= old.mqtt_auto != st->mqtt_auto;
  bool inetx_off = (old.ble_protos & BLE_P_INETX) && !(st->ble_protos & BLE_P_INETX);
  settings_apply(*st);
  if (inetx_off) inetx_disconnect();
  if (restart) {
    // Betriebsart/Panel-WLAN wirken erst nach Neustart; Antwort an den Browser ist bereits raus
    delay(800);
    ESP.restart();
  }
  board_set_backlight(st->brightness);
  if (tz_changed) configTzTime(st->tz.c_str(), "pool.ntp.org", "time.google.com");
  if (mqtt_changed) z2m_restart();
  ui_settings_changed();
}

static void handle_events() {
  AppEvent ev;
  while (app_poll(ev)) {
    switch (ev.type) {
      case EV_LAYOUT_CHANGED: break;  // Layout-Version wird von der Oberflaeche selbst beobachtet
      case EV_BG_CHANGED: ui_bg_changed(ev.s); break;
      case EV_SETTINGS_CHANGED: {
        Settings *st = (Settings *)ev.ptr;
        apply_settings_event(st);
        delete st;
        break;
      }
      case EV_WIFI_CHANGED: {
        Settings *st = (Settings *)ev.ptr;
        Settings cur = settings_copy();
        cur.wifi_ssid = st->wifi_ssid;
        cur.wifi_pass = st->wifi_pass;
        settings_apply(cur);
        delete st;
        net_reconnect();
        break;
      }
      case EV_SAVER_PREVIEW: ui_saver_start(); break;
      case EV_SAVER_LOGO: ui_saver_logo_changed(); break;
      case EV_REBOOT:
        delay(400);
        ESP.restart();
        break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(50);
  log_i("%s %s startet", FW_NAME, FW_VERSION);
  app_events_init();
  settings_load();
  if (!LittleFS.begin(true)) log_e("LittleFS konnte nicht eingebunden werden");
  layout_init();

  if (!board_init()) {
    log_e("Display-Initialisierung fehlgeschlagen");
  }
  log_i("Akkuspannung: %.2f V", board_battery_voltage());
  sd_begin();          // braucht den IO-Expander (CS) aus board_init
  bg_migrate();        // Hintergruende auf die SD-Karte verschieben bzw. alte Rohbilder entfernen
  net_begin();         // WLAN/Netzwerk-Stack zuerst – MQTT setzt darauf auf
  panelsat_begin();    // Satelliten (MQTT/UDP/Bluetooth) – vor z2m und Bluetooth-Scan, die hineinliefern
  z2m_begin();         // vor der Oberflaeche, damit das Geraetemodell existiert
  cerbo_begin();       // Victron Cerbo GX per MQTT – nur mit WITH_CERBO, sonst leer
  victron_begin();     // Victron-Geraete (Instant Readout) – vor truma_begin, das den BLE-Scan betreibt
  mopeka_begin();      // Mopeka-Tanksensoren – ebenfalls vor truma_begin
  inetx_begin();       // Truma iNet X (Bluetooth) – nur mit WITH_TRUMA_INETX, sonst leer
  cpplus_begin();      // Truma CP Plus (LIN-Bus) – nur mit WITH_TRUMA_CPPLUS, sonst leer
  alde_begin();        // Alde Compact 3020/3030 (LIN-Bus) – nur mit WITH_ALDE, sonst leer
  truma_begin();       // Gasflaschen per Bluetooth (startet BLE erst, wenn ein Sensor eingerichtet ist)
  ui_init();
  lv_timer_handler();  // erstes Bild zeichnen, dann Licht an
  board_set_backlight(g_settings.brightness);
  web_begin();
}

void loop() {
  uint32_t wait = lv_timer_handler();
  handle_events();
  net_loop();
  ui_loop();
  delay(wait > 10 ? 10 : (wait < 2 ? 2 : wait));
}
