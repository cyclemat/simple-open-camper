// Satellit: Weboberflaeche zum Einrichten (WLAN, Sendewege, Bluetooth-Geraete, Firmware)
#include "sat_web.h"
#include "sat_config.h"
#include "rd_client.h"
#include "../updater_link.h"
#if SAT_REMOTE
#include <HTTPClient.h>
#endif
#include "sat_io.h"
#include "sat_send.h"
#include "sat_page.h"
#include "sat_display.h"
#include "sat_oled.h"
#include "../sat_proto.h"
#include "../truma.h"
#include "../mopeka.h"
#include "../victron.h"
#include "../hymer.h"
#include "../devsetup.h"

#include <ArduinoJson.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <new>
#include <functional>

static WebServer s_srv(80);
static uint32_t s_restart_at = 0;

// In Stuecken senden statt als ein grosser Textblock: beim CYD (ohne PSRAM, zerstueckelter Speicher) fand sich fuer
// grosse Antworten (z. B. 36 Kacheln) kein zusammenhaengender Block mehr -> Absturz nach dem Anmelden
static void send_json(const JsonDocument &d, int code = 200) {
  struct Chunks : Print {
    char b[512];
    size_t n = 0;
    size_t write(uint8_t c) override {
      b[n++] = (char)c;
      if (n == sizeof(b)) flush();
      return 1;
    }
    size_t write(const uint8_t *p, size_t k) override {
      for (size_t i = 0; i < k; i++) write(p[i]);
      return k;
    }
    void flush() {
      if (n) s_srv.sendContent(b, n);
      n = 0;
    }
  } w;
  s_srv.setContentLength(measureJson(d));
  s_srv.send(code, "application/json", "");
  serializeJson(d, w);
  w.flush();
}
// Angemeldete Zugriffe: Display entlaedt waehrenddessen seine Kacheln (Zusatzdisplay, wenig RAM)
static std::function<void()> G(std::function<void()> fn) {
  return devsetup::guard([fn] {
    sat_display_web_touch();
    fn();
  });
}
static void send_ok() { s_srv.send(200, "application/json", "{\"ok\":true}"); }
static void send_err(const String &msg) {
  JsonDocument d;
  d["error"] = msg;
  send_json(d, 400);
}
static bool body(JsonDocument &d) {
  if (deserializeJson(d, s_srv.arg("plain"))) {
    send_err("Ungültiges JSON");
    return false;
  }
  return true;
}

static const char *vic_type(VicKind k) { return k == VIC_SOLAR ? "solar" : k == VIC_SHUNT ? "shunt" : k == VIC_DCDC ? "dcdc" : "other"; }

