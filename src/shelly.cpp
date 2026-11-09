#include "shelly.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <algorithm>
#include <atomic>
#include <map>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ---------------------------------------------------------------- Modell
// Gen1:  shellies/<id>/relay/<n>            "on"/"off"         -> .../relay/<n>/command  "on"/"off"
//        shellies/<id>/light|white/<n>/status {ison,brightness,temp} -> .../<comp>/set {turn,brightness,temp}
//        shellies/<id>/color/<n>/status      {ison,mode,red,green,blue,gain,brightness,temp}
//        shellies/<id>/online                "true"/"false",  shellies/announce {id,model,...}
// Gen2+: <prefix>/online "true"/"false", <prefix>/events/rpc (NotifyStatus), <prefix>/status/<comp>
//        Befehle und Abfragen als JSON-RPC an <prefix>/rpc, Antworten an <client-id>/rpc
struct SCtl {
  std::string comp;            // Gen2 "switch:0", Gen1 "relay/0"
  std::string label;           // eigener Kanalname (Gen2-Konfiguration)
  CtlKind kind = CTL_SWITCH;
  bool has_bri = false, has_ct = false;
  uint8_t color = COLOR_NONE;
  bool white_ch = false;       // Gen2 rgbw: Weisskanal vorhanden
  bool known = false, on = false;
  int bri = -1, ct = -1;       // 0..100 / Mired
  float hue = -1, sat = -1;
  std::string cmode;
  uint32_t pending_until = 0;
};
struct SDev {
  std::string id;              // Gen1: Geraete-ID, Gen2: Topic-Praefix
  uint8_t gen = 1;
  std::string name, model;
  bool online = true;
  bool confirmed = false;      // Gen2: Geraeteinfo empfangen
  std::vector<SCtl> ctls;
};

static SemaphoreHandle_t s_mtx;
static std::vector<SDev> s_devs;
static std::atomic<uint32_t> s_dev_ver{1}, s_state_ver{1};
static std::map<int, std::pair<std::string, std::string>> s_rpc_wait;   // id -> (praefix, methode)
static int s_rpc_id = 1;
struct Req { std::string topic, payload; };
static std::vector<Req> s_out;   // gedrosselt versenden
static uint32_t s_last_send = 0, s_last_pend = 0;
static bool s_g2_dirty = false;

struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};

static const int CT_MIN_MIRED = 154;   // 6500 K
static const int CT_MAX_MIRED = 370;   // 2700 K
static int k_to_mired(int k) { return k > 0 ? (1000000 + k / 2) / k : -1; }
static int mired_to_k(int m) { return m > 0 ? (1000000 + m / 2) / m : 4000; }

static void rgb_to_hs(int r, int g, int b, float &h, float &s) {
  float R = r / 255.f, G = g / 255.f, B = b / 255.f;
  float mx = std::max(R, std::max(G, B)), mn = std::min(R, std::min(G, B)), d = mx - mn;
  s = mx <= 0 ? 0 : d / mx * 100.f;
  if (d <= 0) h = 0;
  else if (mx == R) h = 60.f * fmodf((G - B) / d, 6.f);
  else if (mx == G) h = 60.f * ((B - R) / d + 2.f);
  else h = 60.f * ((R - G) / d + 4.f);
  if (h < 0) h += 360.f;
}

static void hs_to_rgb(float h, float s, int &r, int &g, int &b) {
  float S = s / 100.f, C = S, X = C * (1 - fabsf(fmodf(h / 60.f, 2) - 1)), m = 1.f - C;
  float R = 0, G = 0, B = 0;
  if (h < 60) { R = C; G = X; }
  else if (h < 120) { R = X; G = C; }
  else if (h < 180) { G = C; B = X; }
  else if (h < 240) { G = X; B = C; }
  else if (h < 300) { R = X; B = C; }
  else { R = C; B = X; }
  r = (int)roundf((R + m) * 255); g = (int)roundf((G + m) * 255); b = (int)roundf((B + m) * 255);
}

static SDev *find_dev(const std::string &id) {
  for (auto &d : s_devs)
    if (d.id == id) return &d;
  return nullptr;
}

static SDev &get_dev(const std::string &id, uint8_t gen) {
  if (SDev *d = find_dev(id)) return *d;
  SDev d;
  d.id = id;
  d.gen = gen;
  d.name = id;
  s_devs.push_back(d);
  s_dev_ver++;
  return s_devs.back();
}

