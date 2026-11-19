#include "app_config.h"
#include "panelsat.h"
#include "sat_proto.h"
#include "hymer.h"
#include "cerbo.h"
#include "net.h"
#include "settings.h"

#if WITH_SATELLITES
#include <Arduino.h>
#include <algorithm>
#include <map>
#include <Preferences.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "i18n/i18n.h"
#include "z2m.h"
#include "settings.h"
#include "truma.h"
#include <NimBLEDevice.h>
#include "ui/ui_remote.h"

static const uint32_t NODE_STALE_MS = 5 * 60 * 1000;   // UDP/Bluetooth: so lange ohne Paket gilt der Satellit als weg
static const uint8_t VIA_MQTT = 1, VIA_UDP = 2, VIA_BLE = 4;

struct Dev {
  std::string sat;          // Kennung des Satelliten ("" = nur per Bluetooth bekannt)
  SatType type;
  std::string mac;
  TrumaInfo t;
  MopekaInfo m;
  VictronInfo v;
  SatPoint p;               // eigene Anschluesse des Satelliten (Sensor/Schalter)
  uint32_t pending_ms = 0;  // Schalter: Befehl unterwegs (Anzeige vorab umgestellt)
  bool named = false;       // Name kam per MQTT/UDP (Bluetooth bringt keinen mit)
  int32_t age_rx = -1;      // "age" beim Empfang
  uint32_t rx_ms = 0;
  uint8_t via = 0;
};

struct Node {
  std::string name, ip, fw;
  std::string ble;          // Bluetooth-MAC des Satelliten (Ziel verschluesselter Schaltbefehle)
  // Der Satellit hat EINE Nonce fuer alle Schalter, neu nach jeder Schaltung. Die zuletzt empfangene gilt fuer alle
  // seine Schalter (je Schalter gemerkt war sie nach dem Schalten eines anderen bis zu 15 s veraltet -> Befehl verworfen).
  uint32_t sw_nonce = 0;
  bool has_nonce = false;
  int rssi = 0;
  int mqtt_online = -1;     // -1 unbekannt, 0 offline, 1 online
  uint32_t seen_ms = 0;
  uint8_t via = 0;
};

static SemaphoreHandle_t s_mtx = nullptr;
namespace {   // dateilokal: gleichnamige Typen anderer Dateien duerfen sich nicht vermischen
struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};
}  // namespace

static std::map<std::string, Dev> s_devs;    // "truma/<mac>" ...
static std::map<std::string, Node> s_nodes;  // Kennung -> Satellit
static volatile uint32_t s_ver = 1;

// Bluetooth: gemeinsames Passwort aller Satelliten (Schluessel fuer die Verschluesselung der Werbepakete).
// Satelliten mit passendem Passwort werden automatisch aufgenommen und gemerkt ("Satellite-<MAC>").
static std::string s_pw = SAT_DEFAULT_PASSWORD;
static uint8_t s_key[16];
static std::vector<std::string> s_known;     // automatisch aufgenommene Bluetooth-Satelliten

static void save_known() {
  Preferences p;
  p.begin("panelsat", false);
  std::string all;
  for (auto &id : s_known) all += (all.empty() ? "" : ",") + id;
  p.putString("known", all.c_str());
  p.end();
}

static void load_config() {
  Preferences p;
  p.begin("panelsat", true);
  s_pw = p.getString("pw", SAT_DEFAULT_PASSWORD).c_str();
  std::string all = p.getString("known", "").c_str();
  p.end();
  sat_key_from_password(s_pw, s_key);
  size_t a = 0;
  while (a < all.size()) {
    size_t b = all.find(',', a);
    if (b == std::string::npos) b = all.size();
    if (b > a) {
      std::string id = all.substr(a, b - a);
      s_known.push_back(id);
      Node &n = s_nodes[id];
      n.name = id;
      n.via = 4;
    }
    a = b + 1;
  }
}

std::string panelsat_password() {
  Lock l;
  return s_pw;
}

void panelsat_set_password(const std::string &pw) {
  {
    Lock l;
    s_pw = pw.empty() ? SAT_DEFAULT_PASSWORD : pw;
    sat_key_from_password(s_pw, s_key);
    s_ver++;
  }
  Preferences p;
  p.begin("panelsat", false);
  p.putString("pw", s_pw.c_str());
  p.end();
}

