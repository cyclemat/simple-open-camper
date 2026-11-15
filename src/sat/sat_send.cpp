#include "sat_send.h"
#include "sat_config.h"
#include "sat_io.h"
#include "../sat_proto.h"
#include "../truma.h"
#include "../mopeka.h"
#include "../victron.h"

#include <WiFi.h>
#include <WiFiUdp.h>
#include <NimBLEDevice.h>
#include <map>
#include <set>
#include <vector>
#include "mqtt_client.h"
#include "esp_random.h"
#include <ctype.h>

// ---------------------------------------------------------------- Momentaufnahme aller Geraete
struct Snap {
  SatType type;
  std::string mac, name, model;   // name/model fuer Home Assistant
  std::string sig;                // Werte ohne "age" – aendert sich nur bei neuen Messwerten
  std::string json;               // komplette Nachricht
  uint8_t adv[31];
  size_t adv_len = 0;
  std::string kind;               // Victron: "solar", "shunt", "dcdc"; Mopeka: Einheit
  SatPoint pt;                    // eigene Anschluesse (SAT_T_SENSOR/SAT_T_SWITCH)
};

static const char *type_name(SatType t) {
  switch (t) {
    case SAT_T_TRUMA: return "truma";
    case SAT_T_MOPEKA: return "mopeka";
    case SAT_T_SENSOR: return "sensor";
    case SAT_T_SWITCH: return "switch";
    default: return "victron";
  }
}
static std::string dev_key(const Snap &s) { return std::string(type_name(s.type)) + "/" + s.mac; }

template <class T>
static void fill(Snap &s, const T &info) {
  JsonDocument d;
  sat_to_json(d.to<JsonObject>(), info);
  serializeJson(d, s.json);
  d.remove("age");
  serializeJson(d, s.sig);
  s.adv_len = sat_adv_encode(s.adv, sizeof(s.adv), info);
}

static std::vector<Snap> collect() {
  std::vector<Snap> out;
  for (auto &t : truma_list()) {
    Snap s;
    s.type = SAT_T_TRUMA;
    s.mac = t.mac;
    s.name = t.name;
    s.model = "LevelControl";
    fill(s, t);
    out.push_back(std::move(s));
  }
  for (auto &m : mopeka_list()) {
    Snap s;
    s.type = SAT_T_MOPEKA;
    s.mac = m.mac;
    s.name = m.name;
    s.model = m.model_text;
    s.kind = m.unit;
    fill(s, m);
    out.push_back(std::move(s));
  }
  for (auto &v : victron_list()) {
    Snap s;
    s.type = SAT_T_VICTRON;
    s.mac = v.mac;
    s.name = v.name;
    s.model = victron_kind_text(v.kind);
    s.kind = v.kind == VIC_SOLAR ? "solar" : v.kind == VIC_SHUNT ? "shunt" : v.kind == VIC_DCDC ? "dcdc" : "";
    fill(s, v);
    out.push_back(std::move(s));
  }
  for (auto &p : sat_io_points()) {
    Snap s;
    s.type = p.kind == SP_SWITCH ? SAT_T_SWITCH : SAT_T_SENSOR;
    s.mac = p.id;
    s.name = p.name;
    // Name, Einheit und Klasse: aendert sich etwas davon, meldet der Satellit die Entitaet in HA neu
    s.model = std::string(sat_kind_name(p.kind)) + "|" + p.unit + "|" + std::to_string(p.cls);
    s.kind = sat_kind_name(p.kind);
    s.pt = p;
    fill(s, p);
    out.push_back(std::move(s));
  }
  return out;
}