// ---------------------------------------------------------------- Zustand
static void h_state() {
  JsonDocument d;
  d["id"] = g_sat.id;
  d["name"] = g_sat.name;
  d["fw"] = SAT_FW_VERSION;
  d["chip"] = sat_io_chip();
  d["uptime"] = millis() / 1000;
  d["heap"] = ESP.getFreeHeap();
  d["display"] = sat_display_present();
  d["oled"] = (bool)SAT_OLED;
  d["upd_slot"] = (uint32_t)updater_slot_size();   // 0 = Aufteilung ohne Updater (Upload ersetzt die Firmware direkt)
  d["upd_ok"] = updater_available();
#if SAT_REMOTE
  d["remote"] = true;   // Zusatzdisplay: Kacheln vom Hauptpanel
  d["rd"] = rd_status();
#endif
  if (sat_display_present()) sat_display_light(d["light"].to<JsonObject>());
  JsonObject w = d["wifi"].to<JsonObject>();
  w["ssid"] = g_sat.wifi_ssid;
  w["connected"] = WiFi.isConnected();
  w["ip"] = WiFi.isConnected() ? WiFi.localIP().toString() : "";
  w["rssi"] = WiFi.isConnected() ? WiFi.RSSI() : 0;
  w["ap"] = (WiFi.getMode() & WIFI_AP) != 0;
  w["ap_ssid"] = WiFi.softAPSSID();
  JsonObject m = d["mqtt"].to<JsonObject>();
  m["host"] = g_sat.mqtt_host;
  m["port"] = g_sat.mqtt_port;
  m["user"] = g_sat.mqtt_user;
  m["has_pass"] = !g_sat.mqtt_pass.isEmpty();
  m["base"] = g_sat.base;
  m["connected"] = sat_mqtt_connected();
  m["status"] = sat_mqtt_status();
  m["ha"] = g_sat.ha;
  m["ha_prefix"] = g_sat.ha_prefix;
  JsonObject md = d["modes"].to<JsonObject>();
  md["mqtt"] = (g_sat.modes & SAT_M_MQTT) != 0;
  md["udp"] = (g_sat.modes & SAT_M_UDP) != 0;
  md["ble"] = (g_sat.modes & SAT_M_BLE) != 0;
  d["udp_target"] = g_sat.udp_target;
  d["udp_sent"] = sat_udp_sent();
  d["adv_count"] = sat_adv_count();
  d["ble_name"] = sat_ble_name_now();
  d["ble_pw"] = g_sat.ble_pw;
  JsonObject b = d["ble"].to<JsonObject>();
  b["truma"] = (g_sat.ble & BLE_P_TRUMA) != 0;
  b["mopeka"] = (g_sat.ble & BLE_P_MOPEKA) != 0;
  b["victron"] = (g_sat.ble & BLE_P_VICTRON) != 0;
  if (hymer_enabled()) {
    b["hymer"] = (g_sat.ble & BLE_P_HYMER) != 0;
    JsonObject hy = d["hymer"].to<JsonObject>();
    hy["paired"] = hymer_paired();
    hy["pairing"] = hymer_pairing();
    hy["connected"] = hymer_connected();
    hy["status"] = hymer_status();
    JsonArray hv = hy["values"].to<JsonArray>();
    for (auto &p : hymer_points()) {
      JsonObject o = hv.add<JsonObject>();
      o["id"] = p.id;
      o["name"] = p.name;
      if (p.kind == SP_SWITCH || p.kind == SP_INPUT) o["on"] = p.on;
      else if (!isnan(p.value)) o["value"] = roundf(p.value * 100) / 100;
      o["unit"] = p.unit;
      o["switch"] = p.kind == SP_SWITCH;
      o["onoff"] = p.cls == SC_ONOFF;
    }
  }
  d["cmd_count"] = sat_cmd_count();
  sat_io_state_json(d["io"].to<JsonObject>());

  JsonObject tr = d["truma"].to<JsonObject>();
  tr["interval"] = truma_interval();
  tr["scanning"] = truma_scan_running();
  tr["max"] = TRUMA_MAX_SENSORS;
  JsonArray ts = tr["sensors"].to<JsonArray>();
  for (auto &t : truma_list()) {
    JsonObject o = ts.add<JsonObject>();
    o["mac"] = t.mac;
    o["name"] = t.name;
    o["has"] = t.has_reading;
    o["percent"] = t.percent;
    o["kg"] = t.kg;
    o["battery"] = t.battery;
    o["age"] = t.age_s;
    o["busy"] = t.busy;
    o["status"] = t.status;
  }
  JsonArray tf = tr["found"].to<JsonArray>();
  for (auto &f : truma_scan_results()) {
    JsonObject o = tf.add<JsonObject>();
    o["mac"] = f.mac;
    o["addr_type"] = f.addr_type;
    o["rssi"] = f.rssi;
    o["configured"] = f.configured;
  }

  JsonObject mo = d["mopeka"].to<JsonObject>();
  mo["scanning"] = mopeka_discovery_running();
  mo["max"] = MOPEKA_MAX_SENSORS;
  JsonArray tanks = mo["tanks"].to<JsonArray>();
  for (size_t i = 0; i < mopeka_tank_count(); i++) tanks.add(mopeka_tank(i).name);
  JsonArray ms = mo["sensors"].to<JsonArray>();
  for (auto &x : mopeka_list()) {
    JsonObject o = ms.add<JsonObject>();
    o["mac"] = x.mac;
    o["name"] = x.name;
    o["model"] = x.model_text;
    o["tank"] = x.tank;
    o["empty"] = x.empty_mm;
    o["full"] = x.full_mm;
    o["capacity"] = x.capacity;
    o["unit"] = x.unit;
    o["has"] = x.has;
    o["percent"] = x.percent;
    o["mm"] = x.level_mm;
    o["amount"] = x.amount;
    o["battery"] = x.battery;
    o["age"] = x.age_s;
    o["status"] = x.status;
  }
  JsonArray mf = mo["found"].to<JsonArray>();
  for (auto &f : mopeka_found()) {
    JsonObject o = mf.add<JsonObject>();
    o["mac"] = f.mac;
    o["model"] = f.model_text;
    o["rssi"] = f.rssi;
    o["button"] = f.button;
    o["configured"] = f.configured;
  }

  JsonObject vi = d["victron"].to<JsonObject>();
  vi["scanning"] = victron_discovery_running();
  vi["max"] = VICTRON_MAX_DEVICES;
  JsonArray vs = vi["devices"].to<JsonArray>();
  for (auto &v : victron_list()) {
    JsonObject o = vs.add<JsonObject>();
    o["mac"] = v.mac;
    o["name"] = v.name;
    o["vtype"] = vic_type(v.kind);
    o["kind_text"] = victron_kind_text(v.kind);
    o["has"] = v.has;
    o["age"] = v.age_s;
    o["status"] = v.status;
    if (v.has) {
      if (!isnan(v.bat_v)) o["v"] = v.bat_v;
      if (!isnan(v.bat_i)) o["i"] = v.bat_i;
      if (v.kind == VIC_SOLAR && !isnan(v.pv_w)) o["pv"] = v.pv_w;
      if (v.kind == VIC_SHUNT && !isnan(v.soc)) o["soc"] = v.soc;
      if (v.state >= 0) o["state_text"] = victron_state_text(v.state);
    }
  }
  JsonArray vf = vi["found"].to<JsonArray>();
  for (auto &f : victron_found()) {
    JsonObject o = vf.add<JsonObject>();
    o["mac"] = f.mac;
    o["name"] = f.name;
    o["vtype"] = vic_type(victron_kind_of_record(f.record));
    o["rssi"] = f.rssi;
    o["configured"] = f.configured;
  }
  send_json(d);
}