static const char *type_name(SatType t) {
  switch (t) {
    case SAT_T_TRUMA: return "truma";
    case SAT_T_MOPEKA: return "mopeka";
    case SAT_T_SENSOR: return "sensor";
    case SAT_T_SWITCH: return "switch";
    default: return "victron";
  }
}
static bool type_of(const std::string &s, SatType &t) {
  if (s == "truma") t = SAT_T_TRUMA;
  else if (s == "mopeka") t = SAT_T_MOPEKA;
  else if (s == "victron") t = SAT_T_VICTRON;
  else if (s == "sensor") t = SAT_T_SENSOR;
  else if (s == "switch") t = SAT_T_SWITCH;
  else return false;
  return true;
}
static bool is_point(SatType t) { return t == SAT_T_SENSOR || t == SAT_T_SWITCH; }
// Geraete eines Sensors gehoeren allen (MAC), Anschluesse nur ihrem Satelliten ("tank1" gibt es mehrfach)
static std::string dev_key(const std::string &sat, SatType t, const std::string &mac) {
  return is_point(t) ? "point/" + sat + "/" + mac : std::string(type_name(t)) + "/" + mac;
}

static std::string point_default_name(SatPointKind k, int idx) {
  if (const char *hn = hymer_point_name(idx)) return tr(hn);   // Hymer-Wert eines Satelliten
  char b[48];
  switch (k) {
    case SP_TANK: snprintf(b, sizeof(b), tr("Tank %d"), idx); break;
    case SP_TEMP: snprintf(b, sizeof(b), tr("Temperatur %d"), idx); break;
    case SP_ANALOG: snprintf(b, sizeof(b), tr("Analog %d"), idx); break;
    case SP_TILT: return idx == 1 ? tr("Neigung längs") : tr("Neigung quer");
    case SP_INPUT: snprintf(b, sizeof(b), tr("Eingang %d"), idx); break;
    case SP_SWITCH: snprintf(b, sizeof(b), tr("Schalter %d"), idx); break;
    default: return tr("Sensor");
  }
  return b;
}

static std::string lower(std::string s) {
  for (auto &c : s) c = tolower((unsigned char)c);
  return s;
}

// ---------------------------------------------------------------- Werte uebernehmen
static void store_json(const std::string &sat, SatType type, const std::string &mac_in, JsonObjectConst d, uint8_t via) {
  std::string mac = lower(mac_in);
  std::string k = dev_key(sat, type, mac);
  Lock l;
  Dev &x = s_devs[k];
  x.sat = sat;
  x.type = type;
  x.mac = mac;
  x.named = true;
  x.via |= via;
  x.rx_ms = millis();
  switch (type) {
    case SAT_T_TRUMA: sat_from_json(d, x.t); x.age_rx = x.t.age_s; break;
    case SAT_T_MOPEKA: sat_from_json(d, x.m); x.age_rx = x.m.age_s; break;
    case SAT_T_VICTRON: sat_from_json(d, x.v); x.age_rx = x.v.age_s; break;
    case SAT_T_SENSOR:
    case SAT_T_SWITCH: {
      bool keep = x.pending_ms && millis() - x.pending_ms < 5000 && x.p.on != (d["on"] | false);
      bool on = x.p.on;
      SatPoint p;
      sat_from_json(d, p);
      p.id = mac;
      sat_point_parse(mac, p.kind, p.idx);
      if (p.name.empty()) p.name = point_default_name(p.kind, p.idx);
      x.p = p;
      if (keep) x.p.on = on;   // Befehl noch unterwegs: nicht zurueckspringen
      else x.pending_ms = 0;
      x.age_rx = x.p.age_s;
      break;
    }
    default: break;
  }
  if (!sat.empty()) {
    Node &n = s_nodes[sat];
    n.seen_ms = millis();
    n.via |= via;
    if (type == SAT_T_SWITCH && x.p.kind == SP_SWITCH) {
      n.sw_nonce = x.p.nonce;
      n.has_nonce = true;
    }
  }
  s_ver++;
}

static void remove_dev(const std::string &sat, SatType type, const std::string &mac) {
  Lock l;
  if (s_devs.erase(dev_key(sat, type, lower(mac)))) s_ver++;
}

