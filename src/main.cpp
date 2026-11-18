// Simple Open Camper (S.O.C.) fuer Waveshare ESP32-S3-Touch-LCD-7B
// Steuert Zigbee2MQTT-Geraete direkt per MQTT; Layout per Web-Editor (http://<panel>/)
#include <Arduino.h>
#include <LittleFS.h>
#include <lvgl.h>

#include "app.h"
#include "app_config.h"
#include "esp_heap_caps.h"
#include "board.h"
#include "layout.h"
#include "net.h"
#include "settings.h"
#include "ui/ui.h"
#include "web_server.h"
#include "z2m.h"
#include "mqtt_broker.h"
#include "truma.h"
#include "victron.h"
#include "cerbo.h"
#include "gx_find.h"
#include "bms.h"
#include "panelsat.h"
#include "mopeka.h"
#include "truma_cpplus.h"
#include "truma_inetx.h"
#include "hymer.h"
#include "alde.h"
#include "sdcard.h"
#include "splash.h"
#include "debug_cmd.h"
#include "zb_direct.h"

static void apply_settings_event(Settings *st) {
  Settings old = settings_copy();
  bool mqtt_changed = old.mqtt_host != st->mqtt_host || old.mqtt_port != st->mqtt_port || old.mqtt_user != st->mqtt_user ||
                      old.mqtt_pass != st->mqtt_pass || old.mqtt_base != st->mqtt_base;
  bool tz_changed = old.tz != st->tz;
  bool restart = old.net_mode != st->net_mode || old.ap_ssid != st->ap_ssid || old.ap_pass != st->ap_pass ||
                 old.ap_channel != st->ap_channel || old.hostname != st->hostname;
  mqtt_changed |= old.mqtt_auto != st->mqtt_auto || old.mqtt_on != st->mqtt_on;
  // Eingebauter Broker an/aus bzw. andere Anmeldung: Panel verbindet sich neu (mit sich selbst bzw. dem alten Broker)
  mqtt_changed |= old.broker_on != st->broker_on || old.broker_user != st->broker_user || old.broker_pass != st->broker_pass;
  bool inetx_off = (old.ble_protos & BLE_P_INETX) && !(st->ble_protos & BLE_P_INETX);
  bool hymer_off = (old.ble_protos & BLE_P_HYMER) && !(st->ble_protos & BLE_P_HYMER);
  settings_apply(*st);
  if (inetx_off) inetx_disconnect();
  if (hymer_off) hymer_disconnect();
  if (restart) {
    // Betriebsart/Panel-WLAN wirken erst nach Neustart; Antwort an den Browser ist bereits raus
    delay(800);
    ESP.restart();
  }
  board_set_backlight(st->brightness);
  if (tz_changed) configTzTime(st->tz.c_str(), "pool.ntp.org", "time.google.com");
  broker_apply();
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
  heap_caps_malloc_extmem_enable(APP_EXTMEM_MIN);   // kleine Speicherstuecke ins PSRAM (app_config.h)
  Serial.begin(115200);
  delay(50);
  log_i("%s %s startet", FW_NAME, FW_VERSION);
  app_events_init();
  debug_mark("start");
  settings_load();
  if (!LittleFS.begin(true)) log_e("LittleFS konnte nicht eingebunden werden");
  layout_init();
  debug_mark("layout");

  if (!board_init()) {
    log_e("Display-Initialisierung fehlgeschlagen");
  }
  debug_mark("display");
  // Startbild (splash.jpg im Projektordner): gleich zeigen, waehrend der Rest startet; verschwindet nach 6 s
  if (splash_show()) {
    lv_timer_handler();
    board_set_backlight(g_settings.brightness);
  }
  debug_mark("startbild");
  log_i("Akkuspannung: %.2f V", board_battery_voltage());
  sd_begin();          // braucht den IO-Expander (CS) aus board_init
  bg_migrate();        // Hintergruende auf die SD-Karte verschieben bzw. alte Rohbilder entfernen
  debug_mark("sd");
  net_begin();         // WLAN/Netzwerk-Stack zuerst – MQTT setzt darauf auf
  debug_mark("wlan");
  panelsat_begin();    // Satelliten (MQTT/UDP/Bluetooth) – vor z2m und Bluetooth-Scan, die hineinliefern
  debug_mark("satelliten");
  z2m_begin();         // vor der Oberflaeche, damit das Geraetemodell existiert
  debug_mark("mqtt");
  cerbo_begin();       // Victron Cerbo GX per MQTT – nur mit WITH_CERBO, sonst leer
  debug_mark("cerbo");
  victron_begin();     // Victron-Geraete (Instant Readout) – vor truma_begin, das den BLE-Scan betreibt
  debug_mark("victron");
  mopeka_begin();      // Mopeka-Tanksensoren – ebenfalls vor truma_begin
  inetx_begin();       // Truma iNet X (Bluetooth) – nur mit WITH_TRUMA_INETX, sonst leer
  debug_mark("inetx");
  hymer_begin();       // Hymer Connect (Bluetooth zur SCU) – nur mit WITH_HYMER, sonst leer
  debug_mark("hymer");
  cpplus_begin();      // Truma CP Plus (LIN-Bus) – nur mit WITH_TRUMA_CPPLUS, sonst leer
  alde_begin();        // Alde Compact 3020/3030 (LIN-Bus) – nur mit WITH_ALDE, sonst leer
  bms_begin();         // Akkus mit BMS (nur gespeicherte Geraete laden; verbunden wird im Bluetooth-Task)
  truma_begin();       // Gasflaschen per Bluetooth (startet BLE erst, wenn ein Sensor eingerichtet ist)
  zbd_begin();         // Zigbee-Funkmodul (ESP32-H2) per UART – nur mit WITH_ZB_DIRECT, sonst leer
  debug_mark("bluetooth");
  ui_init();
  debug_mark("oberflaeche");
  lv_timer_handler();  // erstes Bild zeichnen, dann Licht an
  board_set_backlight(g_settings.brightness);
  web_begin();
  debug_mark("webserver");
}

void loop() {
  debug_poll();       // Befehle ueber die serielle Schnittstelle (debug, tasks, boot, reboot, help)
  static uint8_t marked = 0;   // Speicher nach 10 s und 60 s merken (Geraeteliste von Zigbee2MQTT ist dann da)
  if (marked == 0 && millis() > 10000) { debug_mark("nach 10 s"); marked = 1; }
  if (marked == 1 && millis() > 60000) { debug_mark("nach 60 s"); marked = 2; }
  uint32_t wait = lv_timer_handler();
  handle_events();
  net_loop();
  web_loop();         // Webinterface "bei Bedarf": nach 15 min ohne Zugriff beenden
  ui_loop();
  static uint32_t gx_ms = 0;
  if (millis() - gx_ms >= 1000) {   // GX laenger weg (neue IP?) -> automatisch neu suchen
    gx_ms = millis();
    gx_find_watch();
    static bool brk = false;   // eingebauter Broker: einmal nach dem Start einschalten (wenn gewuenscht)
    if (!brk && millis() > 3000) {
      brk = true;
      broker_apply();
    }
  }
  delay(wait > 10 ? 10 : (wait < 2 ? 2 : wait));
}
