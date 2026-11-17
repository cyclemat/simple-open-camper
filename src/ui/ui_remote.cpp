// Zusatzdisplays (siehe ui_remote.h)
//
// Ablauf:
//  - Das Display meldet sich alle 20 s mit "hello" (UDP an Port RD_UDP_PORT und/oder MQTT <base>/<id>/rd/hello).
//    Pruefwert: HMAC-SHA256 (erste 8 Byte) mit dem Schluessel aus dem Satelliten-Passwort. Unbekannte Displays mit
//    gueltigem Pruefwert werden aufgenommen (hoechstens RD_MAX), zuerst mit den Kacheln der ersten Seite.
//  - Das Panel schickt die Liste der Kacheln ("keys") und je Kachel die fertigen Anzeigedaten ("tile") – bei Aenderung
//    sofort, sonst alle 20 s. Auf dem Weg, auf dem das Display zuletzt zu hoeren war (UDP, MQTT oder beide).
//  - Antippen am Display: "cmd" mit Kachel, Panel-Startnummer (pn) und fortlaufendem Zaehler, ebenfalls mit Pruefwert.
//    Das Panel fuehrt es wie ein Antippen am Panel aus (ui_tile_action).
// Kein eigener Task: ui_remote_tick() laeuft im UI-Takt (100 ms), Socket nicht blockierend.
//
// MQTT bevorzugt (wenn das Display sich per MQTT meldet und das Panel einen Broker hat): jede Kachel liegt einmal fuer
// alle Displays als gespeicherte Nachricht unter <base>/tiles/<Kachel> ('#' -> '~', '+' -> '^'), nur bei Aenderung
// neu; die Auswahl je Display gespeichert unter <base>/<Display>/rd/keys/<Teil>; Uhrzeit alle 30 s unter <base>/time.
// Das Display abonniert nur seine Kacheln. Kein Wiederholen noetig, nichts geht verloren (TCP). UDP bleibt Rueckfall.
#include "ui_remote.h"
#include "ui_internal.h"
#include "../i18n/i18n.h"
#include "../layout.h"
#include "../net.h"
#include "../settings.h"
#include "../z2m.h"
#include "../panelsat.h"
#include "../sat_proto.h"
#include <Arduino.h>
#include <time.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <algorithm>
#include <deque>
#include <map>
#include <functional>
#include "lwip/sockets.h"
#include "mbedtls/md.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {
const size_t RD_MAX = 4;
const uint32_t ONLINE_MS = 90000;    // so lange ohne "hello" gilt ein Display als weg
const uint32_t RESEND_MS = 10000;    // unveraenderte Kacheln so oft wiederholen (UDP kann verloren gehen)
const uint32_t KEYS_MS = 30000;
const int BURST = 3;                 // Pakete je Display und Runde (300 ms) – das CYD puffert nur ~6 Pakete

struct Sent {
  uint32_t sig = 0, ms = 0;
};
struct Rd {
  size_t mq_chunks = 0;   // MQTT: veroeffentlichte Teile der Kachelliste (zum Loeschen ueberzaehliger)
  std::string id, name, ip;
  std::vector<std::string> keys;
  uint32_t udp_ms = 0, mqtt_ms = 0, last_ctr = 0, keys_ms = 0;
  bool keys_dirty = true;
  std::map<std::string, Sent> sent;
  size_t rr = 0;
};
struct MqttIn {
  std::string id, sub, payload;
};

SemaphoreHandle_t s_mtx = nullptr;
struct Lock {
  Lock() {
    if (!s_mtx) s_mtx = xSemaphoreCreateRecursiveMutex();
    xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY);
  }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};
}  // namespace

static std::vector<Rd> s_rd;
static bool s_loaded = false;
static std::deque<MqttIn> s_mq;
static int s_sock = -1;
static uint32_t s_pn = 0;            // Startnummer dieses Panel-Starts (alte Befehle gelten nicht mehr)
static uint32_t s_sock_try = 0, s_send_ms = 0, s_avail_ms = 0;
// Alle Kacheln des Panels mit Namen und Seite (fuer die Auswahl am Display) – im UI-Takt alle 5 s erneuert, weil die
// Namen aus den UI-Daten kommen (Webserver-Task darf ui_tile_model nicht aufrufen)
struct Avail {
  std::string k, n, p;
};
static std::vector<Avail> s_avail;
static std::map<std::string, uint32_t> s_mq_sig;   // MQTT: zuletzt veroeffentlichter Stand je Kachel
static uint32_t s_mq_time_ms = 0;
static std::string topic_key(const std::string &k) {   // Kachel-Schluessel als Teil eines Topics
  std::string t = k;
  for (auto &c : t)
    if (c == '#') c = '~';
    else if (c == '+') c = '^';
  return t;
}