static void store_info(const std::string &sat, JsonObjectConst d, uint8_t via) {
  Lock l;
  Node &n = s_nodes[sat];
  n.name = d["name"] | n.name.c_str();
  n.ip = d["ip"] | "";
  n.fw = d["fw"] | "";
  n.rssi = d["rssi"] | 0;
  if (d["ble"].is<const char *>()) n.ble = lower(d["ble"].as<const char *>());
  n.seen_ms = millis();
  n.via |= via;
  s_ver++;
}

// ---------------------------------------------------------------- MQTT
std::vector<std::string> panelsat_topics() { return {std::string(SAT_BASE) + "/+/#"}; }

bool panelsat_handle(const std::string &topic, const char *payload, size_t len) {
  static const std::string base = std::string(SAT_BASE) + "/";
  if (topic.compare(0, base.size(), base) != 0) return false;
  size_t a = topic.find('/', base.size());
  if (a == std::string::npos) return true;
  std::string sat = topic.substr(base.size(), a - base.size());
  std::string rest = topic.substr(a + 1);
  // Zusatzdisplays ("Display-…"): nur ihre Meldungen an ui_remote, sonst nicht als Satellit fuehren
  if (sat.rfind("Display-", 0) == 0) {
    if (rest.rfind("rd/", 0) == 0) ui_remote_mqtt_in(sat, rest, payload, len);
    return true;
  }
  if (rest == "online") {
    Lock l;
    Node &n = s_nodes[sat];
    n.mqtt_online = (len == 6 && !memcmp(payload, "online", 6)) ? 1 : 0;
    n.via |= VIA_MQTT;
    if (n.mqtt_online) n.seen_ms = millis();
    s_ver++;
    return true;
  }
  if (rest == "cmd" || rest.find("/set") != std::string::npos) return true;   // eigene Befehle
  JsonDocument d;
  if (rest == "info") {
    if (len && !deserializeJson(d, payload, len)) store_info(sat, d.as<JsonObjectConst>(), VIA_MQTT);
    return true;
  }
  size_t b = rest.find('/');
  SatType type;
  if (b == std::string::npos || !type_of(rest.substr(0, b), type)) return true;
  std::string mac = rest.substr(b + 1);
  if (!len) {
    remove_dev(sat, type, mac);
    return true;
  }
  if (deserializeJson(d, payload, len)) return true;
  store_json(sat, type, mac, d.as<JsonObjectConst>(), VIA_MQTT);
  return true;
}

// ---------------------------------------------------------------- UDP
static void udp_task(void *) {
  int sock = -1;
  char *buf = (char *)heap_caps_malloc(1500, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) buf = (char *)malloc(1500);
  for (;;) {
    if (sock < 0) {
      sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      if (sock >= 0) {
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(SAT_UDP_PORT);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
          close(sock);
          sock = -1;
        } else {
          log_i("Satelliten: UDP-Empfang auf Port %d", SAT_UDP_PORT);
        }
      }
      if (sock < 0) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }
    }
    sockaddr_in from = {};
    socklen_t flen = sizeof(from);
    int n = recvfrom(sock, buf, 1499, 0, (sockaddr *)&from, &flen);
    if (n <= 0) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    JsonDocument d;
    if (deserializeJson(d, buf, n)) continue;
    std::string sat = d["sat"] | "";
    std::string type = d["type"] | "";
    if (sat.empty() || sat.rfind("Display-", 0) == 0) continue;   // Zusatzdisplays laufen ueber ui_remote
    // Absender = IP des Satelliten: sofort schaltbar, nicht erst mit dem naechsten Info-Paket (bis 30 s). Die
    // Bluetooth-MAC (Ziel der Schaltbefehle) steckt in der Standard-Kennung "Satellite-<MAC>".
    {
      char ipb[16];
      inet_ntoa_r(from.sin_addr, ipb, sizeof(ipb));
      Lock l;
      Node &nd = s_nodes[sat];
      if (nd.ip != ipb) nd.ip = ipb;
      if (nd.ble.empty() && sat.rfind("Satellite-", 0) == 0 && sat.size() == 22) {
        std::string m;
        for (int i = 0; i < 12; i++) {
          if (i && i % 2 == 0) m += ':';
          m += (char)tolower((unsigned char)sat[10 + i]);
        }
        nd.ble = m;
      }
    }
    if (type == "info") {
      store_info(sat, d["d"].as<JsonObjectConst>(), VIA_UDP);
      continue;
    }
    SatType t;
    std::string mac = d["mac"] | "";
    if (!type_of(type, t) || mac.empty()) continue;
    {
      Lock l;
      Node &nd = s_nodes[sat];
      if (d["name"].is<const char *>()) nd.name = d["name"].as<const char *>();
    }
    store_json(sat, t, mac, d["d"].as<JsonObjectConst>(), VIA_UDP);
  }
}

