#include "sat_proto.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <ctype.h>
#include "mbedtls/aes.h"
#include "mbedtls/sha256.h"

static void put(JsonObject o, const char *k, float v, float scale = 100) {
  if (!isnan(v)) o[k] = roundf(v * scale) / scale;
}
static float getf(JsonObjectConst o, const char *k) { return o[k].is<float>() ? o[k].as<float>() : NAN; }

// ---------------------------------------------------------------- Truma LevelControl
void sat_to_json(JsonObject o, const TrumaInfo &t) {
  o["name"] = t.name;
  o["has"] = t.has_reading;
  if (t.has_reading) {
    o["percent"] = t.percent;
    put(o, "kg", t.kg);
    o["battery"] = t.battery;
    o["temp"] = t.temperature;
    o["quality"] = t.quality;
  }
  o["age"] = t.age_s;
  o["busy"] = t.busy;
  o["status"] = t.status;
}

void sat_from_json(JsonObjectConst o, TrumaInfo &t) {
  t.name = o["name"] | "Gasflasche";
  t.has_reading = o["has"] | false;
  t.percent = o["percent"] | 0;
  t.kg = o["kg"] | 0.0f;
  t.battery = o["battery"] | 0;
  t.temperature = o["temp"] | 0;
  t.quality = o["quality"] | 0;
  t.age_s = o["age"] | -1;
  t.busy = o["busy"] | false;
  t.status = o["status"] | "";
}

// ---------------------------------------------------------------- Mopeka
void sat_to_json(JsonObject o, const MopekaInfo &m) {
  o["name"] = m.name;
  o["model"] = m.model;
  o["model_text"] = m.model_text;
  o["medium"] = (int)m.medium;
  o["tank"] = m.tank;
  o["empty_mm"] = m.empty_mm;
  o["full_mm"] = m.full_mm;
  put(o, "capacity", m.capacity);
  o["unit"] = m.unit;
  o["has"] = m.has;
  if (m.has) {
    o["percent"] = m.percent;
    o["mm"] = m.level_mm;
    put(o, "amount", m.amount);
    o["quality"] = m.quality;
  }
  if (m.age_s >= 0) {
    o["battery"] = m.battery;
    put(o, "battery_v", m.battery_v);
    o["temp"] = m.temperature;
  }
  o["rssi"] = m.rssi;
  o["age"] = m.age_s;
  o["status"] = m.status;
}

void sat_from_json(JsonObjectConst o, MopekaInfo &m) {
  m.name = o["name"] | "Tank";
  m.model = o["model"] | 0;
  m.model_text = o["model_text"] | "";
  m.medium = (MopekaMedium)(o["medium"] | 0);
  m.tank = o["tank"] | 0;
  m.empty_mm = o["empty_mm"] | 38;
  m.full_mm = o["full_mm"] | 366;
  m.capacity = o["capacity"] | 0.0f;
  // unit zeigt auf eine feste Zeichenkette (MopekaInfo speichert nur den Zeiger)
  m.unit = strcmp(o["unit"] | "kg", "kg") == 0 ? "kg" : "l";
  m.has = o["has"] | false;
  m.percent = o["percent"] | 0;
  m.level_mm = o["mm"] | 0;
  m.amount = o["amount"] | 0.0f;
  m.quality = o["quality"] | 0;
  m.battery = o["battery"] | 0;
  m.battery_v = o["battery_v"] | 0.0f;
  m.temperature = o["temp"] | 0;
  m.rssi = o["rssi"] | 0;
  m.age_s = o["age"] | -1;
  m.status = o["status"] | "";
}

// ---------------------------------------------------------------- Victron
void sat_to_json(JsonObject o, const VictronInfo &v) {
  o["name"] = v.name;
  o["kind"] = (int)v.kind;
  o["record"] = v.record;
  o["model"] = v.model;
  o["has"] = v.has;
  o["age"] = v.age_s;
  o["rssi"] = v.rssi;
  o["status"] = v.status;
  if (!v.has) return;
  if (v.state >= 0) o["state"] = v.state;
  if (v.error >= 0) o["error"] = v.error;
  put(o, "v", v.bat_v);
  put(o, "i", v.bat_i);
  switch (v.kind) {
    case VIC_SOLAR:
      put(o, "pv", v.pv_w, 1);
      put(o, "yield", v.yield_kwh);
      put(o, "load", v.load_i, 10);
      break;
    case VIC_SHUNT:
      put(o, "soc", v.soc, 10);
      put(o, "consumed", v.consumed_ah, 10);
      o["remaining"] = v.remaining_min;
      o["aux_mode"] = v.aux_mode;
      if (v.aux_mode < 3) put(o, "aux", v.aux);
      if (v.alarm) o["alarm"] = v.alarm;
      break;
    case VIC_DCDC:
      put(o, "in_v", v.in_v);
      put(o, "in_i", v.in_i, 10);
      if (v.off_reason) o["off_reason"] = v.off_reason;
      break;
    default:
      break;
  }
}

