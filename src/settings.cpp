#include "settings.h"
#include "app_config.h"
#include <Preferences.h>
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

Settings g_settings;
static Preferences prefs;
static SemaphoreHandle_t s_mtx = nullptr;

static void lock() {
  if (!s_mtx) s_mtx = xSemaphoreCreateRecursiveMutex();
  xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY);
}
static void unlock() { xSemaphoreGiveRecursive(s_mtx); }

Settings settings_copy() {
  lock();
  Settings c = g_settings;
  unlock();
  return c;
}

static volatile uint8_t s_ble = BLE_P_DEFAULT;   // Spiegel fuer andere Tasks

void settings_apply(const Settings &s) {
  lock();
  g_settings = s;
  s_ble = s.ble_protos;
  unlock();
  settings_save();
}

bool ble_proto_on(uint8_t p) { return (s_ble & p) != 0; }

void ble_proto_set(uint8_t p, bool on) {
  lock();
  g_settings.ble_protos = on ? (g_settings.ble_protos | p) : (g_settings.ble_protos & ~p);
  s_ble = g_settings.ble_protos;
  unlock();
  settings_save();
}

void settings_load() {
  lock();
  prefs.begin("panel", true);
  g_settings.net_mode = prefs.getUChar("net_mode", NET_STA);
  g_settings.web_mode = prefs.getUChar("web_mode", 0) ? 1 : 0;
  g_settings.mqtt_on = prefs.getBool("mqtt_on", true);
  g_settings.ap_ssid = prefs.getString("ap_ssid", "");
  g_settings.ap_pass = prefs.getString("ap_pass", "");
#if UI_WEB_SETUP
  g_settings.ap_channel = prefs.getUChar("ap_ch", 6);   // Kanal 6: sehen alle Handys (12/13 nicht ueberall)
#else
  g_settings.ap_channel = prefs.getUChar("ap_ch", 13);
#endif
  g_settings.mqtt_auto = prefs.getBool("mqtt_auto", true);
  g_settings.wifi_ssid = prefs.getString("wifi_ssid", "");
  g_settings.wifi_pass = prefs.getString("wifi_pass", "");
  g_settings.hostname = prefs.getString("hostname", DEFAULT_HOSTNAME);
  g_settings.mqtt_host = prefs.getString("mqtt_host", "");
  g_settings.mqtt_port = prefs.getUShort("mqtt_port", 1883);
  g_settings.mqtt_user = prefs.getString("mqtt_user", "");
  g_settings.mqtt_pass = prefs.getString("mqtt_pass", "");
  g_settings.mqtt_base = prefs.getString("mqtt_base", DEFAULT_MQTT_BASE);
  g_settings.brightness = prefs.getUChar("bl", 80);
  g_settings.dim_after = prefs.getUShort("dim_after", 120);
  g_settings.dim_level = prefs.getUChar("dim_lvl", 10);
  g_settings.saver = prefs.getUChar("saver", SAVER_BLACK);
  g_settings.saver_recolor = prefs.getBool("saver_rc", true);
  g_settings.tz = prefs.getString("tz", DEFAULT_TZ);
  g_settings.ble_protos = prefs.getUChar("ble", BLE_P_DEFAULT);
  // Satelliten senden nur noch per UDP/MQTT (Bluetooth als Datenweg entfallen, 2026-11-15): alten Schalter loeschen
  g_settings.ble_protos &= ~BLE_P_SAT;
#if UI_LEDRING
  g_settings.ring_knob = prefs.getBool("rg_knob", true);
  g_settings.ring_warn = prefs.getBool("rg_warn", true);
  g_settings.ring_warn_pct = prefs.getUChar("rg_wpct", 10);
  g_settings.ring_bri = prefs.getUChar("rg_bri", 30);
#endif
  s_ble = g_settings.ble_protos;
  prefs.end();
  // Leer oder noch ein alter Standardname ("zigbee-panel..." aus der Zeit vor der Umbenennung, "simply-open-camper"):
  // auf den neuen Standard umstellen. Selbst vergebene Namen bleiben.
  bool fresh = false;   // etwas wurde neu belegt oder umgestellt: am Ende speichern
  {
    String h = g_settings.hostname;
    h.toLowerCase();
    if (h.isEmpty() || h.startsWith("zigbee") || h == "simply-open-camper") {
      if (g_settings.hostname != DEFAULT_HOSTNAME) fresh = true;
      g_settings.hostname = DEFAULT_HOSTNAME;
    }
  }
  if (g_settings.mqtt_base.isEmpty()) g_settings.mqtt_base = DEFAULT_MQTT_BASE;
  if (g_settings.net_mode > NET_OFF) g_settings.net_mode = NET_STA;
  if (g_settings.saver > SAVER_DUEL) g_settings.saver = SAVER_BLACK;
#if UI_WEB_SETUP
  g_settings.web_mode = 0;   // ohne Einstellungsmenue am Display liesse sich das Webinterface sonst nicht mehr einschalten
#endif
  if (g_settings.ap_channel < 1 || g_settings.ap_channel > 13) g_settings.ap_channel = 13;
#if UI_WEB_SETUP
  if (g_settings.ap_channel >= 12) g_settings.ap_channel = 6;   // Einrichtung per Handy: Kanal 12/13 sehen nicht alle Geraete
#endif
  if (g_settings.ap_ssid.isEmpty()) {
    char b[32];
    snprintf(b, sizeof(b), "SOC-Panel-%04X", (unsigned)(ESP.getEfuseMac() >> 32) & 0xFFFF);
    g_settings.ap_ssid = b;
    fresh = true;
  }
  if (g_settings.ap_pass.length() < 8) {
    // Gut ablesbares Zufallspasswort (ohne 0/O, 1/l/I)
    const char *abc = "abcdefghjkmnpqrstuvwxyz23456789";
    String pw;
    for (int i = 0; i < 10; i++) pw += abc[esp_random() % strlen(abc)];
    g_settings.ap_pass = pw;
    fresh = true;
  }
  unlock();
  if (fresh) settings_save();
}