// ---------------------------------------------------------------- Bluetooth
bool panelsat_on_adv(const std::string &sender, const uint8_t *md, size_t len, int rssi) {
  SatType type;
  std::string mac;
  TrumaInfo t;
  MopekaInfo m;
  VictronInfo v;
  SatPoint sp;
  uint8_t key[16];
  {
    Lock l;
    memcpy(key, s_key, 16);
  }
  // falsches Passwort: Pruefsumme passt nicht, Paket wird verworfen
  if (!sat_adv_decode(md, len, key, sender, type, mac, t, m, v, sp)) return false;
  std::string id = sat_ble_name(sender);
  std::string k = dev_key(id, type, mac);
  Lock l;
  if (std::find(s_known.begin(), s_known.end(), id) == s_known.end()) {   // neu: automatisch aufnehmen
    s_known.push_back(id);
    save_known();
    log_i("Satelliten: %s per Bluetooth aufgenommen", id.c_str());
  }
  Node &n = s_nodes[id];
  if (n.name.empty()) n.name = id;
  n.seen_ms = millis();
  n.via |= VIA_BLE;
  n.ble = sender;
  Dev &x = s_devs[k];
  if (x.sat.empty()) x.sat = id;
  // Bluetooth bringt keine Namen mit: bekannte Namen behalten
  std::string name = x.named ? (type == SAT_T_TRUMA ? x.t.name : type == SAT_T_MOPEKA ? x.m.name : is_point(type) ? x.p.name : x.v.name) : "";
  x.type = type;
  x.mac = mac;
  x.via |= VIA_BLE;
  x.rx_ms = millis();
  switch (type) {
    case SAT_T_TRUMA:
      t.name = name.empty() ? tr("Gasflasche") : name;
      x.t = t;
      x.age_rx = t.age_s;
      break;
    case SAT_T_MOPEKA:
      m.name = name.empty() ? tr("Tank") : name;
      m.rssi = rssi;
      if (x.named) {   // Groesse/Modell aus MQTT/UDP behalten
        m.capacity = x.m.capacity;
        m.model_text = x.m.model_text;
        m.tank = x.m.tank;
      }
      x.m = m;
      x.age_rx = m.age_s;
      break;
    case SAT_T_VICTRON:
      v.name = name.empty() ? victron_kind_text(v.kind) : name;
      v.rssi = rssi;
      x.v = v;
      x.age_rx = v.age_s;
      break;
    case SAT_T_SENSOR:
    case SAT_T_SWITCH: {
      sp.name = name.empty() ? point_default_name(sp.kind, sp.idx) : name;
      if (x.named && !x.p.unit.empty()) sp.unit = x.p.unit;   // eigene Einheit aus MQTT/UDP behalten
      bool keep = x.pending_ms && millis() - x.pending_ms < 5000 && x.p.on != sp.on;
      bool on = x.p.on;
      x.p = sp;
      if (keep) x.p.on = on;
      else x.pending_ms = 0;
      x.age_rx = sp.age_s;
      break;
    }
    default:
      break;
  }
  s_ver++;
  return true;
}

// ---------------------------------------------------------------- Abfragen
static int32_t age_now(const Dev &x) {
  if (x.age_rx < 0) return -1;
  return x.age_rx + (int32_t)((millis() - x.rx_ms) / 1000);
}

static std::string node_name(const std::string &sat) {
  auto it = s_nodes.find(sat);
  if (it == s_nodes.end() || it->second.name.empty()) return sat.empty() ? std::string(tr("Satellit")) : sat;
  return it->second.name;
}

static bool node_offline(const Dev &x) {
  if (x.sat.empty()) return false;
  auto it = s_nodes.find(x.sat);
  return it != s_nodes.end() && it->second.mqtt_online == 0 && !(x.via & (VIA_UDP | VIA_BLE));
}