// ---------------------------------------------------------------- Speicher (NVS "rdisp")
static void save() {
  JsonDocument d;
  JsonArray a = d["d"].to<JsonArray>();
  for (auto &r : s_rd) {
    JsonObject o = a.add<JsonObject>();
    o["i"] = r.id;
    o["n"] = r.name;
    JsonArray k = o["k"].to<JsonArray>();
    for (auto &x : r.keys) k.add(x);
  }
  std::string js;
  serializeJson(d, js);
  Preferences p;
  p.begin("rdisp", false);
  p.putString("j", js.c_str());
  p.end();
}

static void load() {
  if (s_loaded) return;
  s_loaded = true;
  Preferences p;
  if (!p.begin("rdisp", true)) return;
  String js = p.getString("j", "");
  p.end();
  JsonDocument d;
  if (js.isEmpty() || deserializeJson(d, js)) return;
  for (JsonObject o : d["d"].as<JsonArray>()) {
    Rd r;
    r.id = o["i"] | "";
    r.name = o["n"] | "";
    for (const char *k : o["k"].as<JsonArray>()) r.keys.push_back(k);
    if (!r.id.empty() && s_rd.size() < RD_MAX) s_rd.push_back(r);
  }
}

static Rd *find(const std::string &id) {
  for (auto &r : s_rd)
    if (r.id == id) return &r;
  return nullptr;
}

// ---------------------------------------------------------------- Pruefwert
static std::string mac16(const std::string &msg) {
  uint8_t key[16], full[32];
  sat_key_from_password(panelsat_password(), key);
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, 16, (const uint8_t *)msg.data(), msg.size(), full);
  char h[17];
  for (int i = 0; i < 8; i++) snprintf(h + 2 * i, 3, "%02x", full[i]);
  return h;
}
bool ui_remote_sig_ok(const std::string &msg, const std::string &h) { return h.size() == 16 && mac16(msg) == h; }

// ---------------------------------------------------------------- Senden
static void udp_to(const std::string &ip, const std::string &payload) {
  if (s_sock < 0 || ip.empty()) return;
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons(RD_UDP_PORT);
  if (!inet_aton(ip.c_str(), &a.sin_addr)) return;
  sendto(s_sock, payload.data(), payload.size(), 0, (sockaddr *)&a, sizeof(a));
}

static void send_to(const Rd &r, const char *sub, const std::string &payload, uint32_t now) {
  bool udp = r.udp_ms && now - r.udp_ms < ONLINE_MS && !r.ip.empty();
  if (udp) {   // UDP direkt reicht – MQTT nur, wenn das Display per UDP nicht zu hoeren ist (sonst alles doppelt)
    udp_to(r.ip, payload);
    return;
  }
  if (r.mqtt_ms && now - r.mqtt_ms < ONLINE_MS && z2m_connected())
    z2m_publish(std::string(SAT_BASE) + "/" + r.id + "/rd/" + sub, payload);
}

static std::string tile_json(const std::string &key, const TileModel &m) {
  JsonDocument d;
  d["t"] = "tile";
  d["pn"] = s_pn;
  d["k"] = key;
  d["n"] = m.name;
  d["i"] = m.icon;
  d["v"] = m.value;
  if (!m.sub.empty()) d["s"] = m.sub;
  d["a"] = m.accent;
  if (m.fill >= 0) d["f"] = m.fill;
  if (m.lit) d["l"] = 1;
  if (m.lit && m.lit_color) d["lc"] = m.lit_color;
  if (m.dim) d["d"] = 1;
  if (m.act) d["x"] = m.act;
  if (m.bri >= 0) d["b"] = m.bri;
  if (m.color) {
    d["cm"] = 1;
    if (m.hue >= 0) d["h"] = m.hue;
    if (m.sat >= 0) d["sa"] = m.sat;
  }
  if (m.ct >= 0) {
    JsonArray c = d["ct"].to<JsonArray>();
    c.add(m.ct_min);
    c.add(m.ct_max);
    c.add(m.ct);
  }
  if (m.vset) {   // MultiPlus/Ladegeraet: Schalter
    d["vs"] = m.vset;
    d["vm"] = m.vmode;
  }
  if (m.vlim >= 0) d["vl"] = m.vlim;   // Netzstrom-Begrenzung (0,1 A)
  std::string s;
  serializeJson(d, s);
  return s;
}

