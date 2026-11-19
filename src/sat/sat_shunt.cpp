// Satellit als SmartShunt – siehe sat_shunt.h und docs/smartshunt.md.
// Konvention: Strom + = Laden, − = Entladen. Messung alle 100 ms im loop(), Zustand alle save_s Sekunden in den NVS.
#include "sat_shunt.h"
#if SAT_SHUNT
#include <Preferences.h>
#include <Wire.h>
#include <math.h>
#include <algorithm>
#include "sat_io.h"

namespace {   // dateilokal (ODR)

// ---------------------------------------------------------------- Einstellungen
struct Cfg {
  uint8_t ver = 1;
  bool on = true;
  int8_t sda = 5, scl = 6;            // I2C (C3 Super Mini: nicht GPIO 9 = BOOT-Taste)
  uint8_t addr = 0x40;
  float shunt_a = 500, shunt_mv = 50; // Nennwerte des externen Shunts
  bool inv = false;                   // Stromrichtung umkehren
  float cap_ah = 200, rated_h = 20;   // Kapazitaet und Bezugszeit (C20)
  float charged_v = 14.0f;            // Vollerkennung: Spannung …
  float tail_pct = 4;                 // … und Schweifstrom in % der Kapazitaet …
  float det_min = 3;                  // … fuer so viele Minuten
  float floor_pct = 10;               // Entladeboden (nur Restzeit/Alarm, keine Abschaltung)
  float peukert = 1.01f, cef_pct = 99;
  float thr_a = 0.10f;                // Stromschwelle (darunter fuer SOC/Anzeige 0)
  float ttg_min = 3;                  // Mittelung der Restzeit
  bool auto_sync = true;
  float i_off = 0, i_gain = 1, v_off = 0, v_gain = 1;
  float al_soc = 20, al_soc_clr = 30; // Alarm SOC (0 = aus)
  float al_vlo = 0, al_vhi = 0;       // Alarm Unter-/Ueberspannung (0 = aus), Hysterese 0,2 V
  uint16_t save_s = 60;
};

// Zustand, der Neustarts uebersteht
struct State {
  uint8_t ver = 1;
  bool soc_valid = false;
  double rem_ah = 0;
  double chg_ah = 0, dis_ah = 0, chg_wh = 0, dis_wh = 0;   // Lebenszeit-Zaehler (tatsaechlich gemessen)
  double since_full_ah = 0;                                // entnommen seit letzter Vollladung (fuer Entladetiefe)
  uint32_t cycles = 0, syncs_auto = 0, syncs_man = 0, full_dis = 0;
  bool cyc_low = false, fd_armed = true;                   // Zyklus: unter 65 %, dann ueber 90 %
  float deepest_ah = 0, last_ah = 0;
  float v_min = NAN, v_max = NAN, i_max_c = 0, i_max_d = 0, p_max_c = 0, p_max_d = 0;
  uint32_t al_count = 0;
};

Cfg s_c;
State s_s;
SemaphoreHandle_t s_mtx = nullptr;
struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};
uint32_t s_ver = 1;

// Messung
bool s_ok = false, s_init = false;
String s_err = "Startet …";
float s_v = NAN, s_i = NAN, s_p = NAN, s_vsh_mv = NAN, s_chip = NAN, s_i_raw = NAN;
double s_i_avg = 0;            // gleitender Mittelwert fuer die Restzeit
bool s_avg_init = false;
uint32_t s_last_us = 0, s_next_ms = 0, s_retry_ms = 0, s_save_ms = 0;
uint32_t s_full_since = 0;     // Vollbedingungen erfuellt seit (0 = nicht)
bool s_sync_armed = true;      // nach einer Synchronisierung erst wieder, wenn die Bedingungen verlassen wurden
uint8_t s_alarms = 0;          // Bit 0 SOC, 1 Unterspannung, 2 Ueberspannung, 3 Messfehler
float s_shown[4] = {NAN, NAN, NAN, NAN};

