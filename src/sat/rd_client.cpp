// Zusatzdisplay – siehe rd_client.h und src/ui/ui_remote.cpp
#include "rd_client.h"
#if SAT_REMOTE
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <map>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mbedtls/md.h"
#include "sat_config.h"
#include "../sat_proto.h"
#include "../ui/ui_remote.h"   // RD_UDP_PORT

bool sat_mqtt_connected();
bool sat_mqtt_publish(const std::string &topic, const std::string &payload);

namespace {
SemaphoreHandle_t s_mtx = nullptr;
struct Lock {
  Lock() {
    if (!s_mtx) s_mtx = xSemaphoreCreateRecursiveMutex();
    xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY);
  }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};
const uint32_t HELLO_MS = 20000;
}  // namespace

static WiFiUDP s_udp;
static bool s_udp_on = false;
static std::map<std::string, RdTile> s_tiles;
static std::vector<std::string> s_keys;
static uint32_t s_pn = 0, s_ctr = 0, s_ver = 1, s_hello_ms = 0, s_panel_ms = 0, s_hellos = 0;
static bool s_fresh = true;   // noch keine Kachelliste: "frisch gestartet" melden (Panel schickt dann alles)
static std::string s_panel_ip;
static std::string s_tz;

static std::string mac16(const std::string &msg) {
  uint8_t key[16], full[32];
  sat_key_from_password(g_sat.ble_pw.c_str(), key);
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, 16, (const uint8_t *)msg.data(), msg.size(), full);
  char h[17];
  for (int i = 0; i < 8; i++) snprintf(h + 2 * i, 3, "%02x", full[i]);
  return h;
}

static void send_panel(const char *sub, const std::string &payload) {
  if (s_udp_on && WiFi.isConnected()) {
    IPAddress ip;
    bool have = !s_panel_ip.empty() && ip.fromString(s_panel_ip.c_str());
    if (!have) ip = IPAddress(255, 255, 255, 255);   // Panel unbekannt: an alle im Netz
    if (s_udp.beginPacket(ip, RD_UDP_PORT)) {
      s_udp.write((const uint8_t *)payload.data(), payload.size());
      s_udp.endPacket();
    }
  }
  if (sat_mqtt_connected()) sat_mqtt_publish(std::string(sat_topic("rd/").c_str()) + sub, payload);
}

static void hello() {
  std::string id = g_sat.id.c_str();
  uint32_t c = ++s_hellos;
  JsonDocument d;
  d["rd"] = id;
  d["t"] = "hello";
  d["n"] = g_sat.name.c_str();
  d["c"] = c;
  if (s_fresh) d["f"] = 1;
  d["h"] = mac16("hello|" + id + "|" + std::to_string(c));
  std::string s;
  serializeJson(d, s);
  send_panel("hello", s);
}

static void handle(const char *buf, size_t n, const std::string &from_ip) {
  JsonDocument d;
  if (deserializeJson(d, buf, n)) return;
  std::string t = d["t"] | "";
  if (t != "tile" && t != "keys") return;
  Lock l;
  s_pn = d["pn"] | s_pn;
  s_panel_ms = millis();
  if (!from_ip.empty() && from_ip != s_panel_ip) {   // Panel-IP lernen (DHCP kann sie aendern)
    s_panel_ip = from_ip;
    if (g_sat.udp_target != from_ip.c_str()) {
      g_sat.udp_target = from_ip.c_str();
      sat_config_save();
    }
  }
  if (t == "keys") {
    // Uhrzeit und Zeitzone vom Hauptpanel (das Display hat oft kein Internet)
    const char *tz = d["tz"] | "";
    if (*tz && s_tz != tz) {
      s_tz = tz;
      setenv("TZ", tz, 1);
      tzset();
    }
    uint32_t tm = d["tm"] | 0u;
    if (tm > 1600000000u) {
      time_t cur = time(nullptr);
      if (cur < 1600000000 || llabs((long long)cur - (long long)tm) > 2) {
        timeval tv = {(time_t)tm, 0};
        settimeofday(&tv, nullptr);
      }
    }
    s_keys.clear();
    for (const char *k : d["ks"].as<JsonArray>()) s_keys.push_back(k);
    for (auto it = s_tiles.begin(); it != s_tiles.end();)   // abgewaehlte Kacheln vergessen
      if (std::find(s_keys.begin(), s_keys.end(), it->first) == s_keys.end()) it = s_tiles.erase(it);
      else ++it;
    s_fresh = false;
    s_ver++;
    return;
  }
  std::string k = d["k"] | "";
  if (k.empty()) return;
  RdTile &r = s_tiles[k];
  r.key = k;
  r.name = d["n"] | "";
  r.icon = d["i"] | "";
  r.value = d["v"] | "";
  r.sub = d["s"] | "";
  r.accent = d["a"] | 0x9AA5B1u;
  r.fill = d["f"] | -1;
  r.lit = (d["l"] | 0) != 0;
  r.lit_color = d["lc"] | 0u;
  r.dim = (d["d"] | 0) != 0;
  r.act = d["x"] | 0;
  r.bri = d["b"] | -1;
  r.color = (d["cm"] | 0) != 0;
  r.hue = d["h"] | -1;
  r.sat = d["sa"] | -1;
  r.ct = -1;
  if (d["ct"].is<JsonArray>() && d["ct"].size() == 3) {
    r.ct_min = d["ct"][0];
    r.ct_max = d["ct"][1];
    r.ct = d["ct"][2];
  }
  r.ms = millis();
  s_ver++;
}

