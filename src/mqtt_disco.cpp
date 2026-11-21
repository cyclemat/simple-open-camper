// Geraete aus MQTT-Discovery (ESPHome, Tasmota …) – siehe mqtt_disco.h
#include "mqtt_disco.h"
#if WITH_MQTT_DISCO
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <math.h>
#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "i18n/i18n.h"
#include "z2m.h"

namespace {

const size_t MAX_ENTS = 160;

enum Comp : uint8_t { C_SENSOR, C_BINARY, C_SWITCH, C_LIGHT };

struct Ent {
  std::string cfg, uid, name, dev, dev_id, state_t, cmd_t, avail_t, tpl, unit, dev_cla;
  std::string pl_on = "ON", pl_off = "OFF", st_on, st_off, pl_avail = "online", pl_navail = "offline";
  Comp comp = C_SENSOR;
  bool json_light = false;   // Lampe mit schema: json (ESPHome)
  uint8_t idx = 0;
  // Zustand
  bool has = false, on = false, avail = true;
  float val = NAN;
  std::string text;          // Sensor ohne Zahlenwert
  uint32_t ms = 0;
};

SemaphoreHandle_t s_mx = nullptr;
struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mx); }
};

bool s_on = false;
std::string s_pre = "homeassistant";
std::vector<Ent> s_ents;
std::unordered_map<std::string, std::vector<size_t>> s_by_topic;   // Zustands-/Verfuegbarkeits-Topic -> Entitaeten
std::vector<std::string> s_new_topics;
std::set<std::string> s_soc_devs;   // Geraete von Simple Open Camper selbst (Hersteller), auch als via_device
uint16_t s_skipped = 0;
uint32_t s_ver = 1;
// Punktnummern: Hash der unique_id -> Nr. (1..254), ueber Neustarts gleich
std::map<uint32_t, uint8_t> s_map;
bool s_map_dirty = false;
uint32_t s_map_save_ms = 0;

uint32_t fnv(const std::string &s) {
  uint32_t h = 2166136261u;
  for (unsigned char c : s) h = (h ^ c) * 16777619u;
  return h;
}

void map_load() {
  Preferences p;
  if (!p.begin("mqdisco", true)) return;
  size_t n = p.getBytesLength("map");
  if (n && n % 5 == 0 && n <= 5 * 254) {
    std::vector<uint8_t> b(n);
    p.getBytes("map", b.data(), n);
    for (size_t i = 0; i < n; i += 5) {
      uint32_t h = b[i] | b[i + 1] << 8 | b[i + 2] << 16 | (uint32_t)b[i + 3] << 24;
      s_map[h] = b[i + 4];
    }
  }
  p.end();
}

void map_save() {
  std::vector<uint8_t> b;
  for (auto &e : s_map) {
    b.push_back(e.first);
    b.push_back(e.first >> 8);
    b.push_back(e.first >> 16);
    b.push_back(e.first >> 24);
    b.push_back(e.second);
  }
  Preferences p;
  if (!p.begin("mqdisco", false)) return;
  p.putBytes("map", b.data(), b.size());
  p.end();
}

uint8_t idx_for(const std::string &uid) {
  uint32_t h = fnv(uid);
  auto it = s_map.find(h);
  if (it != s_map.end()) return it->second;
  std::set<uint8_t> used;
  for (auto &e : s_map) used.insert(e.second);
  for (uint8_t i = 1; i < 255; i++)
    if (!used.count(i)) {
      if (s_map.size() >= 254) return 0;
      s_map[h] = i;
      s_map_dirty = true;
      return i;
    }
  return 0;
}