// ---------------------------------------------------------------- INA228
bool rd(uint8_t reg, uint8_t n, uint32_t &v) {
  Wire.beginTransmission(s_c.addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)s_c.addr, (int)n) != n) return false;
  v = 0;
  for (int i = 0; i < n; i++) v = v << 8 | Wire.read();
  return true;
}
bool wr16(uint8_t reg, uint16_t v) {
  Wire.beginTransmission(s_c.addr);
  Wire.write(reg);
  Wire.write(v >> 8);
  Wire.write(v & 0xFF);
  return Wire.endTransmission() == 0;
}
bool ina_init() {
  if (!sat_i2c_begin(s_c.sda, s_c.scl)) {
    s_err = "I²C startet nicht (Pins prüfen)";
    return false;
  }
  uint32_t man = 0, dev = 0;
  if (!rd(0x3E, 2, man) || !rd(0x3F, 2, dev)) {
    s_err = "INA228 antwortet nicht (Adresse/Verkabelung)";
    return false;
  }
  if (man != 0x5449 || (dev >> 4) != 0x228) {
    s_err = "Kein INA228 an dieser Adresse";
    return false;
  }
  // CONFIG: Reset-Bits 0, ADCRANGE 0 (±163,84 mV); ADC_CONFIG: dauernd Bus+Shunt+Temperatur, je 1052 µs, 16-fach
  if (!wr16(0x00, 0x0000) || !wr16(0x01, 0xF000 | 5 << 9 | 5 << 6 | 5 << 3 | 2)) {
    s_err = "INA228 lässt sich nicht einstellen";
    return false;
  }
  s_err = "";
  return true;
}
// 20-Bit-Wert (Bit 23..4) mit Vorzeichen
int32_t s20(uint32_t v) {
  int32_t x = (int32_t)(v >> 4);
  if (x & 0x80000) x -= 0x100000;
  return x;
}
bool ina_read(float &vbus, float &vsh, float &chip) {
  uint32_t a, b, t;
  if (!rd(0x04, 3, a) || !rd(0x05, 3, b) || !rd(0x06, 2, t)) return false;
  vsh = s20(a) * 312.5e-9f;               // V (Bereich ±163,84 mV)
  vbus = s20(b) * 195.3125e-6f;           // V
  chip = (int16_t)t * 7.8125e-3f;         // °C
  return true;
}

// ---------------------------------------------------------------- Speicher
void cfg_load() {
  Preferences p;
  p.begin("shunt", true);
  Cfg c;
  if (p.getBytesLength("cfg") == sizeof(Cfg)) {
    p.getBytes("cfg", &c, sizeof(Cfg));
    if (c.ver == 1) s_c = c;
  }
  State st;
  if (p.getBytesLength("st") == sizeof(State)) {
    p.getBytes("st", &st, sizeof(State));
    if (st.ver == 1) s_s = st;
  }
  p.end();
}
void cfg_save() {
  Preferences p;
  p.begin("shunt", false);
  p.putBytes("cfg", &s_c, sizeof(Cfg));
  p.end();
}
void st_save() {
  Preferences p;
  p.begin("shunt", false);
  p.putBytes("st", &s_s, sizeof(State));
  p.end();
  s_save_ms = millis();
}

// ---------------------------------------------------------------- Modell
double eff_discharge(double a) {   // Peukert-gewichteter Entladestrom (a > 0)
  if (a <= 0) return 0;
  double ref = s_c.cap_ah / std::max(1.0f, s_c.rated_h);
  return a * pow(std::max(a / ref, 0.01), s_c.peukert - 1.0);
}
void sync_full(bool manual) {
  if (s_s.since_full_ah > 0) {   // abgeschlossene Entladung: Tiefe merken
    s_s.last_ah = (float)s_s.since_full_ah;
    s_s.deepest_ah = std::max(s_s.deepest_ah, s_s.last_ah);
  }
  s_s.since_full_ah = 0;
  s_s.rem_ah = s_c.cap_ah;
  s_s.soc_valid = true;
  (manual ? s_s.syncs_man : s_s.syncs_auto)++;
  st_save();
  s_ver++;
}

