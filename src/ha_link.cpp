// Home-Assistant-Verbindung (WebSocket /api/ha) – siehe ha_link.h
#include "ha_link.h"
#if WITH_HA_LINK
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_http_server.h>
#include <esp_random.h>
#include <math.h>
#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "i18n/i18n.h"
#include "mqtt_disco.h"
#include "panelsat.h"
#include "settings.h"
#include "soc_version.h"
#include "z2m.h"
#include "ui/ui_internal.h"

namespace {

SemaphoreHandle_t s_mx = nullptr;
struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mx); }
};

bool s_on = false;
std::string s_tok;
httpd_handle_t s_hd = nullptr;
int s_fd = -1;                       // angemeldete HA-Verbindung (-1 = keine)
std::string s_ha_name;
std::deque<std::string> s_out;       // zu senden (httpd-Task)
bool s_want_list = false;
std::set<std::string> s_subs;
std::map<std::string, std::string> s_last;   // zuletzt gesendeter Wert je Kennung
struct Cmd {
  std::string id;
  int on;    // -1 = unveraendert
  int bri;   // -1 = keine Helligkeit
};
std::deque<Cmd> s_cmds;

// von HA bereitgestellte Entitaeten
struct Imp {
  std::string id, name, dom, unit, cls, area;
  uint8_t idx = 0;
  bool has = false, on = false, avail = true;
  float val = NAN;
  std::string text;
  uint32_t ms = 0;
};
std::vector<Imp> s_imps;
uint32_t s_ver = 1;
std::map<uint32_t, uint8_t> s_map;
bool s_map_dirty = false;

uint32_t fnv(const std::string &s) {
  uint32_t h = 2166136261u;
  for (unsigned char c : s) h = (h ^ c) * 16777619u;
  return h;
}

void map_load() {
  Preferences p;
  if (!p.begin("halink", true)) return;
  size_t n = p.getBytesLength("map");
  if (n && n % 5 == 0 && n <= 5 * 254) {
    std::vector<uint8_t> b(n);
    p.getBytes("map", b.data(), n);
    for (size_t i = 0; i < n; i += 5)
      s_map[b[i] | b[i + 1] << 8 | b[i + 2] << 16 | (uint32_t)b[i + 3] << 24] = b[i + 4];
  }
  p.end();
}

void map_save() {
  std::vector<uint8_t> b;
  for (auto &e : s_map) {
    for (int i = 0; i < 4; i++) b.push_back(e.first >> (8 * i));
    b.push_back(e.second);
  }
  Preferences p;
  if (!p.begin("halink", false)) return;
  p.putBytes("map", b.data(), b.size());
  p.end();
}

uint8_t idx_for(const std::string &id) {
  uint32_t h = fnv(id);
  auto it = s_map.find(h);
  if (it != s_map.end()) return it->second;
  std::set<uint8_t> used;
  for (auto &e : s_map) used.insert(e.second);
  for (uint8_t i = 1; i < 255; i++)
    if (!used.count(i)) {
      s_map[h] = i;
      s_map_dirty = true;
      return i;
    }
  return 0;
}

std::string random_token() {
  static const char A[] = "abcdefghijkmnpqrstuvwxyz23456789";
  std::string t;
  for (int i = 0; i < 24; i++) t += A[esp_random() % (sizeof(A) - 1)];
  return t;
}

// ---- senden (aus beliebigem Task in die Warteschlange, abgeschickt im httpd-Task)
void send_work(void *) {
  std::deque<std::string> q;
  int fd;
  httpd_handle_t hd;
  {
    Lock l;
    q.swap(s_out);
    fd = s_fd;
    hd = s_hd;
  }
  if (fd < 0 || !hd) return;
  for (auto &m : q) {
    httpd_ws_frame_t f;
    memset(&f, 0, sizeof(f));
    f.type = HTTPD_WS_TYPE_TEXT;
    f.payload = (uint8_t *)m.data();
    f.len = m.size();
    f.final = true;
    if (httpd_ws_send_frame_async(hd, fd, &f) != ESP_OK) break;
  }
}

void enqueue(const std::string &m) {
  httpd_handle_t hd;
  {
    Lock l;
    if (s_fd < 0 || !s_hd) return;
    if (s_out.size() > 64) s_out.pop_front();
    s_out.push_back(m);
    hd = s_hd;
  }
  httpd_queue_work(hd, send_work, nullptr);
}