void sat_from_json(JsonObjectConst o, VictronInfo &v) {
  v.name = o["name"] | "Victron";
  v.kind = (VicKind)(o["kind"] | 0);
  v.record = o["record"] | 0;
  v.model = o["model"] | 0;
  v.has = o["has"] | false;
  v.age_s = o["age"] | -1;
  v.rssi = o["rssi"] | 0;
  v.status = o["status"] | "";
  v.state = o["state"] | -1;
  v.error = o["error"] | -1;
  v.bat_v = getf(o, "v");
  v.bat_i = getf(o, "i");
  v.pv_w = getf(o, "pv");
  v.yield_kwh = getf(o, "yield");
  v.load_i = getf(o, "load");
  v.soc = getf(o, "soc");
  v.consumed_ah = getf(o, "consumed");
  v.remaining_min = o["remaining"] | -1;
  v.aux_mode = o["aux_mode"] | 3;
  v.aux = getf(o, "aux");
  v.alarm = o["alarm"] | 0;
  v.in_v = getf(o, "in_v");
  v.in_i = getf(o, "in_i");
  v.off_reason = o["off_reason"] | 0;
}

// ---------------------------------------------------------------- eigene Anschluesse ("Punkte")
static const char *const KIND_NAMES[] = {"", "tank", "temp", "analog", "tilt", "input", "switch"};
static const char *const CLASS_NAMES[SC_COUNT] = {"", "temperature", "humidity", "level", "voltage", "pressure", "current", "tilt", "binary", "onoff"};
static const char *const CLASS_UNITS[SC_COUNT] = {"", "\xC2\xB0" "C", "%", "%", "V", "bar", "A", "\xC2\xB0", "", ""};

const char *sat_kind_name(SatPointKind k) { return k <= SP_SWITCH ? KIND_NAMES[k] : ""; }
const char *sat_class_name(uint8_t cls) { return cls < SC_COUNT ? CLASS_NAMES[cls] : ""; }
const char *sat_class_unit(uint8_t cls) { return cls < SC_COUNT ? CLASS_UNITS[cls] : ""; }
uint8_t sat_class_of(const char *name) {
  if (!name) return SC_NONE;
  for (uint8_t i = 1; i < SC_COUNT; i++)
    if (!strcmp(name, CLASS_NAMES[i])) return i;
  return SC_NONE;
}
bool sat_point_is_switch(SatPointKind k) { return k == SP_SWITCH; }

std::string sat_point_id(SatPointKind k, uint8_t idx) { return std::string(sat_kind_name(k)) + std::to_string(idx); }

bool sat_point_parse(const std::string &id, SatPointKind &k, uint8_t &idx) {
  for (uint8_t i = 1; i <= SP_SWITCH; i++) {
    size_t n = strlen(KIND_NAMES[i]);
    if (id.size() > n && id.compare(0, n, KIND_NAMES[i]) == 0 && isdigit((unsigned char)id[n])) {
      int v = atoi(id.c_str() + n);
      if (v < 1 || v > 254) return false;   // ab 100: Hymer-Werte (hymer.h)
      k = (SatPointKind)i;
      idx = (uint8_t)v;
      return true;
    }
  }
  return false;
}

void sat_to_json(JsonObject o, const SatPoint &p) {
  o["name"] = p.name;
  o["kind"] = sat_kind_name(p.kind);
  if (p.cls) o["class"] = sat_class_name(p.cls);
  if (!p.unit.empty()) o["unit"] = p.unit;
  o["ok"] = p.ok;
  if (p.kind == SP_SWITCH || p.kind == SP_INPUT) o["on"] = p.on;
  else if (p.ok && !isnan(p.value)) o["value"] = roundf(p.value * 100) / 100;
  if (p.kind == SP_TANK) {
    o["probes"] = p.probes;
    o["n"] = p.probes_n;
    if (p.odd) o["odd"] = true;
  }
  if (p.kind == SP_SWITCH) o["nonce"] = p.nonce;
  if (!p.status.empty()) o["status"] = p.status;
  o["age"] = p.age_s;
}