void settings_save() {
  lock();
  prefs.begin("panel", false);
  prefs.putUChar("net_mode", g_settings.net_mode);
  prefs.putUChar("web_mode", g_settings.web_mode);
  prefs.putBool("mqtt_on", g_settings.mqtt_on);
  prefs.putString("ap_ssid", g_settings.ap_ssid);
  prefs.putString("ap_pass", g_settings.ap_pass);
  prefs.putUChar("ap_ch", g_settings.ap_channel);
  prefs.putBool("mqtt_auto", g_settings.mqtt_auto);
  prefs.putString("wifi_ssid", g_settings.wifi_ssid);
  prefs.putString("wifi_pass", g_settings.wifi_pass);
  prefs.putString("hostname", g_settings.hostname);
  prefs.putString("mqtt_host", g_settings.mqtt_host);
  prefs.putUShort("mqtt_port", g_settings.mqtt_port);
  prefs.putString("mqtt_user", g_settings.mqtt_user);
  prefs.putString("mqtt_pass", g_settings.mqtt_pass);
  prefs.putString("mqtt_base", g_settings.mqtt_base);
  prefs.putUChar("bl", g_settings.brightness);
  prefs.putUShort("dim_after", g_settings.dim_after);
  prefs.putUChar("dim_lvl", g_settings.dim_level);
  prefs.putUChar("saver", g_settings.saver);
  prefs.putBool("saver_rc", g_settings.saver_recolor);
  prefs.putString("tz", g_settings.tz);
  prefs.putUChar("ble", g_settings.ble_protos);
#if UI_LEDRING
  prefs.putBool("rg_knob", g_settings.ring_knob);
  prefs.putBool("rg_warn", g_settings.ring_warn);
  prefs.putUChar("rg_wpct", g_settings.ring_warn_pct);
  prefs.putUChar("rg_bri", g_settings.ring_bri);
#endif
  prefs.end();
  unlock();
}

bool settings_mqtt_auto_active() {
  return (g_settings.net_mode == NET_AP || g_settings.net_mode == NET_APSTA) && g_settings.mqtt_auto;
}
bool settings_mqtt_configured() { return !g_settings.mqtt_host.isEmpty() || settings_mqtt_auto_active(); }