static SCtl &get_ctl(SDev &d, const std::string &comp, CtlKind kind) {
  for (auto &c : d.ctls)
    if (c.comp == comp) return c;
  SCtl c;
  c.comp = comp;
  c.kind = kind;
  d.ctls.push_back(c);
  std::sort(d.ctls.begin(), d.ctls.end(), [](const SCtl &a, const SCtl &b) { return a.comp < b.comp; });
  s_dev_ver++;
  for (auto &x : d.ctls)
    if (x.comp == comp) return x;
  return d.ctls.back();
}

static bool split_key(const std::string &key, std::string &id, std::string &comp) {
  if (!shelly_is_key(key)) return false;
  size_t h = key.find('#', 7);
  if (h == std::string::npos) return false;
  id = key.substr(7, h - 7);
  comp = key.substr(h + 1);
  return true;
}

// ---------------------------------------------------------------- Senden
static void send(const std::string &topic, const std::string &payload) {
  // Befehle direkt, Abfragen gedrosselt (siehe shelly_tick)
  z2m_publish(topic, payload);
}

static void queue(const std::string &topic, const std::string &payload) {
  Lock l;
  if (s_out.size() < 64) s_out.push_back({topic, payload});
}

static std::string rpc_payload(const std::string &prefix, const char *method, JsonDocument *params, bool track) {
  JsonDocument d;
  int id;
  {
    Lock l;
    id = s_rpc_id++;
    if (s_rpc_id > 30000) s_rpc_id = 1;
    if (track) {
      s_rpc_wait[id] = {prefix, method};
      if (s_rpc_wait.size() > 64) s_rpc_wait.erase(s_rpc_wait.begin());
    }
  }
  d["id"] = id;
  d["src"] = z2m_client_id();
  d["method"] = method;
  if (params) d["params"] = *params;
  std::string s;
  serializeJson(d, s);
  return s;
}

static void rpc_query(const std::string &prefix, const char *method) {
  queue(prefix + "/rpc", rpc_payload(prefix, method, nullptr, true));
}

static void query_gen2(const std::string &prefix) {
  rpc_query(prefix, "Shelly.GetDeviceInfo");
  rpc_query(prefix, "Shelly.GetConfig");
  rpc_query(prefix, "Shelly.GetStatus");
}

// ---------------------------------------------------------------- Gen2-Status anwenden
static bool gen2_comp(const char *key, CtlKind &kind) {
  if (!strncmp(key, "switch:", 7)) { kind = CTL_SWITCH; return true; }
  if (!strncmp(key, "light:", 6) || !strncmp(key, "rgb:", 4) || !strncmp(key, "rgbw:", 5) || !strncmp(key, "cct:", 4)) {
    kind = CTL_LIGHT;
    return true;
  }
  return false;
}

static void apply_gen2(SDev &d, const char *comp, JsonObject st, bool create) {
  CtlKind kind;
  if (!gen2_comp(comp, kind)) return;
  SCtl *c = nullptr;
  for (auto &x : d.ctls)
    if (x.comp == comp) c = &x;
  if (!c) {
    if (!create) return;
    c = &get_ctl(d, comp, kind);
    c->has_bri = kind == CTL_LIGHT;
    if (!strncmp(comp, "rgb", 3)) c->color = COLOR_HS;
    c->white_ch = !strncmp(comp, "rgbw:", 5);
    if (!strncmp(comp, "cct:", 4)) c->has_ct = true;
    c->cmode = c->color ? "hs" : (c->has_ct ? "color_temp" : "");
  }
  if (st["output"].is<bool>()) {
    c->on = st["output"].as<bool>();
    c->known = true;
    c->pending_until = 0;
  }
  if (st["brightness"].is<float>()) c->bri = (int)roundf(st["brightness"].as<float>());
  if (st["ct"].is<int>()) c->ct = k_to_mired(st["ct"].as<int>());
  JsonArray rgb = st["rgb"];
  if (rgb.size() == 3) rgb_to_hs(rgb[0] | 0, rgb[1] | 0, rgb[2] | 0, c->hue, c->sat);
  s_state_ver++;
}

static void apply_gen2_all(SDev &d, JsonObject o, bool create) {
  for (JsonPair kv : o)
    if (kv.value().is<JsonObject>()) apply_gen2(d, kv.key().c_str(), kv.value().as<JsonObject>(), create);
}