// ---------------------------------------------------------------- Einstellungen
static void h_config() {
  JsonDocument d;
  if (!body(d)) return;
  bool wifi_changed = false, id_changed = false;
  if (d["name"].is<const char *>()) g_sat.name = String((const char *)d["name"]).substring(0, 32);
  if (d["id"].is<const char *>()) {
    String id = d["id"].as<const char *>();
    id.trim();
    for (size_t i = 0; i < id.length(); i++) {
      char c = id[i];
      if (!isalnum((unsigned char)c) && c != '-' && c != '_') return send_err("Die Kennung darf nur a–z, 0–9, - und _ enthalten");
    }
    if (id.isEmpty()) id = sat_default_id();
    id_changed = id != g_sat.id;
    g_sat.id = id.substring(0, 32);
  }
  JsonObject w = d["wifi"];
  if (!w.isNull()) {
    if (w["ssid"].is<const char *>() && String((const char *)w["ssid"]) != g_sat.wifi_ssid) {
      g_sat.wifi_ssid = (const char *)w["ssid"];
      wifi_changed = true;
    }
    if (w["pass"].is<const char *>() && strlen((const char *)w["pass"]) > 0) {
      g_sat.wifi_pass = (const char *)w["pass"];
      wifi_changed = true;
    }
  }
  JsonObject m = d["mqtt"];
  if (!m.isNull()) {
    if (m["host"].is<const char *>()) { g_sat.mqtt_host = (const char *)m["host"]; g_sat.mqtt_host.trim(); }
    if (m["port"].is<int>()) g_sat.mqtt_port = constrain((int)m["port"], 1, 65535);
    if (m["user"].is<const char *>()) g_sat.mqtt_user = (const char *)m["user"];
    if (m["pass"].is<const char *>() && strlen((const char *)m["pass"]) > 0) g_sat.mqtt_pass = (const char *)m["pass"];
    if (m["clear_pass"] | false) g_sat.mqtt_pass = "";
    if (m["base"].is<const char *>()) { g_sat.base = (const char *)m["base"]; g_sat.base.trim(); }
    if (g_sat.base.isEmpty()) g_sat.base = SAT_DEFAULT_BASE;
    if (m["ha"].is<bool>()) g_sat.ha = m["ha"];
    if (m["ha_prefix"].is<const char *>()) g_sat.ha_prefix = (const char *)m["ha_prefix"];
    if (g_sat.ha_prefix.isEmpty()) g_sat.ha_prefix = "homeassistant";
  }
  JsonObject md = d["modes"];
  if (!md.isNull()) {
    auto set_bit = [&](const char *k, uint8_t b) {
      if (md[k].is<bool>()) g_sat.modes = md[k].as<bool>() ? (g_sat.modes | b) : (g_sat.modes & ~b);
    };
    set_bit("mqtt", SAT_M_MQTT);
    set_bit("udp", SAT_M_UDP);
  }
  if (d["ble_pw"].is<const char *>()) {
    g_sat.ble_pw = (const char *)d["ble_pw"];
    if (g_sat.ble_pw.isEmpty()) g_sat.ble_pw = SAT_DEFAULT_PASSWORD;
  }
  if (d["udp_target"].is<const char *>()) { g_sat.udp_target = (const char *)d["udp_target"]; g_sat.udp_target.trim(); }
  JsonObject b = d["ble"];
  if (!b.isNull()) {
    auto set_bit = [&](const char *k, uint8_t p) {
      if (b[k].is<bool>()) g_sat.ble = b[k].as<bool>() ? (g_sat.ble | p) : (g_sat.ble & ~p);
    };
    set_bit("truma", BLE_P_TRUMA);
    set_bit("mopeka", BLE_P_MOPEKA);
    set_bit("victron", BLE_P_VICTRON);
    if (hymer_enabled()) set_bit("hymer", BLE_P_HYMER);
  }
  bool hymer_off = !(g_sat.ble & BLE_P_HYMER);
  sat_config_save();
  if (hymer_off) hymer_disconnect();
  if (wifi_changed || id_changed) {
    // WLAN bzw. Topics aendern sich: sauber neu starten
    s_restart_at = millis() + 1500;
    s_srv.send(200, "application/json", "{\"ok\":true,\"restart\":true}");
    return;
  }
  sat_send_reconfigure();
  send_ok();
}