void step(float vbus, float vsh, float chip) {
  uint32_t now = micros();
  double dt = s_last_us ? (uint32_t)(now - s_last_us) / 1e6 : 0;
  s_last_us = now;
  if (dt > 5) dt = 0;   // Luecke (z. B. I2C-Fehler): nichts erfinden
  float r = (s_c.shunt_mv / 1000.0f) / std::max(1.0f, s_c.shunt_a);
  float i_raw = vsh / r;
  float i = (i_raw - s_c.i_off) * s_c.i_gain;
  if (s_c.inv) i = -i;
  float v = (vbus - vsh) * s_c.v_gain + s_c.v_off;   // Low-Side: Bus gegen System-Minus, Shunt abziehen
  float p = v * i;
  s_v = v;
  s_i = i;
  s_p = p;
  s_vsh_mv = vsh * 1000;
  s_chip = chip;
  s_i_raw = i_raw;
  if (dt > 0) {   // Zaehler: tatsaechliche Ladung/Energie (ohne Schwelle)
    double h = dt / 3600.0;
    if (i > 0) s_s.chg_ah += i * h, s_s.chg_wh += p * h;
    else s_s.dis_ah += -i * h, s_s.dis_wh += -p * h;
    if (i < 0) s_s.since_full_ah += -i * h;
    // SOC-Modell: Schwelle, Ladewirkungsgrad, Peukert
    double im = fabs(i) < s_c.thr_a ? 0 : i;
    if (s_s.soc_valid) {
      if (im >= 0) s_s.rem_ah += im * (s_c.cef_pct / 100.0) * h;
      else s_s.rem_ah -= eff_discharge(-im) * h;
      s_s.rem_ah = std::max(0.0, std::min((double)s_c.cap_ah, s_s.rem_ah));
    }
    // Mittelwert fuer die Restzeit (Zeitkonstante ttg_min)
    double tau = std::max(1.0f, s_c.ttg_min * 60.0f);
    if (!s_avg_init) s_i_avg = im, s_avg_init = true;
    else s_i_avg += (im - s_i_avg) * std::min(1.0, dt / tau);
  }
  // Historie
  if (isnan(s_s.v_min) || v < s_s.v_min) s_s.v_min = v;
  if (isnan(s_s.v_max) || v > s_s.v_max) s_s.v_max = v;
  if (i > s_s.i_max_c) s_s.i_max_c = i;
  if (-i > s_s.i_max_d) s_s.i_max_d = -i;
  if (p > s_s.p_max_c) s_s.p_max_c = p;
  if (-p > s_s.p_max_d) s_s.p_max_d = -p;
  // Automatische Synchronisierung: Spannung >= Ladespannung und 0 <= I <= Schweifstrom, ununterbrochen det_min
  bool full = v >= s_c.charged_v && i >= 0 && i <= s_c.cap_ah * s_c.tail_pct / 100.0f;
  if (!full) {
    s_full_since = 0;
    s_sync_armed = true;
  } else if (s_c.auto_sync && s_sync_armed) {
    if (!s_full_since) s_full_since = millis() | 1;
    if (millis() - s_full_since >= (uint32_t)(s_c.det_min * 60000)) {
      sync_full(false);
      s_sync_armed = false;
      log_i("SmartShunt: voll erkannt – 100 %%");
    }
  }
  // Zyklen (unter 65 %, dann ueber 90 %) und vollstaendige Entladungen (unter Entladeboden, einmal je Ereignis)
  if (s_s.soc_valid) {
    double soc = 100.0 * s_s.rem_ah / s_c.cap_ah;
    if (soc < 65) s_s.cyc_low = true;
    if (s_s.cyc_low && soc > 90) {
      s_s.cycles++;
      s_s.cyc_low = false;
    }
    if (soc <= s_c.floor_pct && s_s.fd_armed) {
      s_s.full_dis++;
      s_s.fd_armed = false;
    }
    if (soc > s_c.floor_pct + 10) s_s.fd_armed = true;
  }
  // Alarme mit Hysterese
  uint8_t al = s_alarms & ~0x08;
  if (s_c.al_soc > 0 && s_s.soc_valid) {
    double soc = 100.0 * s_s.rem_ah / s_c.cap_ah;
    if (soc < s_c.al_soc) al |= 1;
    else if (soc > std::max(s_c.al_soc_clr, s_c.al_soc)) al &= ~1;
  } else al &= ~1;
  if (s_c.al_vlo > 0) {
    if (v < s_c.al_vlo) al |= 2;
    else if (v > s_c.al_vlo + 0.2f) al &= ~2;
  } else al &= ~2;
  if (s_c.al_vhi > 0) {
    if (v > s_c.al_vhi) al |= 4;
    else if (v < s_c.al_vhi - 0.2f) al &= ~4;
  } else al &= ~4;
  if (al & ~s_alarms) s_s.al_count++;
  if (al != s_alarms) s_ver++;
  s_alarms = al;
}