void sat_from_json(JsonObjectConst o, SatPoint &p) {
  p.name = o["name"] | "";
  const char *k = o["kind"] | "";
  for (uint8_t i = 1; i <= SP_SWITCH; i++)
    if (!strcmp(k, KIND_NAMES[i])) p.kind = (SatPointKind)i;
  p.cls = sat_class_of(o["class"] | "");
  p.unit = o["unit"] | sat_class_unit(p.cls);
  p.ok = o["ok"] | false;
  p.value = o["value"].is<float>() ? o["value"].as<float>() : NAN;
  p.on = o["on"] | false;
  p.probes = o["probes"] | 0;
  p.probes_n = o["n"] | 0;
  p.odd = o["odd"] | false;
  p.nonce = o["nonce"] | 0u;
  p.status = o["status"] | "";
  p.age_s = o["age"] | -1;
}

// ---------------------------------------------------------------- Bluetooth (verschluesselt)
static const uint8_t ADV_VER = 2;
static const size_t ADV_ENC = 6;     // ab hier verschluesselt
static const size_t ADV_HDR = 13;    // bis hier Kopf (Kennung, Typ, Zaehler, MAC des Sensors, Bits)
static const size_t ADV_MAX = 26;    // Herstellerdaten in einem normalen Werbepaket

static void w16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
static uint16_t r16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint16_t u16f(float v, float scale) { return isnan(v) || v < 0 ? 0xFFFF : (uint16_t)fminf(65534.f, roundf(v * scale)); }
static int16_t i16f(float v, float scale) { return isnan(v) ? 0x7FFF : (int16_t)fmaxf(-32767.f, fminf(32766.f, roundf(v * scale))); }
static float fu16(uint16_t v, float scale) { return v == 0xFFFF ? NAN : v / scale; }
static float fi16(int16_t v, float scale) { return v == 0x7FFF ? NAN : v / scale; }