// Discovery-Felder gibt es lang und abgekuerzt (stat_t = state_topic …)
const char *str(JsonVariantConst o, const char *a, const char *b) {
  if (o[a].is<const char *>()) return o[a];
  if (b && o[b].is<const char *>()) return o[b];
  return nullptr;
}
std::string sstr(JsonVariantConst o, const char *a, const char *b, const char *def = "") {
  const char *s = str(o, a, b);
  return s ? s : def;
}
// "~" am Anfang oder Ende eines Topics = Basis-Topic
std::string topic_of(JsonVariantConst o, const char *a, const char *b, const std::string &base) {
  std::string t = sstr(o, a, b);
  if (t.empty() || base.empty()) return t;
  if (t[0] == '~') return base + t.substr(1);
  if (t.back() == '~') return t.substr(0, t.size() - 1) + base;
  return t;
}
std::string first_id(JsonVariantConst dev) {
  JsonVariantConst ids = dev["ids"].isNull() ? dev["identifiers"] : dev["ids"];
  if (ids.is<JsonArrayConst>() && ids.size()) return ids[0].as<std::string>();
  if (ids.is<const char *>()) return ids.as<std::string>();
  return std::string();
}

// Wert aus einer einfachen Vorlage: "{{ value }}", "{{ value_json.a.b }}", "{{ value_json['a'] }}" (Filter nach | weg)
bool apply_tpl(const std::string &tpl, const char *payload, size_t len, std::string &out) {
  if (tpl.empty()) {
    out.assign(payload, len);
    return true;
  }
  size_t a = tpl.find("{{"), b = tpl.find("}}");
  if (a == std::string::npos || b == std::string::npos || b < a) {
    out.assign(payload, len);
    return true;
  }
  std::string e = tpl.substr(a + 2, b - a - 2);
  size_t bar = e.find('|');
  if (bar != std::string::npos) e = e.substr(0, bar);
  while (!e.empty() && e.front() == ' ') e.erase(0, 1);
  while (!e.empty() && e.back() == ' ') e.pop_back();
  if (e == "value") {
    out.assign(payload, len);
    return true;
  }
  if (e.compare(0, 10, "value_json") != 0) return false;   // andere Ausdruecke kann das Panel nicht
  JsonDocument d;
  if (deserializeJson(d, payload, len)) return false;
  JsonVariantConst v = d.as<JsonVariantConst>();
  size_t i = 10;
  while (i < e.size()) {
    std::string k;
    if (e[i] == '.') {
      size_t j = i + 1;
      while (j < e.size() && e[j] != '.' && e[j] != '[') j++;
      k = e.substr(i + 1, j - i - 1);
      i = j;
    } else if (e[i] == '[') {
      size_t j = e.find(']', i);
      if (j == std::string::npos) return false;
      k = e.substr(i + 1, j - i - 1);
      if (!k.empty() && (k[0] == '\'' || k[0] == '"')) k = k.substr(1, k.size() - 2);
      i = j + 1;
    } else {
      return false;
    }
    if (!k.empty() && isdigit((unsigned char)k[0]) && v.is<JsonArrayConst>()) v = v[atoi(k.c_str())];
    else v = v[k];
    if (v.isNull()) return false;
  }
  if (v.is<const char *>()) out = v.as<const char *>();
  else if (v.is<bool>()) out = v.as<bool>() ? "ON" : "OFF";
  else serializeJson(v, out);
  return true;
}

uint8_t cls_of(const std::string &dc, const std::string &unit) {
  if (dc == "temperature") return SC_TEMP;
  if (dc == "humidity" || dc == "moisture") return SC_HUMIDITY;
  if (dc == "voltage") return SC_VOLTAGE;
  if (dc == "current") return SC_CURRENT;
  if (dc == "power" || dc == "apparent_power") return SC_POWER;
  if (dc == "pressure" || dc == "atmospheric_pressure") return SC_PRESSURE;
  if (dc == "carbon_dioxide") return SC_CO2;
  if (dc == "weight") return SC_WEIGHT;
  if (dc == "volume_flow_rate") return SC_FLOW;
  if (dc == "water" || dc == "volume" || dc == "volume_storage") return SC_VOLUME;
  if (unit == "°C" || unit == "°F") return SC_TEMP;
  if (unit == "V") return SC_VOLTAGE;
  if (unit == "A") return SC_CURRENT;
  if (unit == "W") return SC_POWER;
  return SC_NONE;
}