static void send_round(uint32_t now) {
  // Liste der Displays kopieren (Webserver kann parallel aendern), Kachelzustand danach zurueckschreiben
  std::vector<Rd> rds;
  {
    Lock l;
    rds = s_rd;
  }
  // ---- MQTT: Kacheln aller per MQTT verbundenen Displays einmal gespeichert veroeffentlichen
  bool broker = z2m_connected();
  std::vector<std::string> mq_keys;
  if (broker)
    for (auto &r : rds)
      if (r.mqtt_ms && now - r.mqtt_ms < ONLINE_MS)
        for (auto &k : r.keys)
          if (std::find(mq_keys.begin(), mq_keys.end(), k) == mq_keys.end()) mq_keys.push_back(k);
  if (broker) {
    std::string base = SAT_BASE;
    int pub = 0;
    for (auto &k : mq_keys) {
      if (pub >= 12) break;   // je Runde (150 ms) hoechstens 12 – der Rest folgt
      TileModel m;
      if (!ui_tile_model(k, m)) continue;
      std::string js = tile_json(k, m);
      uint32_t sig = std::hash<std::string>()(js);
      auto it = s_mq_sig.find(k);
      if (it != s_mq_sig.end() && it->second == sig) continue;
      z2m_publish_retained(base + "/tiles/" + topic_key(k), js);
      s_mq_sig[k] = sig;
      pub++;
    }
    for (auto it = s_mq_sig.begin(); it != s_mq_sig.end();)   // nicht mehr gebraucht: beim Broker loeschen
      if (std::find(mq_keys.begin(), mq_keys.end(), it->first) == mq_keys.end()) {
        z2m_publish_retained(base + "/tiles/" + topic_key(it->first), "");
        it = s_mq_sig.erase(it);
      } else {
        ++it;
      }
    if (!mq_keys.empty() && (!s_mq_time_ms || now - s_mq_time_ms > 30000)) {   // Uhrzeit (nicht gespeichert)
      s_mq_time_ms = now;
      time_t tnow = time(nullptr);
      if (tnow > 1600000000) {
        JsonDocument d;
        d["t"] = "time";
        d["pn"] = s_pn;
        d["tm"] = (uint32_t)tnow;
        d["tz"] = settings_copy().tz.c_str();
        std::string s;
        serializeJson(d, s);
        z2m_publish(base + "/time", s);
      }
    }
  } else if (!s_mq_sig.empty()) {
    s_mq_sig.clear();   // Broker weg: nach dem Wiederverbinden alles neu veroeffentlichen
  }

  for (auto &r : rds) {
    bool on = (r.udp_ms && now - r.udp_ms < ONLINE_MS) || (r.mqtt_ms && now - r.mqtt_ms < ONLINE_MS);
    if (!on) continue;
    if (broker && r.mqtt_ms && now - r.mqtt_ms < ONLINE_MS) {
      // MQTT: nur die Kachelliste dieses Displays (gespeichert, in Teilen), Kacheln kommen oben gemeinsam
      if (r.keys_dirty || !r.keys_ms) {
        const size_t CH = 12;
        std::string base = std::string(SAT_BASE) + "/" + r.id + "/rd/keys/";
        size_t parts = 0;
        for (size_t o = 0; o == 0 || o < r.keys.size(); o += CH, parts++) {
          JsonDocument d;
          d["t"] = "keys";
          d["pn"] = s_pn;
          d["o"] = o;
          d["n"] = r.keys.size();
          JsonArray a = d["ks"].to<JsonArray>();
          for (size_t i = o; i < r.keys.size() && i < o + CH; i++) a.add(r.keys[i]);
          std::string s;
          serializeJson(d, s);
          z2m_publish_retained(base + std::to_string(parts), s);
        }
        for (size_t i = parts; i < r.mq_chunks; i++) z2m_publish_retained(base + std::to_string(i), "");   // alte Teile weg
        r.mq_chunks = parts;
        r.keys_dirty = false;
        r.keys_ms = now;
      }
      continue;
    }
    if (r.keys_dirty || now - r.keys_ms > KEYS_MS) {
      // Kachelliste in Teilen (o = ab Eintrag, n = gesamt): ein Paket ueber ~1,4 KB wird geteilt, das CYD verwirft das
      const size_t CH = 12;
      time_t tnow = time(nullptr);
      for (size_t o = 0; o == 0 || o < r.keys.size(); o += CH) {
        JsonDocument d;
        d["t"] = "keys";
        d["pn"] = s_pn;
        d["o"] = o;
        d["n"] = r.keys.size();
        if (o == 0 && tnow > 1600000000) {   // Uhrzeit fuer die Uhr des Displays (es hat oft kein Internet)
          d["tm"] = (uint32_t)tnow;
          d["tz"] = settings_copy().tz.c_str();
        }
        JsonArray a = d["ks"].to<JsonArray>();
        for (size_t i = o; i < r.keys.size() && i < o + CH; i++) a.add(r.keys[i]);
        std::string s;
        serializeJson(d, s);
        send_to(r, "keys", s, now);
      }
      r.keys_dirty = false;
      r.keys_ms = now;
      continue;   // Kacheln erst in der naechsten Runde (sonst zu viele Pakete auf einmal fuer das CYD)
    }
    // Zuerst geaenderte Kacheln (sofort, z. B. nach dem Schalten), danach reihum die Wiederholungen
    int sent = 0;
    std::vector<std::pair<std::string, std::string>> later;   // unveraendert, aber Wiederholung faellig
    for (size_t n = 0; n < r.keys.size(); n++) {
      size_t i = (r.rr + n) % r.keys.size();
      const std::string &k = r.keys[i];
      TileModel m;
      if (!ui_tile_model(k, m)) continue;
      std::string js = tile_json(k, m);
      uint32_t sig = std::hash<std::string>()(js);
      Sent &st = r.sent[k];
      if (st.sig == sig) {
        if (now - st.ms >= RESEND_MS) later.push_back({k, js});
        continue;
      }
      if (sent >= BURST) continue;   // naechste Runde (300 ms) – sonst gehen beim CYD Pakete verloren
      send_to(r, "tile", js, now);
      st.sig = sig;
      st.ms = now;
      sent++;
    }
    for (auto &x : later) {
      if (sent >= BURST) break;
      send_to(r, "tile", x.second, now);
      r.sent[x.first].ms = now;
      sent++;
      auto it = std::find(r.keys.begin(), r.keys.end(), x.first);
      if (it != r.keys.end()) r.rr = (it - r.keys.begin() + 1) % r.keys.size();
    }
  }
  Lock l;
  for (auto &c : rds) {
    Rd *r = find(c.id);
    if (!r) continue;
    r->sent = c.sent;
    r->rr = c.rr;
    r->mq_chunks = c.mq_chunks;
    if (!r->keys_dirty || c.keys_ms > r->keys_ms) {   // inzwischen neu gesetzte Kacheln nicht verschlucken
      r->keys_dirty = c.keys_dirty;
      r->keys_ms = c.keys_ms;
    }
  }
}