// ---- Katalog und Werte (UI-Task)
uint8_t cls_of(const std::string &dc, const std::string &unit) {
  if (dc == "temperature" || unit == "°C" || unit == "°F") return SC_TEMP;
  if (dc == "humidity" || dc == "moisture") return SC_HUMIDITY;
  if (dc == "voltage" || unit == "V") return SC_VOLTAGE;
  if (dc == "current" || unit == "A") return SC_CURRENT;
  if (dc == "power" || unit == "W") return SC_POWER;
  if (dc == "pressure" || dc == "atmospheric_pressure") return SC_PRESSURE;
  if (dc == "carbon_dioxide") return SC_CO2;
  return SC_NONE;
}

// "13,30 V" -> 13.3 + "V"; false = kein Zahlenwert
bool parse_num(const std::string &s, float &v, std::string &unit) {
  std::string t;
  size_t i = 0;
  while (i < s.size() && s[i] == ' ') i++;
  if (i < s.size() && (s[i] == '-' || s[i] == '+')) t += s[i++];
  bool dig = false;
  for (; i < s.size(); i++) {
    char c = s[i];
    if (isdigit((unsigned char)c)) {
      t += c;
      dig = true;
    } else if (c == ',' || c == '.') {
      t += '.';
    } else {
      break;
    }
  }
  if (!dig) return false;
  v = strtof(t.c_str(), nullptr);
  unit = s.substr(i);
  // Leerzeichen/schmale Leerzeichen vorne weg
  while (!unit.empty() && (unit[0] == ' ' || (uint8_t)unit[0] == 0xE2)) {
    if ((uint8_t)unit[0] == 0xE2 && unit.size() >= 3) unit.erase(0, 3);
    else unit.erase(0, 1);
  }
  return true;
}

const char *dev_class_of_unit(const std::string &u) {
  if (u == "V") return "voltage";
  if (u == "A") return "current";
  if (u == "W") return "power";
  if (u == "kWh" || u == "Wh") return "energy";
  if (u == "°C") return "temperature";
  return "";
}

bool is_own(const std::string &key) {
  const std::string a = std::string(SATIO_KEY_PREFIX) + HA_SAT_ID + "/", b = std::string(SATIO_KEY_PREFIX) + MQD_SAT_ID + "/";
  return key.compare(0, a.size(), a) == 0 || key.compare(0, b.size(), b) == 0;
}

std::string group_of_ctl(const std::string &k) {
  if (k.compare(0, 3, "zb:") == 0) return tr("Zigbee (eingebaut)");
  if (k.compare(0, 4, "shb:") == 0) return tr("Shelly (Bluetooth)");
  if (k.compare(0, 7, "shelly:") == 0) return "Shelly";
  if (k.compare(0, 5, "wled:") == 0) return "WLED";
  return "Zigbee2MQTT";
}

void build_list() {
  JsonDocument d;
  d["t"] = "list";
  JsonArray items = d["items"].to<JsonArray>();
  std::set<std::string> seen;
  // schaltbare Geraete
  for (auto &v : z2m_all()) {
    if (seen.count(v.key)) continue;
    seen.insert(v.key);
    JsonObject o = items.add<JsonObject>();
    o["id"] = v.key;
    o["n"] = v.name + (v.ep.empty() ? "" : " " + v.ep);
    o["g"] = group_of_ctl(v.key);
    o["k"] = v.kind == CTL_LIGHT ? "light" : "switch";
    if (v.has_bri) o["b"] = true;
  }
  // Satelliten-Punkte (ohne HA selbst und MQTT-Discovery)
  for (auto &p : panelsat_point_list()) {
    if (is_own(p.key) || seen.count(p.key)) continue;
    seen.insert(p.key);
    JsonObject o = items.add<JsonObject>();
    o["id"] = p.key;
    o["n"] = p.name;
    o["g"] = panelsat_via_text(p.key);
    o["k"] = p.kind == SP_SWITCH ? "switch" : p.kind == SP_INPUT ? "binary" : "sensor";
    if (!p.unit.empty()) o["u"] = p.unit;
    if (p.cls) o["c"] = sat_class_name(p.cls);
  }
  // alles andere (Akkus, Victron, Gas, Heizung …): jede Zeile des Detailfensters ein Sensor
  for (auto &v : ui_arrange_items()) {
    if (seen.count(v.key) || is_own(v.key)) continue;
    ControlView cv;
    if (z2m_get(v.key, cv)) continue;
    seen.insert(v.key);
    std::string dn;
    auto rows = ui_tile_rows(v.key, &dn);
    if (dn.empty()) dn = v.name;
    for (auto &r : rows) {
      JsonObject o = items.add<JsonObject>();
      o["id"] = v.key + "|" + r.first;
      o["n"] = r.first;
      o["g"] = dn;
      o["k"] = "sensor";
      float f;
      std::string u;
      if (parse_num(r.second, f, u)) {
        if (!u.empty()) o["u"] = u;
        const char *c = dev_class_of_unit(u);
        if (*c) o["c"] = c;
      }
    }
  }
  std::string out;
  serializeJson(d, out);
  enqueue(out);
}