static void remember_gen2() {
  // bekannte Gen2-Praefixe merken, damit sie nach einem Neustart sofort abgefragt werden
  std::string list;
  {
    Lock l;
    for (auto &d : s_devs)
      if (d.gen >= 2 && d.confirmed) list += (list.empty() ? "" : ",") + d.id;
    s_g2_dirty = false;
  }
  if (list.size() > 3500) list.resize(3500);
  Preferences p;
  p.begin("shelly", false);
  if (p.getString("g2", "") != String(list.c_str())) p.putString("g2", list.c_str());
  p.end();
}

static void handle_rpc_reply(const char *payload, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, len)) return;
  int id = doc["id"] | -1;
  std::string prefix, method;
  {
    Lock l;
    auto it = s_rpc_wait.find(id);
    if (it == s_rpc_wait.end()) return;
    prefix = it->second.first;
    method = it->second.second;
    s_rpc_wait.erase(it);
  }
  JsonObject res = doc["result"];
  if (res.isNull()) return;   // Fehler oder Befehlsquittung
  Lock l;
  if (method == "Shelly.GetDeviceInfo") {
    if (!res["gen"].is<int>() && !res["id"].is<const char *>()) return;   // kein Shelly
    SDev &d = get_dev(prefix, (uint8_t)(res["gen"] | 2));
    const char *app = res["app"] | (const char *)(res["model"] | "");
    d.model = app;
    if (res["name"].is<const char *>() && *(const char *)res["name"]) d.name = (const char *)res["name"];
    if (!d.confirmed) {
      d.confirmed = true;
      s_g2_dirty = true;
      log_i("Shelly Gen%d gefunden: %s (%s)", d.gen, prefix.c_str(), d.model.c_str());
    }
    s_dev_ver++;
  } else if (method == "Shelly.GetConfig") {
    SDev *d = find_dev(prefix);
    if (!d) return;
    const char *n = res["sys"]["device"]["name"] | "";
    if (*n) d->name = n;
    for (auto &c : d->ctls) {
      const char *cn = res[c.comp]["name"] | "";
      c.label = cn;
    }
    // Kanalnamen auch fuer Komponenten merken, die erst mit GetStatus entstehen
    for (JsonPair kv : res) {
      CtlKind k;
      if (!gen2_comp(kv.key().c_str(), k)) continue;
      const char *cn = kv.value()["name"] | "";
      if (!*cn) continue;
      SCtl &c = get_ctl(*d, kv.key().c_str(), k);
      c.label = cn;
      c.has_bri = k == CTL_LIGHT;
      if (!strncmp(kv.key().c_str(), "rgb", 3)) c.color = COLOR_HS;
      c.white_ch = !strncmp(kv.key().c_str(), "rgbw:", 5);
      if (!strncmp(kv.key().c_str(), "cct:", 4)) c.has_ct = true;
      c.cmode = c.color ? "hs" : (c.has_ct ? "color_temp" : "");
    }
    s_dev_ver++;
  } else if (method == "Shelly.GetStatus") {
    SDev *d = find_dev(prefix);
    if (!d) return;
    d->online = true;
    apply_gen2_all(*d, res, true);
  }
}

// ---------------------------------------------------------------- Gen1
static void gen1_json(SDev &d, const std::string &comp, const char *payload, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, len)) return;
  bool color = comp.compare(0, 6, "color/") == 0;
  SCtl &c = get_ctl(d, comp, CTL_LIGHT);
  c.has_bri = true;
  if (doc["ison"].is<bool>()) {
    c.on = doc["ison"].as<bool>();
    c.known = true;
    c.pending_until = 0;
  }
  if (doc["temp"].is<int>()) {
    if (!c.has_ct) s_dev_ver++;
    c.has_ct = true;
    c.ct = k_to_mired(doc["temp"].as<int>());
  }
  std::string mode = doc["mode"] | "";
  if (color) {
    if (!c.color) s_dev_ver++;
    c.color = COLOR_HS;
    if (doc["red"].is<int>()) rgb_to_hs(doc["red"] | 0, doc["green"] | 0, doc["blue"] | 0, c.hue, c.sat);
    c.cmode = mode == "white" ? "color_temp" : "hs";
    if (mode == "white" && doc["brightness"].is<int>()) c.bri = doc["brightness"].as<int>();
    else if (doc["gain"].is<int>()) c.bri = doc["gain"].as<int>();
  } else {
    if (doc["brightness"].is<int>()) c.bri = doc["brightness"].as<int>();
    c.cmode = c.has_ct ? "color_temp" : "";
  }
  s_state_ver++;
}

