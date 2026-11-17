#include "web_server.h"
#include "app.h"
#include "app_config.h"
#include "board.h"
#include "layout.h"
#include "net.h"
#include "settings.h"
#include "z2m.h"
#include "truma.h"
#include "victron.h"
#include "cerbo.h"
#include "gx_find.h"
#include "bms.h"
#include "panelsat.h"
#include "panelsat_pair.h"
#include "mopeka.h"
#include "power.h"
#include "truma_cpplus.h"
#include "truma_inetx.h"
#include "hymer.h"
#include "hymer_cloud.h"
#include "alde.h"
#include "i18n/i18n.h"
#include "backup.h"
#include "sdcard.h"
#include "generated/editor_html_gz.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include <algorithm>
#include <set>
#include "ui/ui_remote.h"
#include "mqtt_broker.h"
#include <Preferences.h>
#include <atomic>

static httpd_handle_t s_srv = nullptr;

// JSON der Webanfragen im PSRAM: sonst landen hunderte kleine Stuecke (Geraeteliste, Layout) im knappen internen RAM
struct WebPsramAlloc : ArduinoJson::Allocator {
  void *allocate(size_t n) override {
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : malloc(n);
  }
  void deallocate(void *p) override { heap_caps_free(p); }
  void *reallocate(void *p, size_t n) override {
    void *q = heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return q ? q : realloc(p, n);
  }
};
static WebPsramAlloc s_ps;

// ---------------------------------------------------------------- Helfer
static esp_err_t send_json(httpd_req_t *req, const std::string &s, const char *status = nullptr) {
  if (status) httpd_resp_set_status(req, status);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, s.c_str(), s.size());
}

static esp_err_t send_err(httpd_req_t *req, const char *status, const std::string &msg) {
  JsonDocument d(&s_ps);
  d["error"] = msg;
  std::string s;
  serializeJson(d, s);
  return send_json(req, s, status);
}

static esp_err_t send_ok(httpd_req_t *req) { return send_json(req, "{\"ok\":true}"); }

static std::string query(httpd_req_t *req, const char *key) {
  char q[160] = {0}, v[64] = {0};
  if (httpd_req_get_url_query_str(req, q, sizeof(q)) != ESP_OK) return "";
  if (httpd_query_key_value(q, key, v, sizeof(v)) != ESP_OK) return "";
  return v;
}

static char *read_body(httpd_req_t *req, size_t max, size_t &len) {
  len = req->content_len;
  if (len == 0 || len > max) return nullptr;
  char *buf = (char *)heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) return nullptr;
  size_t got = 0;
  while (got < len) {
    int r = httpd_req_recv(req, buf + got, len - got);
    if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (r <= 0) {
      heap_caps_free(buf);
      return nullptr;
    }
    got += r;
  }
  buf[len] = 0;
  return buf;
}

// ---------------------------------------------------------------- Editor
static esp_err_t h_index(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  return httpd_resp_send(req, (const char *)EDITOR_HTML_GZ, EDITOR_HTML_GZ_LEN);
}

// ---------------------------------------------------------------- Status
static const char *reset_text() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "Einschalten";
    case ESP_RST_SW: return "Neustart per Software";
    case ESP_RST_PANIC: return "Absturz (Panic)";
    case ESP_RST_INT_WDT: return "Absturz (Interrupt-Watchdog)";
    case ESP_RST_TASK_WDT: return "Absturz (Task-Watchdog)";
    case ESP_RST_WDT: return "Absturz (Watchdog)";
    case ESP_RST_BROWNOUT: return "Unterspannung";
    case ESP_RST_EXT: return "Reset-Taste";
    case ESP_RST_DEEPSLEEP: return "Aufwachen";
    default: return "unbekannt";
  }
}

