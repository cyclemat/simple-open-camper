#include "wled.h"
#include "i18n/i18n.h"

#if !WITH_WLED
void wled_init() {}
std::vector<std::string> wled_topics() { return {}; }
void wled_on_connect() {}
bool wled_handle(const std::string &, const char *, size_t) { return false; }
void wled_tick() {}
uint32_t wled_devices_version() { return 0; }
uint32_t wled_state_version() { return 0; }
bool wled_get(const std::string &, ControlView &) { return false; }
void wled_all(std::vector<ControlView> &) {}
size_t wled_count() { return 0; }
void wled_toggle(const std::string &) {}
void wled_set_on(const std::string &, bool) {}
void wled_set_brightness_pct(const std::string &, int) {}
void wled_set_color_hs(const std::string &, float, float) {}
#else

#include <Arduino.h>
#include <atomic>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *PREFIX = "wled/";
static const size_t PREFIX_LEN = 5;

struct WDev {
  std::string id;              // Teil nach "wled/" (Device Topic)
  std::string name;            // aus <ds> der XML-Meldung
  bool online = true;
  bool known = false, on = false;
  int bri = -1;                // 0..255, letzte Helligkeit im eingeschalteten Zustand
  int r = -1, g = -1, b = -1;
  bool asked = false;          // Zustand schon angefragt
  uint32_t pending_until = 0;
};

static SemaphoreHandle_t s_mtx;
static std::vector<WDev> s_devs;
static std::atomic<uint32_t> s_dev_ver{1}, s_state_ver{1};
static uint32_t s_last_pend = 0;

struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};

static WDev *find_dev(const std::string &id) {
  for (auto &d : s_devs)
    if (d.id == id) return &d;
  return nullptr;
}

static WDev &get_dev(const std::string &id) {
  if (WDev *d = find_dev(id)) return *d;
  WDev d;
  d.id = id;
  s_devs.push_back(d);
  s_dev_ver++;
  return s_devs.back();
}

static void rgb_to_hs(int r, int g, int b, float &h, float &s) {
  float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
  float mx = fmaxf(rf, fmaxf(gf, bf)), mn = fminf(rf, fminf(gf, bf)), d = mx - mn;
  s = mx <= 0 ? 0 : d / mx * 100.0f;
  if (d <= 0) h = 0;
  else if (mx == rf) h = 60.0f * fmodf((gf - bf) / d, 6.0f);
  else if (mx == gf) h = 60.0f * ((bf - rf) / d + 2.0f);
  else h = 60.0f * ((rf - gf) / d + 4.0f);
  if (h < 0) h += 360.0f;
}

static void hs_to_rgb(float h, float s, int &r, int &g, int &b) {
  float c = s / 100.0f, x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1)), m = 1 - c;
  float rf = 0, gf = 0, bf = 0;
  if (h < 60) { rf = c; gf = x; }
  else if (h < 120) { rf = x; gf = c; }
  else if (h < 180) { gf = c; bf = x; }
  else if (h < 240) { gf = x; bf = c; }
  else if (h < 300) { rf = x; bf = c; }
  else { rf = c; bf = x; }
  r = lroundf((rf + m) * 255);
  g = lroundf((gf + m) * 255);
  b = lroundf((bf + m) * 255);
}

// Wert eines XML-Elements, z. B. <ds>Kueche</ds>
static bool xml_tag(const char *p, size_t len, const char *tag, std::string &out) {
  std::string s(p, len), open = std::string("<") + tag + ">", close = std::string("</") + tag + ">";
  size_t a = s.find(open);
  if (a == std::string::npos) return false;
  a += open.size();
  size_t e = s.find(close, a);
  if (e == std::string::npos) return false;
  out = s.substr(a, e - a);
  return true;
}

// ---------------------------------------------------------------- oeffentliche API
void wled_init() { s_mtx = xSemaphoreCreateRecursiveMutex(); }

std::vector<std::string> wled_topics() { return {"wled/+/g", "wled/+/c", "wled/+/status", "wled/+/v"}; }

void wled_on_connect() {
  Lock l;
  for (auto &d : s_devs) d.asked = false;
}