static TrumaInfo truma_of(const Dev &x) {
  TrumaInfo t = x.t;
  t.key = TRUMA_KEY_PREFIX + x.mac;
  t.mac = x.mac;
  t.age_s = age_now(x);
  if (node_offline(x)) t.status = tr(TXT_SAT_OFFLINE);
  return t;
}
static MopekaInfo mopeka_of(const Dev &x) {
  MopekaInfo m = x.m;
  m.key = MOPEKA_KEY_PREFIX + x.mac;
  m.mac = x.mac;
  m.age_s = age_now(x);
  if (node_offline(x)) m.status = tr(TXT_SAT_OFFLINE);
  return m;
}
static VictronInfo victron_of(const Dev &x) {
  VictronInfo v = x.v;
  v.key = VICTRON_KEY_PREFIX + x.mac;
  v.mac = x.mac;
  // Alter = seit dem letzten Paket des Satelliten (Victron sendet laufend, die Kachel wird danach "kein Empfang")
  v.age_s = age_now(x);
  if (node_offline(x)) {
    v.status = tr(TXT_SAT_OFFLINE);
    v.age_s = 9999;
  }
  return v;
}

template <class F>
static bool find(const std::string &prefix, SatType type, const std::string &key, F fn) {
  if (key.compare(0, prefix.size(), prefix) != 0) return false;
  Lock l;
  auto it = s_devs.find(std::string(type_name(type)) + "/" + key.substr(prefix.size()));
  if (it == s_devs.end()) return false;
  fn(it->second);
  return true;
}

bool panelsat_truma_get(const std::string &key, TrumaInfo &out) {
  return find(TRUMA_KEY_PREFIX, SAT_T_TRUMA, key, [&](const Dev &x) { out = truma_of(x); });
}
bool panelsat_mopeka_get(const std::string &key, MopekaInfo &out) {
  return find(MOPEKA_KEY_PREFIX, SAT_T_MOPEKA, key, [&](const Dev &x) { out = mopeka_of(x); });
}
bool panelsat_victron_get(const std::string &key, VictronInfo &out) {
  return find(VICTRON_KEY_PREFIX, SAT_T_VICTRON, key, [&](const Dev &x) { out = victron_of(x); });
}

std::vector<TrumaInfo> panelsat_truma_list() {
  std::vector<TrumaInfo> r;
  Lock l;
  for (auto &e : s_devs)
    if (e.second.type == SAT_T_TRUMA) r.push_back(truma_of(e.second));
  return r;
}
std::vector<MopekaInfo> panelsat_mopeka_list() {
  std::vector<MopekaInfo> r;
  Lock l;
  for (auto &e : s_devs)
    if (e.second.type == SAT_T_MOPEKA) r.push_back(mopeka_of(e.second));
  return r;
}
std::vector<VictronInfo> panelsat_victron_list() {
  std::vector<VictronInfo> r;
  Lock l;
  for (auto &e : s_devs)
    if (e.second.type == SAT_T_VICTRON) r.push_back(victron_of(e.second));
  return r;
}

static const Dev *dev_of_key(const std::string &key) {
  std::string k;
  if (panelsat_is_point_key(key)) k = "point/" + key.substr(strlen(SATIO_KEY_PREFIX));
  else if (truma_is_key(key)) k = "truma/" + key.substr(strlen(TRUMA_KEY_PREFIX));
  else if (mopeka_is_key(key)) k = "mopeka/" + key.substr(strlen(MOPEKA_KEY_PREFIX));
  else if (key.rfind(VICTRON_KEY_PREFIX, 0) == 0) k = "victron/" + key.substr(strlen(VICTRON_KEY_PREFIX));
  else return nullptr;
  auto it = s_devs.find(k);
  return it == s_devs.end() ? nullptr : &it->second;
}

std::string panelsat_sat_of(const std::string &key) {
  Lock l;
  const Dev *x = dev_of_key(key);
  return x ? (x->sat.empty() ? std::string("?") : x->sat) : "";
}