static esp_err_t h_info(httpd_req_t *req) {
  Settings st = settings_copy();
  JsonDocument d(&s_ps);
  d["fw"] = FW_VERSION;
  d["round"] = (bool)UI_ROUND;   // rundes Display (CrowPanel 1.46"): der Editor zeigt dann Kreis und Reihenfolge
  d["lcd_w"] = LCD_H_RES;
  d["lcd_h"] = LCD_V_RES;
  d["knob"] = (bool)UI_KNOB;
  d["ledring"] = UI_LEDRING;          // Anzahl LEDs im Ring, 0 = keiner
  d["web_setup"] = (bool)UI_WEB_SETUP; // kein Einstellungsmenue am Display: alles hier im Browser
  d["lang"] = i18n_lang();   // Sprache der Oberflaeche – der Editor folgt ihr
  d["hostname"] = st.hostname;
  d["ip"] = net_ip();
  d["time_valid"] = net_time_valid();
  d["net"]["mode"] = net_mode();
  d["net"]["ap_active"] = net_ap_active();
  d["net"]["ap_ssid"] = st.ap_ssid;
  d["net"]["ap_ip"] = net_ap_ip();
  d["net"]["ap_clients"] = net_ap_clients();
  d["net"]["status"] = net_status_text();
  d["wifi"]["ssid"] = net_ssid();
  d["wifi"]["connected"] = net_connected();
  d["wifi"]["rssi"] = net_rssi();
  d["mqtt"]["connected"] = z2m_connected();
  d["mqtt"]["auto"] = settings_mqtt_auto_active();
  d["mqtt"]["status"] = z2m_status_text();
  d["mqtt"]["devlist"] = z2m_diag_text();
  d["mqtt"]["bridge"] = z2m_bridge_online();
  d["devices"] = (int)z2m_count();
  d["heap"] = (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  d["heap_min"] = (int)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  d["heap_block"] = (int)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  d["reset"] = reset_text();
  d["psram"] = (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  d["uptime"] = (int)(millis() / 1000);
  d["battery"] = board_battery_voltage();
  PowerInfo pw = power_get();
  if (pw.has_battery) d["battery_pct"] = pw.percent;
  d["power"] = power_src_text(pw.src);
  d["touch"] = board_touch_info();
  d["fs_used"] = (int)LittleFS.usedBytes();
  d["fs_total"] = (int)LittleFS.totalBytes();
  d["sd"]["ok"] = sd_ok();
  d["sd"]["status"] = sd_status();
  d["sd"]["total"] = (double)sd_total();
  d["sd"]["free"] = (double)sd_free();
  d["screen"]["w"] = LCD_H_RES;
  d["screen"]["h"] = LCD_V_RES;
  d["screen"]["topbar"] = UI_TOPBAR_H;
  std::string s;
  serializeJson(d, s);
  return send_json(req, s);
}

// ---------------------------------------------------------------- Victron-Hilfen
static const char *vic_type(VicKind k) {
  return k == VIC_SOLAR ? "solar" : k == VIC_SHUNT ? "shunt" : k == VIC_DCDC ? "dcdc" : k == VIC_GRID ? "grid"
       : k == VIC_CHARGER ? "charger" : k == VIC_DCSYS ? "dcsys" : k == VIC_INVERTER ? "inverter"
       : k == VIC_PSTATION ? "pstation" : k == VIC_PROTECT ? "protect" : k == VIC_LITHIUM ? "lithium" : "other";
}

static void put(JsonObject o, const char *k, float v, float scale = 100) {
  if (!isnan(v)) o[k] = roundf(v * scale) / scale;
}

// Messwerte eines Mopeka-Sensors – dieselben Felder wie bei Truma, dazu Fuellhoehe und Einheit
static void mop_state(JsonObject st, const MopekaInfo &m) {
  if (m.has) {
    st["level"] = m.percent;
    if (m.capacity > 0) {
      st["amount"] = m.amount;
      st["unit"] = m.unit;
      if (!strcmp(m.unit, "kg")) st["kg"] = m.amount;
    }
    st["mm"] = m.level_mm;
    st["quality"] = m.quality;
  }
  if (m.age_s >= 0) {
    st["battery"] = m.battery;
    st["temp"] = m.temperature;
  }
  st["age"] = m.age_s;
  st["busy"] = false;
  st["status"] = m.status;
}

// Truma-Heizung (CP Plus / iNet X)
static const char *WATER_NAMES[4] = {"off", "eco", "hot", "boost"};
static const char *ENERGY_NAMES[4] = {"none", "gas", "electric", "mix"};

static void heat_state(JsonObject st, const TrumaHeatInfo &h) {
  st["connected"] = h.connected;
  st["has"] = h.has;
  st["on"] = h.heat_on;
  if (!isnan(h.room_target)) st["target"] = h.room_target;
  if (!isnan(h.room_actual)) st["room"] = h.room_actual;
  st["mode"] = h.heat_mode;
  JsonArray mn = st["modes"].to<JsonArray>();
  for (int i = 0; i < h.mode_count; i++) mn.add(h.mode_names[i]);
  st["running"] = h.running;
  st["water"] = WATER_NAMES[h.water & 3];
  if (!isnan(h.water_actual)) st["water_temp"] = h.water_actual;
  st["energy"] = ENERGY_NAMES[h.energy & 3];
  st["el_power"] = h.el_power;
  st["energy_writable"] = h.energy_writable;
  st["energy_style"] = h.energy_style == HES_ALDE ? "alde" : h.energy_style == HES_INETX ? "inetx" : "truma";
  st["water_hot"] = h.water_hot;
  if (!isnan(h.outdoor)) st["outdoor"] = h.outdoor;
  if (!isnan(h.fluid)) st["fluid"] = h.fluid;
  if (!isnan(h.battery_v)) st["battery_v"] = h.battery_v;
  st["error"] = h.error;
  st["busy"] = h.busy;
  st["age"] = h.age_s;
  st["status"] = h.status;
}

static bool heat_get(const std::string &key, TrumaHeatInfo &h) {
  if (key == TRUMA_HEAT_KEY_CPPLUS && cpplus_enabled()) return cpplus_get(h);
  if (key == TRUMA_HEAT_KEY_INETX && inetx_enabled() && ble_proto_on(BLE_P_INETX)) return inetx_get(h);
  if (key == TRUMA_HEAT_KEY_ALDE && alde_enabled()) return alde_get(h);
  return false;
}

static void vic_state(JsonObject st, const VictronInfo &v) {
  st["status"] = v.status;
  st["age"] = v.age_s;
  st["rssi"] = v.rssi;
  if (!v.has) return;
  st["has"] = true;
  if (v.state >= 0) {
    st["state"] = v.state;
    st["state_text"] = victron_state_text(v.state);
  }
  if (v.error > 0) st["error"] = v.error;
  put(st, "v", v.bat_v);
  put(st, "i", v.bat_i);
  if (v.kind == VIC_SOLAR) {
    put(st, "pv", v.pv_w, 1);
    put(st, "yield", v.yield_kwh);
    put(st, "load", v.load_i, 10);
  } else if (v.kind == VIC_SHUNT) {
    put(st, "soc", v.soc, 10);
    put(st, "consumed", v.consumed_ah, 10);
    if (v.remaining_min >= 0) st["remaining"] = v.remaining_min;
    if (v.aux_mode < 3) {
      st["aux_mode"] = v.aux_mode;
      put(st, "aux", v.aux);
    }
    if (v.alarm) st["alarm"] = v.alarm;
  } else if (v.kind == VIC_PSTATION) {
    put(st, "soc", v.soc, 10);
    put(st, "in_w", v.ps_in_w, 1);
    put(st, "out_w", v.ps_out_w, 1);
    put(st, "ac_in", v.ps_ac_in_w, 1);
    put(st, "dc_in", v.ps_dc_in_w, 1);
    put(st, "ac_out", v.ps_ac_out_w, 1);
    put(st, "dc_out", v.ps_dc_out_w, 1);
    if (v.ac_on >= 0) st["ac_on"] = v.ac_on == 1;
    if (v.dc_on >= 0) st["dc_on"] = v.dc_on == 1;
    if (v.remaining_min >= 0) st["remaining"] = v.remaining_min;
  } else if (v.kind == VIC_DCDC) {
    put(st, "in_v", v.in_v);
    put(st, "in_i", v.in_i, 10);
    if (v.off_reason) st["off_reason"] = v.off_reason;
  } else if (v.kind == VIC_INVERTER) {
    put(st, "ac_w", v.ac_w, 1);
    put(st, "ac_v", v.ac_v, 1);
    put(st, "ac_i", v.ac_i, 10);
  } else if (v.kind == VIC_GRID) {
    put(st, "ac_w", v.ac_w, 1);
    put(st, "ac_v", v.ac_v, 1);
    put(st, "ac_i", v.ac_i, 10);
    put(st, "ac_limit", v.ac_limit, 10);
    if (v.ac_source >= 0) {
      st["source"] = v.ac_source;
      st["source_text"] = cerbo_source_text(v.ac_source);
    }
    if (v.ac_connected >= 0) st["ac_connected"] = v.ac_connected == 1;
  } else if (v.kind == VIC_CHARGER) {
    put(st, "w", v.power_w, 1);
    if (!v.gx) {   // Ladegeraet per Bluetooth
      put(st, "ac_i", v.ac_i, 10);
      put(st, "temp", v.aux, 1);
    }
    if (v.mode > 0) {
      st["mode"] = v.mode;
      st["mode_text"] = cerbo_mode_text(v.mode);
    }
  } else if (v.kind == VIC_PROTECT) {
    put(st, "in_v", v.in_v);
    if (v.output >= 0) st["out_on"] = v.output == 1;
    if (v.alarm) st["alarm"] = v.alarm;
  } else if (v.kind == VIC_LITHIUM) {
    if (v.aux_mode == 2) put(st, "temp", v.aux, 1);
    JsonArray cl = st["cells"].to<JsonArray>();
    for (float c : v.cell_v)
      if (!isnan(c)) cl.add(roundf(c * 100) / 100);
    st["balancer"] = v.balancer;
    if (v.alarm) st["alarm"] = v.alarm;
  } else if (v.kind == VIC_DCSYS) {
    put(st, "w", v.power_w, 1);
    if (!v.gx && *victron_meter_text(v.meter)) st["meter_text"] = victron_meter_text(v.meter);
  }
  if (v.gx) {
    st["gx"] = true;
    if (v.kind == VIC_SHUNT) put(st, "w", v.power_w, 1);
  }
}

static esp_err_t h_devices(httpd_req_t *req) {
  auto all = z2m_all();
  Layout lay = layout_get();
  JsonDocument d(&s_ps);
  d["connected"] = z2m_connected();
  d["bridge"] = z2m_bridge_online();
  JsonArray arr = d["devices"].to<JsonArray>();
  for (auto &v : all) {
    JsonObject o = arr.add<JsonObject>();
    o["key"] = v.key;
    o["ieee"] = v.ieee;
    if (!v.ep.empty()) o["ep"] = v.ep;
    o["name"] = v.name;
    o["label"] = layout_label(lay, v);
    o["model"] = v.model;
    o["vendor"] = v.vendor;
    o["description"] = v.description;
    o["kind"] = v.kind == CTL_LIGHT ? "light" : "switch";
    o["bri"] = v.has_bri;
    if (v.has_ct) {
      JsonArray ct = o["ct"].to<JsonArray>();
      ct.add(v.ct_min);
      ct.add(v.ct_max);
    }
    if (v.color) o["color"] = (v.color & COLOR_HS) ? "hs" : "xy";
    o["online"] = v.online;
    JsonObject st = o["state"].to<JsonObject>();
    if (v.known) st["on"] = v.on;
    if (v.bri >= 0) st["bri"] = v.bri_pct();
    if (v.ct >= 0) st["ct"] = v.ct;
    if (v.hue >= 0) st["hue"] = v.hue;
    if (v.sat >= 0) st["sat"] = v.sat;
    if (v.cx >= 0) st["x"] = v.cx;
    if (v.cy >= 0) st["y"] = v.cy;
    if (!v.cmode.empty()) st["mode"] = v.cmode;
  }
  // Gasflaschen (Truma LevelControl) als eigene Geraeteart
  uint16_t iv = truma_interval();
  for (auto &g : truma_list()) {
    JsonObject o = arr.add<JsonObject>();
    o["key"] = g.key;
    o["ieee"] = g.mac;
    o["name"] = g.name;
    auto it = lay.names.find(g.key);
    o["label"] = it != lay.names.end() && !it->second.empty() ? it->second : g.name;
    o["model"] = "LevelControl";
    o["vendor"] = "Truma";
    o["kind"] = "gas";
    o["online"] = g.has_reading && (g.age_s < 0 || g.age_s < iv * 60 * 3);
    JsonObject st = o["state"].to<JsonObject>();
    if (g.has_reading) {
      st["level"] = g.percent;
      st["kg"] = g.kg;
      st["battery"] = g.battery;
      st["temp"] = g.temperature;
      st["quality"] = g.quality;
      st["age"] = g.age_s;
    }
    st["busy"] = g.busy;
    st["status"] = g.status;
  }
  // Mopeka-Tanksensoren: im Editor dieselbe Geraeteart wie die Truma-Gasflaschen
  for (auto &m : mopeka_list()) {
    JsonObject o = arr.add<JsonObject>();
    o["key"] = m.key;
    o["ieee"] = m.mac;
    o["name"] = m.name;
    auto it = lay.names.find(m.key);
    o["label"] = it != lay.names.end() && !it->second.empty() ? it->second : m.name;
    o["model"] = m.model_text;
    o["vendor"] = "Mopeka";
    o["kind"] = "gas";
    o["online"] = m.has && m.age_s >= 0 && m.age_s < 600;
    JsonObject st = o["state"].to<JsonObject>();
    mop_state(st, m);
  }
  // Truma-Heizung – nur die in der Firmware eingeschalteten Bedienteile
  for (const char *key : {TRUMA_HEAT_KEY_CPPLUS, TRUMA_HEAT_KEY_INETX, TRUMA_HEAT_KEY_ALDE}) {
    TrumaHeatInfo h;
    if (!heat_get(key, h)) continue;
    JsonObject o = arr.add<JsonObject>();
    o["key"] = h.key;
    o["ieee"] = h.key == TRUMA_HEAT_KEY_INETX ? "BLE" : "LIN";
    o["name"] = "Heizung";
    auto it = lay.names.find(h.key);
    o["label"] = it != lay.names.end() && !it->second.empty() ? it->second : "Heizung";
    bool alde = h.key == TRUMA_HEAT_KEY_ALDE;
    // als std::string: ArduinoJson merkt sich bei const char* nur den Zeiger, h lebt nicht bis zum Serialisieren
    o["model"] = alde ? h.source.substr(5) : std::string(h.key == TRUMA_HEAT_KEY_CPPLUS ? "CP Plus" : "iNet X");
    o["vendor"] = alde ? "Alde" : "Truma";
    o["kind"] = "heat";
    o["online"] = h.connected;
    heat_state(o["state"].to<JsonObject>(), h);
  }
  // Victron (Instant Readout)
  for (auto &v : victron_list()) {
    JsonObject o = arr.add<JsonObject>();
    o["key"] = v.key;
    o["ieee"] = v.mac;
    o["name"] = v.name;
    auto it = lay.names.find(v.key);
    o["label"] = it != lay.names.end() && !it->second.empty() ? it->second : v.name;
    o["model"] = victron_kind_text(v.kind);
    o["vendor"] = "Victron";
    o["kind"] = "victron";
    o["vtype"] = vic_type(v.kind);
    o["online"] = v.has && v.age_s >= 0 && v.age_s < 120;
    vic_state(o["state"].to<JsonObject>(), v);
  }
  // Akkus mit BMS (Bluetooth): wie ein Batteriewaechter
  for (auto &b : bms_list()) {
    VictronInfo v;
    victron_from_bms(b, v);
    JsonObject o = arr.add<JsonObject>();
    o["key"] = v.key;
    o["ieee"] = v.mac;
    o["name"] = v.name;
    auto it = lay.names.find(v.key);
    o["label"] = it != lay.names.end() && !it->second.empty() ? it->second : v.name;
    o["model"] = b.model_text.empty() ? std::string(bms_model_text(b.model)) : b.model_text;
    o["vendor"] = bms_model_text(b.model);
    o["kind"] = "victron";
    o["vtype"] = vic_type(v.kind);
    o["online"] = b.connected && b.has;
    vic_state(o["state"].to<JsonObject>(), v);
  }
  // Satelliten: Gasflaschen, Tanks und Victron von eigenen ESP32 (MQTT/UDP/Bluetooth)
  {
    std::set<std::string> local;
    for (JsonObject o : arr) local.insert(o["key"].as<std::string>());
    for (auto &g : panelsat_truma_list()) {
      if (local.count(g.key)) continue;
      JsonObject o = arr.add<JsonObject>();
      o["key"] = g.key;
      o["ieee"] = g.mac;
      o["name"] = g.name;
      auto it = lay.names.find(g.key);
      o["label"] = it != lay.names.end() && !it->second.empty() ? it->second : g.name;
      o["model"] = "LevelControl";
      o["vendor"] = "Truma";
      o["kind"] = "gas";
      o["sat"] = panelsat_via_text(g.key);
      o["online"] = g.has_reading && g.status != "Satellit offline";
      JsonObject st = o["state"].to<JsonObject>();
      if (g.has_reading) {
        st["level"] = g.percent;
        st["kg"] = g.kg;
        st["battery"] = g.battery;
        st["temp"] = g.temperature;
        st["quality"] = g.quality;
        st["age"] = g.age_s;
      }
      st["busy"] = g.busy;
      st["status"] = g.status;
    }
    for (auto &m : panelsat_mopeka_list()) {
      if (local.count(m.key)) continue;
      JsonObject o = arr.add<JsonObject>();
      o["key"] = m.key;
      o["ieee"] = m.mac;
      o["name"] = m.name;
      auto it = lay.names.find(m.key);
      o["label"] = it != lay.names.end() && !it->second.empty() ? it->second : m.name;
      o["model"] = m.model_text;
      o["vendor"] = "Mopeka";
      o["kind"] = "gas";
      o["sat"] = panelsat_via_text(m.key);
      o["online"] = m.has && m.age_s >= 0 && m.age_s < 600;
      mop_state(o["state"].to<JsonObject>(), m);
    }
    for (auto &v : panelsat_victron_list()) {
      if (local.count(v.key)) continue;
      JsonObject o = arr.add<JsonObject>();
      o["key"] = v.key;
      o["ieee"] = v.mac;
      o["name"] = v.name;
      auto it = lay.names.find(v.key);
      o["label"] = it != lay.names.end() && !it->second.empty() ? it->second : v.name;
      o["model"] = victron_kind_text(v.kind);
      o["vendor"] = "Victron";
      o["kind"] = "victron";
      o["vtype"] = vic_type(v.kind);
      o["sat"] = panelsat_via_text(v.key);
      o["online"] = v.has && v.age_s >= 0 && v.age_s < 120;
      vic_state(o["state"].to<JsonObject>(), v);
    }
  }
  // Satelliten: eigene Anschluesse (Tanksonden, DS18B20, Analog, Neigung, Eingaenge, Schalter)
  for (auto &p : panelsat_point_list()) {
    JsonObject o = arr.add<JsonObject>();
    o["key"] = p.key;
    o["ieee"] = p.sat + "/" + p.id;
    o["name"] = p.name;
    auto it = lay.names.find(p.key);
    o["label"] = it != lay.names.end() && !it->second.empty() ? it->second : p.name;
    o["model"] = std::string(sat_kind_name(p.kind));
    o["vendor"] = "Satellit";
    o["kind"] = "satio";
    o["sio"] = std::string(sat_kind_name(p.kind));
    o["sat"] = panelsat_via_text(p.key);
    o["online"] = p.status != "Satellit offline";
    JsonObject st = o["state"].to<JsonObject>();
    st["ok"] = p.ok;
    if (p.ok && !isnan(p.value)) st["value"] = p.value;
    if (!p.unit.empty()) st["unit"] = p.unit;
    if (p.cls) st["class"] = std::string(sat_class_name(p.cls));
    st["on"] = p.on;
    if (p.kind == SP_TANK) {
      st["probes"] = p.probes;
      st["n"] = p.probes_n;
      if (p.odd) st["odd"] = true;
    }
    if (!p.status.empty()) st["status"] = p.status;
  }
  // Victron Cerbo GX (MQTT): Solar, Netz, Ladegeraet, Batterie, DC-System
  for (auto &v : cerbo_list()) {
    JsonObject o = arr.add<JsonObject>();
    o["key"] = v.key;
    o["ieee"] = "Cerbo GX";
    o["name"] = v.name;
    auto it = lay.names.find(v.key);
    o["label"] = it != lay.names.end() && !it->second.empty() ? it->second : v.name;
    o["model"] = std::string("Cerbo GX – ") + victron_kind_text(v.kind);
    o["vendor"] = "Victron";
    o["kind"] = "victron";
    o["vtype"] = vic_type(v.kind);
    o["online"] = v.age_s == 0;
    vic_state(o["state"].to<JsonObject>(), v);
  }
  std::string s;
  serializeJson(d, s);
  return send_json(req, s);
}

// ---------------------------------------------------------------- Layout
static esp_err_t h_layout_get(httpd_req_t *req) { return send_json(req, layout_json()); }

static esp_err_t h_layout_post(httpd_req_t *req) {
  size_t len;
  char *body = read_body(req, 256 * 1024, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere oder zu große Anfrage");
  std::string msg;
  int code = layout_store_json(body, len, query(req, "force") == "1", msg);
  heap_caps_free(body);
  if (code == 409) return send_err(req, "409 Conflict", msg);
  if (code != 200) return send_err(req, code == 400 ? "400 Bad Request" : "500 Internal Server Error", msg);
  app_post(EV_LAYOUT_CHANGED);
  return send_json(req, "{\"ok\":true,\"rev\":" + std::to_string(layout_get().rev) + "}");
}

static esp_err_t h_rename(httpd_req_t *req) {
  size_t len;
  char *body = read_body(req, 2048, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  heap_caps_free(body);
  if (e) return send_err(req, "400 Bad Request", "Ungültiges JSON");
  std::string err = z2m_rename(d["ieee"] | "", d["name"] | "");
  if (!err.empty()) return send_err(req, "400 Bad Request", err);
  return send_ok(req);
}

// ---------------------------------------------------------------- Hintergruende
static bool valid_page(const std::string &p) {
  if (p.empty() || p.size() > 16) return false;
  for (char c : p)
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  return true;
}

static esp_err_t h_bg_get(httpd_req_t *req) {
  std::string page = query(req, "page");
  if (!valid_page(page)) return send_err(req, "400 Bad Request", "Seite fehlt");
  std::string path = bg_jpg_path(page);
  FILE *f = path.empty() ? nullptr : fopen(path.c_str(), "rb");
  if (!f) return send_err(req, "404 Not Found", "Kein Hintergrund");
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  char *buf = (char *)malloc(8192);
  size_t n;
  while (buf && (n = fread(buf, 1, 8192, f)) > 0) {
    if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) break;
  }
  free(buf);
  fclose(f);
  return httpd_resp_send_chunk(req, nullptr, 0);
}

static esp_err_t h_bg_post(httpd_req_t *req) {
  std::string page = query(req, "page");
  if (!valid_page(page)) return send_err(req, "400 Bad Request", "Seite fehlt");
  if (query(req, "delete") == "1") {
    bg_remove(page);
    layout_refresh_bg();
    app_post(EV_BG_CHANGED, nullptr, page.c_str());
    return send_ok(req);
  }
  if (query(req, "fmt") != "jpg") return send_err(req, "400 Bad Request", "Hintergrund muss als JPEG kommen (fmt=jpg)");
  size_t len = req->content_len;
  if (len == 0 || len > BG_JPG_MAX) return send_err(req, "400 Bad Request", "Bild ist zu groß (höchstens 2 MB)");
  bool on_sd = sd_ok();
  uint64_t room = on_sd ? sd_free() : (uint64_t)(LittleFS.totalBytes() - LittleFS.usedBytes()) + std::max<long>(0, fs_size(bg_jpg_path(page)));
  if (room < len + 128 * 1024) return send_err(req, "400 Bad Request", on_sd ? "SD-Karte ist voll" : "Zu wenig Speicher im Panel – SD-Karte einstecken");
  size_t got;
  char *buf = read_body(req, BG_JPG_MAX, got);
  if (!buf) return send_err(req, "500 Internal Server Error", "Übertragung abgebrochen");
  int w, h;
  if (!bg_jpeg_size((const uint8_t *)buf, got, w, h) || w != LCD_H_RES || h != LCD_V_RES) {
    heap_caps_free(buf);
    return send_err(req, "400 Bad Request", "Bild muss ein JPEG mit 1024×600 Pixeln sein");
  }
  bg_remove(page);   // auch alte Kopien (Flash bzw. Rohbild) loswerden
  bool ok = fs_write(bg_store_path(page), buf, got);
  heap_caps_free(buf);
  layout_refresh_bg();
  app_post(EV_BG_CHANGED, nullptr, page.c_str());
  if (!ok) return send_err(req, "500 Internal Server Error", on_sd ? "Schreiben auf die SD-Karte fehlgeschlagen" : "Speichern im Flash fehlgeschlagen");
  return send_ok(req);
}

// ---------------------------------------------------------------- Sicherung (SD-Karte)
static esp_err_t h_backup_get(httpd_req_t *req) { return send_json(req, backup_list_json()); }

static esp_err_t h_backup_post(httpd_req_t *req) {
  size_t len;
  char *body = read_body(req, 1024, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  heap_caps_free(body);
  if (e) return send_err(req, "400 Bad Request", "Ungültiges JSON");
  std::string msg;
  if (d["create"] | false) {
    std::string name;
    if (!backup_create(name, msg)) return send_err(req, "400 Bad Request", msg);
    JsonDocument r(&s_ps);
    r["ok"] = true;
    r["name"] = name;
    std::string s;
    serializeJson(r, s);
    return send_json(req, s);
  }
  if (d["restore"].is<const char *>()) {
    if (!backup_restore((const char *)d["restore"], msg)) return send_err(req, "500 Internal Server Error", msg);
    app_post(EV_REBOOT);   // Einstellungen und Layout werden beim Start neu gelesen
    return send_ok(req);
  }
  if (d["delete"].is<const char *>()) {
    if (!backup_delete((const char *)d["delete"], msg)) return send_err(req, "400 Bad Request", msg);
    return send_ok(req);
  }
  return send_err(req, "400 Bad Request", "Unbekannter Befehl");
}

// ---------------------------------------------------------------- Einstellungen
static esp_err_t h_settings_get(httpd_req_t *req) {
  Settings st = settings_copy();
  JsonDocument d(&s_ps);
  d["hostname"] = st.hostname;
  d["tz"] = st.tz;
  JsonObject ble = d["ble"].to<JsonObject>();
  ble["truma"] = (st.ble_protos & BLE_P_TRUMA) != 0;
  ble["mopeka"] = (st.ble_protos & BLE_P_MOPEKA) != 0;
  ble["victron"] = (st.ble_protos & BLE_P_VICTRON) != 0;
  if (inetx_enabled()) ble["inetx"] = (st.ble_protos & BLE_P_INETX) != 0;
  if (hymer_enabled()) ble["hymer"] = (st.ble_protos & BLE_P_HYMER) != 0;
  d["mqtt"]["on"] = st.mqtt_on;
  d["mqtt"]["host"] = st.mqtt_host;
  d["mqtt"]["port"] = st.mqtt_port;
  d["mqtt"]["user"] = st.mqtt_user;
  d["mqtt"]["base"] = st.mqtt_base;
  d["mqtt"]["has_pass"] = !st.mqtt_pass.isEmpty();
  d["mqtt"]["auto"] = st.mqtt_auto;
  d["broker"]["avail"] = (bool)WITH_BROKER;   // eingebauter Broker (nur P4)
  d["broker"]["on"] = st.broker_on;
  d["broker"]["user"] = st.broker_user;
  d["broker"]["has_pass"] = !st.broker_pass.isEmpty();
  {
    BrokerInfo bi = broker_info();
    d["broker"]["running"] = bi.running;
    d["broker"]["clients"] = bi.clients;
    d["broker"]["retained"] = bi.retained;
    d["broker"]["retained_kb"] = (uint32_t)(bi.retained_bytes / 1024);
    d["broker"]["msgs"] = bi.msgs;
  }
  d["wifi"]["ssid"] = st.wifi_ssid;
  d["net"]["mode"] = st.net_mode;
  d["net"]["ap_ssid"] = st.ap_ssid;
  d["net"]["ap_pass"] = st.ap_pass;   // steht ohnehin am Panel; noetig, um es dem Nutzer anzuzeigen
  d["net"]["ap_channel"] = st.ap_channel;
  d["display"]["brightness"] = st.brightness;
  d["display"]["dim_after"] = st.dim_after;
  d["display"]["dim_level"] = st.dim_level;
  d["display"]["saver"] = st.saver;
  d["display"]["saver_recolor"] = st.saver_recolor;
  d["display"]["saver_logo"] = LittleFS.exists("/saver_logo.bin");
#if UI_LEDRING
  d["display"]["ring_knob"] = st.ring_knob;
  d["display"]["ring_warn"] = st.ring_warn;
  d["display"]["ring_warn_pct"] = st.ring_warn_pct;
  d["display"]["ring_bri"] = st.ring_bri;
#endif
  d["net"]["web_mode"] = st.web_mode;
  std::string s;
  serializeJson(d, s);
  return send_json(req, s);
}

static esp_err_t h_settings_post(httpd_req_t *req) {
  size_t len;
  char *body = read_body(req, 4096, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  heap_caps_free(body);
  if (e) return send_err(req, "400 Bad Request", "Ungültiges JSON");
  Settings *st = new Settings(settings_copy());
  if (d["hostname"].is<const char *>()) st->hostname = (const char *)d["hostname"];
  if (d["tz"].is<const char *>()) st->tz = (const char *)d["tz"];
  if (!d["ble"].isNull()) {
    auto set_proto = [&](const char *k, uint8_t p) {
      if (d["ble"][k].is<bool>()) st->ble_protos = d["ble"][k].as<bool>() ? (st->ble_protos | p) : (st->ble_protos & ~p);
    };
    set_proto("truma", BLE_P_TRUMA);
    set_proto("mopeka", BLE_P_MOPEKA);
    set_proto("victron", BLE_P_VICTRON);
    set_proto("inetx", BLE_P_INETX);
    set_proto("hymer", BLE_P_HYMER);
  }
  JsonObject m = d["mqtt"];
  if (!m.isNull()) {
    if (m["host"].is<const char *>()) st->mqtt_host = (const char *)m["host"];
    if (m["port"].is<int>()) st->mqtt_port = constrain((int)m["port"], 1, 65535);
    if (m["user"].is<const char *>()) st->mqtt_user = (const char *)m["user"];
    if (m["base"].is<const char *>()) st->mqtt_base = (const char *)m["base"];
    if (m["pass"].is<const char *>() && strlen((const char *)m["pass"]) > 0) st->mqtt_pass = (const char *)m["pass"];
    if (m["clear_pass"] | false) st->mqtt_pass = "";
    if (m["auto"].is<bool>()) st->mqtt_auto = m["auto"].as<bool>();
    if (m["on"].is<bool>()) st->mqtt_on = m["on"].as<bool>();
  }
  JsonObject bk = d["broker"];
  if (!bk.isNull()) {
    if (bk["on"].is<bool>()) st->broker_on = bk["on"].as<bool>();
    if (bk["user"].is<const char *>()) st->broker_user = (const char *)bk["user"];
    if (bk["pass"].is<const char *>() && strlen((const char *)bk["pass"]) > 0) st->broker_pass = (const char *)bk["pass"];
    if (bk["clear_pass"] | false) st->broker_pass = "";
  }
  JsonObject n = d["net"];
  if (!n.isNull()) {
    if (n["mode"].is<int>()) st->net_mode = constrain((int)n["mode"], 0, 3);
    if (n["ap_ssid"].is<const char *>() && strlen((const char *)n["ap_ssid"]) > 0)
      st->ap_ssid = String((const char *)n["ap_ssid"]).substring(0, 32);
    if (n["ap_pass"].is<const char *>()) {
      String pw = (const char *)n["ap_pass"];
      if (pw.length() > 0 && (pw.length() < 8 || pw.length() > 63)) {
        delete st;
        return send_err(req, "400 Bad Request", "Das WLAN-Passwort braucht 8 bis 63 Zeichen");
      }
      if (pw.length() >= 8) st->ap_pass = pw;
    }
    if (n["ap_channel"].is<int>()) st->ap_channel = constrain((int)n["ap_channel"], 1, 13);
    if (n["web_mode"].is<int>()) st->web_mode = UI_WEB_SETUP ? 0 : ((int)n["web_mode"] ? 1 : 0);   // ohne Menue immer an
  }
  JsonObject di = d["display"];
  if (!di.isNull()) {
    if (di["brightness"].is<int>()) st->brightness = constrain((int)di["brightness"], 3, 100);
    if (di["dim_after"].is<int>()) st->dim_after = constrain((int)di["dim_after"], 0, 7200);
    if (di["dim_level"].is<int>()) st->dim_level = constrain((int)di["dim_level"], 0, 100);
    if (di["saver"].is<int>()) st->saver = constrain((int)di["saver"], 0, 4);
    if (di["saver_recolor"].is<bool>()) st->saver_recolor = di["saver_recolor"].as<bool>();
#if UI_LEDRING
    if (di["ring_knob"].is<bool>()) st->ring_knob = di["ring_knob"].as<bool>();
    if (di["ring_warn"].is<bool>()) st->ring_warn = di["ring_warn"].as<bool>();
    if (di["ring_warn_pct"].is<int>()) st->ring_warn_pct = constrain((int)di["ring_warn_pct"], 1, 90);
    if (di["ring_bri"].is<int>()) st->ring_bri = constrain((int)di["ring_bri"], 0, 100);
#endif
  }
  st->hostname.trim();
  st->mqtt_host.trim();
  if (st->hostname.isEmpty()) st->hostname = DEFAULT_HOSTNAME;
  if (st->mqtt_base.isEmpty()) st->mqtt_base = DEFAULT_MQTT_BASE;
  Settings cur = settings_copy();
  bool restart = cur.net_mode != st->net_mode || cur.ap_ssid != st->ap_ssid || cur.ap_pass != st->ap_pass ||
                 cur.ap_channel != st->ap_channel || cur.hostname != st->hostname;
  if (!app_post(EV_SETTINGS_CHANGED, st)) {
    delete st;
    return send_err(req, "503 Service Unavailable", "Panel ist beschäftigt");
  }
  return send_json(req, restart ? "{\"ok\":true,\"restart\":true}" : "{\"ok\":true}");
}

static esp_err_t h_time_post(httpd_req_t *req) {
  size_t len;
  char *body = read_body(req, 256, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  heap_caps_free(body);
  if (e) return send_err(req, "400 Bad Request", "Ungültiges JSON");
  if (!net_time_valid()) net_set_time(d["epoch"] | 0UL);
  return send_ok(req);
}

// Im Panel-WLAN ohne Internet: alle unbekannten Adressen auf den Editor umleiten (Anmeldeseite am Handy)
// Unbekannte Adresse im Panel-WLAN: auf die Panel-Seite umleiten. Das faengt auch die Pruefadressen der Handys ab
// (Android /generate_204, Apple /hotspot-detect.html, Windows /connecttest.txt …) – das Handy zeigt dann die
// Anmeldeseite des WLANs, also den Editor (Captive Portal wie bei WLED).
static esp_err_t h_not_found(httpd_req_t *req, httpd_err_code_t) {
  if (net_ap_active()) {
    static char loc[40];
    snprintf(loc, sizeof(loc), "http://%s/", net_ap_ip().c_str());
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", loc);
    return httpd_resp_send(req, nullptr, 0);
  }
  httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Nicht gefunden");
  return ESP_FAIL;
}

static esp_err_t h_wifi_post(httpd_req_t *req) {
  size_t len;
  char *body = read_body(req, 1024, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  heap_caps_free(body);
  if (!e && (d["scan"] | false)) {   // {"scan":true}: WLAN-Suche anstossen, Ergebnis per GET /api/wifi
    net_web_scan_request();
    return send_ok(req);
  }
  if (e || !d["ssid"].is<const char *>()) return send_err(req, "400 Bad Request", "SSID fehlt");
  Settings *st = new Settings(settings_copy());
  st->wifi_ssid = (const char *)d["ssid"];
  st->wifi_pass = d["pass"] | "";
  if (!app_post(EV_WIFI_CHANGED, st)) {
    delete st;
    return send_err(req, "503 Service Unavailable", "Panel ist beschäftigt");
  }
  return send_ok(req);
}

// Ergebnis der WLAN-Suche: {"running":true} oder {"running":false,"failed":false,"nets":[{"ssid","rssi","secure"}]}
static esp_err_t h_wifi_get(httpd_req_t *req) {
  JsonDocument d(&s_ps);
  std::vector<WifiNet> nets;
  bool failed = false;
  bool done = net_web_scan_result(nets, failed);
  d["running"] = !done;
  if (done) {
    d["failed"] = failed;
    JsonArray a = d["nets"].to<JsonArray>();
    for (auto &n : nets) {
      JsonObject o = a.add<JsonObject>();
      o["ssid"] = n.ssid.c_str();
      o["rssi"] = n.rssi;
      o["secure"] = n.secure;
    }
  }
  std::string s;
  serializeJson(d, s);
  return send_json(req, s);
}

// {"devices":true}: Geraeteliste neu holen, sonst Zustaende neu abfragen
static esp_err_t h_refresh(httpd_req_t *req) {
  size_t len = 0;
  char *body = read_body(req, 256, len);
  bool devices = false;
  if (body) {
    JsonDocument d(&s_ps);
    if (!deserializeJson(d, body, len)) devices = d["devices"] | false;
    heap_caps_free(body);
  }
  if (devices) z2m_reload_devices();
  else z2m_refresh_states();
  return send_ok(req);
}

static esp_err_t h_reboot(httpd_req_t *req) {
  send_ok(req);
  app_post(EV_REBOOT);
  return ESP_OK;
}

// ---------------------------------------------------------------- Truma LevelControl
static esp_err_t h_truma_get(httpd_req_t *req) {
  JsonDocument d(&s_ps);
  d["interval"] = truma_interval();
  d["scanning"] = truma_scan_running();
  d["max"] = TRUMA_MAX_SENSORS;
  JsonArray sa = d["sensors"].to<JsonArray>();
  for (auto &g : truma_list()) {
    JsonObject o = sa.add<JsonObject>();
    o["key"] = g.key;
    o["mac"] = g.mac;
    o["name"] = g.name;
    o["has"] = g.has_reading;
    o["level"] = g.percent;
    o["kg"] = g.kg;
    o["battery"] = g.battery;
    o["temp"] = g.temperature;
    o["quality"] = g.quality;
    o["age"] = g.age_s;
    o["busy"] = g.busy;
    o["status"] = g.status;
  }
  JsonArray fa = d["found"].to<JsonArray>();
  for (auto &f : truma_scan_results()) {
    JsonObject o = fa.add<JsonObject>();
    o["mac"] = f.mac;
    o["type"] = f.addr_type;
    o["rssi"] = f.rssi;
    o["configured"] = f.configured;
  }
  std::string s;
  serializeJson(d, s);
  return send_json(req, s);
}

// Aktionen: {"interval":10} {"scan":true} {"measure":"mac"|""} {"add":{"mac","type","name"}}
//           {"remove":"mac"} {"rename":{"mac","name"}}
static esp_err_t h_truma_post(httpd_req_t *req) {
  size_t len;
  char *body = read_body(req, 2048, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  heap_caps_free(body);
  if (e) return send_err(req, "400 Bad Request", "Ungültiges JSON");
  if (d["interval"].is<int>()) truma_set_interval(d["interval"].as<int>());
  if (d["scan"] | false) truma_scan_start();
  if (d["measure"].is<const char *>()) truma_measure(d["measure"].as<const char *>());
  if (!d["add"].isNull()) {
    std::string err = truma_add(d["add"]["mac"] | "", d["add"]["type"] | 0, d["add"]["name"] | "");
    if (!err.empty()) return send_err(req, "400 Bad Request", err);
  }
  if (d["remove"].is<const char *>()) truma_remove(d["remove"].as<const char *>());
  if (!d["rename"].isNull()) truma_rename(d["rename"]["mac"] | "", d["rename"]["name"] | "");
  return send_ok(req);
}

// ---------------------------------------------------------------- Mopeka
static esp_err_t h_mopeka_get(httpd_req_t *req) {
  JsonDocument d(&s_ps);
  d["scanning"] = mopeka_discovery_running();
  d["max"] = MOPEKA_MAX_SENSORS;
  JsonArray ta = d["tanks"].to<JsonArray>();
  for (size_t i = 0; i < mopeka_tank_count(); i++) {
    const MopekaTank &t = mopeka_tank(i);
    JsonObject o = ta.add<JsonObject>();
    o["name"] = t.name;
    o["empty"] = t.empty_mm;
    o["full"] = t.full_mm;
    o["capacity"] = t.capacity;
  }
  JsonArray sa = d["sensors"].to<JsonArray>();
  for (auto &m : mopeka_list()) {
    JsonObject o = sa.add<JsonObject>();
    o["key"] = m.key;
    o["mac"] = m.mac;
    o["name"] = m.name;
    o["model"] = m.model_text;
    o["medium"] = m.medium == MOP_PROPANE ? "gas" : m.medium == MOP_WATER ? "water" : "air";
    o["tank"] = m.tank == MOPEKA_TANK_CUSTOM ? -1 : m.tank;
    o["empty"] = m.empty_mm;
    o["full"] = m.full_mm;
    o["capacity"] = m.capacity;
    o["rssi"] = m.rssi;
    mop_state(o, m);
  }
  JsonArray fa = d["found"].to<JsonArray>();
  for (auto &f : mopeka_found()) {
    JsonObject o = fa.add<JsonObject>();
    o["mac"] = f.mac;
    o["model"] = f.model_text;
    o["rssi"] = f.rssi;
    o["button"] = f.button;
    o["configured"] = f.configured;
  }
  std::string s;
  serializeJson(d, s);
  return send_json(req, s);
}

// Aktionen: {"scan":true} {"add":{"mac","name","tank"}} {"remove":"mac"} {"rename":{"mac","name"}}
//           {"tank":{"mac","tank":0..n|-1,"empty","full","capacity"}}
static esp_err_t h_mopeka_post(httpd_req_t *req) {
  size_t len;
  char *body = read_body(req, 2048, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  heap_caps_free(body);
  if (e) return send_err(req, "400 Bad Request", "Ungültiges JSON");
  if (d["scan"] | false) mopeka_discovery_start();
  if (!d["add"].isNull()) {
    std::string err = mopeka_add(d["add"]["mac"] | "", d["add"]["name"] | "", d["add"]["tank"] | 0);
    if (!err.empty()) return send_err(req, "400 Bad Request", err);
  }
  if (d["remove"].is<const char *>()) mopeka_remove(d["remove"].as<const char *>());
  if (!d["rename"].isNull()) mopeka_rename(d["rename"]["mac"] | "", d["rename"]["name"] | "");
  if (!d["tank"].isNull()) {
    int t = d["tank"]["tank"] | 0;
    std::string err = mopeka_set_tank(d["tank"]["mac"] | "", t < 0 ? MOPEKA_TANK_CUSTOM : t, d["tank"]["empty"] | 0,
                                      d["tank"]["full"] | 0, d["tank"]["capacity"] | 0.0f);
    if (!err.empty()) return send_err(req, "400 Bad Request", err);
  }
  return send_ok(req);
}

// ---------------------------------------------------------------- Hymer Connect: Konto, QR-Code, Kopplung
static esp_err_t h_hymer_get(httpd_req_t *req) {
  JsonDocument d(&s_ps);
  d["enabled"] = hymer_enabled() && ble_proto_on(BLE_P_HYMER);
  d["paired"] = hymer_paired();
  d["pairing"] = hymer_pairing();
  d["connected"] = hymer_connected();
  d["registered"] = hymer_mobile_registered();
  d["status"] = hymer_status();
  d["has_qr"] = !hymer_cloud_qr().empty();
  d["has_token"] = !hymer_cloud_conf().empty();
  d["has_basic"] = hymer_cloud_has_basic();
  d["brand"] = hymer_cloud_brand();
  d["user"] = hymer_cloud_user();
  d["cloud_state"] = hymer_cloud_state();
  d["cloud_text"] = hymer_cloud_text();
  d["online"] = net_connected();
  if (d["enabled"] && !hymer_paired()) {   // Geraete in der Naehe: antippen = genau mit diesem koppeln
    hymer_browse();
    JsonArray ca = d["cands"].to<JsonArray>();
    for (auto &c : hymer_candidates()) {
      JsonObject o = ca.add<JsonObject>();
      o["mac"] = c.mac;
      o["name"] = c.name;
      o["rssi"] = c.rssi;
      o["hymer"] = c.score >= 2;
    }
  }
  std::string s;
  serializeJson(d, s);
  return send_json(req, s);
}

// {"login":{"user","pass","brand","qr","basic"}} | {"forget_cloud":true}
static esp_err_t h_hymer_post(httpd_req_t *req) {
  size_t len;
  char *body = read_body(req, 4096, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  memset(body, 0, len);   // enthaelt das Passwort
  heap_caps_free(body);
  if (e) return send_err(req, "400 Bad Request", "Ungültiges JSON");
  if (!hymer_enabled()) return send_err(req, "400 Bad Request", "Hymer ist in der Firmware nicht eingeschaltet");
  if (d["forget_cloud"] | false) {
    hymer_cloud_forget();
    return send_ok(req);
  }
  // Kopplung wie am Panel (Einstellungen > Hymer): danach am Bedienteil "CONNECTION" druecken
  if (d["pair_with"].is<const char *>()) {
    hymer_pair_with(d["pair_with"].as<const char *>());
    return send_ok(req);
  }
  if (d["pair"] | false) {
    hymer_pair_start();
    return send_ok(req);
  }
  if (d["forget"] | false) {
    hymer_forget();
    return send_ok(req);
  }
  if (!d["login"].isNull()) {
    if (!net_connected()) return send_err(req, "400 Bad Request", "Das Panel hat kein Internet – Camp-WLAN mit Internet nötig");
    HymerCloudReq r;
    r.user = d["login"]["user"] | "";
    r.pass = d["login"]["pass"] | "";
    r.brand = d["login"]["brand"] | "hymer";
    r.qr = d["login"]["qr"] | "";
    r.basic = d["login"]["basic"] | "";
    r.own_basic = d["login"]["own_basic"] | false;
    std::string err;
    bool ok = hymer_cloud_start(r, err);
    std::fill(r.pass.begin(), r.pass.end(), 0);
    if (!ok) return send_err(req, "400 Bad Request", err);
  }
  return send_ok(req);
}

// ---------------------------------------------------------------- Truma-Heizung
static esp_err_t h_heat_get(httpd_req_t *req) {
  JsonDocument d(&s_ps);
  JsonObject c = d["cpplus"].to<JsonObject>();
  c["enabled"] = cpplus_enabled();
  TrumaHeatInfo h;
  if (cpplus_enabled() && cpplus_get(h)) heat_state(c["state"].to<JsonObject>(), h);
  JsonObject a = d["alde"].to<JsonObject>();
  a["enabled"] = alde_enabled();
  if (alde_enabled() && alde_get(h)) heat_state(a["state"].to<JsonObject>(), h);
  JsonObject x = d["inetx"].to<JsonObject>();
  x["enabled"] = inetx_enabled() && ble_proto_on(BLE_P_INETX);
  x["paired"] = inetx_paired();
  x["pairing"] = inetx_pairing();
  if (inetx_enabled() && inetx_get(h)) heat_state(x["state"].to<JsonObject>(), h);
  std::string s;
  serializeJson(d, s);
  return send_json(req, s);
}

// Aktionen: {"src":"cpplus"|"inetx", "heat":{"on","target","mode"}, "water":"off|eco|hot|boost",
//            "energy":{"e":"gas|mix|electric","power":900}, "clock":true, "pair":true, "forget":true}
static esp_err_t h_heat_post(httpd_req_t *req) {
  size_t len;
  char *body = read_body(req, 1024, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  heap_caps_free(body);
  if (e) return send_err(req, "400 Bad Request", "Ungültiges JSON");
  std::string src = d["src"] | "";
  bool cp = src == "cpplus", al = src == "alde";
  if (cp ? !cpplus_enabled() : al ? !alde_enabled() : (src != "inetx" || !inetx_enabled() || !ble_proto_on(BLE_P_INETX)))
    return send_err(req, "400 Bad Request", "Dieses Bedienteil ist in der Firmware nicht eingeschaltet");
  if (!d["heat"].isNull()) {
    bool on = d["heat"]["on"] | false;
    float t = d["heat"]["target"] | 20.0f;
    uint8_t m = d["heat"]["mode"] | 0;
    if (cp) cpplus_set_heat(on, t, m);
    else if (al) alde_set_heat(on, t);
    else inetx_set_heat(on, t, m);
  }
  if (d["water"].is<const char *>()) {
    std::string w = d["water"].as<const char *>();
    TrumaWater tw = w == "eco" ? TW_ECO : w == "hot" ? TW_HOT : w == "boost" ? TW_BOOST : TW_OFF;
    if (cp) cpplus_set_water(tw);
    else if (al) alde_set_water(tw);
    else inetx_set_water(tw);
  }
  if (!d["energy"].isNull()) {
    std::string en = d["energy"]["e"] | "gas";
    TrumaEnergy te = en == "mix" ? TE_MIX : en == "electric" ? TE_ELECTRIC : en == "none" ? TE_NONE : TE_GAS;
    uint16_t p = d["energy"]["power"] | 0;
    if (cp) cpplus_set_energy(te, p);
    else if (al) {
      alde_set_gas(te == TE_GAS || te == TE_MIX);
      alde_set_electric(te == TE_GAS || te == TE_NONE ? 0 : p / 1000);
    } else inetx_set_energy(te, p);
  }
  if (cp && (d["clock"] | false)) cpplus_sync_clock();
  if (!cp && (d["pair"] | false)) inetx_pair_start();
  if (!cp && (d["forget"] | false)) inetx_forget();
  return send_ok(req);
}

// ---------------------------------------------------------------- Victron
static esp_err_t h_victron_get(httpd_req_t *req) {
  JsonDocument d(&s_ps);
  d["scanning"] = victron_discovery_running();
  d["max"] = VICTRON_MAX_DEVICES;
  JsonArray da = d["devices"].to<JsonArray>();
  for (auto &v : victron_list()) {
    JsonObject o = da.add<JsonObject>();
    o["key"] = v.key;
    o["mac"] = v.mac;
    o["name"] = v.name;
    o["vtype"] = vic_type(v.kind);
    o["kind_text"] = victron_kind_text(v.kind);
    vic_state(o["state"].to<JsonObject>(), v);
  }
  JsonArray fa = d["found"].to<JsonArray>();
  for (auto &f : victron_found()) {
    JsonObject o = fa.add<JsonObject>();
    o["mac"] = f.mac;
    o["name"] = f.name;
    o["model"] = f.model;
    o["vtype"] = vic_type(victron_kind_of_record(f.record));
    o["rssi"] = f.rssi;
    o["configured"] = f.configured;
  }
  std::string s;
  serializeJson(d, s);
  return send_json(req, s);
}

// Aktionen: {"scan":true} {"add":{"mac","key","name"}} {"key":{"mac","key"}} {"remove":"mac"} {"rename":{"mac","name"}}
// ---------------------------------------------------------------- Akkus mit BMS
#if WITH_BMS
static esp_err_t h_bms_get(httpd_req_t *req) {
  JsonDocument d(&s_ps);
  d["enabled"] = true;
  d["on"] = ble_proto_on(BLE_P_BMS);
  d["scanning"] = bms_discovery_running();
  d["ef_uid_set"] = bms_ef_uid_set();
  d["ef_uid_hint"] = bms_ef_uid_hint();
  d["ef_login_state"] = bms_ef_login_state();
  d["ef_login_text"] = bms_ef_login_text();
  d["max"] = BMS_MAX;
  JsonArray bd = d["devices"].to<JsonArray>();
  for (auto &b : bms_list()) {
    JsonObject o = bd.add<JsonObject>();
    o["key"] = b.key;
    o["mac"] = b.mac;
    o["name"] = b.name;
    o["model"] = b.model_text;
    o["connected"] = b.connected;
    o["has"] = b.has;
    o["age"] = b.age_s;
    if (b.has) {
      o["soc"] = b.soc;
      if (isfinite(b.volt)) o["v"] = b.volt;
      if (isfinite(b.amp)) o["a"] = b.amp;
      if (isfinite(b.temp)) o["t"] = b.temp;
      if (b.cycles >= 0) o["cyc"] = b.cycles;
      if (b.soh >= 0) o["soh"] = b.soh;
      o["ok"] = b.ok;
    }
    o["status"] = b.status;
    o["dev"] = b.dev;
    o["sum"] = bms_value_text(b);
    o["pin"] = b.pin_set;        // Kopplungs-PIN gespeichert
    o["pin_need"] = b.pin_need;  // Akku hat beim Koppeln eine PIN verlangt
  }
  JsonArray bf = d["found"].to<JsonArray>();
  for (auto &f : bms_found()) {
    JsonObject o = bf.add<JsonObject>();
    o["mac"] = f.mac;
    o["name"] = f.name;
    o["addr_type"] = f.addr_type;
    o["rssi"] = f.rssi;
    o["model"] = (int)f.model;
    o["model_text"] = bms_model_text(f.model);
    o["cap"] = f.capacity_ah;
    o["cells"] = f.cells;
    o["configured"] = f.configured;
  }
  JsonArray bs = d["seen"].to<JsonArray>();
  for (auto &x : bms_seen()) {
    JsonObject o = bs.add<JsonObject>();
    o["mac"] = x.mac;
    o["name"] = x.name;
    o["addr_type"] = x.addr_type;
    o["rssi"] = x.rssi;
    if (x.mid) o["mid"] = x.mid;
    if (!x.svc.empty()) o["svc"] = x.svc;
  }
  JsonArray bl = d["log"].to<JsonArray>();
  for (auto &line : bms_log()) bl.add(line);
  std::string s;
  serializeJson(d, s);
  return send_json(req, s);
}

// {"on":bool} | {"scan":true} | {"add":{mac,addr_type,name,model,cap,cells[,pin]}} | {"remove":mac} | {"rename":{mac,name}}
// | {"pin":{mac,pin}} (Kopplungs-PIN, "" = loeschen)
static esp_err_t h_bms_post(httpd_req_t *req) {
  size_t len;
  char *body = read_body(req, 1024, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  heap_caps_free(body);
  if (e) return send_err(req, "400 Bad Request", "Ungültiges JSON");
  if (d["on"].is<bool>()) ble_proto_set(BLE_P_BMS, d["on"].as<bool>());
  if (d["scan"] | false) {
    ble_proto_set(BLE_P_BMS, true);   // Suchen schaltet die Auswertung ein
    bms_discovery_start();
  }
  if (!d["add"].isNull()) {
    std::string err = bms_add(d["add"]["mac"] | "", d["add"]["addr_type"] | 0, d["add"]["name"] | "",
                              (BmsModel)(int)(d["add"]["model"] | 1), d["add"]["cap"] | 0, d["add"]["cells"] | 0);
    if (!err.empty()) return send_err(req, "400 Bad Request", err);
    std::string pin = d["add"]["pin"] | "";
    if (!pin.empty()) bms_set_pin(d["add"]["mac"] | "", pin);   // Kopplungs-PIN gleich mit speichern
  }
  if (d["remove"].is<const char *>()) bms_remove(d["remove"].as<const char *>());
  if (!d["rename"].isNull()) bms_rename(d["rename"]["mac"] | "", d["rename"]["name"] | "");
  if (!d["pin"].isNull()) bms_set_pin(d["pin"]["mac"] | "", d["pin"]["pin"] | "");
  if (d["ef_uid"].is<const char *>()) bms_ef_set_uid(d["ef_uid"].as<const char *>());
  if (!d["ef_login"].isNull()) {
    std::string err = bms_ef_login_start(d["ef_login"]["email"] | "", d["ef_login"]["password"] | "");
    if (!err.empty()) return send_err(req, "400 Bad Request", err);
  }
  if (!d["output"].isNull() &&
      !bms_set_output(d["output"]["mac"] | "", d["output"]["ac"] | true, d["output"]["on"] | false))
    return send_err(req, "400 Bad Request", "Dieses Gerät lässt sich nicht schalten");
  return send_ok(req);
}
#endif

static esp_err_t h_victron_post(httpd_req_t *req) {
  size_t len;
  char *body = read_body(req, 2048, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  heap_caps_free(body);
  if (e) return send_err(req, "400 Bad Request", "Ungültiges JSON");
  if (d["scan"] | false) victron_discovery_start();
  if (!d["add"].isNull()) {
    std::string err = victron_add(d["add"]["mac"] | "", d["add"]["key"] | "", d["add"]["name"] | "");
    if (!err.empty()) return send_err(req, "400 Bad Request", err);
  }
  if (!d["key"].isNull()) {
    std::string err = victron_set_key(d["key"]["mac"] | "", d["key"]["key"] | "");
    if (!err.empty()) return send_err(req, "400 Bad Request", err);
  }
  if (d["remove"].is<const char *>()) victron_remove(d["remove"].as<const char *>());
  if (!d["rename"].isNull()) victron_rename(d["rename"]["mac"] | "", d["rename"]["name"] | "");
  return send_ok(req);
}

// ---------------------------------------------------------------- Satelliten
static esp_err_t h_sat_get(httpd_req_t *req) {
  JsonDocument d(&s_ps);
  d["enabled"] = panelsat_enabled();
  d["ble"] = ble_proto_on(BLE_P_SAT);
  d["password"] = panelsat_password();
  JsonArray a = d["satellites"].to<JsonArray>();
  for (auto &n : panelsat_nodes()) {
    JsonObject o = a.add<JsonObject>();
    o["id"] = n.id;
    o["name"] = n.name;
    o["ip"] = n.ip;
    o["fw"] = n.fw;
    o["rssi"] = n.rssi;
    o["online"] = n.online;
    o["mqtt"] = (n.via & 1) != 0;
    o["udp"] = (n.via & 2) != 0;
    o["bt"] = (n.via & 4) != 0;
    o["age"] = n.age_s;
    o["devices"] = n.devices;
  }
  // Satelliten im Einrichtungsmodus (BOOT 3 s am Satelliten) und Stand der Einrichtung
  JsonObject pr = d["pair"].to<JsonObject>();
  pr["wifi_off"] = settings_copy().net_mode == NET_OFF;
  pr["searching"] = satpair_searching();
  pr["left"] = satpair_search_left();
  pr["state"] = satpair_state();
  pr["status"] = satpair_status();
  pr["ip"] = satpair_ip();
  JsonArray pc = pr["cands"].to<JsonArray>();
  for (auto &c : satpair_candidates()) {
    JsonObject o = pc.add<JsonObject>();
    o["mac"] = c.mac;
    o["name"] = c.name;
    o["rssi"] = c.rssi;
  }
  std::string s;
  serializeJson(d, s);
  return send_json(req, s);
}

// {"forget":"<id>"} {"password":"..."}
static esp_err_t h_sat_post(httpd_req_t *req) {
  size_t len;
  char *body = read_body(req, 512, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  heap_caps_free(body);
  if (e) return send_err(req, "400 Bad Request", "Ungültiges JSON");
  if (d["forget"].is<const char *>()) panelsat_forget(d["forget"].as<const char *>());
  if (d["password"].is<const char *>()) panelsat_set_password(d["password"].as<const char *>());
  // Neuen Satelliten hinzufuegen: {"search":true} sucht 3 Minuten per Bluetooth
  if (d["search"] | false) {
    if (settings_copy().net_mode == NET_OFF) return send_err(req, "409 Conflict", "Satelliten brauchen WLAN – am Panel ist „WLAN aus“ eingestellt.");
    satpair_search_start();
  }
  // Neuen Satelliten einrichten: {"pair":"<mac>"} – WLAN-Daten je Betriebsart und Passwort per Bluetooth
  if (d["pair"].is<const char *>()) {
    std::string err = satpair_start(d["pair"].as<const char *>());
    if (!err.empty()) return send_err(req, "409 Conflict", err.c_str());
  }
  // Schalter am Satelliten: {"switch":"satio:<sat>/switch1","on":true}
  if (d["switch"].is<const char *>()) {
    std::string err;
    if (!panelsat_switch(d["switch"].as<const char *>(), d["on"] | false, &err)) return send_err(req, "409 Conflict", err.c_str());
  }
  return send_ok(req);
}

// ---------------------------------------------------------------- Zusatzdisplays
static bool web_authed(httpd_req_t *req);
static esp_err_t web_deny(httpd_req_t *req);
// Pruefwert eines Zusatzdisplays aus dem Kopf X-SOC-Sig
static bool rd_sig(httpd_req_t *req, const std::string &msg) {
  char h[20] = "";
  if (httpd_req_get_hdr_value_str(req, "X-SOC-Sig", h, sizeof(h)) != ESP_OK) return false;
  return ui_remote_sig_ok(msg, h);
}

// Editor (angemeldet): Liste der Displays. Zusatzdisplay (ohne Anmeldung, mit Pruefwert "avail|<id>"): Kachelauswahl
static esp_err_t h_rdisp_get(httpd_req_t *req) {
  char q[96] = "", id[48] = "", av[4] = "";
  if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) {
    httpd_query_key_value(q, "id", id, sizeof(id));
    httpd_query_key_value(q, "avail", av, sizeof(av));
  }
  bool authed = web_authed(req);
  if (av[0] == '1' && (authed || rd_sig(req, std::string("avail|") + id)))
    return send_json(req, ui_remote_avail_json(id));   // Auswahl am Zusatzdisplay
  if (!authed) return web_deny(req);
  return send_json(req, ui_remote_json());
}

// {"id":"Display-…","tiles":["key",…],"name":"…"} | {"forget":"Display-…"}
static esp_err_t h_rdisp_post(httpd_req_t *req) {
  size_t len;
  char *body = read_body(req, 4096, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  bool authed = web_authed(req);
  bool dev = !authed && rd_sig(req, std::string("sel|") + std::string(body, len));   // Zusatzdisplay selbst
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  heap_caps_free(body);
  if (!authed && !dev) return web_deny(req);
  if (e) return send_err(req, "400 Bad Request", "Ungültiges JSON");
  if (!authed && (!d["forget"].isNull() || !d["name"].isNull())) return web_deny(req);   // Display darf nur waehlen
  if (d["forget"].is<const char *>()) ui_remote_forget(d["forget"].as<const char *>());
  if (d["id"].is<const char *>()) {
    std::vector<std::string> keys;
    for (const char *k : d["tiles"].as<JsonArrayConst>()) keys.push_back(k);
    if (!ui_remote_set(d["id"].as<const char *>(), keys, d["name"] | "")) return send_err(req, "404 Not Found", "Display unbekannt");
  }
  return send_ok(req);
}

// ---------------------------------------------------------------- Cerbo GX
static esp_err_t h_cerbo_get(httpd_req_t *req) {
  JsonDocument d(&s_ps);
  d["enabled"] = cerbo_enabled();
  CerboConfig c = cerbo_config();
  d["host"] = c.host;
  d["port"] = c.port;
  d["portal"] = c.portal;
  d["user"] = c.user;
  d["has_pass"] = !c.pass.empty();
  d["connected"] = cerbo_connected();
  d["status"] = cerbo_status();
  d["portal_id"] = cerbo_portal_id();
  d["find_running"] = gx_find_running();
  d["find_status"] = gx_find_status();
  std::string s;
  serializeJson(d, s);
  return send_json(req, s);
}

// {"host","port","portal","user","pass","clear_pass"} – fehlende Felder bleiben, wie sie sind
static esp_err_t h_cerbo_post(httpd_req_t *req) {
  if (!cerbo_enabled()) return send_err(req, "400 Bad Request", "Cerbo GX ist in der Firmware ausgeschaltet");
  size_t len;
  char *body = read_body(req, 1024, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  heap_caps_free(body);
  if (e) return send_err(req, "400 Bad Request", "Ungültiges JSON");
  CerboConfig c = cerbo_config();
  auto trim = [](std::string t) {
    while (!t.empty() && t.back() == ' ') t.pop_back();
    while (!t.empty() && t.front() == ' ') t.erase(0, 1);
    return t;
  };
  if (d["host"].is<const char *>()) c.host = trim(d["host"].as<const char *>()).substr(0, 64);
  if (d["port"].is<int>()) c.port = constrain((int)d["port"], 1, 65535);
  if (d["portal"].is<const char *>()) c.portal = trim(d["portal"].as<const char *>()).substr(0, 32);
  if (d["user"].is<const char *>()) c.user = trim(d["user"].as<const char *>()).substr(0, 64);
  if (d["pass"].is<const char *>() && strlen(d["pass"].as<const char *>()) > 0) c.pass = d["pass"].as<const char *>();
  if (d["clear_pass"] | false) c.pass.clear();
  if (d["find"] | false) {   // GX im Netz suchen und eintragen (Ergebnis ueber GET find_status)
    if (!gx_find_start() && !gx_find_running()) return send_err(req, "409 Conflict", gx_find_status().c_str());
    return send_ok(req);
  }
  cerbo_set_config(c);
  return send_ok(req);
}

// ---------------------------------------------------------------- Bildschirmschoner
static esp_err_t h_saver_preview(httpd_req_t *req) {
  app_post(EV_SAVER_PREVIEW);
  return send_ok(req);
}

static esp_err_t h_logo_get(httpd_req_t *req) {
  File f = LittleFS.open("/saver_logo.png", "r");
  if (!f) return send_err(req, "404 Not Found", "Kein eigenes Logo");
  httpd_resp_set_type(req, "image/png");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  char *buf = (char *)malloc(4096);
  size_t n;
  while ((n = f.read((uint8_t *)buf, 4096)) > 0)
    if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) break;
  free(buf);
  f.close();
  return httpd_resp_send_chunk(req, nullptr, 0);
}

// Logo: fmt=bin (uint16 Breite, uint16 Hoehe, dann RGB565+Alpha) oder fmt=png (Vorschau fuer den Editor)
static esp_err_t h_logo_post(httpd_req_t *req) {
  if (query(req, "delete") == "1") {
    LittleFS.remove("/saver_logo.bin");
    LittleFS.remove("/saver_logo.png");
    app_post(EV_SAVER_LOGO);
    return send_ok(req);
  }
  std::string fmt = query(req, "fmt");
  bool bin = fmt == "bin";
  if (!bin && fmt != "png") return send_err(req, "400 Bad Request", "fmt muss bin oder png sein");
  size_t len = req->content_len;
  if (len < 8 || len > 400 * 300 * 3 + 4) return send_err(req, "400 Bad Request", "Logo zu groß (höchstens 400 × 300)");
  const char *tmp = bin ? "/saver_logo.bin.tmp" : "/saver_logo.png.tmp";
  File f = LittleFS.open(tmp, "w");
  if (!f) return send_err(req, "500 Internal Server Error", "Datei konnte nicht angelegt werden");
  char *buf = (char *)malloc(8192);
  size_t got = 0;
  bool ok = true;
  uint8_t hdr[4];
  while (got < len) {
    int r = httpd_req_recv(req, buf, std::min((size_t)8192, len - got));
    if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (r <= 0) {
      ok = false;
      break;
    }
    if (got < 4)
      for (int i = 0; i < r && got + i < 4; i++) hdr[got + i] = buf[i];
    f.write((const uint8_t *)buf, r);
    got += r;
  }
  free(buf);
  f.close();
  if (ok && bin) {
    uint16_t w = hdr[0] | (hdr[1] << 8), h = hdr[2] | (hdr[3] << 8);
    ok = w && h && w <= 400 && h <= 300 && len == (size_t)w * h * 3 + 4;
  }
  if (!ok) {
    LittleFS.remove(tmp);
    return send_err(req, "400 Bad Request", "Logo-Datei ist ungültig");
  }
  const char *dst = bin ? "/saver_logo.bin" : "/saver_logo.png";
  LittleFS.remove(dst);
  LittleFS.rename(tmp, dst);
  if (bin) app_post(EV_SAVER_LOGO);
  return send_ok(req);
}

// ---------------------------------------------------------------- OTA
static esp_err_t h_ota(httpd_req_t *req) {
  const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
  if (!part) return send_err(req, "500 Internal Server Error", "Keine OTA-Partition");
  if (req->content_len == 0 || req->content_len > part->size) return send_err(req, "400 Bad Request", "Datei zu groß");
  esp_ota_handle_t h;
  // Stueckweise loeschen (beim Schreiben): OTA_SIZE_UNKNOWN loescht vorab die ganze Partition (> 3 MB, mehrere Sekunden)
  // – waehrenddessen liest niemand vom Netz, und die Verbindung riss beim P4 nach ~130 KB ab
  if (esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &h) != ESP_OK) return send_err(req, "500 Internal Server Error", "OTA-Start fehlgeschlagen");
  char *buf = (char *)malloc(8192);
  size_t got = 0;
  bool ok = true;
  while (got < req->content_len) {
    int r = httpd_req_recv(req, buf, std::min((size_t)8192, req->content_len - got));
    if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (r <= 0 || esp_ota_write(h, buf, r) != ESP_OK) {
      ok = false;
      break;
    }
    got += r;
  }
  free(buf);
  if (!ok) {
    esp_ota_abort(h);
    return send_err(req, "500 Internal Server Error", "Übertragung abgebrochen");
  }
  if (esp_ota_end(h) != ESP_OK) return send_err(req, "400 Bad Request", "Ungültige Firmware-Datei");
  if (esp_ota_set_boot_partition(part) != ESP_OK) return send_err(req, "500 Internal Server Error", "Boot-Partition nicht gesetzt");
  send_ok(req);
  app_post(EV_REBOOT);
  return ESP_OK;
}

// ---------------------------------------------------------------- Start
static const uint32_t WEB_IDLE_MS = 15UL * 60 * 1000;   // "bei Bedarf": so lange ohne Zugriff, dann aus
static volatile uint32_t s_last_req = 0;
typedef esp_err_t (*WebHandler)(httpd_req_t *);

// ---------------------------------------------------------------- Zugangspasswort
// Wie bei Satelliten und Victron-Display (devsetup.h): ab Werk "panelsat", beim ersten Oeffnen muss ein eigenes
// vergeben werden; Anmeldung per Cookie "socauth" (4 Sitzungen, nur im RAM), 5 Fehlversuche -> 30 s Pause.
// Alles im httpd-Task; das Zuruecksetzen aus dem Panel-Menue schreibt NVS und setzt nur ein Kennzeichen.
static const char *WEB_DEFAULT_PW = "panelsat";
static std::string s_pw;
static bool s_pw_set = false, s_pw_loaded = false;
static std::string s_tok[4];
static uint8_t s_tok_next = 0, s_fails = 0;
static uint32_t s_lock_until = 0;
static std::atomic<bool> s_pw_reload{false};

static void pw_load() {
  if (s_pw_reload.exchange(false)) {
    s_pw_loaded = false;
    for (auto &t : s_tok) t.clear();   // alle abmelden
  }
  if (s_pw_loaded) return;
  Preferences p;
  bool ok = p.begin("devsetup", true);
  s_pw = ok ? p.getString("apw", WEB_DEFAULT_PW).c_str() : WEB_DEFAULT_PW;
  s_pw_set = ok && p.getBool("pwset", false);
  if (ok) p.end();
  s_pw_loaded = true;
}
static void pw_save() {
  Preferences p;
  if (!p.begin("devsetup", false)) return;
  p.putString("apw", s_pw.c_str());
  p.putBool("pwset", s_pw_set);
  p.end();
}
bool web_password_set() {
  Preferences p;
  bool ok = p.begin("devsetup", true), set = ok && p.getBool("pwset", false);
  if (ok) p.end();
  return set;
}
void web_password_reset() {
  Preferences p;
  if (p.begin("devsetup", false)) {
    p.putString("apw", WEB_DEFAULT_PW);
    p.putBool("pwset", false);
    p.end();
  }
  s_pw_reload = true;
  log_w("Zugangspasswort der Weboberflaeche zurueckgesetzt");
}

static bool web_authed(httpd_req_t *req) {
  pw_load();
  if (!s_pw_set) return false;
  char c[384];
  size_t n = httpd_req_get_hdr_value_len(req, "Cookie");
  if (!n || n >= sizeof(c) || httpd_req_get_hdr_value_str(req, "Cookie", c, sizeof(c)) != ESP_OK) return false;
  const char *p = strstr(c, "socauth=");
  if (!p || strlen(p + 8) < 32) return false;
  std::string t(p + 8, 32);
  for (auto &k : s_tok)
    if (!k.empty() && k == t) return true;
  return false;
}
static esp_err_t web_deny(httpd_req_t *req) {
  httpd_resp_set_status(req, "401 Unauthorized");
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_sendstr(req, s_pw_set ? "{\"auth\":\"login\",\"error\":\"Bitte anmelden.\"}"
                                          : "{\"auth\":\"setup\",\"error\":\"Bitte zuerst ein Zugangspasswort festlegen.\"}");
}
static void new_session(httpd_req_t *req) {
  static char cookie[80];   // httpd merkt sich nur den Zeiger bis zum Senden
  char t[33];
  for (int i = 0; i < 4; i++) snprintf(t + i * 8, 9, "%08lx", (unsigned long)esp_random());
  s_tok[s_tok_next] = t;
  s_tok_next = (s_tok_next + 1) % 4;
  snprintf(cookie, sizeof(cookie), "socauth=%s; Path=/; HttpOnly; SameSite=Strict", t);
  httpd_resp_set_hdr(req, "Set-Cookie", cookie);
}
static bool pw_locked(httpd_req_t *req) {
  if (s_lock_until && (int32_t)(millis() - s_lock_until) < 0) {
    send_err(req, "429 Too Many Requests", "Zu viele Versuche – bitte 30 Sekunden warten.");
    return true;
  }
  s_lock_until = 0;
  return false;
}
static esp_err_t pw_wrong(httpd_req_t *req) {
  if (++s_fails >= 5) {
    s_fails = 0;
    s_lock_until = (millis() + 30000) | 1;
  }
  return send_err(req, "403 Forbidden", "Passwort falsch.");
}

// GET: {"setup":bool,"authed":bool}
static esp_err_t h_auth_get(httpd_req_t *req) {
  pw_load();
  JsonDocument d(&s_ps);
  d["setup"] = !s_pw_set;
  d["authed"] = web_authed(req);
  std::string s;
  serializeJson(d, s);
  return send_json(req, s);
}
// POST {"pw":"…"} = anmelden | {"old":"…","new":"…"} = Passwort festlegen/aendern (erstes Mal mit "panelsat")
static esp_err_t h_auth_post(httpd_req_t *req) {
  pw_load();
  size_t len;
  char *body = read_body(req, 512, len);
  if (!body) return send_err(req, "400 Bad Request", "Leere Anfrage");
  JsonDocument d(&s_ps);
  DeserializationError e = deserializeJson(d, body, len);
  heap_caps_free(body);
  if (e) return send_err(req, "400 Bad Request", "Ungültiges JSON");
  if (pw_locked(req)) return ESP_OK;
  if (d["new"].is<const char *>()) {
    std::string oldp = d["old"] | "", newp = d["new"] | "";
    if (oldp != s_pw) return pw_wrong(req);
    if (newp.size() < 8 || newp.size() > 63) return send_err(req, "400 Bad Request", "Das neue Passwort braucht 8 bis 63 Zeichen.");
    if (newp == WEB_DEFAULT_PW) return send_err(req, "400 Bad Request", "Bitte ein anderes als das Standardpasswort nehmen.");
    s_fails = 0;
    s_pw = newp;
    s_pw_set = true;
    pw_save();
    for (auto &t : s_tok) t.clear();   // alle anderen abmelden
    new_session(req);
    return send_ok(req);
  }
  if (!s_pw_set) return web_deny(req);
  if (s_pw != (const char *)(d["pw"] | "")) return pw_wrong(req);
  s_fails = 0;
  new_session(req);
  return send_ok(req);
}

// Ohne Anmeldung: die Seite selbst (Editor, Captive Portal) und die Anmeldung. /api/rdisp prueft selbst (Zusatzdisplays).
static bool web_open(const char *uri) {
  if (strncmp(uri, "/api/", 5) != 0) return true;
  return !strncmp(uri, "/api/auth", 9) || !strncmp(uri, "/api/rdisp", 10);
}

// Jede Anfrage merkt sich die Zeit (fuer das Abschalten bei Bedarf), dann der eigentliche Handler
static esp_err_t tramp(httpd_req_t *req) {
  s_last_req = millis();
  if (!web_open(req->uri) && !web_authed(req)) return web_deny(req);
  // Waehrend der Anfrage auch kleine Speicherstuecke (Texte, Kopien der Geraeteliste) ins PSRAM legen
  heap_caps_malloc_extmem_enable(16);
  esp_err_t r = ((WebHandler)req->user_ctx)(req);
  heap_caps_malloc_extmem_enable(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL);
  return r;
}

static void reg(const char *uri, httpd_method_t m, WebHandler fn) {
  httpd_uri_t u;
  memset(&u, 0, sizeof(u));
  u.uri = uri;
  u.method = m;
  u.handler = tramp;
  u.user_ctx = (void *)fn;
  httpd_register_uri_handler(s_srv, &u);
}

void web_begin() {
  if (net_mode() == NET_OFF) return;   // ohne WLAN erreicht ihn niemand
  if (g_settings.web_mode == 0) web_start();
  else log_i("Webinterface: nur bei Bedarf (Einstellungen > Netzwerk)");
}

bool web_running() { return s_srv != nullptr; }
bool web_busy() { return s_srv && millis() - s_last_req < 10000; }

void web_stop() {
  if (!s_srv) return;
  httpd_stop(s_srv);
  s_srv = nullptr;
  log_i("Webinterface beendet");
}

void web_loop() {
  if (!s_srv || g_settings.web_mode == 0) return;
  if (millis() - s_last_req > WEB_IDLE_MS) web_stop();
}

uint32_t web_idle_left_s() {
  if (!s_srv || g_settings.web_mode == 0) return 0;
  uint32_t idle = millis() - s_last_req;
  return idle >= WEB_IDLE_MS ? 0 : (WEB_IDLE_MS - idle) / 1000;
}

void web_start() {
  s_last_req = millis();
  if (s_srv) return;
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.stack_size = 10240;
  cfg.max_uri_handlers = 48;
  cfg.lru_purge_enable = true;
  cfg.recv_wait_timeout = 20;
  cfg.send_wait_timeout = 20;
  cfg.core_id = 0;
  if (httpd_start(&s_srv, &cfg) != ESP_OK) {
    log_e("HTTP-Server konnte nicht starten");
    s_srv = nullptr;
    return;
  }
  log_i("Webinterface gestartet");
  reg("/", HTTP_GET, h_index);
  reg("/index.html", HTTP_GET, h_index);
  reg("/api/info", HTTP_GET, h_info);
  reg("/api/auth", HTTP_GET, h_auth_get);
  reg("/api/auth", HTTP_POST, h_auth_post);
  reg("/api/devices", HTTP_GET, h_devices);
  reg("/api/layout", HTTP_GET, h_layout_get);
  reg("/api/layout", HTTP_POST, h_layout_post);
  reg("/api/rename", HTTP_POST, h_rename);
  reg("/api/bg", HTTP_GET, h_bg_get);
  reg("/api/bg", HTTP_POST, h_bg_post);
  reg("/api/settings", HTTP_GET, h_settings_get);
  reg("/api/settings", HTTP_POST, h_settings_post);
  reg("/api/wifi", HTTP_POST, h_wifi_post);
  reg("/api/wifi", HTTP_GET, h_wifi_get);
  reg("/api/refresh", HTTP_POST, h_refresh);
  reg("/api/reboot", HTTP_POST, h_reboot);
  reg("/api/ota", HTTP_POST, h_ota);
  reg("/api/time", HTTP_POST, h_time_post);
  reg("/api/saver", HTTP_POST, h_saver_preview);
  reg("/api/truma", HTTP_GET, h_truma_get);
  reg("/api/truma", HTTP_POST, h_truma_post);
  reg("/api/heat", HTTP_GET, h_heat_get);
  reg("/api/heat", HTTP_POST, h_heat_post);
  reg("/api/hymer", HTTP_GET, h_hymer_get);
  reg("/api/hymer", HTTP_POST, h_hymer_post);
  reg("/api/mopeka", HTTP_GET, h_mopeka_get);
  reg("/api/mopeka", HTTP_POST, h_mopeka_post);
  reg("/api/victron", HTTP_GET, h_victron_get);
  reg("/api/victron", HTTP_POST, h_victron_post);
#if WITH_BMS
  reg("/api/bms", HTTP_GET, h_bms_get);
  reg("/api/bms", HTTP_POST, h_bms_post);
#endif
  reg("/api/cerbo", HTTP_GET, h_cerbo_get);
  reg("/api/cerbo", HTTP_POST, h_cerbo_post);
  reg("/api/satellites", HTTP_GET, h_sat_get);
  reg("/api/satellites", HTTP_POST, h_sat_post);
  reg("/api/rdisp", HTTP_GET, h_rdisp_get);
  reg("/api/rdisp", HTTP_POST, h_rdisp_post);
  reg("/api/saver_logo", HTTP_GET, h_logo_get);
  reg("/api/saver_logo", HTTP_POST, h_logo_post);
  reg("/api/backup", HTTP_GET, h_backup_get);
  reg("/api/backup", HTTP_POST, h_backup_post);
  httpd_register_err_handler(s_srv, HTTPD_404_NOT_FOUND, h_not_found);
}