static bool mac_bytes(const std::string &mac, uint8_t *out) {
  unsigned b[6];
  if (sscanf(mac.c_str(), "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) return false;
  for (int i = 0; i < 6; i++) out[i] = (uint8_t)b[i];
  return true;
}

static uint16_t crc16(const uint8_t *p, size_t n) {   // CRC-16/CCITT-FALSE
  uint16_t c = 0xFFFF;
  while (n--) {
    c ^= (uint16_t)(*p++) << 8;
    for (int i = 0; i < 8; i++) c = (c & 0x8000) ? (c << 1) ^ 0x1021 : c << 1;
  }
  return c;
}

void sat_key_from_password(const std::string &pw, uint8_t key[16]) {
  std::string in = "PanelSat:" + (pw.empty() ? std::string(SAT_DEFAULT_PASSWORD) : pw);
  uint8_t h[32];
  mbedtls_sha256((const unsigned char *)in.data(), in.size(), h, 0);
  memcpy(key, h, 16);
}

std::string sat_ble_name(const std::string &ble_mac) {
  std::string r = "Satellite-";
  for (char c : ble_mac)
    if (c != ':') r += (char)toupper((unsigned char)c);
  return r;
}

static void ctr(uint8_t *p, size_t n, const uint8_t key[16], const uint8_t sender[6], uint16_t counter, uint8_t type) {
  uint8_t nonce[16] = {0}, block[16];
  memcpy(nonce, sender, 6);
  w16(nonce + 6, counter);
  nonce[8] = type;
  nonce[9] = 0x50;
  nonce[10] = 0x53;
  size_t off = 0;
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, key, 128);
  mbedtls_aes_crypt_ctr(&aes, n, &off, nonce, block, p, p);
  mbedtls_aes_free(&aes);
}

static size_t header(uint8_t *out, size_t cap, size_t payload, SatType type, const std::string &mac, uint8_t flags,
                     const uint8_t *raw6 = nullptr) {
  if (cap < ADV_HDR + payload) return 0;
  if (raw6) memcpy(out + 6, raw6, 6);
  else if (!mac_bytes(mac, out + 6)) return 0;
  out[0] = 0xFF;
  out[1] = 0xFF;
  out[2] = 0x50;   // 'P'
  out[3] = (uint8_t)(type << 4) | ADV_VER;
  out[4] = out[5] = 0;
  out[12] = flags;
  return ADV_HDR;
}

size_t sat_adv_encode(uint8_t *out, size_t cap, const TrumaInfo &t) {
  if (!header(out, cap, 8, SAT_T_TRUMA, t.mac, (t.has_reading ? 1 : 0) | (t.busy ? 2 : 0))) return 0;
  uint8_t *p = out + ADV_HDR;
  p[0] = (uint8_t)t.percent;
  w16(p + 1, u16f(t.kg, 10));
  p[3] = (uint8_t)t.battery;
  p[4] = (uint8_t)(int8_t)t.temperature;
  p[5] = (uint8_t)t.quality;
  w16(p + 6, t.age_s < 0 ? 0xFFFF : (uint16_t)std::min<int32_t>(65534, t.age_s / 60));
  return ADV_HDR + 8;
}

size_t sat_adv_encode(uint8_t *out, size_t cap, const MopekaInfo &m) {
  uint8_t flags = (m.has ? 1 : 0) | (strcmp(m.unit, "kg") ? 4 : 0);
  if (!header(out, cap, 10, SAT_T_MOPEKA, m.mac, flags)) return 0;
  uint8_t *p = out + ADV_HDR;
  p[0] = (uint8_t)m.percent;
  w16(p + 1, m.capacity > 0 ? u16f(m.amount, 10) : 0xFFFF);
  p[3] = (uint8_t)m.battery;
  p[4] = (uint8_t)(int8_t)m.temperature;
  p[5] = (uint8_t)m.quality;
  p[6] = (uint8_t)m.medium;
  w16(p + 7, (uint16_t)std::max(0, m.level_mm));
  p[9] = m.age_s < 0 ? 0xFF : (uint8_t)std::min<int32_t>(254, m.age_s);
  return ADV_HDR + 10;
}

size_t sat_adv_encode(uint8_t *out, size_t cap, const VictronInfo &v) {
  if (!header(out, cap, 11, SAT_T_VICTRON, v.mac, (v.has ? 1 : 0) | (v.error > 0 ? 8 : 0))) return 0;
  uint8_t *p = out + ADV_HDR;
  memset(p, 0xFF, 11);
  p[0] = (uint8_t)v.kind;
  p[1] = v.state < 0 ? 0xFF : (uint8_t)v.state;
  w16(p + 2, u16f(v.bat_v, 100));
  w16(p + 4, (uint16_t)i16f(v.bat_i, 10));
  switch (v.kind) {
    case VIC_SOLAR:
      w16(p + 6, u16f(v.pv_w, 1));
      w16(p + 8, u16f(v.yield_kwh, 100));
      break;
    case VIC_SHUNT:
      w16(p + 6, u16f(v.soc, 10));
      w16(p + 8, v.remaining_min < 0 ? 0xFFFF : (uint16_t)std::min(65534, v.remaining_min));
      break;
    case VIC_DCDC:
      w16(p + 6, u16f(v.in_v, 100));
      w16(p + 8, (uint16_t)i16f(v.in_i, 10));
      break;
    default:
      break;
  }
  p[10] = v.age_s < 0 ? 0xFF : (uint8_t)std::min<int32_t>(254, v.age_s);
  return ADV_HDR + 11;
}

size_t sat_adv_encode(uint8_t *out, size_t cap, const SatPoint &p) {
  uint8_t id[6] = {(uint8_t)p.kind, p.idx, p.cls, 0, 0, 0};
  uint8_t flags = (p.ok ? 1 : 0) | (p.on ? 2 : 0) | (p.odd ? 8 : 0);
  SatType t = p.kind == SP_SWITCH ? SAT_T_SWITCH : SAT_T_SENSOR;
  if (!header(out, cap, 11, t, "", flags, id)) return 0;
  uint8_t *q = out + ADV_HDR;
  float v = p.value;
  memcpy(q, &v, 4);
  q[4] = p.probes;
  q[5] = (p.probes_n & 0x7F) | (p.odd ? 0x80 : 0);
  q[6] = p.nonce & 0xFF;
  q[7] = (p.nonce >> 8) & 0xFF;
  q[8] = (p.nonce >> 16) & 0xFF;
  q[9] = (p.nonce >> 24) & 0xFF;
  q[10] = p.age_s < 0 ? 0xFF : (uint8_t)std::min<int32_t>(254, p.age_s);
  return ADV_HDR + 11;
}

size_t sat_adv_seal(uint8_t *md, size_t len, size_t cap, const uint8_t key[16], const std::string &sender, uint16_t counter) {
  uint8_t s[6];
  if (len < ADV_HDR || len + 2 > cap || len + 2 > ADV_MAX || !mac_bytes(sender, s)) return 0;
  w16(md + 4, counter);
  w16(md + len, crc16(md + ADV_ENC, len - ADV_ENC));
  len += 2;
  ctr(md + ADV_ENC, len - ADV_ENC, key, s, counter, md[3]);
  return len;
}

bool sat_adv_decode(const uint8_t *in, size_t len, const uint8_t key[16], const std::string &sender, SatType &type,
                    std::string &mac, TrumaInfo &t, MopekaInfo &m, VictronInfo &v, SatPoint &sp) {
  uint8_t s[6];
  if (len < ADV_HDR + 2 || len > ADV_MAX || in[0] != 0xFF || in[1] != 0xFF || in[2] != 0x50 || (in[3] & 0x0F) != ADV_VER)
    return false;
  if (!mac_bytes(sender, s)) return false;
  uint8_t md[ADV_MAX];
  memcpy(md, in, len);
  ctr(md + ADV_ENC, len - ADV_ENC, key, s, r16(md + 4), md[3]);
  len -= 2;
  if (crc16(md + ADV_ENC, len - ADV_ENC) != r16(md + len)) return false;   // falsches Passwort oder gestoert
  type = (SatType)(md[3] >> 4);
  char b[18];
  snprintf(b, sizeof(b), "%02x:%02x:%02x:%02x:%02x:%02x", md[6], md[7], md[8], md[9], md[10], md[11]);
  mac = b;
  uint8_t flags = md[12];
  const uint8_t *p = md + ADV_HDR;
  size_t n = len - ADV_HDR;
  switch (type) {
    case SAT_T_TRUMA: {
      if (n < 8) return false;
      t.has_reading = flags & 1;
      t.busy = flags & 2;
      t.percent = p[0];
      float kg = fu16(r16(p + 1), 10);
      t.kg = isnan(kg) ? 0 : kg;
      t.battery = p[3];
      t.temperature = (int8_t)p[4];
      t.quality = p[5];
      uint16_t age = r16(p + 6);
      t.age_s = age == 0xFFFF ? -1 : age * 60;
      t.status = t.has_reading ? "OK" : "Warte auf Sensor …";
      return true;
    }
    case SAT_T_MOPEKA: {
      if (n < 10) return false;
      m.has = flags & 1;
      m.unit = (flags & 4) ? "l" : "kg";
      m.percent = p[0];
      float am = fu16(r16(p + 1), 10);
      m.amount = isnan(am) ? 0 : am;
      m.capacity = isnan(am) ? 0 : 1;   // nur "Inhalt bekannt" – die echte Groesse steht am Satelliten
      m.battery = p[3];
      m.temperature = (int8_t)p[4];
      m.quality = p[5];
      m.medium = (MopekaMedium)p[6];
      m.level_mm = r16(p + 7);
      m.age_s = p[9] == 0xFF ? -1 : p[9];
      m.status = m.has ? "OK" : "Warte auf Sensor …";
      return true;
    }
    case SAT_T_VICTRON: {
      if (n < 11) return false;
      v.has = flags & 1;
      v.error = (flags & 8) ? 1 : -1;
      v.kind = (VicKind)p[0];
      v.state = p[1] == 0xFF ? -1 : p[1];
      v.bat_v = fu16(r16(p + 2), 100);
      v.bat_i = fi16((int16_t)r16(p + 4), 10);
      v.pv_w = v.yield_kwh = v.load_i = v.soc = v.consumed_ah = v.in_v = v.in_i = v.aux = NAN;
      v.remaining_min = -1;
      v.aux_mode = 3;
      if (v.kind == VIC_SOLAR) {
        v.pv_w = fu16(r16(p + 6), 1);
        v.yield_kwh = fu16(r16(p + 8), 100);
      } else if (v.kind == VIC_SHUNT) {
        v.soc = fu16(r16(p + 6), 10);
        uint16_t rem = r16(p + 8);
        v.remaining_min = rem == 0xFFFF ? -1 : rem;
      } else if (v.kind == VIC_DCDC) {
        v.in_v = fu16(r16(p + 6), 100);
        v.in_i = fi16((int16_t)r16(p + 8), 10);
      }
      v.age_s = p[10] == 0xFF ? -1 : p[10];
      v.status = "OK";
      return true;
    }
    case SAT_T_SENSOR:
    case SAT_T_SWITCH: {
      if (n < 11 || md[6] < SP_TANK || md[6] > SP_SWITCH || !md[7]) return false;
      sp.kind = (SatPointKind)md[6];
      sp.idx = md[7];
      sp.cls = md[8] < SC_COUNT ? md[8] : SC_NONE;
      sp.id = sat_point_id(sp.kind, sp.idx);
      mac = sp.id;
      sp.unit = sat_class_unit(sp.cls);
      sp.ok = flags & 1;
      sp.on = flags & 2;
      float val;
      memcpy(&val, p, 4);
      sp.value = val;
      sp.probes = p[4];
      sp.probes_n = p[5] & 0x7F;
      sp.odd = p[5] & 0x80;
      sp.nonce = (uint32_t)p[6] | ((uint32_t)p[7] << 8) | ((uint32_t)p[8] << 16) | ((uint32_t)p[9] << 24);
      sp.age_s = p[10] == 0xFF ? -1 : p[10];
      return true;
    }
    default:
      return false;
  }
}

// ---------------------------------------------------------------- Schaltbefehle Panel -> Satellit
static const uint8_t CMD_VER = 1;
static const size_t CMD_LEN = 16;

static void cmd_ctr(uint8_t *p, size_t n, const uint8_t key[16], const uint8_t target[6], uint16_t counter) {
  uint8_t nonce[16] = {0}, block[16];
  memcpy(nonce, target, 6);
  w16(nonce + 6, counter);
  nonce[8] = 0x43;   // 'C'
  nonce[9] = 0x4D;
  nonce[10] = 0x44;
  size_t off = 0;
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, key, 128);
  mbedtls_aes_crypt_ctr(&aes, n, &off, nonce, block, p, p);
  mbedtls_aes_free(&aes);
}