void add_topic(const std::string &t, size_t i) {
  if (t.empty()) return;
  auto &v = s_by_topic[t];
  if (v.empty()) s_new_topics.push_back(t);
  if (std::find(v.begin(), v.end(), i) == v.end()) v.push_back(i);
}

void rebuild_topics() {
  s_by_topic.clear();
  for (size_t i = 0; i < s_ents.size(); i++) {
    if (!s_ents[i].state_t.empty()) s_by_topic[s_ents[i].state_t].push_back(i);
    if (!s_ents[i].avail_t.empty()) s_by_topic[s_ents[i].avail_t].push_back(i);
  }
}

// Konfiguration einer Entitaet (unter Sperre)
void handle_config(const std::string &comp_s, const std::string &obj_key, const char *payload, size_t len) {
  // leere Nachricht = Entitaet entfernt
  if (!len) {
    for (size_t i = 0; i < s_ents.size(); i++)
      if (s_ents[i].cfg == obj_key) {
        s_ents.erase(s_ents.begin() + i);
        rebuild_topics();
        s_ver++;
        return;
      }
    return;
  }
  Comp comp;
  if (comp_s == "sensor") comp = C_SENSOR;
  else if (comp_s == "binary_sensor") comp = C_BINARY;
  else if (comp_s == "switch") comp = C_SWITCH;
  else if (comp_s == "light") comp = C_LIGHT;
  else {
    s_skipped++;
    return;
  }
  JsonDocument d;
  if (deserializeJson(d, payload, len)) return;
  JsonVariantConst o = d.as<JsonVariantConst>();
  JsonVariantConst dev = o["dev"].isNull() ? o["device"] : o["dev"];
  std::string mf = sstr(dev, "mf", "manufacturer"), did = first_id(dev);
  std::string via = sstr(dev, "via_device", nullptr);
  // eigene Geraete und Zigbee2MQTT ueberspringen
  if (mf == "Simple Open Camper") {
    if (!did.empty()) s_soc_devs.insert(did);
    s_skipped++;
    return;
  }
  if ((!via.empty() && s_soc_devs.count(via)) || did.compare(0, 12, "zigbee2mqtt_") == 0 ||
      via.compare(0, 12, "zigbee2mqtt_") == 0) {
    s_skipped++;
    return;
  }
  std::string base = sstr(o, "~", nullptr);
  Ent e;
  e.comp = comp;
  e.cfg = obj_key;
  e.uid = sstr(o, "uniq_id", "unique_id");
  if (e.uid.empty()) e.uid = obj_key;
  e.dev = sstr(dev, "name", nullptr);
  e.dev_id = did.empty() ? e.dev : did;
  std::string nm = sstr(o, "name", nullptr);
  e.name = nm.empty() ? e.dev : nm;
  if (e.dev.empty()) e.dev = tr("MQTT-Gerät");
  e.state_t = topic_of(o, "stat_t", "state_topic", base);
  e.cmd_t = topic_of(o, "cmd_t", "command_topic", base);
  e.avail_t = topic_of(o, "avty_t", "availability_topic", base);
  JsonVariantConst av = o["avty"].isNull() ? o["availability"] : o["avty"];
  if (e.avail_t.empty() && av.is<JsonArrayConst>() && av.size()) e.avail_t = topic_of(av[0], "t", "topic", base);
  e.tpl = sstr(o, "val_tpl", "value_template");
  if (e.tpl.empty()) e.tpl = sstr(o, "stat_val_tpl", "state_value_template");
  e.unit = sstr(o, "unit_of_meas", "unit_of_measurement");
  e.dev_cla = sstr(o, "dev_cla", "device_class");
  e.pl_on = sstr(o, "pl_on", "payload_on", "ON");
  e.pl_off = sstr(o, "pl_off", "payload_off", "OFF");
  e.st_on = sstr(o, "stat_on", "state_on");
  e.st_off = sstr(o, "stat_off", "state_off");
  e.pl_avail = sstr(o, "pl_avail", "payload_available", "online");
  e.pl_navail = sstr(o, "pl_not_avail", "payload_not_available", "offline");
  e.json_light = comp == C_LIGHT && sstr(o, "schema", nullptr) == "json";
  if (e.state_t.empty()) {   // ohne Zustand nichts anzuzeigen
    s_skipped++;
    return;
  }
  // vorhandene Entitaet aktualisieren (Zustand behalten) oder neu
  for (size_t i = 0; i < s_ents.size(); i++)
    if (s_ents[i].uid == e.uid) {
      Ent &x = s_ents[i];
      e.idx = x.idx;
      e.has = x.has;
      e.on = x.on;
      e.val = x.val;
      e.text = x.text;
      e.avail = x.avail;
      e.ms = x.ms;
      x = e;
      rebuild_topics();
      add_topic(x.state_t, i);
      add_topic(x.avail_t, i);
      s_ver++;
      return;
    }
  if (s_ents.size() >= MAX_ENTS) {
    s_skipped++;
    return;
  }
  e.idx = idx_for(e.uid);
  if (!e.idx) {
    s_skipped++;
    return;
  }
  s_ents.push_back(e);
  size_t i = s_ents.size() - 1;
  add_topic(s_ents[i].state_t, i);
  add_topic(s_ents[i].avail_t, i);
  s_ver++;
}