static void h_wifiscan() {
  int n = WiFi.scanNetworks(false, false);
  JsonDocument d;
  JsonArray a = d["networks"].to<JsonArray>();
  for (int i = 0; i < n && a.size() < 30; i++) {   // Liste kommt nach Signal sortiert: erster Eintrag je Name
    if (WiFi.SSID(i).isEmpty()) continue;
    bool dup = false;
    for (int j = 0; j < i && !dup; j++) dup = WiFi.SSID(j) == WiFi.SSID(i);
    if (dup) continue;
    JsonObject o = a.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  send_json(d);
}

// ---------------------------------------------------------------- Geraete
static void h_truma() {
  JsonDocument d;
  if (!body(d)) return;
  if (d["scan"] | false) truma_scan_start();
  if (!d["add"].isNull()) {
    std::string err = truma_add(d["add"]["mac"] | "", d["add"]["addr_type"] | 0, d["add"]["name"] | "");
    if (!err.empty()) return send_err(err.c_str());
  }
  if (d["remove"].is<const char *>()) truma_remove(d["remove"].as<const char *>());
  if (!d["rename"].isNull()) truma_rename(d["rename"]["mac"] | "", d["rename"]["name"] | "");
  if (d["measure"].is<const char *>()) truma_measure(d["measure"].as<const char *>());
  if (d["interval"].is<int>()) truma_set_interval(constrain((int)d["interval"], 1, 1440));
  send_ok();
}

static void h_mopeka() {
  JsonDocument d;
  if (!body(d)) return;
  if (d["scan"] | false) mopeka_discovery_start();
  if (!d["add"].isNull()) {
    std::string err = mopeka_add(d["add"]["mac"] | "", d["add"]["name"] | "", d["add"]["tank"] | 0);
    if (!err.empty()) return send_err(err.c_str());
  }
  if (d["remove"].is<const char *>()) mopeka_remove(d["remove"].as<const char *>());
  if (!d["rename"].isNull()) mopeka_rename(d["rename"]["mac"] | "", d["rename"]["name"] | "");
  if (!d["tank"].isNull()) {
    std::string err = mopeka_set_tank(d["tank"]["mac"] | "", d["tank"]["tank"] | 0, d["tank"]["empty"] | 38, d["tank"]["full"] | 366,
                                      d["tank"]["capacity"] | 11.0f);
    if (!err.empty()) return send_err(err.c_str());
  }
  send_ok();
}

static void h_victron() {
  JsonDocument d;
  if (!body(d)) return;
  if (d["scan"] | false) victron_discovery_start();
  if (!d["add"].isNull()) {
    std::string err = victron_add(d["add"]["mac"] | "", d["add"]["key"] | "", d["add"]["name"] | "");
    if (!err.empty()) return send_err(err.c_str());
  }
  if (!d["key"].isNull()) {
    std::string err = victron_set_key(d["key"]["mac"] | "", d["key"]["key"] | "");
    if (!err.empty()) return send_err(err.c_str());
  }
  if (d["remove"].is<const char *>()) victron_remove(d["remove"].as<const char *>());
  if (!d["rename"].isNull()) victron_rename(d["rename"]["mac"] | "", d["rename"]["name"] | "");
  send_ok();
}

// Hymer Connect: {"pair":true} | {"forget":true} | {"switch":"switch112","on":true}
static void h_hymer() {
  JsonDocument d;
  if (!body(d)) return;
  if (!hymer_enabled()) return send_err("Hymer ist in dieser Firmware nicht eingebaut.");
  if (d["pair"] | false) hymer_pair_start();
  if (d["forget"] | false) hymer_forget();
  if (d["switch"].is<const char *>()) {
    SatPointKind k;
    uint8_t idx;
    if (!sat_point_parse(d["switch"].as<const char *>(), k, idx) || k != SP_SWITCH ||
        !sat_io_switch(idx, d["on"].is<bool>() ? (d["on"].as<bool>() ? 1 : 0) : 2))
      return send_err("Schalten ging nicht – Hymer verbunden?");
  }
  send_ok();
}

// ---------------------------------------------------------------- eigene Anschluesse (GPIO)
// Display (nur Firmware mit Anzeige): Kacheln, Reihenfolge, Namen, Helligkeit
static void h_display_get() {
  try {
    JsonDocument d;
    sat_display_json(d.to<JsonObject>());
    send_json(d);
  } catch (const std::bad_alloc &) {   // nicht neu starten, wenn der Speicher knapp ist
    send_err("Zu wenig Speicher – bitte weniger Kacheln auswählen");
  }
}

static void h_display_set() {
  JsonDocument d;
  if (!body(d)) return;
  if (!sat_display_apply(d.as<JsonObjectConst>())) return send_err("Diese Firmware hat kein Display");
  send_ok();
}

// Kleines OLED (nur Firmware mit SAT_OLED): was angezeigt wird, Drehen, Kontrast
static void h_oled_get() {
  JsonDocument d;
  sat_oled_json(d.to<JsonObject>());
  send_json(d);
}
static void h_oled_set() {
  JsonDocument d;
  if (!body(d)) return;
  if (!sat_oled_apply(d.as<JsonObjectConst>())) return send_err("Diese Firmware hat kein OLED");
  send_ok();
}

static void h_io_get() {
  JsonDocument d;
  sat_io_config_json(d["config"].to<JsonObject>());
  sat_io_pins_json(d["pins"].to<JsonArray>());
  send_json(d);
}

static void h_io_set() {
  JsonDocument d;
  if (!body(d)) return;
  String err = sat_io_set_config(d.as<JsonObjectConst>());
  if (!err.isEmpty()) return send_err(err);
  send_ok();
}

static void h_io_action() {
  JsonDocument d;
  if (!body(d)) return;
  String err = sat_io_action(d.as<JsonObjectConst>());
  if (!err.isEmpty()) return send_err(err);
  send_ok();
}

// ---------------------------------------------------------------- Firmware
static bool s_upd_ok = false, s_upd_auth = false;
static void h_update_done() {
  if (!s_upd_auth) return devsetup::deny();
  s_srv.send(200, "application/json", s_upd_ok ? "{\"ok\":true,\"restart\":true}" : "{\"error\":\"Update fehlgeschlagen\"}");
  if (s_upd_ok) s_restart_at = millis() + 1500;
}
static void h_update_data() {
  HTTPUpload &u = s_srv.upload();
  if (u.status == UPLOAD_FILE_START) {
    s_upd_auth = devsetup::upload_allowed();   // Firmware nur angemeldet
    // Mit Updater (partitions_upd.csv) liegt daneben nur der 1-MB-Updater: dort nur ihn erneuern, die Firmware selbst
    // geht ueber den Update-Modus (sonst waere der Updater halb ueberschrieben)
    size_t want = s_srv.hasArg("size") ? (size_t)s_srv.arg("size").toInt() : 0, slot = updater_slot_size();
    bool fits = !slot || (want && want <= slot);
    s_upd_ok = s_upd_auth && fits && Update.begin(want ? want : UPDATE_SIZE_UNKNOWN);
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (s_upd_ok && Update.write(u.buf, u.currentSize) != u.currentSize) s_upd_ok = false;
  } else if (u.status == UPLOAD_FILE_END) {
    s_upd_ok = s_upd_ok && Update.end(true);
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    s_upd_ok = false;
  }
}

// ---------------------------------------------------------------- Start/Ablauf
void sat_web_begin() {
  devsetup::begin(s_srv);   // Anmeldung, Pflicht-Passwort, Captive Portal (auch onNotFound)
  s_srv.on("/", HTTP_GET, [] {
    sat_display_web_touch();
    s_srv.send_P(200, "text/html; charset=utf-8", SAT_PAGE);
  });
  s_srv.on("/api/state", HTTP_GET, G(h_state));
  s_srv.on("/api/config", HTTP_POST, G(h_config));
  s_srv.on("/api/wifiscan", HTTP_GET, G(h_wifiscan));
  s_srv.on("/api/truma", HTTP_POST, G(h_truma));
  s_srv.on("/api/mopeka", HTTP_POST, G(h_mopeka));
  s_srv.on("/api/victron", HTTP_POST, G(h_victron));
  s_srv.on("/api/hymer", HTTP_POST, G(h_hymer));
  s_srv.on("/api/io", HTTP_GET, G(h_io_get));
#if SAT_REMOTE
  // Zusatzdisplay: Kachelliste beim Hauptpanel holen (nur durchreichen, nichts speichern) und Auswahl dort ablegen
  s_srv.on("/api/rd/avail", HTTP_GET, G([] {
             std::string ip = rd_panel_ip();
             if (ip.empty()) return send_err("Hauptpanel noch unbekannt – zuerst am Panel anlernen.");
             HTTPClient http;
             http.setTimeout(6000);
             String url = String("http://") + ip.c_str() + "/api/rdisp?avail=1&id=" + g_sat.id;
             if (!http.begin(url)) return send_err("Hauptpanel nicht erreichbar");
             // Hauptpanel mit Zugangspasswort: ausweisen mit dem Pruefwert aus dem Satelliten-Passwort
             http.addHeader("X-SOC-Sig", rd_sign(std::string("avail|") + g_sat.id.c_str()).c_str());
             int code = http.GET();
             if (code != 200) {
               http.end();
               return send_err(code == 401 ? String("Hauptpanel lehnt ab – Satelliten-Passwort stimmt nicht mit dem Panel überein.")
                                           : "Hauptpanel antwortet nicht (Weboberfläche dort aus?) – Code " + String(code));
             }
             String body = http.getString();
             http.end();
             s_srv.send(200, "application/json", body);
           }));
  s_srv.on("/api/rd/sel", HTTP_POST, G([] {
             JsonDocument d;
             if (!body(d)) return;
             std::string ip = rd_panel_ip();
             if (ip.empty()) return send_err("Hauptpanel noch unbekannt");
             JsonDocument o;
             o["id"] = g_sat.id;
             o["tiles"] = d["tiles"];
             String js;
             serializeJson(o, js);
             HTTPClient http;
             http.setTimeout(6000);
             if (!http.begin(String("http://") + ip.c_str() + "/api/rdisp")) return send_err("Hauptpanel nicht erreichbar");
             http.addHeader("Content-Type", "application/json");
             http.addHeader("X-SOC-Sig", rd_sign(std::string("sel|") + js.c_str()).c_str());
             int code = http.POST(js);
             http.end();
             if (code != 200) return send_err("Hauptpanel lehnt ab – Code " + String(code));
             send_ok();
           }));
#endif
  s_srv.on("/api/updater", HTTP_POST, G([] {   // in den kleinen Updater starten (nur mit partitions_upd)
             String err = updater_start(g_sat.wifi_ssid, g_sat.wifi_pass, g_sat.id);
             s_srv.send(400, "application/json", "{\"error\":\"" + err + "\"}");
           }));
  if (SAT_OLED) {
    s_srv.on("/api/oled", HTTP_GET, G(h_oled_get));
    s_srv.on("/api/oled", HTTP_POST, G(h_oled_set));
  }
  if (sat_display_present()) {
    s_srv.on("/api/display", HTTP_GET, G(h_display_get));
    s_srv.on("/api/display", HTTP_POST, G(h_display_set));
  }
  s_srv.on("/api/io", HTTP_POST, G(h_io_set));
  s_srv.on("/api/io/action", HTTP_POST, G(h_io_action));
  s_srv.on("/api/reboot", HTTP_POST, G([] {
             send_ok();
             s_restart_at = millis() + 1000;
           }));
  s_srv.on("/api/update", HTTP_POST, h_update_done, h_update_data);
  s_srv.begin();
}

void sat_web_loop() {
  devsetup::loop();
  s_srv.handleClient();
  if (s_restart_at && (int32_t)(millis() - s_restart_at) > 0) ESP.restart();
}
