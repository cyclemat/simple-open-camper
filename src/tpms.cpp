// Reifendruck (TPMS) – siehe tpms.h
#include "tpms.h"

#if WITH_TPMS
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <atomic>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {   // dateilokal

const uint32_t DISCOVERY_MS = 180000;
const uint32_t STALE_MS = 70000;   // wie die App: 70 s ohne Paket = nicht mehr da

struct Sensor {
  std::string id, name;
  uint8_t pos = 0, type = 0;
  bool has = false;
  float bar = NAN, temp = NAN;
  int batt_mv = -1, rssi = 0;
  uint32_t last_ms = 0;
};

SemaphoreHandle_t s_mx = nullptr;
std::vector<Sensor> s_sens;
std::vector<TpmsFound> s_found;
uint32_t s_disc_until = 0;
std::atomic<uint32_t> s_ver{1};

struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mx); }
};

std::string hex6(const uint8_t *p, bool rev) {
  char b[18];
  if (rev) snprintf(b, sizeof(b), "%02x:%02x:%02x:%02x:%02x:%02x", p[5], p[4], p[3], p[2], p[1], p[0]);
  else snprintf(b, sizeof(b), "%02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2], p[3], p[4], p[5]);
  return b;
}
int16_t le16(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
int32_t le32(const uint8_t *p) { return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24); }

// Batterie-Code Typ B -> mV (Formel der App; 0 = 1999 mV, 100 = 3000 mV)
int batt_code_mv(int8_t code) {
  if (code < 0) return -1;
  float v = code <= 4 ? code / 4.0f * 224 + 1136 : code <= 28 ? (code - 4) / 24.0f * 224 + 1360 : (code - 28) / 72.0f * 121 + 1584;
  return (int)lroundf(v / 2 / 1023 * 3.6f * 1000);
}

// Radposition aus dem Namen: "TPMS1_…" (1 VL, 2 VR, 3 HL, 4 HR) bzw. "SSPS-FL-…"
uint8_t pos_of_name(const std::string &n) {
  std::string u = n;
  for (auto &c : u) c = toupper((unsigned char)c);
  if (u.size() >= 5 && u.rfind("TPMS", 0) == 0 && u[4] >= '1' && u[4] <= '4') return u[4] - '0';
  if (u.rfind("SSPS-", 0) == 0 && u.size() >= 7) {
    std::string p = u.substr(5, 2);
    return p == "FL" ? 1 : p == "FR" ? 2 : p == "RL" ? 3 : p == "RR" ? 4 : 0;
  }
  return 0;
}

void save() {
  Preferences p;
  if (!p.begin("tpms", false)) return;
  p.clear();
  p.putUChar("n", (uint8_t)s_sens.size());
  for (size_t i = 0; i < s_sens.size(); i++) {
    std::string k = std::to_string(i);
    p.putString(("i" + k).c_str(), s_sens[i].id.c_str());
    p.putString(("m" + k).c_str(), s_sens[i].name.c_str());
    p.putUChar(("p" + k).c_str(), s_sens[i].pos);
  }
  p.end();
}

void load() {
  Preferences p;
  if (!p.begin("tpms", true)) return;
  int n = p.getUChar("n", 0);
  for (int i = 0; i < n && i < TPMS_MAX; i++) {
    std::string k = std::to_string(i);
    Sensor s;
    s.id = p.getString(("i" + k).c_str(), "").c_str();
    s.name = p.getString(("m" + k).c_str(), "").c_str();
    s.pos = p.getUChar(("p" + k).c_str(), 0);
    if (!s.id.empty()) s_sens.push_back(s);
  }
  p.end();
}

Sensor *find(const std::string &id) {
  for (auto &s : s_sens)
    if (s.id == id) return &s;
  return nullptr;
}

}  // namespace

void tpms_begin() {
  if (s_mx) return;
  s_mx = xSemaphoreCreateRecursiveMutex();
  load();
}

uint32_t tpms_version() { return s_ver; }

bool tpms_has_devices() {
  if (!s_mx) return false;
  Lock l;
  return !s_sens.empty();
}