size_t sat_cmd_seal(uint8_t *out, size_t cap, const uint8_t key[16], const std::string &target_ble_mac, uint16_t counter,
                    SatPointKind kind, uint8_t idx, uint8_t val, uint32_t nonce) {
  uint8_t t[6];
  if (cap < CMD_LEN || !mac_bytes(target_ble_mac, t)) return 0;
  out[0] = 0xFF;
  out[1] = 0xFF;
  out[2] = 0x43;   // 'C'
  out[3] = CMD_VER;
  w16(out + 4, counter);
  out[6] = 1;
  out[7] = (uint8_t)kind;
  out[8] = idx;
  out[9] = val;
  out[10] = nonce & 0xFF;
  out[11] = (nonce >> 8) & 0xFF;
  out[12] = (nonce >> 16) & 0xFF;
  out[13] = (nonce >> 24) & 0xFF;
  w16(out + 14, crc16(out + 6, 8));
  cmd_ctr(out + 6, CMD_LEN - 6, key, t, counter);
  return CMD_LEN;
}

bool sat_cmd_open(const uint8_t *in, size_t len, const uint8_t key[16], const std::string &own_ble_mac, SatPointKind &kind,
                  uint8_t &idx, uint8_t &val, uint32_t &nonce) {
  uint8_t t[6];
  if (len != CMD_LEN || in[0] != 0xFF || in[1] != 0xFF || in[2] != 0x43 || in[3] != CMD_VER) return false;
  if (!mac_bytes(own_ble_mac, t)) return false;
  uint8_t md[CMD_LEN];
  memcpy(md, in, len);
  cmd_ctr(md + 6, CMD_LEN - 6, key, t, r16(md + 4));
  if (crc16(md + 6, 8) != r16(md + 14)) return false;   // anderes Ziel oder falsches Passwort
  if (md[6] != 1) return false;
  kind = (SatPointKind)md[7];
  idx = md[8];
  val = md[9];
  nonce = (uint32_t)md[10] | ((uint32_t)md[11] << 8) | ((uint32_t)md[12] << 16) | ((uint32_t)md[13] << 24);
  return true;
}