void handle_state(Ent &e, const std::string &topic, const char *payload, size_t len) {
  if (topic == e.avail_t && topic != e.state_t) {
    std::string p(payload, len);
    e.avail = p != e.pl_navail;
    s_ver++;
    return;
  }
  std::string v;
  if (e.comp == C_LIGHT && e.json_light) {
    JsonDocument d;
    if (deserializeJson(d, payload, len)) return;
    v = d["state"] | "";
  } else if (!apply_tpl(e.tpl, payload, len, v)) {
    return;
  }
  e.ms = millis();
  e.has = true;
  if (e.comp == C_SENSOR) {
    char *end = nullptr;
    float f = strtof(v.c_str(), &end);
    if (end && end != v.c_str() && !isnan(f)) {
      e.val = f;
      e.text.clear();
    } else {
      e.val = NAN;
      e.text = v.substr(0, 40);
    }
  } else {
    const std::string &on = e.st_on.empty() ? e.pl_on : e.st_on;
    e.on = v == on || v == "ON" || v == "on" || v == "true" || v == "1";
    if (!e.st_off.empty() && v == e.st_off) e.on = false;
  }
  s_ver++;
}

}  // namespace

// ---------------------------------------------------------------- oeffentlich

void mqd_begin() {
  if (!s_mx) s_mx = xSemaphoreCreateRecursiveMutex();
  Preferences p;
  if (p.begin("mqdisco", true)) {
    s_on = p.getBool("on", false);
    s_pre = p.getString("pre", "homeassistant").c_str();
    p.end();
  }
  if (s_pre.empty()) s_pre = "homeassistant";
  map_load();
}

bool mqd_enabled() { return s_on; }
std::string mqd_prefix() {
  Lock l;
  return s_pre;
}

void mqd_set(bool on, const std::string &prefix) {
  std::string pre = prefix;
  while (!pre.empty() && (pre.back() == '/' || pre.back() == ' ')) pre.pop_back();
  if (pre.empty()) pre = "homeassistant";
  bool changed;
  {
    Lock l;
    changed = on != s_on || pre != s_pre;
    s_on = on;
    s_pre = pre;
    if (changed) {
      s_ents.clear();
      s_by_topic.clear();
      s_new_topics.clear();
      s_soc_devs.clear();
      s_skipped = 0;
      s_ver++;
    }
  }
  Preferences p;
  if (p.begin("mqdisco", false)) {
    p.putBool("on", on);
    p.putString("pre", pre.c_str());
    p.end();
  }
  if (changed) z2m_restart();   // neu abonnieren
}

MqdInfo mqd_info() {
  Lock l;
  MqdInfo r;
  r.entities = s_ents.size();
  std::set<std::string> devs;
  for (auto &e : s_ents) devs.insert(e.dev_id);
  r.devices = devs.size();
  r.skipped = s_skipped;
  return r;
}