static bool handle_gen1(const std::string &topic, const char *payload, size_t len) {
  // topic beginnt mit "shellies/"
  std::string rest = topic.substr(9);
  if (rest == "announce") {
    JsonDocument doc;
    if (deserializeJson(doc, payload, len)) return true;
    const char *id = doc["id"] | "";
    if (!*id) return true;
    if (doc["gen"].is<int>() && doc["gen"].as<int>() >= 2) return true;   // Gen2 meldet sich anders
    Lock l;
    SDev &d = get_dev(id, 1);   // ohne Kanaele unsichtbar, bis Zustaende kommen
    d.model = doc["model"] | "";
    s_dev_ver++;
    return true;
  }
  size_t s1 = rest.find('/');
  if (s1 == std::string::npos) return true;
  std::string id = rest.substr(0, s1), sub = rest.substr(s1 + 1);
  std::string val(payload, len);
  Lock l;
  if (sub == "online") {
    SDev *d = find_dev(id);
    if (d) {
      d->online = val == "true";
      s_state_ver++;
    }
    return true;
  }
  // relay/<n>
  if (sub.compare(0, 6, "relay/") == 0 && sub.find('/', 6) == std::string::npos) {
    if (val != "on" && val != "off" && val != "overpower") return true;
    SDev &d = get_dev(id, 1);
    SCtl &c = get_ctl(d, sub, CTL_SWITCH);
    c.on = val == "on";
    c.known = true;
    c.pending_until = 0;
    d.online = true;
    s_state_ver++;
    return true;
  }
  // light|white|color/<n>/status
  size_t s2 = sub.find('/');
  if (s2 != std::string::npos) {
    std::string kind = sub.substr(0, s2);
    size_t s3 = sub.find('/', s2 + 1);
    if ((kind == "light" || kind == "white" || kind == "color") && s3 != std::string::npos && sub.substr(s3) == "/status") {
      SDev &d = get_dev(id, 1);
      d.online = true;
      gen1_json(d, sub.substr(0, s3), payload, len);
    }
  }
  return true;
}

// ---------------------------------------------------------------- oeffentliche API
void shelly_init() {
  s_mtx = xSemaphoreCreateRecursiveMutex();
  Preferences p;
  if (!p.begin("shelly", true)) return;
  String g2 = p.getString("g2", "");
  p.end();
  Lock l;
  int start = 0;
  while (start < (int)g2.length()) {
    int e = g2.indexOf(',', start);
    if (e < 0) e = g2.length();
    String id = g2.substring(start, e);
    if (id.length()) {
      SDev &d = get_dev(id.c_str(), 2);
      d.confirmed = true;
      d.online = false;   // bis sich das Geraet meldet
    }
    start = e + 1;
  }
}

std::vector<std::string> shelly_topics() {
  return {"shellies/announce", "shellies/+/online", "shellies/+/relay/+", "shellies/+/light/+/status",
          "shellies/+/white/+/status", "shellies/+/color/+/status", "+/online", "+/events/rpc", "+/status/+",
          z2m_client_id() + "/rpc"};
}

void shelly_on_connect() {
  queue("shellies/command", "announce");
  queue("shellies/command", "update");
  std::vector<std::string> g2;
  {
    Lock l;
    for (auto &d : s_devs)
      if (d.gen >= 2) g2.push_back(d.id);
  }
  for (auto &p : g2) query_gen2(p);
}