bool wled_handle(const std::string &topic, const char *payload, size_t len) {
  if (topic.compare(0, PREFIX_LEN, PREFIX) != 0) return false;
  size_t slash = topic.find('/', PREFIX_LEN);
  if (slash == std::string::npos) return false;
  std::string id = topic.substr(PREFIX_LEN, slash - PREFIX_LEN), sub = topic.substr(slash + 1);
  if (id.empty() || id == "all") return true;
  bool ask = false;
  {
    Lock l;
    WDev &d = get_dev(id);
    if (sub == "status") {
      d.online = len >= 6 && !strncmp(payload, "online", 6);
    } else if (sub == "g") {
      int v = atoi(std::string(payload, len).c_str());
      d.known = true;
      d.on = v > 0;
      if (v > 0) d.bri = v;
      else if (d.bri < 0) d.bri = 128;
      d.pending_until = 0;
    } else if (sub == "c") {
      std::string hex(payload, len);
      if (!hex.empty() && hex[0] == '#') hex = hex.substr(1);
      if (hex.size() >= 6) {
        unsigned long v = strtoul(hex.substr(hex.size() - 6).c_str(), nullptr, 16);   // #WWRRGGBB: letzte 6 Stellen
        d.r = (v >> 16) & 0xFF;
        d.g = (v >> 8) & 0xFF;
        d.b = v & 0xFF;
      }
    } else if (sub == "v") {
      std::string name, ac;
      if (xml_tag(payload, len, "ds", name) && !name.empty() && name != d.name) {
        d.name = name;
        s_dev_ver++;
      }
      if (xml_tag(payload, len, "ac", ac)) {
        int v = atoi(ac.c_str());
        d.known = true;
        d.on = v > 0;
        if (v > 0) d.bri = v;
      }
    } else {
      return true;
    }
    // Neu entdeckt und Zustand unbekannt: einmal nachfragen (die JSON-API meldet danach den Zustand)
    if (d.online && !d.known && !d.asked) {
      d.asked = true;
      ask = true;
    }
    s_state_ver++;
  }
  if (ask) z2m_publish(std::string(PREFIX) + id + "/api", "{\"v\":true}");
  return true;
}

void wled_tick() {
  uint32_t now = millis();
  if (now - s_last_pend < 500) return;
  s_last_pend = now;
  Lock l;
  for (auto &d : s_devs)
    if (d.pending_until && (int32_t)(now - d.pending_until) > 0) {
      d.pending_until = 0;
      s_state_ver++;
    }
}

uint32_t wled_devices_version() { return s_dev_ver; }
uint32_t wled_state_version() { return s_state_ver; }

static void fill_view(const WDev &d, ControlView &v) {
  v.key = "wled:" + d.id;
  v.ieee = d.id;
  v.ep.clear();
  v.name = d.name.empty() ? tr("WLED ") + d.id : d.name;
  v.model = "WLED";
  v.vendor = "WLED";
  v.description = std::string(PREFIX) + d.id;
  v.kind = CTL_LIGHT;
  v.has_bri = true;
  v.has_ct = false;
  v.color = COLOR_HS;
  v.bri_max = 255;
  v.known = d.known;
  v.on = d.on;
  v.online = d.online;
  v.pending = d.pending_until != 0;
  v.bri = d.bri;
  v.ct = -1;
  v.hue = v.sat = -1;
  if (d.r >= 0) rgb_to_hs(d.r, d.g, d.b, v.hue, v.sat);
  v.cx = v.cy = -1;
  v.cmode = "hs";
}

bool wled_get(const std::string &key, ControlView &out) {
  if (!wled_is_key(key)) return false;
  Lock l;
  WDev *d = find_dev(key.substr(5));
  if (!d) return false;
  fill_view(*d, out);
  return true;
}

void wled_all(std::vector<ControlView> &out) {
  Lock l;
  for (auto &d : s_devs) {
    ControlView v;
    fill_view(d, v);
    out.push_back(std::move(v));
  }
}

size_t wled_count() {
  Lock l;
  return s_devs.size();
}

// ---------------------------------------------------------------- Befehle
static void command(const std::string &key, const std::string &sub, const std::string &payload, int on, int bri) {
  if (!wled_is_key(key)) return;
  std::string id = key.substr(5);
  {
    Lock l;
    WDev *d = find_dev(id);
    if (!d) return;
    // sofort anzeigen, bis WLED bestaetigt
    if (on >= 0) d->on = on == 1;
    if (bri > 0) {
      d->bri = bri;
      d->on = true;
    }
    d->known = true;
    d->pending_until = millis() + 3000;
    s_state_ver++;
  }
  z2m_publish(std::string(PREFIX) + id + sub, payload);
}

void wled_toggle(const std::string &key) {
  ControlView v;
  int on = wled_get(key, v) && v.known ? (v.on ? 0 : 1) : -1;
  command(key, "", "T", on, -1);
}

void wled_set_on(const std::string &key, bool on) { command(key, "", on ? "ON" : "OFF", on ? 1 : 0, -1); }

void wled_set_brightness_pct(const std::string &key, int pct) {
  int v = constrain((pct * 255 + 50) / 100, 1, 255);
  command(key, "", std::to_string(v), 1, v);
}

void wled_set_color_hs(const std::string &key, float hue, float sat) {
  int r, g, b;
  hs_to_rgb(hue, sat, r, g, b);
  char hex[8];
  snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
  {
    Lock l;
    if (WDev *d = find_dev(key.substr(5))) {
      d->r = r;
      d->g = g;
      d->b = b;
    }
  }
  command(key, "/col", hex, 1, -1);
}

#endif