std::vector<std::string> mqd_topics() {
  if (!s_on || !s_mx) return {};
  Lock l;
  std::vector<std::string> t = {s_pre + "/+/+/config", s_pre + "/+/+/+/config"};
  for (auto &e : s_by_topic) t.push_back(e.first);
  s_new_topics.clear();
  return t;
}

std::vector<std::string> mqd_take_new_topics() {
  if (!s_mx) return {};
  // Punktnummern gesammelt speichern (nicht bei jeder neuen Entitaet in den Flash schreiben)
  if (s_map_dirty && millis() - s_map_save_ms > 5000) {
    s_map_save_ms = millis();
    s_map_dirty = false;
    Lock l;
    map_save();
  }
  Lock l;
  std::vector<std::string> t;
  t.swap(s_new_topics);
  return t;
}

bool mqd_handle(const std::string &topic, const char *payload, size_t len) {
  if (!s_on || !s_mx) return false;
  Lock l;
  const std::string pre = s_pre + "/";
  if (topic.compare(0, pre.size(), pre) == 0 && topic.size() > 7 && topic.compare(topic.size() - 7, 7, "/config") == 0) {
    // <praefix>/<art>/[<knoten>/]<objekt>/config
    std::string rest = topic.substr(pre.size(), topic.size() - pre.size() - 7);
    size_t s = rest.find('/');
    if (s == std::string::npos) return true;
    handle_config(rest.substr(0, s), rest, payload, len);
    return true;
  }
  auto it = s_by_topic.find(topic);
  if (it == s_by_topic.end()) return false;
  for (size_t i : it->second)
    if (i < s_ents.size()) handle_state(s_ents[i], topic, payload, len);
  return true;
}

std::vector<SatPoint> mqd_points() {
  std::vector<SatPoint> r;
  if (!s_on || !s_mx) return r;
  Lock l;
  uint32_t now = millis();
  for (auto &e : s_ents) {
    SatPoint p;
    p.kind = e.comp == C_SENSOR ? SP_ANALOG : e.comp == C_BINARY ? SP_INPUT : SP_SWITCH;
    p.idx = e.idx;
    p.id = std::string(sat_kind_name(p.kind)) + std::to_string(e.idx);
    p.name = e.name;
    p.unit = e.unit;
    p.cls = e.comp == C_SENSOR ? cls_of(e.dev_cla, e.unit) : e.comp == C_BINARY ? SC_BINARY : SC_ONOFF;
    p.ok = e.has && (e.comp != C_SENSOR || !isnan(e.val));
    p.value = e.val;
    p.on = e.on;
    p.age_s = e.has ? (int32_t)((now - e.ms) / 1000) : -1;
    if (!e.avail) p.status = tr("offline");
    else if (e.comp == C_SENSOR && e.has && isnan(e.val)) p.status = e.text;
    r.push_back(p);
  }
  return r;
}

bool mqd_switch(uint8_t idx, bool on, std::string *err) {
  std::string topic, payload;
  {
    Lock l;
    for (auto &e : s_ents)
      if (e.idx == idx && (e.comp == C_SWITCH || e.comp == C_LIGHT)) {
        if (e.cmd_t.empty()) break;
        topic = e.cmd_t;
        payload = e.json_light ? std::string("{\"state\":\"") + (on ? "ON" : "OFF") + "\"}" : (on ? e.pl_on : e.pl_off);
        e.on = on;   // sofort zeigen, Bestaetigung kommt ueber das Zustands-Topic
        s_ver++;
        break;
      }
  }
  if (topic.empty() || !z2m_connected()) {
    if (err) *err = tr("Nicht verbunden.");
    return false;
  }
  z2m_publish(topic, payload);
  return true;
}

uint32_t mqd_version() { return s_on ? s_ver : 0; }

std::string mqd_device_name(uint8_t idx) {
  if (!s_mx) return std::string();
  Lock l;
  for (auto &e : s_ents)
    if (e.idx == idx) return e.dev;
  return std::string();
}

#endif