static std::string sat_via_text_raw(const std::string &key) {
  Lock l;
  const Dev *x = dev_of_key(key);
  if (!x) return "";
  std::string via;
  if (x->via & VIA_MQTT) via += "MQTT";
  if (x->via & VIA_UDP) via += via.empty() ? "UDP" : ", UDP";
  if (x->via & VIA_BLE) via += via.empty() ? "Bluetooth" : ", Bluetooth";
  return node_name(x->sat) + " (" + via + ")";
}

bool panelsat_measure(const std::string &key) {
  std::string sat, mac;
  {
    Lock l;
    const Dev *x = dev_of_key(key);
    if (!x || x->sat.empty() || x->type != SAT_T_TRUMA) return false;
    auto it = s_nodes.find(x->sat);
    if (it == s_nodes.end() || it->second.mqtt_online != 1) return false;
    sat = x->sat;
    mac = x->mac;
  }
  z2m_publish(std::string(SAT_BASE) + "/" + sat + "/cmd", "{\"measure\":\"" + mac + "\"}");
  return true;
}

static SatPoint point_of(const Dev &x) {
  SatPoint p = x.p;
  p.sat = x.sat;
  p.key = SATIO_KEY_PREFIX + x.sat + "/" + x.mac;
  p.age_s = age_now(x);
  if (node_offline(x)) {
    p.status = tr(TXT_SAT_OFFLINE);
    p.ok = false;
  }
  return p;
}

static std::vector<SatPoint> sat_point_list_raw() {
  std::vector<SatPoint> r;
  Lock l;
  for (auto &e : s_devs)
    if (is_point(e.second.type)) r.push_back(point_of(e.second));
  return r;
}

static bool sat_point_get_raw(const std::string &key, SatPoint &out) {
  if (!panelsat_is_point_key(key)) return false;
  Lock l;
  const Dev *x = dev_of_key(key);
  if (!x || !is_point(x->type)) return false;
  out = point_of(*x);
  return true;
}

// Schaltbefehl per Bluetooth: 3 s lang ein verschluesseltes Werbepaket senden
[[maybe_unused]] static void ble_send(const uint8_t *md, size_t len) {
  truma_ble_init();
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->stop();
  NimBLEAdvertisementData d;
  d.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  d.setManufacturerData(md, len);
  adv->setAdvertisementData(d);
  adv->setConnectableMode(BLE_GAP_CONN_MODE_NON);
  adv->setMinInterval(32);   // 20 ms
  adv->setMaxInterval(48);   // 30 ms
  adv->start(3000);
}

static bool udp_send_to(const std::string &ip, const uint8_t *md, size_t len) {
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) return false;
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons(SAT_UDP_PORT);
  bool ok = inet_aton(ip.c_str(), &a.sin_addr) && sendto(sock, md, len, 0, (sockaddr *)&a, sizeof(a)) == (int)len;
  close(sock);
  return ok;
}

static bool sat_switch_raw(const std::string &key, bool on, std::string *err) {
  static uint16_t counter = (uint16_t)esp_random();
  std::string sat, pid, ip, ble;
  uint8_t key16[16];
  uint32_t nonce;
  uint8_t idx;
  int via = 0;   // Bits: 1 MQTT, 2 UDP
  {
    Lock l;
    const Dev *x = dev_of_key(key);
    if (!x || x->type != SAT_T_SWITCH) {
      if (err) *err = tr("Schalter nicht gefunden.");
      return false;
    }
    auto it = s_nodes.find(x->sat);
    if (it == s_nodes.end()) {
      if (err) *err = tr("Satellit unbekannt.");
      return false;
    }
    const Node &n = it->second;
    sat = x->sat;
    pid = x->mac;
    idx = x->p.idx;
    nonce = n.has_nonce ? n.sw_nonce : x->p.nonce;
    ip = n.ip;
    ble = n.ble;
    memcpy(key16, s_key, 16);
    // Weg waehlen: MQTT (mit Broker) > UDP (WLAN). Bluetooth als Datenweg ist entfallen (zu langsam).
    // Mit Broker per MQTT; schickt der Satellit auch per UDP, den Befehl ZUSAETZLICH per UDP: die "online"-Meldung auf
    // dem Broker kann veraltet sein (Satellit hat MQTT abgeschaltet), dann kam bisher nichts an. Doppelt ausgefuehrt
    // wird nichts (MQTT: fester Zustand, UDP: Nonce nach der ersten Schaltung verbraucht).
    if (n.mqtt_online == 1 && z2m_connected()) via |= 1;
    if ((x->via & VIA_UDP) && !ip.empty() && !ble.empty()) via |= 2;
  }
  if (!via) {
    if (err) *err = tr("Der Satellit ist gerade nicht erreichbar.");
    return false;
  }
  if (via & 1) z2m_publish(std::string(SAT_BASE) + "/" + sat + "/switch/" + pid + "/set", on ? "ON" : "OFF");
  if (via & 2) {
    uint8_t md[20];
    size_t len = sat_cmd_seal(md, sizeof(md), key16, ble, ++counter, SP_SWITCH, idx, on ? 1 : 0, nonce);
    bool sent = len > 0;
    if (sent) sent = udp_send_to(ip, md, len);
    // UDP kann verloren gehen: kurz danach nochmal (der Satellit fuehrt es nur einmal aus - danach ist die Nonce verbraucht)
    if (sent) {
      vTaskDelay(pdMS_TO_TICKS(40));
      udp_send_to(ip, md, len);
    }
    if (!sent && !(via & 1)) {
      if (err) *err = tr("Senden fehlgeschlagen.");
      return false;
    }
  }
  Lock l;
  auto it = s_devs.find("point/" + sat + "/" + pid);
  if (it != s_devs.end()) {
    it->second.p.on = on;   // sofort anzeigen, der Satellit bestaetigt
    it->second.pending_ms = millis();
  }
  s_ver++;
  return true;
}