// Wert einer Kennung als JSON-Objekt (fuer den Vergleich als Text)
bool value_of(const std::string &id, JsonObject o) {
  ControlView cv;
  if (z2m_get(id, cv)) {
    o["on"] = cv.on;
    if (cv.has_bri && cv.bri_pct() >= 0) o["bri"] = cv.bri_pct();
    if (!cv.online) o["av"] = false;
    return true;
  }
  SatPoint p;
  if (panelsat_point_get(id, p)) {
    if (p.kind == SP_SWITCH || p.kind == SP_INPUT) o["on"] = p.on;
    else if (p.ok && !isnan(p.value)) o["v"] = p.value;
    if (!p.ok && p.kind != SP_SWITCH) o["av"] = false;
    return true;
  }
  size_t bar = id.find('|');
  if (bar == std::string::npos) return false;
  std::string key = id.substr(0, bar), label = id.substr(bar + 1);
  for (auto &r : ui_tile_rows(key)) {
    if (r.first != label) continue;
    float f;
    std::string u;
    if (parse_num(r.second, f, u)) o["v"] = f;
    else if (r.second == "–" || r.second.empty()) o["av"] = false;
    else o["v"] = r.second;
    return true;
  }
  o["av"] = false;
  return true;
}

// ---- Nachrichten von HA (httpd-Task)
void handle_msg(httpd_req_t *req, int fd, const char *buf, size_t len) {
  JsonDocument d;
  if (deserializeJson(d, buf, len)) return;
  std::string t = d["t"] | "";
  if (t == "hello") {
    bool ok;
    {
      Lock l;
      ok = s_on && !s_tok.empty() && s_tok == (const char *)(d["tok"] | "");
      if (ok) {
        s_fd = fd;
        s_hd = req->handle;
        s_ha_name = d["name"] | "Home Assistant";
        s_subs.clear();
        s_last.clear();
        s_out.clear();
      }
    }
    if (ok) {
      JsonDocument r;
      r["t"] = "ok";
      r["name"] = g_settings.hostname.c_str();
      r["ver"] = SOC_VERSION;
      std::string s;
      serializeJson(r, s);
      enqueue(s);
      log_i("Home Assistant verbunden (%s)", s_ha_name.c_str());
    } else {
      httpd_ws_frame_t f;
      memset(&f, 0, sizeof(f));
      static const char E[] = "{\"t\":\"err\",\"e\":\"Zugangscode falsch oder Verbindung am Panel aus\"}";
      f.type = HTTPD_WS_TYPE_TEXT;
      f.payload = (uint8_t *)E;
      f.len = sizeof(E) - 1;
      f.final = true;
      httpd_ws_send_frame(req, &f);
      httpd_sess_trigger_close(req->handle, fd);
      log_w("Home Assistant: Anmeldung abgelehnt");
    }
    return;
  }
  Lock l;
  if (fd != s_fd) return;   // nicht angemeldet
  if (t == "list") {
    s_want_list = true;
  } else if (t == "sub") {
    s_subs.clear();
    for (JsonVariantConst v : d["ids"].as<JsonArrayConst>()) s_subs.insert(v.as<std::string>());
    s_last.clear();   // alles einmal senden
  } else if (t == "cmd") {
    if (s_cmds.size() < 32)
      s_cmds.push_back({d["id"] | "", d["on"].is<bool>() ? (d["on"].as<bool>() ? 1 : 0) : -1, d["bri"] | -1});
  } else if (t == "ents") {
    std::vector<Imp> neu;
    for (JsonVariantConst e : d["list"].as<JsonArrayConst>()) {
      Imp im;
      im.id = e["id"] | "";
      if (im.id.empty()) continue;
      im.name = e["n"] | im.id.c_str();
      im.dom = e["dom"] | "";
      im.unit = e["u"] | "";
      im.cls = e["c"] | "";
      im.area = e["area"] | "";
      for (auto &o : s_imps)
        if (o.id == im.id) {   // Zustand behalten
          im.has = o.has;
          im.on = o.on;
          im.val = o.val;
          im.text = o.text;
          im.avail = o.avail;
          im.ms = o.ms;
        }
      im.idx = idx_for(im.id);
      if (im.idx && neu.size() < 120) neu.push_back(im);
    }
    s_imps.swap(neu);
    s_ver++;
  } else if (t == "hst") {
    for (JsonPairConst kv : d["v"].as<JsonObjectConst>()) {
      for (auto &im : s_imps) {
        if (im.id != kv.key().c_str()) continue;
        JsonObjectConst o = kv.value().as<JsonObjectConst>();
        std::string s = o["s"] | "";
        im.avail = !(o["av"].is<bool>() && !o["av"].as<bool>()) && s != "unavailable" && s != "unknown";
        im.has = true;
        im.ms = millis();
        im.on = s == "on" || s == "open" || s == "home" || s == "playing";
        char *end = nullptr;
        float f = strtof(s.c_str(), &end);
        if (end && end != s.c_str()) {
          im.val = f;
          im.text.clear();
        } else {
          im.val = NAN;
          im.text = s.substr(0, 40);
        }
      }
    }
    s_ver++;
  } else if (t == "ping") {
    s_out.push_back("{\"t\":\"pong\"}");
    if (s_hd) httpd_queue_work(s_hd, send_work, nullptr);
  }
}

