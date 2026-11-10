// Satellit: Weboberflaeche zum Einrichten (WLAN, Sendewege, Bluetooth-Geraete, Firmware)
#include "sat_web.h"
#include "sat_config.h"
#include "sat_io.h"
#include "sat_send.h"
#include "sat_page.h"
#include "../sat_proto.h"
#include "../truma.h"
#include "../mopeka.h"
#include "../victron.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

static WebServer s_srv(80);
static DNSServer s_dns;
static bool s_dns_on = false;
static uint32_t s_restart_at = 0;

static void send_json(const JsonDocument &d, int code = 200) {
  String s;
  serializeJson(d, s);
  s_srv.send(code, "application/json", s);
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
    set_bit("ble", SAT_M_BLE);
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
  }
  sat_config_save();
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
  for (int i = 0; i < n && i < 30; i++) {
    if (WiFi.SSID(i).isEmpty()) continue;
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

// ---------------------------------------------------------------- eigene Anschluesse (GPIO)
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
static bool s_upd_ok = false;
static void h_update_done() {
  s_srv.send(200, "application/json", s_upd_ok ? "{\"ok\":true,\"restart\":true}" : "{\"error\":\"Update fehlgeschlagen\"}");
  if (s_upd_ok) s_restart_at = millis() + 1500;
}
static void h_update_data() {
  HTTPUpload &u = s_srv.upload();
  if (u.status == UPLOAD_FILE_START) {
    s_upd_ok = Update.begin(UPDATE_SIZE_UNKNOWN);
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
  s_srv.on("/", HTTP_GET, [] { s_srv.send_P(200, "text/html; charset=utf-8", SAT_PAGE); });
  s_srv.on("/api/state", HTTP_GET, h_state);
  s_srv.on("/api/config", HTTP_POST, h_config);
  s_srv.on("/api/wifiscan", HTTP_GET, h_wifiscan);
  s_srv.on("/api/truma", HTTP_POST, h_truma);
  s_srv.on("/api/mopeka", HTTP_POST, h_mopeka);
  s_srv.on("/api/victron", HTTP_POST, h_victron);
  s_srv.on("/api/io", HTTP_GET, h_io_get);
  s_srv.on("/api/io", HTTP_POST, h_io_set);
  s_srv.on("/api/io/action", HTTP_POST, h_io_action);
  s_srv.on("/api/reboot", HTTP_POST, [] {
    send_ok();
    s_restart_at = millis() + 1000;
  });
  s_srv.on("/api/update", HTTP_POST, h_update_done, h_update_data);
  // Einrichtungs-WLAN: alle unbekannten Adressen auf die Startseite (Anmeldeseite am Handy)
  s_srv.onNotFound([] {
    if (WiFi.getMode() & WIFI_AP) {
      s_srv.sendHeader("Location", "http://192.168.4.1/");
      s_srv.send(302, "text/plain", "");
    } else {
      s_srv.send(404, "text/plain", "Nicht gefunden");
    }
  });
  s_srv.begin();
}

void sat_web_loop() {
  bool ap = (WiFi.getMode() & WIFI_AP) != 0;
  if (ap && !s_dns_on) s_dns_on = s_dns.start(53, "*", WiFi.softAPIP());
  if (!ap && s_dns_on) {
    s_dns.stop();
    s_dns_on = false;
  }
  if (s_dns_on) s_dns.processNextRequest();
  s_srv.handleClient();
  if (s_restart_at && (int32_t)(millis() - s_restart_at) > 0) ESP.restart();
}