void panelsat_forget(const std::string &id) {
  Lock l;
  s_nodes.erase(id);
  auto kn = std::find(s_known.begin(), s_known.end(), id);
  if (kn != s_known.end()) {
    s_known.erase(kn);
    save_known();
  }
  for (auto it = s_devs.begin(); it != s_devs.end();) {
    if (it->second.sat == id) it = s_devs.erase(it);
    else ++it;
  }
  s_ver++;
}

std::vector<SatNode> panelsat_nodes() {
  std::vector<SatNode> r;
  Lock l;
  uint32_t now = millis();
  for (auto &e : s_nodes) {
    SatNode n;
    n.id = e.first;
    n.name = e.second.name.empty() ? e.first : e.second.name;
    n.ip = e.second.ip;
    n.fw = e.second.fw;
    n.rssi = e.second.rssi;
    n.via = e.second.via;
    n.age_s = e.second.seen_ms ? (int32_t)((now - e.second.seen_ms) / 1000) : -1;
    n.online = e.second.mqtt_online == 1 || (e.second.seen_ms && now - e.second.seen_ms < NODE_STALE_MS);
    for (auto &d : s_devs)
      if (d.second.sat == e.first) n.devices++;
    r.push_back(n);
  }
  return r;
}

static uint32_t sat_version_raw() { return s_ver; }

void panelsat_begin() {
  if (!s_mtx) s_mtx = xSemaphoreCreateRecursiveMutex();
  load_config();
  // UDP nur mit WLAN (ohne WLAN kommt nichts an); gemessen < 2 KB Stack gebraucht
  if (net_mode() != NET_OFF) xTaskCreatePinnedToCore(udp_task, "panelsat", 3072, nullptr, 2, nullptr, 0);
}

#else   // ---------------------------------------------------------------- ausgebaut

void panelsat_begin() {}
static uint32_t sat_version_raw() { return 0; }
std::vector<std::string> panelsat_topics() { return {}; }
bool panelsat_handle(const std::string &, const char *, size_t) { return false; }
std::vector<SatNode> panelsat_nodes() { return {}; }
bool panelsat_truma_get(const std::string &, TrumaInfo &) { return false; }
bool panelsat_mopeka_get(const std::string &, MopekaInfo &) { return false; }
bool panelsat_victron_get(const std::string &, VictronInfo &) { return false; }
std::vector<TrumaInfo> panelsat_truma_list() { return {}; }
std::vector<MopekaInfo> panelsat_mopeka_list() { return {}; }
std::vector<VictronInfo> panelsat_victron_list() { return {}; }
std::string panelsat_sat_of(const std::string &) { return ""; }
static std::string sat_via_text_raw(const std::string &) { return ""; }
bool panelsat_measure(const std::string &) { return false; }
void panelsat_forget(const std::string &) {}
bool panelsat_on_adv(const std::string &, const uint8_t *, size_t, int) { return false; }
std::string panelsat_password() { return ""; }
void panelsat_set_password(const std::string &) {}
static std::vector<SatPoint> sat_point_list_raw() { return {}; }
static bool sat_point_get_raw(const std::string &, SatPoint &) { return false; }
static bool sat_switch_raw(const std::string &, bool, std::string *) { return false; }