// ---------------------------------------------------------------- Empfangen
static void handle(const char *buf, size_t n, const std::string &ip, bool via_udp) {
  JsonDocument d;
  if (deserializeJson(d, buf, n)) return;
  std::string id = d["rd"] | "", t = d["t"] | "", h = d["h"] | "";
  uint32_t c = d["c"] | 0u;
  if (id.empty() || id.size() > 40 || h.size() != 16) return;
  uint32_t now = millis();
  if (t == "hello") {
    if (mac16("hello|" + id + "|" + std::to_string(c)) != h) return;
    Lock l;
    Rd *r = find(id);
    if (!r) {
      if (s_rd.size() >= RD_MAX) return;
      Rd nr;
      nr.id = id;
      Layout lay = layout_get();   // neues Display: zuerst die Kacheln der ersten Seite
      if (!lay.pages.empty())
        for (auto &tl : lay.pages[0].tiles) nr.keys.push_back(tl.key);
      s_rd.push_back(nr);
      r = &s_rd.back();
      log_w("Zusatzdisplay %s aufgenommen", id.c_str());
      save();
    }
    std::string nm = d["n"] | "";
    if (!nm.empty() && nm != r->name) {
      r->name = nm.substr(0, 32);
      save();
    }
    if (via_udp) {
      r->ip = ip;
      r->udp_ms = now;
    } else {
      r->mqtt_ms = now;
    }
    if (d["f"] | 0) {   // Display frisch gestartet: alles sofort schicken, sein Befehlszaehler beginnt neu
      r->sent.clear();
      r->keys_dirty = true;
      r->last_ctr = 0;
    }
    return;
  }
  if (t == "cmd") {
    std::string k = d["k"] | "", a = d["a"] | "";
    int32_t v = d["v"] | 0;
    uint32_t pn = d["pn"] | 0u;
    if (k.empty() || pn != s_pn) return;   // Befehl aus einem frueheren Panel-Start
    // Pruefwert: Umschalten ohne a/v (wie bisher), Dimmen/Farbe mit Aktion und Wert
    std::string msg = a.empty() ? "cmd|" + id + "|" + k + "|" + std::to_string(pn) + "|" + std::to_string(c)
                                : "cmd|" + id + "|" + k + "|" + a + "|" + std::to_string(v) + "|" + std::to_string(pn) + "|" + std::to_string(c);
    if (mac16(msg) != h) return;
    {
      Lock l;
      Rd *r = find(id);
      if (!r || c <= r->last_ctr) return;   // unbekannt oder wiederholt
      r->last_ctr = c;
      if (std::find(r->keys.begin(), r->keys.end(), k) == r->keys.end()) return;   // nur eigene Kacheln
      r->sent.erase(k);   // neuen Zustand gleich schicken
      if (via_udp) {
        r->ip = ip;
        r->udp_ms = now;
      } else {
        r->mqtt_ms = now;
      }
    }
    if (!a.empty()) {   // Dimm-Fenster: Helligkeit ('b'), Farbe ('h' = hue*1000+sat), Farbtemperatur ('c', Mired)
      if (!ui_tile_set(k, a[0], v)) log_w("Zusatzdisplay %s: %s %s=%d nicht moeglich", id.c_str(), k.c_str(), a.c_str(), (int)v);
      return;
    }
    std::string err;
    if (!ui_tile_action(k, &err)) log_w("Zusatzdisplay %s: %s – %s", id.c_str(), k.c_str(), err.c_str());
  }
}