bool tpms_on_adv(const NimBLEAdvertisedDevice *dev) {
  if (!s_mx) return false;
  std::string id, name = dev->haveName() ? dev->getName() : "";
  uint8_t type = 0, pos = 0;
  float bar = NAN, temp = NAN;
  int batt = -1;
  // Typ A: Service Data 0xEEFD / 0xFAF0, genau 11 Byte
  for (uint16_t u : {(uint16_t)0xEEFD, (uint16_t)0xFAF0}) {
    std::string sd = dev->getServiceData(NimBLEUUID(u));
    if (sd.size() != 11) continue;
    const uint8_t *p = (const uint8_t *)sd.data();
    id = hex6(p, true);
    int kpa = le16(p + 6);
    if (kpa >= 0) bar = kpa / 100.0f;
    batt = le16(p + 8);
    if (p[10] != 0xFF) temp = (int)p[10] - 50;
    type = 1;
    break;
  }
  // Typ B: Herstellerdaten Firma 0x0100, 16 Byte nach der Firmen-ID, mit Geraetenamen
  if (!type && dev->haveManufacturerData() && !name.empty()) {
    std::string md = dev->getManufacturerData();
    if (md.size() == 18 && (uint8_t)md[0] == 0x00 && (uint8_t)md[1] == 0x01) {
      const uint8_t *p = (const uint8_t *)md.data() + 2;
      id = hex6(p, false);
      bar = le32(p + 6) / 100000.0f;
      temp = le32(p + 10) / 100.0f;
      batt = batt_code_mv((int8_t)p[14]);
      pos = pos_of_name(name);
      type = 2;
    }
  }
  if (!type) return false;
  if (!pos) pos = pos_of_name(name);
  Lock l;
  bool hit = false;
  if (Sensor *s = find(id)) {
    if (!s->has) log_i("TPMS %s: %.2f bar %.1f °C", id.c_str(), bar, temp);
    s->type = type;
    s->has = true;
    s->bar = bar;
    s->temp = temp;
    s->batt_mv = batt;
    s->rssi = dev->getRSSI();
    s->last_ms = millis();
    s_ver++;
    hit = true;
  }
  if (s_disc_until) {
    if ((int32_t)(millis() - s_disc_until) >= 0) {
      s_disc_until = 0;
      s_ver++;
    } else {
      TpmsFound *f = nullptr;
      for (auto &x : s_found)
        if (x.id == id) f = &x;
      if (!f && s_found.size() < 12) {
        s_found.push_back(TpmsFound());
        f = &s_found.back();
        f->id = id;
        log_i("TPMS gefunden: %s Typ %c \"%s\"", id.c_str(), type == 1 ? 'A' : 'B', name.c_str());
      }
      if (f) {
        if (!name.empty()) f->name = name;
        f->pos = pos;
        f->type = type;
        f->bar = bar;
        f->temp = temp;
        f->rssi = dev->getRSSI();
        f->configured = hit;
        s_ver++;
      }
    }
  }
  return true;
}

std::vector<TpmsInfo> tpms_list() {
  std::vector<TpmsInfo> r;
  if (!s_mx) return r;
  Lock l;
  uint32_t now = millis();
  for (auto &s : s_sens) {
    TpmsInfo i;
    i.key = TPMS_KEY_PREFIX + s.id;
    i.id = s.id;
    i.name = s.name;
    i.pos = s.pos;
    i.type = s.type;
    i.rssi = s.rssi;
    i.age_s = s.last_ms ? (int32_t)((now - s.last_ms) / 1000) : -1;
    bool fresh = s.last_ms && now - s.last_ms < STALE_MS;
    i.has = s.has && fresh;
    i.bar = s.bar;
    i.temp = s.temp;
    i.batt_mv = s.batt_mv;
    i.status = !s.last_ms ? "Warte auf Sensor …" : fresh ? "OK" : "Kein Empfang – Sensor schlaeft (Rad steht) oder ausser Reichweite";
    r.push_back(i);
  }
  return r;
}

void tpms_discovery_start() {
  if (!s_mx) return;
  Lock l;
  s_found.clear();
  s_disc_until = millis() + DISCOVERY_MS;
  if (!s_disc_until) s_disc_until = 1;
  s_ver++;
}

bool tpms_discovery_running() {
  if (!s_mx) return false;
  Lock l;
  if (s_disc_until && (int32_t)(millis() - s_disc_until) >= 0) {
    s_disc_until = 0;
    s_ver++;
  }
  return s_disc_until != 0;
}

std::vector<TpmsFound> tpms_found() {
  if (!s_mx) return {};
  Lock l;
  return s_found;
}

std::string tpms_add(const std::string &id, const std::string &name, int pos) {
  if (!s_mx) return "Reifendruck nicht gestartet";
  Lock l;
  if (find(id)) return "";
  if (s_sens.size() >= TPMS_MAX) return "Es sind höchstens sechs Reifensensoren möglich";
  Sensor s;
  s.id = id;
  s.name = name;
  s.pos = pos < 0 || pos > 4 ? 0 : pos;
  for (auto &f : s_found)
    if (f.id == id) {
      f.configured = true;
      if (!pos) s.pos = f.pos;
    }
  s_sens.push_back(s);
  save();
  s_ver++;
  return "";
}

void tpms_remove(const std::string &id) {
  if (!s_mx) return;
  Lock l;
  for (size_t i = 0; i < s_sens.size(); i++)
    if (s_sens[i].id == id) {
      s_sens.erase(s_sens.begin() + i);
      save();
      s_ver++;
      return;
    }
}

void tpms_rename(const std::string &id, const std::string &name) {
  if (!s_mx) return;
  Lock l;
  if (Sensor *s = find(id)) {
    s->name = name;
    save();
    s_ver++;
  }
}

void tpms_set_pos(const std::string &id, int pos) {
  if (!s_mx) return;
  Lock l;
  if (Sensor *s = find(id)) {
    s->pos = pos < 0 || pos > 4 ? 0 : pos;
    save();
    s_ver++;
  }
}

const char *tpms_pos_text(uint8_t pos) {
  switch (pos) {
    case 1: return "vorne links";
    case 2: return "vorne rechts";
    case 3: return "hinten links";
    case 4: return "hinten rechts";
    default: return "";
  }
}

#endif   // WITH_TPMS