#endif

// ---------------------------------------------------------------- Hymer Connect direkt am Panel
// Die Werte der SCU erscheinen wie die Anschluesse eines Satelliten namens "Hymer": Schluessel "satio:Hymer/<punkt>".
#include "settings.h"
#include "i18n/i18n.h"

static const std::string &hy_prefix() {
  static const std::string p = std::string(SATIO_KEY_PREFIX) + HYMER_SAT_ID + "/";
  return p;
}
static bool hy_key(const std::string &k) { return hymer_enabled() && k.rfind(hy_prefix(), 0) == 0; }
static bool hy_on() { return hymer_enabled() && ble_proto_on(BLE_P_HYMER); }
static SatPoint hy_local(SatPoint p) {
  p.sat = HYMER_SAT_ID;
  p.key = hy_prefix() + p.id;
  return p;
}

// ---------------------------------------------------------------- Victron GX: Relais, Tanks, Temperaturen, Eingaenge
// Was am GX eingerichtet ist, erscheint wie die Anschluesse eines Satelliten "GX": Schluessel "satio:GX/<punkt>".
#define GX_SAT_ID "GX"
static const std::string &gx_prefix() {
  static const std::string p = std::string(SATIO_KEY_PREFIX) + GX_SAT_ID + "/";
  return p;
}
static bool gx_key(const std::string &k) { return cerbo_enabled() && k.rfind(gx_prefix(), 0) == 0; }
static bool gx_on() { return cerbo_enabled() && cerbo_configured(); }
static SatPoint gx_local(SatPoint p) {
  p.sat = GX_SAT_ID;
  p.key = gx_prefix() + p.id;
  return p;
}

uint32_t panelsat_version() {
  return sat_version_raw() + (hy_on() ? hymer_version() * 131 : 0) + (gx_on() ? cerbo_version() * 977 : 0);
}

std::vector<SatPoint> panelsat_point_list() {
  std::vector<SatPoint> r = sat_point_list_raw();
  if (hy_on())
    for (auto &p : hymer_points()) r.push_back(hy_local(p));
  if (gx_on())
    for (auto &p : cerbo_points()) r.push_back(gx_local(p));
  return r;
}

bool panelsat_point_get(const std::string &key, SatPoint &out) {
  if (gx_key(key)) {
    if (!gx_on()) return false;
    for (auto &p : cerbo_points())
      if (gx_prefix() + p.id == key) {
        out = gx_local(p);
        return true;
      }
    return false;
  }
  if (!hy_key(key)) return sat_point_get_raw(key, out);
  if (!hy_on()) return false;
  for (auto &p : hymer_points())
    if (hy_prefix() + p.id == key) {
      out = hy_local(p);
      return true;
    }
  return false;
}

bool panelsat_switch(const std::string &key, bool on, std::string *err) {
  SatPointKind k;
  uint8_t idx;
  if (gx_key(key)) {
    if (gx_on() && sat_point_parse(key.substr(gx_prefix().size()), k, idx) && k == SP_SWITCH && cerbo_set_relay(idx, on))
      return true;
    if (err) *err = tr("Der GX ist nicht verbunden oder das Relais steht nicht auf „manuell“.");
    return false;
  }
  if (!hy_key(key)) return sat_switch_raw(key, on, err);
  if (hy_on() && sat_point_parse(key.substr(hy_prefix().size()), k, idx) && k == SP_SWITCH && hymer_switch(idx, on ? 1 : 0))
    return true;
#if WITH_HYMER
  if (err) *err = tr("Hymer ist gerade nicht verbunden.");
#else
  if (err) *err = tr("Nicht verbunden.");
#endif
  return false;
}

std::string panelsat_via_text(const std::string &key) {
  if (gx_key(key)) return tr("Victron GX (MQTT)");
  if (hy_key(key)) return tr("Hymer Connect (Bluetooth)");
  return sat_via_text_raw(key);
}