static void sock_open() {
  s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s_sock < 0) return;
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons(RD_UDP_PORT);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(s_sock, (sockaddr *)&a, sizeof(a)) < 0) {
    close(s_sock);
    s_sock = -1;
    return;
  }
  int fl = fcntl(s_sock, F_GETFL, 0);
  fcntl(s_sock, F_SETFL, fl | O_NONBLOCK);
}

void ui_remote_tick() {
  uint32_t now = millis();
  if (!s_pn) s_pn = esp_random() | 1;
  {
    Lock l;
    load();
  }
  if (net_mode() == NET_OFF) return;
  if (s_sock < 0 && now - s_sock_try > 10000) {
    s_sock_try = now;
    sock_open();
  }
  if (s_sock >= 0) {
    char buf[512];
    for (int i = 0; i < 8; i++) {
      sockaddr_in from = {};
      socklen_t fl = sizeof(from);
      int n = recvfrom(s_sock, buf, sizeof(buf) - 1, 0, (sockaddr *)&from, &fl);
      if (n <= 0) break;
      char ipb[16];
      inet_ntoa_r(from.sin_addr, ipb, sizeof(ipb));
      handle(buf, n, ipb, true);
    }
  }
  for (int i = 0; i < 8; i++) {   // MQTT-Nachrichten aus dem MQTT-Task
    MqttIn m;
    {
      Lock l;
      if (s_mq.empty()) break;
      m = s_mq.front();
      s_mq.pop_front();
    }
    handle(m.payload.data(), m.payload.size(), "", false);
  }
  if (!s_avail_ms || now - s_avail_ms >= 5000) {
    s_avail_ms = now;
    std::vector<Avail> av;
    Layout lay = layout_get();
    for (auto &pg : lay.pages)
      for (auto &t : pg.tiles) {
        if (std::any_of(av.begin(), av.end(), [&](const Avail &a) { return a.k == t.key; })) continue;
        TileModel m;
        if (ui_tile_model(t.key, m)) av.push_back({t.key, m.name, pg.name});
      }
    Lock l;
    s_avail.swap(av);
  }
  if (now - s_send_ms >= 150) {   // je Runde hoechstens BURST Pakete: ~20 Kacheln pro Sekunde
    s_send_ms = now;
    bool any;
    {
      Lock l;
      any = !s_rd.empty();
    }
    if (any) send_round(now);
  }
}