bool shelly_handle(const std::string &topic, const char *payload, size_t len) {
  if (topic.compare(0, 9, "shellies/") == 0) return handle_gen1(topic, payload, len);
  if (topic == z2m_client_id() + "/rpc") {
    handle_rpc_reply(payload, len);
    return true;
  }
  // Gen2: <prefix>/online, <prefix>/events/rpc, <prefix>/status/<comp>
  size_t s1 = topic.find('/');
  if (s1 == std::string::npos || s1 == 0) return false;
  std::string prefix = topic.substr(0, s1), sub = topic.substr(s1 + 1);
  bool is_online = sub == "online", is_events = sub == "events/rpc", is_status = sub.compare(0, 7, "status/") == 0;
  if (!is_online && !is_events && !is_status) return false;
  bool known, confirmed = false;
  {
    Lock l;
    SDev *d = find_dev(prefix);
    known = d && d->gen >= 2;
    if (known) confirmed = d->confirmed;
  }
  if (!known) {
    // unbekanntes Geraet: nachfragen, ob es ein Shelly ist (Antwort legt es an)
    if ((is_online && len == 4 && !strncmp(payload, "true", 4)) || is_events) {
      static std::string last;
      static uint32_t last_t = 0;
      if (prefix != last || millis() - last_t > 30000) {
        last = prefix;
        last_t = millis();
        query_gen2(prefix);
      }
    }
    return is_events;   // fremde "online"/"status"-Topics nicht beanspruchen
  }
  if (is_online) {
    bool on = len == 4 && !strncmp(payload, "true", 4);
    Lock l;
    SDev *d = find_dev(prefix);
    bool was = d->online;
    d->online = on;
    s_state_ver++;
    if (on && (!was || d->ctls.empty())) query_gen2(prefix);
    return true;
  }
  JsonDocument doc;
  if (deserializeJson(doc, payload, len)) return true;
  Lock l;
  SDev *d = find_dev(prefix);
  if (!d) return true;
  if (is_status) {
    apply_gen2(*d, sub.substr(7).c_str(), doc.as<JsonObject>(), confirmed);
  } else {
    const char *m = doc["method"] | "";
    if (!strcmp(m, "NotifyStatus") || !strcmp(m, "NotifyFullStatus")) {
      d->online = true;
      apply_gen2_all(*d, doc["params"].as<JsonObject>(), confirmed);
    }
  }
  return true;
}

void shelly_tick() {
  uint32_t now = millis();
  Req r;
  bool have = false;
  {
    Lock l;
    if (!s_out.empty() && now - s_last_send > 120 && z2m_connected()) {
      r = s_out.front();
      s_out.erase(s_out.begin());
      s_last_send = now;
      have = true;
    }
  }
  if (have) send(r.topic, r.payload);
  if (now - s_last_pend > 500) {
    s_last_pend = now;
    bool save;
    {
      Lock l;
      for (auto &d : s_devs)
        for (auto &c : d.ctls)
          if (c.pending_until && (int32_t)(now - c.pending_until) > 0) {
            c.pending_until = 0;
            s_state_ver++;
          }
      save = s_g2_dirty;
    }
    if (save) remember_gen2();
  }
}

void shelly_refresh() {
  {
    Lock l;
    for (auto &d : s_devs)
      for (auto &c : d.ctls) c.known = false;
    s_state_ver++;
  }
  shelly_on_connect();
}

uint32_t shelly_devices_version() { return s_dev_ver; }
uint32_t shelly_state_version() { return s_state_ver; }

static void fill_view(const SDev &d, const SCtl &c, ControlView &v) {
  v.key = "shelly:" + d.id + "#" + c.comp;
  v.ieee = d.id;
  v.name = c.label.empty() ? d.name : c.label;
  // Mehrkanalige Geraete ohne eigene Kanalnamen: Kanalnummer anhaengen
  v.ep.clear();
  if (c.label.empty() && d.ctls.size() > 1) {
    size_t p = c.comp.find_last_of(":/");
    int n = p == std::string::npos ? 0 : atoi(c.comp.c_str() + p + 1);
    v.ep = std::to_string(n + 1);
  }
  v.model = d.model.empty() ? (d.gen >= 2 ? "Gen2+" : "Gen1") : d.model;
  v.vendor = "Shelly";
  v.description = c.comp;
  v.kind = c.kind;
  v.has_bri = c.has_bri;
  v.has_ct = c.has_ct;
  v.color = c.color;
  v.bri_max = 100;
  v.ct_min = CT_MIN_MIRED;
  v.ct_max = CT_MAX_MIRED;
  v.known = c.known;
  v.on = c.on;
  v.online = d.online;
  v.pending = c.pending_until != 0;
  v.bri = c.bri;
  v.ct = c.ct;
  v.hue = c.hue;
  v.sat = c.sat;
  v.cx = v.cy = -1;
  v.cmode = c.cmode;
}

bool shelly_get(const std::string &key, ControlView &out) {
  std::string id, comp;
  if (!split_key(key, id, comp)) return false;
  Lock l;
  SDev *d = find_dev(id);
  if (!d) return false;
  for (auto &c : d->ctls)
    if (c.comp == comp) {
      fill_view(*d, c, out);
      return true;
    }
  return false;
}