uint32_t s_tick_ms = 0, s_check_ms = 0;

}  // namespace

// ---------------------------------------------------------------- oeffentlich

void ha_link_begin() {
  if (!s_mx) s_mx = xSemaphoreCreateRecursiveMutex();
  Preferences p;
  if (p.begin("halink", true)) {
    s_on = p.getBool("on", false);
    s_tok = p.getString("tok", "").c_str();
    p.end();
  }
  if (s_tok.empty()) {
    s_tok = random_token();
    Preferences w;
    if (w.begin("halink", false)) {
      w.putString("tok", s_tok.c_str());
      w.end();
    }
  }
  map_load();
}

bool ha_link_enabled() { return s_on; }

void ha_link_set_enabled(bool on) {
  {
    Lock l;
    s_on = on;
    if (!on) {
      if (s_hd && s_fd >= 0) httpd_sess_trigger_close(s_hd, s_fd);
      s_fd = -1;
      s_imps.clear();
      s_ver++;
    }
  }
  Preferences p;
  if (p.begin("halink", false)) {
    p.putBool("on", on);
    p.end();
  }
}

std::string ha_link_new_token() {
  std::string t = random_token();
  {
    Lock l;
    s_tok = t;
    if (s_hd && s_fd >= 0) httpd_sess_trigger_close(s_hd, s_fd);   // alter Code gilt nicht mehr
    s_fd = -1;
  }
  Preferences p;
  if (p.begin("halink", false)) {
    p.putString("tok", t.c_str());
    p.end();
  }
  return t;
}

HaLinkInfo ha_link_info() {
  HaLinkInfo r;
  if (!s_mx) return r;
  Lock l;
  r.on = s_on;
  r.connected = s_fd >= 0;
  r.token = s_tok;
  r.ha_name = s_ha_name;
  r.subs = s_subs.size();
  r.imports = s_imps.size();
  return r;
}

void ha_link_server(void *hd) {
  if (!s_mx) return;
  Lock l;
  s_hd = (httpd_handle_t)hd;
  if (!hd) s_fd = -1;
}

int ha_link_ws(void *rq) {
  httpd_req_t *req = (httpd_req_t *)rq;
  if (req->method == HTTP_GET) return s_on ? ESP_OK : ESP_FAIL;   // Handshake
  httpd_ws_frame_t f;
  memset(&f, 0, sizeof(f));
  if (httpd_ws_recv_frame(req, &f, 0) != ESP_OK) return ESP_FAIL;
  if (f.type != HTTPD_WS_TYPE_TEXT || !f.len) return ESP_OK;
  if (f.len > 64 * 1024) return ESP_FAIL;
  char *buf = (char *)heap_caps_malloc(f.len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) return ESP_FAIL;
  f.payload = (uint8_t *)buf;
  esp_err_t e = httpd_ws_recv_frame(req, &f, f.len);
  if (e == ESP_OK) {
    buf[f.len] = 0;
    handle_msg(req, httpd_req_to_sockfd(req), buf, f.len);
  }
  heap_caps_free(buf);
  return e;
}