// ---------------------------------------------------------------- MQTT
static esp_mqtt_client_handle_t s_mqtt = nullptr;
static volatile bool s_mqtt_conn = false;
static volatile bool s_mqtt_fresh = false;     // gerade verbunden: alles neu senden
static String s_mqtt_status = "aus";
static std::string s_m_host, s_m_user, s_m_pass, s_m_id, s_m_lwt;
static std::map<std::string, std::string> s_pub_sig;           // Topic -> zuletzt gesendete Werte
static std::map<std::string, uint32_t> s_pub_ms;
static std::map<std::string, std::vector<std::string>> s_ha;   // Geraet -> Discovery-Topics
static bool s_ha_sat = false;

static void publish(const std::string &topic, const std::string &payload, bool retain = true) {
  if (s_mqtt && s_mqtt_conn) esp_mqtt_client_enqueue(s_mqtt, topic.c_str(), payload.data(), payload.size(), 1, retain, true);
}

static void handle_cmd(const char *data, int len) {
  JsonDocument d;
  if (deserializeJson(d, data, len)) return;
  if (d["measure"].is<const char *>()) truma_measure(d["measure"].as<const char *>());
  if (d["switch"].is<const char *>()) {   // {"switch":"switch1","on":true}  (ohne "on": umschalten)
    SatPointKind k;
    uint8_t idx;
    if (sat_point_parse(d["switch"].as<const char *>(), k, idx) && k == SP_SWITCH)
      sat_io_switch(idx, d["on"].is<bool>() ? (d["on"].as<bool>() ? 1 : 0) : 2);
  }
  if (d["reboot"] | false) ESP.restart();
}

// <base>/<id>/switch/<punkt>/set  "ON" / "OFF" / "TOGGLE" (auch 1/0, true/false)
static void handle_set(const std::string &topic, const char *data, int len) {
  std::string pre = sat_topic("switch/").c_str();
  if (topic.compare(0, pre.size(), pre) != 0 || topic.size() < pre.size() + 5) return;
  std::string pid = topic.substr(pre.size(), topic.size() - pre.size() - 4);
  SatPointKind k;
  uint8_t idx;
  if (!sat_point_parse(pid, k, idx) || k != SP_SWITCH) return;
  std::string v(data, len);
  for (auto &c : v) c = toupper((unsigned char)c);
  uint8_t val = (v == "ON" || v == "1" || v == "TRUE") ? 1 : (v == "OFF" || v == "0" || v == "FALSE") ? 0 : 2;
  sat_io_switch(idx, val);
}