float soc_pct() { return s_s.soc_valid ? (float)(100.0 * s_s.rem_ah / s_c.cap_ah) : NAN; }
// Restzeit in Stunden bis zum Entladeboden (NAN = keine gueltige Schaetzung: Laden, Ruhe, SOC unbekannt)
float ttg_h() {
  if (!s_s.soc_valid || s_i_avg >= 0 || fabs(s_i_avg) < s_c.thr_a) return NAN;
  double avail = std::max(0.0, s_s.rem_ah - s_c.cap_ah * s_c.floor_pct / 100.0);
  double rate = eff_discharge(-s_i_avg);
  return rate > 0 ? (float)(avail / rate) : NAN;
}
String alarm_text() {
  String t;
  if (s_alarms & 1) t += "Ladezustand niedrig";
  if (s_alarms & 2) t += String(t.length() ? ", " : "") + "Unterspannung";
  if (s_alarms & 4) t += String(t.length() ? ", " : "") + "Überspannung";
  return t;
}

}  // namespace

// ---------------------------------------------------------------- oeffentlich
void sat_shunt_begin() {
  if (!s_mtx) s_mtx = xSemaphoreCreateRecursiveMutex();
  Lock l;
  cfg_load();
  s_init = s_c.on && ina_init();
  log_i("SmartShunt: %s", s_init ? "INA228 bereit" : s_err.c_str());
}

void sat_shunt_loop() {
  if (!s_c.on) return;
  uint32_t now = millis();
  if ((int32_t)(now - s_next_ms) < 0) return;
  s_next_ms = now + 100;
  Lock l;
  if (!s_init) {
    if ((int32_t)(now - s_retry_ms) < 0) return;
    s_retry_ms = now + 5000;
    s_init = ina_init();
    if (!s_init) {
      s_ok = false;
      s_alarms |= 8;
      return;
    }
  }
  float vb, vs, ct;
  if (!ina_read(vb, vs, ct)) {
    s_ok = false;
    s_init = false;
    s_err = "Messung fehlgeschlagen (I²C)";
    s_alarms |= 8;
    s_last_us = 0;   // Luecke nicht integrieren
    s_ver++;
    return;
  }
  if (fabs(vs) > 0.160f) s_err = "Shunt-Messbereich überschritten";   // ADC am Anschlag
  else s_err = "";
  s_ok = true;
  step(vb, vs, ct);
  // sichtbare Aenderung -> neue Version (Senden ans Panel)
  float cur[4] = {roundf(s_v * 100) / 100, roundf(s_i * 10) / 10, roundf(s_p), roundf(soc_pct())};
  for (int k = 0; k < 4; k++)
    if (!(cur[k] == s_shown[k]) && !(isnan(cur[k]) && isnan(s_shown[k]))) {
      memcpy(s_shown, cur, sizeof(cur));
      s_ver++;
      break;
    }
  if (now - s_save_ms >= (uint32_t)std::max<uint16_t>(10, s_c.save_s) * 1000) st_save();
}

uint32_t sat_shunt_version() { return s_ver; }

void sat_shunt_points(std::vector<SatPoint> &out) {
  if (!s_c.on || !s_mtx) return;
  Lock l;
  struct P {
    const char *name;
    uint8_t cls;
    const char *unit;
    float v;
    int dec;
  };
  float ttg = ttg_h();
  P ps[] = {
      {"Spannung", SC_VOLTAGE, "V", s_v, 2},
      {"Strom", SC_CURRENT, "A", s_i, 1},
      {"Leistung", SC_POWER, "W", s_p, 0},
      {"Ladezustand", SC_LEVEL, "%", soc_pct(), 0},
      {"Verbraucht", SC_NONE, "Ah", s_s.soc_valid ? (float)-(s_c.cap_ah - s_s.rem_ah) : NAN, 1},
      {"Restzeit", SC_NONE, "h", ttg, 1},
      {"Geladen gesamt", SC_NONE, "kWh", (float)(s_s.chg_wh / 1000), 2},
      {"Entladen gesamt", SC_NONE, "kWh", (float)(s_s.dis_wh / 1000), 2},
      {"Ladezyklen", SC_NONE, "", (float)s_s.cycles, 0},
      {"Shunt-Temperatur (Chip)", SC_TEMP, "°C", s_chip, 1},
  };
  String al = alarm_text();
  for (int k = 0; k < (int)(sizeof(ps) / sizeof(ps[0])); k++) {
    SatPoint p;
    p.kind = SP_ANALOG;
    p.idx = SHUNT_IDX_BASE + 1 + k;
    p.id = sat_point_id(p.kind, p.idx);
    p.name = std::string("Batterie ") + ps[k].name;
    p.cls = ps[k].cls;
    p.unit = ps[k].unit;
    bool live = k < 4 || k == 9;
    p.ok = (!live || s_ok) && !isnan(ps[k].v);
    float st = powf(10, -ps[k].dec);
    p.value = p.ok ? roundf(ps[k].v / st) * st : NAN;
    if (!p.ok) p.status = (k == 3 && !s_s.soc_valid) ? "unbekannt – einmal voll laden oder setzen" : s_err.c_str();
    if (k == 3 && al.length()) p.status = al.c_str();
    p.age_s = s_ok ? 0 : -1;
    out.push_back(p);
  }
}