void ha_link_tick() {
  if (!s_on || !s_mx) return;
  uint32_t now = millis();
  // Verbindung noch da?
  if (now - s_check_ms > 2000) {
    s_check_ms = now;
    Lock l;
    if (s_fd >= 0 && (!s_hd || httpd_ws_get_fd_info(s_hd, s_fd) != HTTPD_WS_CLIENT_WEBSOCKET)) {
      s_fd = -1;
      log_i("Home Assistant getrennt");
    }
    if (s_map_dirty) {
      s_map_dirty = false;
      map_save();
    }
  }
  if (now - s_tick_ms < 700) return;
  s_tick_ms = now;
  bool list;
  std::deque<Cmd> cmds;
  std::vector<std::string> subs;
  {
    Lock l;
    if (s_fd < 0) return;
    list = s_want_list;
    s_want_list = false;
    cmds.swap(s_cmds);
    subs.assign(s_subs.begin(), s_subs.end());
  }
  if (list) build_list();
  for (auto &c : cmds) {
    ControlView cv;
    if (z2m_get(c.id, cv)) {
      if (c.bri >= 0) z2m_set_brightness_pct(c.id, c.bri);
      else if (c.on >= 0) z2m_set_on(c.id, c.on == 1);
    } else if (c.on >= 0) {
      std::string err;
      if (!panelsat_switch(c.id, c.on == 1, &err)) log_w("HA-Befehl %s: %s", c.id.c_str(), err.c_str());
    }
  }
  // geaenderte Werte gesammelt senden
  JsonDocument d;
  d["t"] = "st";
  JsonObject v = d["v"].to<JsonObject>();
  size_t n = 0;
  for (auto &id : subs) {
    JsonDocument one;
    JsonObject o = one.to<JsonObject>();
    if (!value_of(id, o)) continue;
    std::string s;
    serializeJson(o, s);
    {
      Lock l;
      auto it = s_last.find(id);
      if (it != s_last.end() && it->second == s) continue;
      s_last[id] = s;
    }
    v[id] = o;
    if (++n >= 40) break;   // Rest beim naechsten Mal
  }
  if (n) {
    std::string out;
    serializeJson(d, out);
    enqueue(out);
  }
}

std::vector<SatPoint> ha_link_points() {
  std::vector<SatPoint> r;
  if (!s_on || !s_mx) return r;
  Lock l;
  uint32_t now = millis();
  for (auto &im : s_imps) {
    SatPoint p;
    bool sw = im.dom == "light" || im.dom == "switch" || im.dom == "input_boolean" || im.dom == "fan";
    bool bin = im.dom == "binary_sensor";
    if (!sw && !bin && im.dom != "sensor" && im.dom != "number" && im.dom != "input_number") continue;
    p.kind = sw ? SP_SWITCH : bin ? SP_INPUT : SP_ANALOG;
    p.idx = im.idx;
    p.id = std::string(sat_kind_name(p.kind)) + std::to_string(im.idx);
    p.name = im.name;
    p.unit = im.unit;
    p.cls = sw ? SC_ONOFF : bin ? SC_BINARY : cls_of(im.cls, im.unit);
    p.ok = im.has && im.avail && (sw || bin || !isnan(im.val));
    p.value = im.val;
    p.on = im.on;
    p.age_s = im.has ? (int32_t)((now - im.ms) / 1000) : -1;
    if (!im.avail) p.status = tr("offline");
    else if (!sw && !bin && im.has && isnan(im.val)) p.status = im.text;
    r.push_back(p);
  }
  return r;
}

bool ha_link_switch(uint8_t idx, bool on, std::string *err) {
  std::string id;
  {
    Lock l;
    for (auto &im : s_imps)
      if (im.idx == idx) {
        id = im.id;
        im.on = on;   // sofort zeigen, HA bestaetigt mit dem neuen Zustand
        s_ver++;
        break;
      }
    if (id.empty() || s_fd < 0) {
      if (err) *err = tr("Home Assistant ist nicht verbunden.");
      return false;
    }
  }
  JsonDocument d;
  d["t"] = "call";
  d["id"] = id;
  d["on"] = on;
  std::string s;
  serializeJson(d, s);
  enqueue(s);
  return true;
}

uint32_t ha_link_version() { return s_on ? s_ver : 0; }

std::string ha_link_device_name(uint8_t idx) {
  if (!s_mx) return std::string();
  Lock l;
  for (auto &im : s_imps)
    if (im.idx == idx) return im.area.empty() ? std::string("Home Assistant") : im.area;
  return std::string();
}

#endif