void rd_begin() {
  s_panel_ip = g_sat.udp_target.c_str();
}

void rd_loop() {
  uint32_t now = millis();
  if (!WiFi.isConnected()) return;
  if (!s_udp_on) s_udp_on = s_udp.begin(RD_UDP_PORT);
  if (s_udp_on) {
    for (int i = 0; i < 6; i++) {
      int n = s_udp.parsePacket();
      if (n <= 0) break;
      char buf[1024];
      int r = s_udp.read(buf, sizeof(buf));
      if (r > 0) handle(buf, r, s_udp.remoteIP().toString().c_str());
    }
  }
  // Melden: alle 20 s, ohne Kachelliste alle 4 s
  if (!s_hello_ms || now - s_hello_ms > (s_fresh ? 4000u : HELLO_MS)) {
    s_hello_ms = now;
    hello();
  }
}

void rd_mqtt_in(const std::string &sub, const char *data, size_t len) {
  if (sub == "tile" || sub == "keys") handle(data, len, "");
}

std::vector<RdTile> rd_tiles() {
  Lock l;
  std::vector<RdTile> r;
  for (auto &k : s_keys) {
    auto it = s_tiles.find(k);
    if (it != s_tiles.end()) r.push_back(it->second);
  }
  return r;
}

bool rd_action(const std::string &key) {
  uint32_t pn;
  {
    Lock l;
    pn = s_pn;
  }
  if (!pn) return false;
  std::string id = g_sat.id.c_str();
  uint32_t c = ++s_ctr;
  JsonDocument d;
  d["rd"] = id;
  d["t"] = "cmd";
  d["k"] = key;
  d["pn"] = pn;
  d["c"] = c;
  d["h"] = mac16("cmd|" + id + "|" + key + "|" + std::to_string(pn) + "|" + std::to_string(c));
  std::string s;
  serializeJson(d, s);
  send_panel("cmd", s);
  return true;
}

bool rd_set(const std::string &key, char what, int value) {
  uint32_t pn;
  {
    Lock l;
    pn = s_pn;
  }
  if (!pn) return false;
  std::string id = g_sat.id.c_str(), a(1, what);
  uint32_t c = ++s_ctr;
  JsonDocument d;
  d["rd"] = id;
  d["t"] = "cmd";
  d["k"] = key;
  d["a"] = a;
  d["v"] = value;
  d["pn"] = pn;
  d["c"] = c;
  d["h"] = mac16("cmd|" + id + "|" + key + "|" + a + "|" + std::to_string(value) + "|" + std::to_string(pn) + "|" +
                 std::to_string(c));
  std::string s;
  serializeJson(d, s);
  send_panel("cmd", s);
  return true;
}

bool rd_tile(const std::string &key, RdTile &out) {
  Lock l;
  auto it = s_tiles.find(key);
  if (it == s_tiles.end()) return false;
  out = it->second;
  return true;
}

uint32_t rd_version() { return s_ver; }
bool rd_panel_seen() { return s_panel_ms && millis() - s_panel_ms < 60000; }

std::string rd_status() {
  Lock l;
  if (!WiFi.isConnected()) return "kein WLAN";
  if (!rd_panel_seen()) return s_panel_ip.empty() ? "suche Hauptpanel …" : "Hauptpanel " + s_panel_ip + " antwortet nicht";
  return "verbunden mit " + (s_panel_ip.empty() ? std::string("Hauptpanel (MQTT)") : s_panel_ip) + ", " +
         std::to_string(s_keys.size()) + " Kachel(n)";
}
#endif