void shelly_all(std::vector<ControlView> &out) {
  Lock l;
  for (auto &d : s_devs)
    for (auto &c : d.ctls) {
      ControlView v;
      fill_view(d, c, v);
      out.push_back(std::move(v));
    }
}

size_t shelly_count() {
  Lock l;
  size_t n = 0;
  for (auto &d : s_devs) n += d.ctls.size();
  return n;
}

// ---------------------------------------------------------------- Befehle
// Werte: on (-1 = unveraendert), bri 1..100 (-1), ct Mired (-1), hue/sat (<0 = unveraendert)
static void command(const std::string &key, int on, int bri, int ct, float hue, float sat) {
  std::string id, comp;
  if (!split_key(key, id, comp)) return;
  std::string topic, payload;
  {
    Lock l;
    SDev *d = find_dev(id);
    if (!d) return;
    SCtl *c = nullptr;
    for (auto &x : d->ctls)
      if (x.comp == comp) c = &x;
    if (!c) return;
    if (on < 0 && (bri >= 0 || ct >= 0 || hue >= 0)) on = 1;   // Einstellen schaltet ein
    if (d->gen >= 2) {
      JsonDocument p;
      size_t colon = comp.find(':');
      std::string type = comp.substr(0, colon);
      p["id"] = atoi(comp.c_str() + colon + 1);
      if (on >= 0) p["on"] = on == 1;
      if (bri >= 0 && c->has_bri) p["brightness"] = bri;
      if (ct >= 0 && c->has_ct) p["ct"] = mired_to_k(ct);
      if (hue >= 0 && c->color) {
        int r, g, b;
        hs_to_rgb(hue, sat, r, g, b);
        JsonArray a = p["rgb"].to<JsonArray>();
        a.add(r); a.add(g); a.add(b);
      }
      const char *method = type == "switch" ? "Switch.Set" : type == "light" ? "Light.Set" : type == "rgb" ? "RGB.Set"
                           : type == "rgbw" ? "RGBW.Set" : "CCT.Set";
      topic = d->id + "/rpc";
      // Ohne Rueckmeldungs-Zuordnung; der neue Zustand kommt per NotifyStatus
      JsonDocument rq;
      rq["id"] = s_rpc_id++;
      rq["src"] = z2m_client_id();
      rq["method"] = method;
      rq["params"] = p;
      serializeJson(rq, payload);
    } else if (c->kind == CTL_SWITCH) {
      topic = "shellies/" + d->id + "/" + comp + "/command";
      payload = on == 1 ? "on" : "off";
    } else {
      JsonDocument p;
      if (on >= 0) p["turn"] = on == 1 ? "on" : "off";
      bool color = comp.compare(0, 6, "color/") == 0;
      if (hue >= 0 && c->color) {
        int r, g, b;
        hs_to_rgb(hue, sat, r, g, b);
        if (c->has_ct) p["mode"] = "color";   // Lampen mit Weiss- und Farbmodus
        p["red"] = r; p["green"] = g; p["blue"] = b;
        if (bri < 0 && c->bri > 0) p["gain"] = c->bri;
      }
      if (ct >= 0 && c->has_ct) {
        if (color) p["mode"] = "white";
        p["temp"] = mired_to_k(ct);
      }
      if (bri >= 0) {
        bool gain = color && c->cmode != "color_temp" && ct < 0;
        p[gain ? "gain" : "brightness"] = bri;
      }
      topic = "shellies/" + d->id + "/" + comp + "/set";
      serializeJson(p, payload);
    }
    c->pending_until = millis() + 3000;
    s_state_ver++;
  }
  send(topic, payload);
}

void shelly_toggle(const std::string &key) {
  ControlView v;
  if (!shelly_get(key, v)) return;
  command(key, v.on ? 0 : 1, -1, -1, -1, -1);
}
void shelly_set_on(const std::string &key, bool on) { command(key, on ? 1 : 0, -1, -1, -1, -1); }
void shelly_set_brightness_pct(const std::string &key, int pct) { command(key, -1, constrain(pct, 1, 100), -1, -1, -1); }
void shelly_set_ct(const std::string &key, int mired) {
  command(key, -1, -1, constrain(mired, CT_MIN_MIRED, CT_MAX_MIRED), -1, -1);
}
void shelly_set_color_hs(const std::string &key, float hue, float sat) {
  command(key, -1, -1, -1, fmodf(hue < 0 ? 0 : hue, 360.f), constrain(sat, 0.f, 100.f));
}