void sat_shunt_json(JsonObject o) {
  if (!s_mtx) return;
  Lock l;
  JsonObject c = o["cfg"].to<JsonObject>();
  c["on"] = s_c.on;
  c["sda"] = s_c.sda;
  c["scl"] = s_c.scl;
  c["addr"] = s_c.addr;
  c["shunt_a"] = s_c.shunt_a;
  c["shunt_mv"] = s_c.shunt_mv;
  c["inv"] = s_c.inv;
  c["cap_ah"] = s_c.cap_ah;
  c["rated_h"] = s_c.rated_h;
  c["charged_v"] = s_c.charged_v;
  c["tail_pct"] = s_c.tail_pct;
  c["det_min"] = s_c.det_min;
  c["floor_pct"] = s_c.floor_pct;
  c["peukert"] = s_c.peukert;
  c["cef_pct"] = s_c.cef_pct;
  c["thr_a"] = s_c.thr_a;
  c["ttg_min"] = s_c.ttg_min;
  c["auto_sync"] = s_c.auto_sync;
  c["i_off"] = s_c.i_off;
  c["i_gain"] = s_c.i_gain;
  c["v_off"] = s_c.v_off;
  c["v_gain"] = s_c.v_gain;
  c["al_soc"] = s_c.al_soc;
  c["al_soc_clr"] = s_c.al_soc_clr;
  c["al_vlo"] = s_c.al_vlo;
  c["al_vhi"] = s_c.al_vhi;
  c["save_s"] = s_c.save_s;
  JsonObject m = o["live"].to<JsonObject>();
  m["ok"] = s_ok;
  if (s_err.length()) m["err"] = s_err;
  auto put = [&](const char *k, float v, float sc) {
    if (!isnan(v)) m[k] = roundf(v * sc) / sc;
  };
  put("v", s_v, 1000);
  put("i", s_i, 1000);
  put("p", s_p, 10);
  put("vsh_mv", s_vsh_mv, 10000);
  put("chip", s_chip, 10);
  put("soc", soc_pct(), 10);
  if (s_s.soc_valid) put("rem_ah", (float)s_s.rem_ah, 10);
  put("ttg_h", ttg_h(), 100);
  if (s_alarms) m["alarm"] = alarm_text();
  m["full_s"] = s_full_since ? (millis() - s_full_since) / 1000 : 0;
  JsonObject h = o["hist"].to<JsonObject>();
  h["chg_ah"] = roundf(s_s.chg_ah * 10) / 10;
  h["dis_ah"] = roundf(s_s.dis_ah * 10) / 10;
  h["chg_kwh"] = roundf(s_s.chg_wh / 10) / 100;
  h["dis_kwh"] = roundf(s_s.dis_wh / 10) / 100;
  h["cycles"] = s_s.cycles;
  h["full_dis"] = s_s.full_dis;
  h["syncs_auto"] = s_s.syncs_auto;
  h["syncs_man"] = s_s.syncs_man;
  h["deepest_ah"] = roundf(s_s.deepest_ah * 10) / 10;
  h["last_ah"] = roundf(s_s.last_ah * 10) / 10;
  if (!isnan(s_s.v_min)) h["v_min"] = roundf(s_s.v_min * 100) / 100;
  if (!isnan(s_s.v_max)) h["v_max"] = roundf(s_s.v_max * 100) / 100;
  h["i_max_c"] = roundf(s_s.i_max_c * 10) / 10;
  h["i_max_d"] = roundf(s_s.i_max_d * 10) / 10;
  h["p_max_c"] = roundf(s_s.p_max_c);
  h["p_max_d"] = roundf(s_s.p_max_d);
  h["alarms"] = s_s.al_count;
}