static void mqtt_event(void *, esp_event_base_t, int32_t id, void *data) {
  auto *ev = (esp_mqtt_event_handle_t)data;
  switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
      s_mqtt_conn = true;
      s_mqtt_fresh = true;
      s_mqtt_status = "verbunden";
      esp_mqtt_client_subscribe(s_mqtt, sat_topic("cmd").c_str(), 0);
      esp_mqtt_client_subscribe(s_mqtt, sat_topic("switch/+/set").c_str(), 0);
      break;
    case MQTT_EVENT_DISCONNECTED:
      s_mqtt_conn = false;
      s_mqtt_status = "getrennt, verbinde neu …";
      break;
    case MQTT_EVENT_ERROR:
      if (ev->error_handle && ev->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        s_mqtt_status = "Zugang abgelehnt – Benutzer/Passwort prüfen";
      else if (ev->error_handle && ev->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        s_mqtt_status = "Broker nicht erreichbar";
      break;
    case MQTT_EVENT_DATA:
      if (ev->current_data_offset == 0 && ev->data_len == ev->total_data_len && ev->data_len < 512) {
        std::string topic(ev->topic, ev->topic_len);
        if (topic.size() > 4 && topic.compare(topic.size() - 4, 4, "/set") == 0) handle_set(topic, ev->data, ev->data_len);
        else handle_cmd(ev->data, ev->data_len);
      }
      break;
    default:
      break;
  }
}

static void mqtt_stop() {
  if (!s_mqtt) return;
  esp_mqtt_client_stop(s_mqtt);
  esp_mqtt_client_destroy(s_mqtt);
  s_mqtt = nullptr;
  s_mqtt_conn = false;
}

static void mqtt_start() {
  mqtt_stop();
  s_pub_sig.clear();
  s_pub_ms.clear();
  s_ha.clear();
  s_ha_sat = false;
  if (!(g_sat.modes & SAT_M_MQTT)) {
    s_mqtt_status = "aus";
    return;
  }
  if (g_sat.mqtt_host.isEmpty()) {
    s_mqtt_status = "kein Broker eingetragen";
    return;
  }
  s_m_host = g_sat.mqtt_host.c_str();
  s_m_user = g_sat.mqtt_user.c_str();
  s_m_pass = g_sat.mqtt_pass.c_str();
  s_m_id = std::string("panelsat-") + g_sat.id.c_str();
  s_m_lwt = sat_topic("online").c_str();
  esp_mqtt_client_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.broker.address.hostname = s_m_host.c_str();
  cfg.broker.address.port = g_sat.mqtt_port;
  cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
  cfg.credentials.client_id = s_m_id.c_str();
  if (!s_m_user.empty()) cfg.credentials.username = s_m_user.c_str();
  if (!s_m_pass.empty()) cfg.credentials.authentication.password = s_m_pass.c_str();
  cfg.session.last_will.topic = s_m_lwt.c_str();
  cfg.session.last_will.msg = "offline";
  cfg.session.last_will.qos = 1;
  cfg.session.last_will.retain = 1;
  cfg.session.keepalive = 30;
  cfg.network.reconnect_timeout_ms = 5000;
  cfg.buffer.size = 2048;
  cfg.buffer.out_size = 2048;
  cfg.task.stack_size = 6144;
  s_mqtt = esp_mqtt_client_init(&cfg);
  if (!s_mqtt) {
    s_mqtt_status = "MQTT-Client konnte nicht starten";
    return;
  }
  esp_mqtt_client_register_event(s_mqtt, MQTT_EVENT_ANY, mqtt_event, nullptr);
  esp_mqtt_client_start(s_mqtt);
  s_mqtt_status = "verbinde …";
}

bool sat_mqtt_connected() { return s_mqtt_conn; }
String sat_mqtt_status() { return s_mqtt_status; }

// ---------------------------------------------------------------- Home-Assistant-Discovery
struct HaField {
  const char *key, *name, *unit, *dev_class, *state_class;
};
static const HaField HA_TRUMA[] = {
    {"percent", "Füllstand", "%", nullptr, "measurement"},
    {"kg", "Gas", "kg", "weight", "measurement"},
    {"battery", "Batterie", "%", "battery", "measurement"},
    {"temp", "Temperatur", "°C", "temperature", "measurement"},
};
static const HaField HA_MOPEKA_KG[] = {
    {"percent", "Füllstand", "%", nullptr, "measurement"},
    {"amount", "Inhalt", "kg", "weight", "measurement"},
    {"battery", "Batterie", "%", "battery", "measurement"},
    {"temp", "Temperatur", "°C", "temperature", "measurement"},
};
static const HaField HA_MOPEKA_L[] = {
    {"percent", "Füllstand", "%", nullptr, "measurement"},
    {"amount", "Inhalt", "L", "volume_storage", "measurement"},
    {"battery", "Batterie", "%", "battery", "measurement"},
    {"temp", "Temperatur", "°C", "temperature", "measurement"},
};
static const HaField HA_SOLAR[] = {
    {"pv", "PV-Leistung", "W", "power", "measurement"},
    {"v", "Batteriespannung", "V", "voltage", "measurement"},
    {"i", "Ladestrom", "A", "current", "measurement"},
    {"yield", "Ertrag heute", "kWh", "energy", "total_increasing"},
};
static const HaField HA_SHUNT[] = {
    {"soc", "Ladestand", "%", "battery", "measurement"},
    {"v", "Spannung", "V", "voltage", "measurement"},
    {"i", "Strom", "A", "current", "measurement"},
    {"consumed", "Verbraucht", "Ah", nullptr, "measurement"},
    {"remaining", "Restlaufzeit", "min", "duration", "measurement"},
};
static const HaField HA_DCDC[] = {
    {"v", "Ausgang Spannung", "V", "voltage", "measurement"},
    {"i", "Ausgang Strom", "A", "current", "measurement"},
    {"in_v", "Eingang Spannung", "V", "voltage", "measurement"},
};

static std::string nocolon(const std::string &mac) {
  std::string r;
  for (char c : mac)
    if (c != ':') r += c;
  return r;
}

static void ha_clear(const std::string &key) {
  auto it = s_ha.find(key);
  if (it == s_ha.end()) return;
  for (auto &t : it->second) publish(t, "");
  s_ha.erase(it);
}

static void ha_sensor(std::vector<std::string> &topics, const std::string &obj, const std::string &state_topic,
                      const HaField &f, JsonObjectConst dev, bool diag = false) {
  std::string id = g_sat.id.c_str();
  std::string topic = std::string(g_sat.ha_prefix.c_str()) + "/sensor/" + id + "/" + obj + "_" + f.key + "/config";
  JsonDocument d;
  d["name"] = f.name;
  d["uniq_id"] = id + "_" + obj + "_" + f.key;
  d["stat_t"] = state_topic;
  d["val_tpl"] = std::string("{{ value_json.") + f.key + " if value_json." + f.key + " is defined else None }}";
  if (f.unit) d["unit_of_meas"] = f.unit;
  if (f.dev_class) d["dev_cla"] = f.dev_class;
  if (f.state_class) d["stat_cla"] = f.state_class;
  if (diag) d["ent_cat"] = "diagnostic";
  d["avty_t"] = sat_topic("online").c_str();
  d["dev"] = dev;
  std::string s;
  serializeJson(d, s);
  publish(topic, s);
  topics.push_back(topic);
}

static void ha_sat_device() {
  std::vector<std::string> topics;
  JsonDocument dev;
  dev["ids"].add(g_sat.id.c_str());
  dev["name"] = g_sat.name.c_str();
  dev["mf"] = "Simply Open Camper";
  dev["mdl"] = "Satellit";
  dev["sw"] = SAT_FW_VERSION;
  HaField rssi = {"rssi", "WLAN-Signal", "dBm", "signal_strength", "measurement"};
  ha_sensor(topics, "sat", sat_topic("info").c_str(), rssi, dev.as<JsonObjectConst>(), true);
  s_ha["sat"] = topics;
  s_ha_sat = true;
}

static void ha_device(const Snap &s) {
  std::string key = dev_key(s);
  ha_clear(key);
  const HaField *f = nullptr;
  size_t n = 0;
  const char *mf = "Victron";
  if (s.type == SAT_T_TRUMA) { f = HA_TRUMA; n = 4; mf = "Truma"; }
  else if (s.type == SAT_T_MOPEKA) { f = s.kind == "kg" ? HA_MOPEKA_KG : HA_MOPEKA_L; n = 4; mf = "Mopeka"; }
  else if (s.kind == "solar") { f = HA_SOLAR; n = 4; }
  else if (s.kind == "shunt") { f = HA_SHUNT; n = 5; }
  else if (s.kind == "dcdc") { f = HA_DCDC; n = 3; }
  if (!f) return;   // Victron-Geraeteart noch unbekannt – spaeter erneut
  std::string obj = nocolon(s.mac);
  JsonDocument dev;
  dev["ids"].add(std::string(g_sat.id.c_str()) + "_" + obj);
  dev["name"] = s.name;
  dev["mf"] = mf;
  dev["mdl"] = s.model;
  dev["via_device"] = g_sat.id.c_str();
  std::vector<std::string> topics;
  std::string st = sat_topic(key.c_str()).c_str();
  for (size_t i = 0; i < n; i++) ha_sensor(topics, obj, st, f[i], dev.as<JsonObjectConst>());
  s_ha[key] = topics;
}

// Eigene Anschluesse: Entitaeten direkt am Satelliten-Geraet in Home Assistant
static void ha_point(const Snap &s) {
  std::string key = dev_key(s);
  ha_clear(key);
  const SatPoint &p = s.pt;
  std::string id = g_sat.id.c_str();
  const char *comp = p.kind == SP_SWITCH ? "switch" : p.kind == SP_INPUT ? "binary_sensor" : "sensor";
  std::string topic = std::string(g_sat.ha_prefix.c_str()) + "/" + comp + "/" + id + "/" + p.id + "/config";
  std::string st = sat_topic(key.c_str()).c_str();
  JsonDocument d;
  d["name"] = p.name;
  d["uniq_id"] = id + "_" + p.id;
  d["stat_t"] = st;
  d["avty_t"] = sat_topic("online").c_str();
  if (p.kind == SP_SWITCH || p.kind == SP_INPUT) {
    d["val_tpl"] = "{{ 'ON' if value_json.on else 'OFF' }}";
    if (p.kind == SP_SWITCH) d["cmd_t"] = st + "/set";
  } else {
    d["val_tpl"] = "{{ value_json.value if value_json.value is defined else None }}";
    if (!p.unit.empty()) d["unit_of_meas"] = p.unit;
    d["stat_cla"] = "measurement";
    const char *dc = p.cls == SC_TEMP ? "temperature" : p.cls == SC_HUMIDITY ? "humidity" : p.cls == SC_VOLTAGE ? "voltage"
                   : p.cls == SC_PRESSURE ? "pressure" : p.cls == SC_CURRENT ? "current" : nullptr;
    if (dc) d["dev_cla"] = dc;
    if (p.kind == SP_TANK) d["ic"] = "mdi:water-percent";
    if (p.kind == SP_TILT) d["ic"] = "mdi:angle-acute";
  }
  JsonObject dev = d["dev"].to<JsonObject>();
  dev["ids"].add(id);
  dev["name"] = g_sat.name.c_str();
  std::string out;
  serializeJson(d, out);
  publish(topic, out);
  s_ha[key] = {topic};
}

static std::map<std::string, std::string> s_ha_sig;   // Geraet -> Name/Art, bei Aenderung neu melden

static void mqtt_publish_all(const std::vector<Snap> &snaps, bool force) {
  uint32_t now = millis();
  if (force) {
    publish(sat_topic("online").c_str(), "online");
    s_pub_sig.clear();
    s_ha_sig.clear();
    s_ha_sat = false;
  }
  std::set<std::string> present;
  for (auto &s : snaps) {
    std::string key = dev_key(s);
    std::string topic = sat_topic(key.c_str()).c_str();
    present.insert(topic);
    auto it = s_pub_sig.find(topic);
    bool changed = it == s_pub_sig.end() || it->second != s.sig;
    // Messwerte eigener Sensoren hoechstens alle 5 s (Schalter und Eingaenge sofort)
    if (changed && s.type == SAT_T_SENSOR && s.pt.kind != SP_INPUT && it != s_pub_sig.end() && now - s_pub_ms[topic] < 5000)
      changed = false;
    if (changed || now - s_pub_ms[topic] > 60000) {
      publish(topic, s.json);
      s_pub_sig[topic] = s.sig;
      s_pub_ms[topic] = now;
    }
    if (g_sat.ha) {
      std::string hs = s.name + "|" + s.kind + "|" + s.model;
      if (s_ha_sig[key] != hs) {
        if (s.type == SAT_T_SENSOR || s.type == SAT_T_SWITCH) ha_point(s);
        else ha_device(s);
        if (s_ha.count(key)) s_ha_sig[key] = hs;
      }
    }
  }
  // am Satelliten entfernte Geraete: Topic und Discovery leeren
  for (auto it = s_pub_sig.begin(); it != s_pub_sig.end();) {
    if (!present.count(it->first)) {
      publish(it->first, "");
      std::string key = it->first.substr(sat_topic("").length());
      ha_clear(key);
      s_ha_sig.erase(key);
      s_pub_ms.erase(it->first);
      it = s_pub_sig.erase(it);
    } else ++it;
  }
  if (g_sat.ha && !s_ha_sat) ha_sat_device();
  if (!g_sat.ha && !s_ha.empty()) {
    std::vector<std::string> keys;
    for (auto &e : s_ha) keys.push_back(e.first);
    for (auto &k : keys) ha_clear(k);
    s_ha_sig.clear();
    s_ha_sat = false;
  }
}

// ---------------------------------------------------------------- Info
static std::string info_json() {
  JsonDocument d;
  d["name"] = g_sat.name.c_str();
  d["fw"] = SAT_FW_VERSION;
  d["chip"] = sat_io_chip();
  d["ip"] = WiFi.localIP().toString().c_str();
  d["rssi"] = WiFi.isConnected() ? WiFi.RSSI() : 0;
  d["uptime"] = millis() / 1000;
  d["heap"] = ESP.getFreeHeap();
  d["modes"] = g_sat.modes;
  d["ble"] = sat_ble_mac().c_str();   // Ziel fuer verschluesselte Schaltbefehle
  std::string s;
  serializeJson(d, s);
  return s;
}

// ---------------------------------------------------------------- UDP
static WiFiUDP s_udp;
static uint32_t s_udp_count = 0;
static std::map<std::string, std::string> s_udp_sig;
static std::map<std::string, uint32_t> s_udp_ms;

static void udp_send(const std::string &payload) {
  IPAddress ip(255, 255, 255, 255);
  if (!g_sat.udp_target.isEmpty() && !ip.fromString(g_sat.udp_target)) return;
  if (g_sat.udp_target.isEmpty()) {
    // im eigenen Netz an die Broadcast-Adresse (255.255.255.255 lassen manche Router nicht durch)
    IPAddress me = WiFi.localIP(), mask = WiFi.subnetMask();
    for (int i = 0; i < 4; i++) ip[i] = (me[i] & mask[i]) | (~mask[i] & 0xFF);
  }
  if (!s_udp.beginPacket(ip, SAT_UDP_PORT)) return;
  s_udp.write((const uint8_t *)payload.data(), payload.size());
  if (s_udp.endPacket()) s_udp_count++;
}

static void udp_publish_all(const std::vector<Snap> &snaps, bool info) {
  uint32_t now = millis();
  std::string sat = g_sat.id.c_str(), satname = g_sat.name.c_str();
  if (info) {
    std::string p = "{\"sat\":\"" + sat + "\",\"type\":\"info\",\"d\":" + info_json() + "}";
    udp_send(p);
  }
  for (auto &s : snaps) {
    std::string key = dev_key(s);
    auto it = s_udp_sig.find(key);
    // bei neuen Werten sofort, sonst alle 15 s (UDP kann verloren gehen)
    if (it != s_udp_sig.end() && it->second == s.sig && now - s_udp_ms[key] < 15000) continue;
    JsonDocument d;
    d["sat"] = sat;
    d["name"] = satname;
    d["type"] = type_name(s.type);
    d["mac"] = s.mac;
    d["d"] = serialized(s.json);
    std::string p;
    serializeJson(d, p);
    udp_send(p);
    s_udp_sig[key] = s.sig;
    s_udp_ms[key] = now;
  }
}

uint32_t sat_udp_sent() { return s_udp_count; }

// ---------------------------------------------------------------- Schaltbefehle vom Panel (UDP und Bluetooth)
static uint8_t s_cmd_key[16];
static std::string s_cmd_pw = "\x01", s_own_mac;
static volatile uint32_t s_cmd_ok = 0;

static void cmd_key_update() {
  if (s_own_mac.empty()) s_own_mac = sat_ble_mac().c_str();
  if (s_cmd_pw == g_sat.ble_pw.c_str()) return;
  s_cmd_pw = g_sat.ble_pw.c_str();
  sat_key_from_password(s_cmd_pw, s_cmd_key);
}

static bool cmd_handle(const uint8_t *md, size_t len) {
  if (s_own_mac.empty()) return false;
  SatPointKind k;
  uint8_t idx, val;
  uint32_t nonce;
  if (!sat_cmd_open(md, len, s_cmd_key, s_own_mac, k, idx, val, nonce) || k != SP_SWITCH) return false;
  if (!sat_io_switch_nonce(idx, val, nonce)) return false;   // alt oder schon ausgefuehrt
  s_cmd_ok++;
  return true;
}

static void udp_receive() {
  for (int i = 0; i < 4; i++) {
    int n = s_udp.parsePacket();
    if (n <= 0) return;
    uint8_t b[64];
    int r = s_udp.read(b, sizeof(b));
    if (r == 16 && b[0] == 0xFF && b[1] == 0xFF && b[2] == 0x43) cmd_handle(b, r);
  }
}

// ---------------------------------------------------------------- Bluetooth-Werbepakete
static bool s_adv_on = false;
static size_t s_adv_idx = 0;
static uint32_t s_adv_n = 0;
static uint16_t s_adv_counter = (uint16_t)esp_random();
static uint8_t s_adv_key[16];
static std::string s_adv_pw = "\x01", s_adv_sender, s_adv_name;

static void adv_stop() {
  if (!s_adv_on) return;
  NimBLEDevice::getAdvertising()->stop();
  s_adv_on = false;
}

static bool s_adv_suspended = false;   // Einrichtungsmodus (sat_pair.cpp) nutzt die Werbung selbst

void sat_send_adv_suspend(bool on) {
  s_adv_suspended = on;
  if (on) adv_stop();
}

static void adv_next(const std::vector<Snap> &snaps, bool urgent) {
  if (s_adv_suspended) return;
  std::vector<const Snap *> list;
  for (auto &s : snaps)
    if (s.adv_len) list.push_back(&s);
  s_adv_n = list.size();
  if (list.empty()) {
    adv_stop();
    return;
  }
  const Snap *s = nullptr;
  if (urgent) {   // gerade geschaltet: Schalter abwechselnd mit der normalen Reihe, damit das Panel es schnell sieht
    static size_t sw_idx = 0;
    std::vector<const Snap *> sws;
    for (auto *x : list)
      if (x->type == SAT_T_SWITCH) sws.push_back(x);
    if (!sws.empty()) s = sws[sw_idx++ % sws.size()];
  }
  if (!s) s = list[s_adv_idx++ % list.size()];
  truma_ble_init();
  if (s_adv_pw != g_sat.ble_pw.c_str()) {   // Passwort geaendert (oder erster Aufruf)
    s_adv_pw = g_sat.ble_pw.c_str();
    sat_key_from_password(s_adv_pw, s_adv_key);
  }
  if (s_adv_sender.empty()) {
    s_adv_sender = NimBLEDevice::getAddress().toString();
    for (auto &c : s_adv_sender) c = tolower((unsigned char)c);
    s_adv_name = sat_ble_name(s_adv_sender);
    NimBLEDevice::setDeviceName(s_adv_name);
  }
  uint8_t md[31];
  memcpy(md, s->adv, s->adv_len);
  size_t len = sat_adv_seal(md, s->adv_len, sizeof(md), s_adv_key, s_adv_sender, ++s_adv_counter);
  if (!len) return;
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData data;
  data.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  data.setManufacturerData(md, len);
  adv->setAdvertisementData(data);
  if (!s_adv_on) {
    NimBLEAdvertisementData scan;   // Name "Satellite-<MAC>" fuer aktive Suchen (Handy-Apps, Panel-Einrichtung)
    scan.setName(s_adv_name);
    adv->setScanResponseData(scan);
    adv->setConnectableMode(BLE_GAP_CONN_MODE_NON);
    adv->setMinInterval(160);   // 100 ms
    adv->setMaxInterval(240);   // 150 ms
    s_adv_on = adv->start();
  }
}

uint32_t sat_adv_count() { return s_adv_n; }
String sat_ble_name_now() { return s_adv_name.empty() ? sat_ble_name(sat_ble_mac().c_str()).c_str() : s_adv_name.c_str(); }

// ---------------------------------------------------------------- Ablauf
static uint32_t s_last_ms = 0, s_info_ms = 0, s_udp_info_ms = 0, s_seen_ver = 0;
static std::vector<Snap> s_snaps;
static uint32_t s_snap_ms = 0;
static bool s_udp_on = false;

void sat_send_begin() { sat_send_reconfigure(); }

void sat_send_reconfigure() {
  mqtt_start();
  // fester Port: das Panel schickt Schaltbefehle hierher
  if ((g_sat.modes & SAT_M_UDP) && !s_udp_on) s_udp_on = s_udp.begin(SAT_UDP_PORT);
  s_udp_sig.clear();
  if (!(g_sat.modes & SAT_M_BLE)) adv_stop();
  s_seen_ver = 0;
}

static uint32_t s_adv_ms = 0;

void sat_send_loop() {
  uint32_t now = millis();
  cmd_key_update();
  // Bluetooth: Schaltbefehle des Panels nur hoeren, wenn es Schalter gibt
  sat_ble_listen((g_sat.modes & SAT_M_BLE) && sat_io_has_switches());
  if (s_udp_on && WiFi.isConnected()) udp_receive();
  uint32_t ver = truma_version() * 7 + mopeka_version() * 13 + victron_version() * 31 + sat_io_version() * 61;
  bool fresh_io = ver != s_seen_ver && now - sat_io_last_switch_ms() < 1000;
  // Bluetooth-Rotation: je Paket 1 s, bei vielen Werten 0,5 s; nach dem Schalten sofort
  if (g_sat.modes & SAT_M_BLE) {
    uint32_t slot = s_adv_n > 8 ? 500 : 1000;
    bool urgent = now - sat_io_last_switch_ms() < 4000;
    if (fresh_io || now - s_adv_ms >= slot) {
      if (ver != s_seen_ver) {
        s_seen_ver = ver;
        s_snap_ms = now;
        s_snaps = collect();
      }
      s_adv_ms = now;
      adv_next(s_snaps, urgent);
    }
  }
  if (!fresh_io && now - s_last_ms < 1000) return;
  s_last_ms = now;
  // Werte neu einsammeln bei Aenderungen, sonst alle 5 s (fuer das Alter)
  if (ver != s_seen_ver || now - s_snap_ms > 5000 || s_snaps.empty()) {
    s_seen_ver = ver;
    s_snap_ms = now;
    s_snaps = collect();
  }
  bool wifi = WiFi.isConnected();
  if ((g_sat.modes & SAT_M_MQTT) && s_mqtt_conn) {
    bool fresh = s_mqtt_fresh;
    s_mqtt_fresh = false;
    mqtt_publish_all(s_snaps, fresh);
    if (fresh || now - s_info_ms > 60000) {
      s_info_ms = now;
      publish(sat_topic("info").c_str(), info_json());
    }
  }
  if ((g_sat.modes & SAT_M_UDP) && wifi && s_udp_on) {
    bool info = now - s_udp_info_ms > 30000 || !s_udp_info_ms;
    if (info) s_udp_info_ms = now;
    udp_publish_all(s_snaps, info);
  }
}

// Bluetooth-Scan (truma.cpp): Herstellerdaten 0xFFFF – im Satelliten nur Schaltbefehle des Panels ('C')
bool panelsat_on_adv(const std::string &, const uint8_t *md, size_t len, int) {
  if (len != 16 || md[2] != 0x43) return false;
  return cmd_handle(md, len);
}

uint32_t sat_cmd_count() { return s_cmd_ok; }