void ui_remote_mqtt_in(const std::string &id, const std::string &sub, const char *payload, size_t len) {
  if (sub != "rd/hello" && sub != "rd/cmd") return;   // eigene rd/tile, rd/keys kommen hier auch vorbei
  Lock l;
  if (s_mq.size() >= 16) return;
  s_mq.push_back({id, sub, std::string(payload, len)});
}

// ---------------------------------------------------------------- Webserver
std::string ui_remote_json() {
  Lock l;
  load();
  uint32_t now = millis();
  JsonDocument d;
  JsonArray a = d["displays"].to<JsonArray>();
  for (auto &r : s_rd) {
    JsonObject o = a.add<JsonObject>();
    o["id"] = r.id;
    o["name"] = r.name;
    o["ip"] = r.ip;
    bool u = r.udp_ms && now - r.udp_ms < ONLINE_MS, m = r.mqtt_ms && now - r.mqtt_ms < ONLINE_MS;
    o["online"] = u || m;
    o["udp"] = u;
    o["mqtt"] = m;
    uint32_t last = std::max(r.udp_ms, r.mqtt_ms);
    o["age"] = last ? (int32_t)((now - last) / 1000) : -1;
    JsonArray k = o["tiles"].to<JsonArray>();
    for (auto &x : r.keys) k.add(x);
  }
  std::string s;
  serializeJson(d, s);
  return s;
}

std::string ui_remote_avail_json(const std::string &id) {
  Lock l;
  load();
  JsonDocument d;
  JsonArray a = d["avail"].to<JsonArray>();
  for (auto &x : s_avail) {
    JsonObject o = a.add<JsonObject>();
    o["k"] = x.k;
    o["n"] = x.n;
    o["p"] = x.p;
  }
  JsonArray t = d["tiles"].to<JsonArray>();
  if (Rd *r = find(id))
    for (auto &k : r->keys) t.add(k);
  std::string s;
  serializeJson(d, s);
  return s;
}

std::vector<RdListItem> ui_remote_list() {
  Lock l;
  load();
  uint32_t now = millis();
  std::vector<RdListItem> out;
  for (auto &r : s_rd) {
    RdListItem i;
    i.id = r.id;
    i.name = r.name.empty() ? r.id : r.name;
    i.ip = r.ip;
    i.udp = r.udp_ms && now - r.udp_ms < ONLINE_MS;
    i.mqtt = r.mqtt_ms && now - r.mqtt_ms < ONLINE_MS;
    i.online = i.udp || i.mqtt;
    i.tiles = r.keys.size();
    out.push_back(i);
  }
  return out;
}

bool ui_remote_set(const std::string &id, const std::vector<std::string> &keys, const std::string &name) {
  Lock l;
  load();
  Rd *r = find(id);
  if (!r) return false;
  r->keys.clear();
  for (auto &k : keys)
    if (r->keys.size() < 48 && std::find(r->keys.begin(), r->keys.end(), k) == r->keys.end()) r->keys.push_back(k);
  if (!name.empty()) r->name = name.substr(0, 32);
  r->keys_dirty = true;
  r->sent.clear();
  save();
  return true;
}

void ui_remote_forget(const std::string &id) {
  Lock l;
  load();
  for (auto it = s_rd.begin(); it != s_rd.end(); ++it)
    if (it->id == id) {
      s_rd.erase(it);
      save();
      return;
    }
}