String sat_shunt_set(JsonObjectConst o) {
  Lock l;
  Cfg c = s_c;
  auto f = [&](const char *k, float &v, float lo, float hi) {
    if (o[k].is<float>()) v = std::max(lo, std::min(hi, o[k].as<float>()));
  };
  if (o["on"].is<bool>()) c.on = o["on"];
  if (o["sda"].is<int>()) c.sda = o["sda"];
  if (o["scl"].is<int>()) c.scl = o["scl"];
  if (o["addr"].is<int>()) c.addr = o["addr"];
  if (o["inv"].is<bool>()) c.inv = o["inv"];
  if (o["auto_sync"].is<bool>()) c.auto_sync = o["auto_sync"];
  f("shunt_a", c.shunt_a, 1, 5000);
  f("shunt_mv", c.shunt_mv, 10, 150);
  f("cap_ah", c.cap_ah, 1, 32500);
  f("rated_h", c.rated_h, 1, 100);
  f("charged_v", c.charged_v, 5, 80);
  f("tail_pct", c.tail_pct, 0.5f, 10);
  f("det_min", c.det_min, 0, 100);
  f("floor_pct", c.floor_pct, 0, 99);
  f("peukert", c.peukert, 1.0f, 1.5f);
  f("cef_pct", c.cef_pct, 50, 100);
  f("thr_a", c.thr_a, 0, 2);
  f("ttg_min", c.ttg_min, 0, 12);
  f("i_off", c.i_off, -100, 100);
  f("i_gain", c.i_gain, 0.5f, 1.5f);
  f("v_off", c.v_off, -5, 5);
  f("v_gain", c.v_gain, 0.5f, 1.5f);
  f("al_soc", c.al_soc, 0, 99);
  f("al_soc_clr", c.al_soc_clr, 0, 100);
  f("al_vlo", c.al_vlo, 0, 80);
  f("al_vhi", c.al_vhi, 0, 80);
  if (o["save_s"].is<int>()) c.save_s = std::max(10, std::min(3600, o["save_s"].as<int>()));
  if (c.sda == c.scl || c.sda < 0 || c.scl < 0) return "SDA und SCL brauchen zwei verschiedene Pins";
  if (c.shunt_mv / 1000 > 0.16f) return "Shunt-Spannung zu groß für den INA228 (max. ±163 mV)";
  bool bus = c.sda != s_c.sda || c.scl != s_c.scl || c.addr != s_c.addr || c.on != s_c.on;
  if (c.cap_ah != s_c.cap_ah && s_s.soc_valid) s_s.rem_ah = std::min((double)c.cap_ah, s_s.rem_ah);
  s_c = c;
  cfg_save();
  if (bus) s_init = s_c.on && ina_init();
  s_ver++;
  return "";
}

String sat_shunt_action(JsonObjectConst o) {
  Lock l;
  String a = o["act"] | "";
  if (a == "sync") {
    sync_full(true);
    return "";
  }
  if (a == "soc") {
    float soc = o["soc"] | -1.0f;
    if (soc < 0 || soc > 100) return "Ladezustand 0–100 %";
    s_s.rem_ah = s_c.cap_ah * soc / 100.0;
    s_s.soc_valid = true;
    st_save();
    s_ver++;
    return "";
  }
  if (a == "zero") {   // Nullstrom: nur bei nachweislich stromlosem Shunt
    if (!s_ok || isnan(s_i_raw)) return "Keine Messung";
    if (fabs(s_i_raw) > s_c.shunt_a * 0.02f) return "Zu viel Strom für einen Nullabgleich – alle Verbraucher/Lader trennen";
    s_c.i_off = s_i_raw;
    cfg_save();
    s_ver++;
    return "";
  }
  if (a == "reset_hist") {   // Historie (nicht den Ladezustand)
    State st;
    st.soc_valid = s_s.soc_valid;
    st.rem_ah = s_s.rem_ah;
    s_s = st;
    st_save();
    s_ver++;
    return "";
  }
  return "Unbekannte Aktion";
}

void sat_shunt_for_pins(const std::function<void(int, const String &, bool, bool)> &f) {
  if (!s_c.on) return;
  f(s_c.sda, "SmartShunt I²C SDA", true, false);
  f(s_c.scl, "SmartShunt I²C SCL", true, false);
}
#endif
