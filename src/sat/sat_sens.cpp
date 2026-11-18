// Zusaetzliche Sensoren am Satelliten (siehe sat_sens.h). Schlanke eigene Treiber statt Bibliotheken (Flash).
// Laeuft im loop() des Satelliten (ueber sat_io_loop), kein eigener Task; nur der Durchflusszaehler nutzt einen Interrupt.
#include "sat_sens.h"
#if SAT_SENS
#include <Preferences.h>
#include <Wire.h>
#include <math.h>
#include <algorithm>
#include "sat_io.h"
#include "sat_oled.h"

// Einzelne Typen im Build abschalten (satellite/platformio.ini, [sensors]) – abgeschaltete fehlen auch in der Auswahl
#ifndef SENS_BME280
#define SENS_BME280 1
#endif
#ifndef SENS_SHT
#define SENS_SHT 1
#endif
#ifndef SENS_AHT20
#define SENS_AHT20 1
#endif
#ifndef SENS_SCD4X
#define SENS_SCD4X 1
#endif
#ifndef SENS_INA
#define SENS_INA 1
#endif
#ifndef SENS_DHT22
#define SENS_DHT22 1
#endif
#ifndef SENS_HX711
#define SENS_HX711 1
#endif
#ifndef SENS_US
#define SENS_US 1
#endif
#ifndef SENS_FLOW
#define SENS_FLOW 1
#endif
#ifndef SENS_FLOAT
#define SENS_FLOAT 1
#endif

namespace {
enum SensType : uint8_t {
  ST_NONE = 0, ST_BME280 = 1, ST_BMP280 = 2, ST_SHT3X = 3, ST_SHT4X = 4, ST_AHT20 = 5, ST_SCD4X = 6, ST_INA219 = 7,
  ST_INA226 = 8, ST_DHT22 = 9, ST_HX711 = 10, ST_US = 11, ST_FLOW = 12, ST_OHM = 13, ST_FSW = 14, ST_COUNT = 15
};
struct TypeInfo {
  uint8_t id;
  const char *name;
  bool on;              // im Build enthalten
  bool i2c;
  uint8_t addr;         // Standardadresse
};
const TypeInfo TYPES[] = {
    {ST_BME280, "BME280 (Temperatur, Feuchte, Druck)", SENS_BME280, true, 0x76},
    {ST_BMP280, "BMP280 (Temperatur, Druck)", SENS_BME280, true, 0x76},
    {ST_SHT3X, "SHT3x (Temperatur, Feuchte)", SENS_SHT, true, 0x44},
    {ST_SHT4X, "SHT4x (Temperatur, Feuchte)", SENS_SHT, true, 0x44},
    {ST_AHT20, "AHT20 (Temperatur, Feuchte)", SENS_AHT20, true, 0x38},
    {ST_SCD4X, "SCD40/41 (CO₂, Temperatur, Feuchte)", SENS_SCD4X, true, 0x62},
    {ST_INA219, "INA219 (Spannung, Strom, Leistung)", SENS_INA, true, 0x40},
    {ST_INA226, "INA226 (Spannung, Strom, Leistung)", SENS_INA, true, 0x40},
    {ST_DHT22, "DHT22 (Temperatur, Feuchte)", SENS_DHT22, false, 0},
    {ST_HX711, "HX711 Wägezelle (Gewicht, z. B. Gasflasche)", SENS_HX711, false, 0},
    {ST_US, "Tank: Ultraschall (JSN-SR04T, HC-SR04)", SENS_US, false, 0},
    {ST_FLOW, "Durchfluss (YF-S201, Impulse)", SENS_FLOW, false, 0},
    {ST_OHM, "Tank: Schwimmer mit Widerstandsgeber (Ohm)", SENS_FLOAT, false, 0},
    {ST_FSW, "Tank: Schwimmerschalter voll/leer", SENS_FLOAT, false, 0},
};
const TypeInfo *type_info(uint8_t t) {
  for (auto &x : TYPES)
    if (x.id == t) return &x;
  return nullptr;
}
bool type_ok(uint8_t t) {
  const TypeInfo *ti = type_info(t);
  return ti && ti->on;
}
bool is_tank(uint8_t t) { return t == ST_US || t == ST_OHM || t == ST_FSW; }

struct Cfg {
  uint8_t type = ST_NONE;
  String name;
  int8_t p1 = -1, p2 = -1;   // DHT: Daten; HX711: DT/SCK; Ultraschall: Trig/Echo; Durchfluss: Impuls; Ohm: Analog;
                             // Schwimmerschalter: Voll/Leer
  uint8_t addr = 0;          // I2C (0 = Standard des Typs)
  float a = 0, b = 0, c = 0, d = 0;   // je Typ, siehe defaults()
  bool inv = false;
};
struct All {
  int8_t sda = -1, scl = -1;   // I2C (ohne OLED einstellbar; -1 = Standard des Boards)
  Cfg s[SAT_SENS_MAX];
};
All s_all;

// Vorgaben je Typ (beim Anlegen): a/b/c/d
void defaults(Cfg &c) {
  c.a = c.b = c.c = c.d = 0;
  switch (c.type) {
    case ST_INA219:
    case ST_INA226: c.a = 100; break;                 // Shunt in Milliohm (Module: R100 = 0,1 Ohm)
    case ST_HX711: c.a = 21000; c.d = 11; break;      // Zaehlwerte je kg (grob, kalibrieren!), Inhalt voll (kg)
    case ST_US: c.a = 25; c.b = 100; break;           // Abstand bei voll / leer (cm)
    case ST_FLOW: c.a = 450; break;                   // Impulse je Liter (YF-S201)
    case ST_OHM: c.a = 330; c.b = 240; c.c = 33; break;   // Vorwiderstand, Ohm bei leer, Ohm bei voll (EU-Geber)
    default: break;
  }
}

struct State {
  float v[4] = {NAN, NAN, NAN, NAN};
  bool ok = false;
  String status;
  uint32_t ms = 0;
  bool init = false;
  float raw = NAN;       // Rohwert fuer "uebernehmen" (cm, Ohm, Zaehlwert)
  // BME280-Kalibrierung
  uint16_t T1 = 0, P1 = 0;
  int16_t T2 = 0, T3 = 0, P2 = 0, P3 = 0, P4 = 0, P5 = 0, P6 = 0, P7 = 0, P8 = 0, P9 = 0, H2 = 0, H4 = 0, H5 = 0;
  uint8_t H1 = 0, H3 = 0;
  int8_t H6 = 0;
};
State s_st[SAT_SENS_MAX];
uint32_t s_ver = 1, s_meas_ms = 0, s_fast_ms = 0;
SemaphoreHandle_t s_mtx = nullptr;
struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};

// ---------------------------------------------------------------- I2C
int8_t bus_sda() {
#if SAT_OLED
  return SAT_OLED_SDA;
#else
  return s_all.sda >= 0 ? s_all.sda : SDA;
#endif
}
int8_t bus_scl() {
#if SAT_OLED
  return SAT_OLED_SCL;
#else
  return s_all.scl >= 0 ? s_all.scl : SCL;
#endif
}
bool any_i2c(const All &a) {
  for (auto &c : a.s)
    if (c.type && type_info(c.type) && type_info(c.type)->i2c) return true;
  return false;
}
uint8_t addr_of(const Cfg &c) { return c.addr ? c.addr : (type_info(c.type) ? type_info(c.type)->addr : 0); }
bool i2c_w(uint8_t a, const uint8_t *d, size_t n) {
  Wire.beginTransmission(a);
  Wire.write(d, n);
  return Wire.endTransmission() == 0;
}
bool i2c_cmd16(uint8_t a, uint16_t cmd) {
  uint8_t b[2] = {(uint8_t)(cmd >> 8), (uint8_t)cmd};
  return i2c_w(a, b, 2);
}
bool i2c_r(uint8_t a, uint8_t *d, size_t n) {
  if (Wire.requestFrom((int)a, (int)n) != (int)n) return false;
  for (size_t i = 0; i < n; i++) d[i] = Wire.read();
  return true;
}
bool reg_r(uint8_t a, uint8_t reg, uint8_t *d, size_t n) {
  Wire.beginTransmission(a);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  return i2c_r(a, d, n);
}
bool reg_w8(uint8_t a, uint8_t reg, uint8_t v) {
  uint8_t b[2] = {reg, v};
  return i2c_w(a, b, 2);
}
bool reg_w16(uint8_t a, uint8_t reg, uint16_t v) {
  uint8_t b[3] = {reg, (uint8_t)(v >> 8), (uint8_t)v};
  return i2c_w(a, b, 3);
}
// Sensirion-Pruefsumme (CRC-8, Polynom 0x31, Start 0xFF)
uint8_t crc8(const uint8_t *d) {
  uint8_t c = 0xFF;
  for (int i = 0; i < 2; i++) {
    c ^= d[i];
    for (int b = 0; b < 8; b++) c = c & 0x80 ? (c << 1) ^ 0x31 : c << 1;
  }
  return c;
}
float temp_hum_ok(State &s, float t, float h, float off) {   // Werte setzen, liefert t
  s.v[0] = t + off;
  s.v[1] = h;
  s.ok = true;
  return t;
}

// ---------------------------------------------------------------- Treiber
#if SENS_BME280
bool bme_init(const Cfg &c, State &s) {
  uint8_t a = addr_of(c), id = 0, b[26];
  if (!reg_r(a, 0xD0, &id, 1)) return false;
  if (id != 0x60 && id != 0x58) {
    s.status = "unbekannter Chip " + String(id, HEX);
    return false;
  }
  if (!reg_r(a, 0x88, b, 26)) return false;
  auto u16 = [&](int i) { return (uint16_t)(b[i] | b[i + 1] << 8); };
  s.T1 = u16(0); s.T2 = (int16_t)u16(2); s.T3 = (int16_t)u16(4);
  s.P1 = u16(6); s.P2 = (int16_t)u16(8); s.P3 = (int16_t)u16(10); s.P4 = (int16_t)u16(12); s.P5 = (int16_t)u16(14);
  s.P6 = (int16_t)u16(16); s.P7 = (int16_t)u16(18); s.P8 = (int16_t)u16(20); s.P9 = (int16_t)u16(22);
  s.H1 = b[25];
  if (id == 0x60) {
    uint8_t h[7];
    if (!reg_r(a, 0xE1, h, 7)) return false;
    s.H2 = (int16_t)(h[0] | h[1] << 8);
    s.H3 = h[2];
    s.H4 = (int16_t)((int8_t)h[3] * 16 | (h[4] & 0x0F));
    s.H5 = (int16_t)((int8_t)h[5] * 16 | (h[4] >> 4));
    s.H6 = (int8_t)h[6];
  }
  reg_w8(a, 0xF5, 0x00);   // kein Filter
  return true;
}
// Einzelmessung (Forced Mode): wenig Eigenerwaermung
bool bme_read(const Cfg &c, State &s) {
  uint8_t a = addr_of(c), b[8];
  if (c.type == ST_BME280) reg_w8(a, 0xF2, 0x01);   // Feuchte x1
  reg_w8(a, 0xF4, 0x25);                            // Temp x1, Druck x1, Forced
  delay(12);
  if (!reg_r(a, 0xF7, b, 8)) return false;
  int32_t adc_P = (int32_t)b[0] << 12 | b[1] << 4 | b[2] >> 4, adc_T = (int32_t)b[3] << 12 | b[4] << 4 | b[5] >> 4;
  int32_t adc_H = b[6] << 8 | b[7];
  int32_t v1 = ((((adc_T >> 3) - ((int32_t)s.T1 << 1))) * s.T2) >> 11;
  int32_t v2 = (((((adc_T >> 4) - (int32_t)s.T1) * ((adc_T >> 4) - (int32_t)s.T1)) >> 12) * s.T3) >> 14;
  int32_t t_fine = v1 + v2;
  float t = ((t_fine * 5 + 128) >> 8) / 100.0f;
  int64_t p1 = (int64_t)t_fine - 128000, p2 = p1 * p1 * s.P6;
  p2 += (p1 * s.P5) << 17;
  p2 += (int64_t)s.P4 << 35;
  p1 = ((p1 * p1 * s.P3) >> 8) + ((p1 * s.P2) << 12);
  p1 = ((((int64_t)1) << 47) + p1) * s.P1 >> 33;
  float hpa = NAN;
  if (p1) {
    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - p2) * 3125) / p1;
    int64_t q1 = ((int64_t)s.P9 * (p >> 13) * (p >> 13)) >> 25, q2 = ((int64_t)s.P8 * p) >> 19;
    p = ((p + q1 + q2) >> 8) + ((int64_t)s.P7 << 4);
    hpa = p / 25600.0f;
  }
  float h = NAN;
  if (c.type == ST_BME280) {
    int32_t v = t_fine - 76800;
    v = (((((adc_H << 14) - ((int32_t)s.H4 << 20) - ((int32_t)s.H5 * v)) + 16384) >> 15) *
         (((((((v * (int32_t)s.H6) >> 10) * (((v * (int32_t)s.H3) >> 11) + 32768)) >> 10) + 2097152) * (int32_t)s.H2 + 8192) >> 14));
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)s.H1) >> 4);
    v = v < 0 ? 0 : v > 419430400 ? 419430400 : v;
    h = (v >> 12) / 1024.0f;
  }
  s.v[0] = t + c.a;
  s.v[1] = h;
  s.v[2] = hpa;
  s.ok = true;
  return true;
}
#endif

#if SENS_SHT
bool sht_read(const Cfg &c, State &s) {
  uint8_t a = addr_of(c), b[6];
  if (c.type == ST_SHT3X) {
    if (!i2c_cmd16(a, 0x2400)) return false;   // hohe Wiederholgenauigkeit, ohne Clock-Stretching
    delay(16);
  } else {
    uint8_t cmd = 0xFD;                        // SHT4x: hohe Genauigkeit
    if (!i2c_w(a, &cmd, 1)) return false;
    delay(10);
  }
  if (!i2c_r(a, b, 6) || crc8(b) != b[2] || crc8(b + 3) != b[5]) return false;
  float t = -45 + 175.0f * (b[0] << 8 | b[1]) / 65535.0f;
  float h = c.type == ST_SHT3X ? 100.0f * (b[3] << 8 | b[4]) / 65535.0f : -6 + 125.0f * (b[3] << 8 | b[4]) / 65535.0f;
  temp_hum_ok(s, t, constrain(h, 0.0f, 100.0f), c.a);
  return true;
}
#endif

#if SENS_AHT20
bool aht_read(const Cfg &c, State &s) {
  uint8_t a = addr_of(c), st = 0, b[7];
  if (!i2c_r(a, &st, 1)) return false;
  if (!(st & 0x08)) {   // nicht kalibriert: initialisieren
    uint8_t init[3] = {0xBE, 0x08, 0x00};
    i2c_w(a, init, 3);
    delay(10);
  }
  uint8_t trig[3] = {0xAC, 0x33, 0x00};
  if (!i2c_w(a, trig, 3)) return false;
  delay(80);
  if (!i2c_r(a, b, 7) || (b[0] & 0x80)) return false;   // noch beschaeftigt
  uint32_t hr = (uint32_t)b[1] << 12 | b[2] << 4 | b[3] >> 4, tr = (uint32_t)(b[3] & 0x0F) << 16 | b[4] << 8 | b[5];
  temp_hum_ok(s, tr / 1048576.0f * 200 - 50, hr / 1048576.0f * 100, c.a);
  return true;
}
#endif

#if SENS_SCD4X
bool scd_init(const Cfg &c, State &) {
  uint8_t a = addr_of(c);
  i2c_cmd16(a, 0x3F86);   // laufende Messung stoppen (falls von vorher)
  delay(500);
  return i2c_cmd16(a, 0x21B1);   // Messung alle 5 s
}
bool scd_read(const Cfg &c, State &s) {
  uint8_t a = addr_of(c), r[3], b[9];
  if (!i2c_cmd16(a, 0xE4B8)) return false;   // Daten bereit?
  delay(1);
  if (!i2c_r(a, r, 3) || crc8(r) != r[2]) return false;
  if (!((r[0] << 8 | r[1]) & 0x07FF)) return s.ok;   // noch kein neuer Wert: alten behalten
  if (!i2c_cmd16(a, 0xEC05)) return false;
  delay(1);
  if (!i2c_r(a, b, 9) || crc8(b) != b[2] || crc8(b + 3) != b[5] || crc8(b + 6) != b[8]) return false;
  s.v[0] = b[0] << 8 | b[1];
  s.v[1] = -45 + 175.0f * (b[3] << 8 | b[4]) / 65536.0f + c.a;
  s.v[2] = 100.0f * (b[6] << 8 | b[7]) / 65536.0f;
  s.ok = true;
  return true;
}
#endif

#if SENS_INA
bool ina_init(const Cfg &c, State &) {
  uint8_t a = addr_of(c);
  // INA219: 32 V, +-320 mV, 12 Bit, dauernd; INA226: 16-fach gemittelt, 1,1 ms, dauernd
  return reg_w16(a, 0x00, c.type == ST_INA219 ? 0x399F : 0x4527);
}
bool ina_read(const Cfg &c, State &s) {
  uint8_t a = addr_of(c), b[2];
  if (!reg_r(a, 0x01, b, 2)) return false;
  int16_t sh = (int16_t)(b[0] << 8 | b[1]);
  if (!reg_r(a, 0x02, b, 2)) return false;
  uint16_t bus = b[0] << 8 | b[1];
  float v = c.type == ST_INA219 ? (bus >> 3) * 0.004f : bus * 0.00125f;
  float vsh = c.type == ST_INA219 ? sh * 10e-6f : sh * 2.5e-6f;
  float ohm = (c.a > 0 ? c.a : 100) / 1000.0f;
  float amp = vsh / ohm * (c.inv ? -1 : 1);
  s.v[0] = v;
  s.v[1] = amp;
  s.v[2] = v * amp;
  s.ok = true;
  return true;
}
#endif

#if SENS_DHT22
// DHT22: Start (Leitung 1-2 ms Low), dann 40 Bit; Laenge des High-Pulses = Bitwert. Zeitkritisch (~5 ms gesperrt).
bool dht_read(const Cfg &c, State &s) {
  static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  int pin = c.p1;
  uint8_t d[5] = {0};
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(2);
  bool ok = true;
  auto wait = [&](int level, uint32_t max_us) -> int32_t {
    uint32_t t0 = micros();
    while (digitalRead(pin) == level)
      if (micros() - t0 > max_us) return -1;
    return micros() - t0;
  };
  portENTER_CRITICAL(&mux);
  pinMode(pin, INPUT_PULLUP);
  if (wait(HIGH, 100) < 0 || wait(LOW, 100) < 0 || wait(HIGH, 100) < 0) ok = false;
  for (int i = 0; ok && i < 40; i++) {
    if (wait(LOW, 80) < 0) {
      ok = false;
      break;
    }
    int32_t hi = wait(HIGH, 100);
    if (hi < 0) {
      ok = false;
      break;
    }
    if (hi > 40) d[i / 8] |= 0x80 >> (i % 8);
  }
  portEXIT_CRITICAL(&mux);
  if (!ok || ((d[0] + d[1] + d[2] + d[3]) & 0xFF) != d[4]) return false;
  float h = (d[0] << 8 | d[1]) / 10.0f, t = ((d[2] & 0x7F) << 8 | d[3]) / 10.0f;
  if (d[2] & 0x80) t = -t;
  temp_hum_ok(s, t, h, c.a);
  return true;
}
#endif

#if SENS_HX711
// HX711 Kanal A, Verstaerkung 128: 24 Bit, danach ein Takt. SCK darf nicht laenger als 60 us High sein.
bool hx_raw(const Cfg &c, int32_t &out) {
  static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  uint32_t t0 = millis();
  while (digitalRead(c.p1) == HIGH)
    if (millis() - t0 > 200) return false;   // nicht bereit
  uint32_t v = 0;
  portENTER_CRITICAL(&mux);
  for (int i = 0; i < 25; i++) {
    digitalWrite(c.p2, HIGH);
    delayMicroseconds(1);
    if (i < 24) v = v << 1 | (digitalRead(c.p1) ? 1 : 0);
    digitalWrite(c.p2, LOW);
    delayMicroseconds(1);
  }
  portEXIT_CRITICAL(&mux);
  out = (int32_t)(v << 8) >> 8;   // 24 Bit mit Vorzeichen
  return true;
}
bool hx_read(const Cfg &c, State &s) {
  int32_t r[5];
  int n = 0;
  for (int i = 0; i < 5; i++)
    if (hx_raw(c, r[n])) n++;
  if (!n) return false;
  std::sort(r, r + n);
  s.raw = r[n / 2];
  float kg = c.a ? (s.raw - c.b) / c.a : NAN;
  if (c.d > 0) {   // Gasflasche o. ae.: Inhalt = Gewicht - Leergewicht
    float net = kg - c.c;
    s.v[0] = net;
    s.v[1] = constrain(net / c.d * 100, 0.0f, 100.0f);
  } else {
    s.v[0] = kg;
    s.v[1] = NAN;
  }
  s.ok = !isnan(kg);
  return true;
}
#endif

#if SENS_US
// Ultraschall: Trig 20 us High, Echo-Pulsdauer / 58 = cm. Median aus 3 Messungen.
bool us_read(const Cfg &c, State &s) {
  float m[3];
  int n = 0;
  for (int i = 0; i < 3; i++) {
    digitalWrite(c.p1, LOW);
    delayMicroseconds(4);
    digitalWrite(c.p1, HIGH);
    delayMicroseconds(20);
    digitalWrite(c.p1, LOW);
    unsigned long us = pulseIn(c.p2, HIGH, 30000);
    if (us) m[n++] = us / 58.0f;
    delay(60);
  }
  if (!n) {
    s.status = "kein Echo";
    return false;
  }
  std::sort(m, m + n);
  s.raw = m[n / 2];
  float span = c.b - c.a;
  s.v[0] = span ? constrain((c.b - s.raw) / span * 100, 0.0f, 100.0f) : NAN;
  s.status = String(s.raw, 0) + " cm";
  s.ok = !isnan(s.v[0]);
  return true;
}
#endif

#if SENS_FLOAT
// Widerstandsgeber: 3,3 V – Vorwiderstand – Pin – Geber – GND. R = Rv * U / (3,3 V - U)
bool ohm_read(const Cfg &c, State &s) {
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(c.p1);
  mv /= 8;
  if (mv >= 3290) {
    s.status = "offen (Geber nicht angeschlossen?)";
    return false;
  }
  s.raw = (c.a > 0 ? c.a : 330) * mv / (3300.0f - mv);
  float span = c.c - c.b;
  s.v[0] = span ? constrain((s.raw - c.b) / span * 100, 0.0f, 100.0f) : NAN;
  s.status = String(s.raw, 0) + " Ω";
  s.ok = !isnan(s.v[0]);
  return true;
}
// Schwimmerschalter: Voll-Schalter meldet "oben erreicht", Leer-Schalter "unten unterschritten" (Kontakt = Low;
// inv dreht das um). Ergebnis 100 / 50 / 0 %.
bool fsw_read(const Cfg &c, State &s) {
  bool full = c.p1 >= 0 && (digitalRead(c.p1) == (c.inv ? HIGH : LOW));
  bool empty = c.p2 >= 0 && (digitalRead(c.p2) == (c.inv ? HIGH : LOW));
  if (full && empty) {
    s.status = "Schalter widersprechen sich";
    s.v[0] = NAN;
    s.ok = false;
    return true;
  }
  s.v[0] = full ? 100 : empty ? 0 : 50;
  s.status = full ? "voll" : empty ? "leer" : "dazwischen";
  s.ok = true;
  return true;
}
#endif

#if SENS_FLOW
volatile uint32_t s_pulses[SAT_SENS_MAX];
double s_liters[SAT_SENS_MAX];          // gesamt (in NVS gesichert)
uint32_t s_flow_last[SAT_SENS_MAX], s_flow_ms[SAT_SENS_MAX];
double s_saved[SAT_SENS_MAX];
uint32_t s_save_ms = 0;
void IRAM_ATTR flow_isr(void *arg) { s_pulses[(uintptr_t)arg]++; }
bool flow_read(int i, const Cfg &c, State &s) {
  uint32_t now = millis(), p = s_pulses[i], dp = p - s_flow_last[i];
  float ppl = c.a > 0 ? c.a : 450;
  float dt = s_flow_ms[i] ? (now - s_flow_ms[i]) / 60000.0f : 0;
  s_flow_last[i] = p;
  s_flow_ms[i] = now;
  s_liters[i] += dp / ppl;
  s.v[0] = dt > 0 ? dp / ppl / dt : 0;   // l/min
  s.v[1] = s_liters[i];
  s.ok = true;
  return true;
}
void flow_save(bool force) {
  if (!force && millis() - s_save_ms < 60000) return;
  s_save_ms = millis();
  Preferences p;
  bool open = false;
  for (int i = 0; i < SAT_SENS_MAX; i++) {
    if (s_all.s[i].type != ST_FLOW || fabs(s_liters[i] - s_saved[i]) < 0.05) continue;
    if (!open) open = p.begin("satsens", false);
    if (!open) return;
    p.putDouble(("fl" + String(i)).c_str(), s_liters[i]);
    s_saved[i] = s_liters[i];
  }
  if (open) p.end();
}
#endif

// ---------------------------------------------------------------- Einrichten und Messen
void detach_all() {
#if SENS_FLOW
  for (int i = 0; i < SAT_SENS_MAX; i++)
    if (s_all.s[i].type == ST_FLOW && s_all.s[i].p1 >= 0) detachInterrupt(s_all.s[i].p1);
#endif
}
void setup_all() {
  bool i2c = any_i2c(s_all);
  if (i2c && !sat_i2c_begin(bus_sda(), bus_scl())) log_w("Sensoren: I2C startet nicht");
  for (int i = 0; i < SAT_SENS_MAX; i++) {
    Cfg &c = s_all.s[i];
    State &s = s_st[i];
    s = State();
    if (!c.type || !type_ok(c.type)) continue;
    switch (c.type) {
      case ST_HX711:
        pinMode(c.p1, INPUT);
        pinMode(c.p2, OUTPUT);
        digitalWrite(c.p2, LOW);
        break;
      case ST_US:
        pinMode(c.p1, OUTPUT);
        digitalWrite(c.p1, LOW);
        pinMode(c.p2, INPUT);
        break;
      case ST_FLOW:
#if SENS_FLOW
        pinMode(c.p1, INPUT_PULLUP);
        s_flow_last[i] = s_pulses[i];
        s_flow_ms[i] = millis();
        attachInterruptArg(c.p1, flow_isr, (void *)(uintptr_t)i, FALLING);
#endif
        break;
      case ST_OHM: analogSetPinAttenuation(c.p1, ADC_11db); break;
      case ST_FSW:
        if (c.p1 >= 0) pinMode(c.p1, INPUT_PULLUP);
        if (c.p2 >= 0) pinMode(c.p2, INPUT_PULLUP);
        break;
      default: break;
    }
  }
}

bool measure(int i) {
  Cfg &c = s_all.s[i];
  State &s = s_st[i];
  if (!c.type || !type_ok(c.type)) return false;
  bool ok = false;
  // I2C-Sensoren beim ersten Mal (und nach Fehlern) einrichten
  if (!s.init) {
    s.init = true;
    switch (c.type) {
#if SENS_BME280
      case ST_BME280:
      case ST_BMP280: s.init = bme_init(c, s); break;
#endif
#if SENS_SCD4X
      case ST_SCD4X: s.init = scd_init(c, s); break;
#endif
#if SENS_INA
      case ST_INA219:
      case ST_INA226: s.init = ina_init(c, s); break;
#endif
      default: break;
    }
    if (!s.init) {
      if (s.status.isEmpty()) s.status = "antwortet nicht (Adresse 0x" + String(addr_of(c), HEX) + ")";
      s.ok = false;
      return true;
    }
  }
  switch (c.type) {
#if SENS_BME280
    case ST_BME280:
    case ST_BMP280: ok = bme_read(c, s); break;
#endif
#if SENS_SHT
    case ST_SHT3X:
    case ST_SHT4X: ok = sht_read(c, s); break;
#endif
#if SENS_AHT20
    case ST_AHT20: ok = aht_read(c, s); break;
#endif
#if SENS_SCD4X
    case ST_SCD4X: ok = scd_read(c, s); break;
#endif
#if SENS_INA
    case ST_INA219:
    case ST_INA226: ok = ina_read(c, s); break;
#endif
#if SENS_DHT22
    case ST_DHT22: ok = dht_read(c, s); break;
#endif
#if SENS_HX711
    case ST_HX711: ok = hx_read(c, s); break;
#endif
#if SENS_US
    case ST_US: ok = us_read(c, s); break;
#endif
#if SENS_FLOAT
    case ST_OHM: ok = ohm_read(c, s); break;
    case ST_FSW: ok = fsw_read(c, s); break;
#endif
#if SENS_FLOW
    case ST_FLOW: ok = flow_read(i, c, s); break;
#endif
    default: break;
  }
  if (ok) {
    s.ms = millis();
    if (!is_tank(c.type)) s.status = "";
  } else {
    s.ok = false;
    if (s.status.isEmpty() || s.status.endsWith("cm") || s.status.endsWith("Ω"))
      s.status = type_info(c.type)->i2c ? "antwortet nicht (Adresse 0x" + String(addr_of(c), HEX) + ")" : "keine Antwort";
    if (type_info(c.type)->i2c) s.init = false;   // beim naechsten Mal neu einrichten
  }
  return true;
}

// Werte eines Steckplatzes: Bezeichnung, Klasse, Einheit (bis zu 4)
struct Val {
  const char *suffix;
  uint8_t cls;
  const char *unit;
  uint8_t dec;
};
int values_of(const Cfg &c, Val *v) {
  switch (c.type) {
    case ST_BME280: v[0] = {"Temperatur", SC_TEMP, "°C", 1}; v[1] = {"Feuchte", SC_HUMIDITY, "%", 0}; v[2] = {"Luftdruck", SC_PRESSURE, "hPa", 0}; return 3;
    case ST_BMP280: v[0] = {"Temperatur", SC_TEMP, "°C", 1}; v[1] = {nullptr, 0, "", 0}; v[2] = {"Luftdruck", SC_PRESSURE, "hPa", 0}; return 3;
    case ST_SHT3X:
    case ST_SHT4X:
    case ST_AHT20:
    case ST_DHT22: v[0] = {"Temperatur", SC_TEMP, "°C", 1}; v[1] = {"Feuchte", SC_HUMIDITY, "%", 0}; return 2;
    case ST_SCD4X: v[0] = {"CO₂", SC_CO2, "ppm", 0}; v[1] = {"Temperatur", SC_TEMP, "°C", 1}; v[2] = {"Feuchte", SC_HUMIDITY, "%", 0}; return 3;
    case ST_INA219:
    case ST_INA226: v[0] = {"Spannung", SC_VOLTAGE, "V", 2}; v[1] = {"Strom", SC_CURRENT, "A", 2}; v[2] = {"Leistung", SC_POWER, "W", 0}; return 3;
    case ST_HX711:
      v[0] = {c.d > 0 ? "Inhalt" : "Gewicht", SC_WEIGHT, "kg", 1};
      v[1] = c.d > 0 ? Val{"Füllstand", SC_LEVEL, "%", 0} : Val{nullptr, 0, "", 0};
      return 2;
    case ST_FLOW: v[0] = {"Durchfluss", SC_FLOW, "l/min", 1}; v[1] = {"Verbrauch", SC_VOLUME, "l", 1}; return 2;
    default: return 0;
  }
}

void cfg_to_json(JsonObject o, const All &a) {
  o["sda"] = a.sda;
  o["scl"] = a.scl;
  JsonArray l = o["list"].to<JsonArray>();
  for (auto &c : a.s) {
    JsonObject x = l.add<JsonObject>();
    x["type"] = c.type;
    if (!c.type) continue;
    x["name"] = c.name;
    x["p1"] = c.p1;
    x["p2"] = c.p2;
    x["addr"] = c.addr;
    x["a"] = c.a;
    x["b"] = c.b;
    x["c"] = c.c;
    x["d"] = c.d;
    x["inv"] = c.inv;
  }
}
void cfg_from_json(JsonObjectConst o, All &a) {
  a.sda = o["sda"] | -1;
  a.scl = o["scl"] | -1;
  JsonArrayConst l = o["list"];
  for (int i = 0; i < SAT_SENS_MAX; i++) {
    Cfg &c = a.s[i];
    c = Cfg();
    if (i >= (int)l.size()) continue;
    JsonObjectConst x = l[i];
    c.type = x["type"] | 0;
    if (!c.type) continue;
    c.name = (const char *)(x["name"] | "");
    c.name = c.name.substring(0, 32);
    c.p1 = x["p1"] | -1;
    c.p2 = x["p2"] | -1;
    c.addr = x["addr"] | 0;
    if (x["a"].isNull()) defaults(c);   // neu angelegt: Vorgaben des Typs
    else {
      c.a = x["a"] | 0.0f;
      c.b = x["b"] | 0.0f;
      c.c = x["c"] | 0.0f;
      c.d = x["d"] | 0.0f;
    }
    c.inv = x["inv"] | false;
  }
}
void cfg_save() {
  JsonDocument d;
  cfg_to_json(d.to<JsonObject>(), s_all);
  String s;
  serializeJson(d, s);
  Preferences p;
  if (!p.begin("satsens", false)) return;
  p.putString("cfg", s);
  p.end();
}

String validate(const All &a) {
  for (int i = 0; i < SAT_SENS_MAX; i++) {
    const Cfg &c = a.s[i];
    if (!c.type) continue;
    String n = "Sensor " + String(i + 1);
    if (!type_ok(c.type)) return n + ": dieser Typ ist in dieser Firmware nicht enthalten";
    switch (c.type) {
      case ST_DHT22:
      case ST_FLOW:
      case ST_OHM:
        if (c.p1 < 0) return n + ": bitte den GPIO wählen";
        break;
      case ST_HX711:
      case ST_US:
        if (c.p1 < 0 || c.p2 < 0) return n + ": bitte beide GPIOs wählen";
        break;
      case ST_FSW:
        if (c.p1 < 0 && c.p2 < 0) return n + ": bitte mindestens einen Schalter (voll oder leer) wählen";
        break;
      default: break;
    }
  }
  int ms = -1, mc = -1;
  if (any_i2c(a) && sat_io_mpu_pins(ms, mc)) {
#if !SAT_OLED
    int sda = a.sda >= 0 ? a.sda : SDA, scl = a.scl >= 0 ? a.scl : SCL;
    if (ms != sda || mc != scl)
      return "I²C-Sensoren und MPU-6050 bitte an dieselben Pins (der Satellit hat einen I²C-Bus): MPU an SDA " + String(ms) +
             ", SCL " + String(mc);
#endif
  }
  // Pins gegen die Anschluesse (GPIO-Karte) und untereinander pruefen
  String err, used[64];
  auto use = [&](int pin, const String &what, bool, bool) {
    if (pin < 0 || !err.isEmpty() || pin >= 64) return;
    String o = sat_io_pin_owner(pin);
    if (!o.isEmpty()) err = "GPIO " + String(pin) + " ist schon belegt: " + o + " und " + what;
    else if (!used[pin].isEmpty()) err = "GPIO " + String(pin) + " ist doppelt belegt: " + used[pin] + " und " + what;
    else used[pin] = what;
  };
  for (int i = 0; i < SAT_SENS_MAX; i++) {
    const Cfg &c = a.s[i];
    String n = c.name.length() ? c.name : "Sensor " + String(i + 1);
    if (!c.type || (type_info(c.type) && type_info(c.type)->i2c)) continue;
    use(c.p1, n, true, c.type == ST_OHM);
    use(c.p2, n, true, false);
  }
#if !SAT_OLED
  if (any_i2c(a) && !sat_io_mpu_pins(ms, mc)) {   // eigene I2C-Pins (ohne MPU-6050)
    use(a.sda >= 0 ? a.sda : SDA, "Sensoren I²C SDA", true, false);
    use(a.scl >= 0 ? a.scl : SCL, "Sensoren I²C SCL", true, false);
  }
#endif
  return err;
}
}  // namespace

// ---------------------------------------------------------------- oeffentlich
void sat_sens_begin() {
  if (!s_mtx) s_mtx = xSemaphoreCreateRecursiveMutex();
  Preferences p;
  if (p.begin("satsens", true)) {
    String s = p.getString("cfg", "");
#if SENS_FLOW
    for (int i = 0; i < SAT_SENS_MAX; i++) s_liters[i] = s_saved[i] = p.getDouble(("fl" + String(i)).c_str(), 0);
#endif
    p.end();
    JsonDocument d;
    if (!s.isEmpty() && !deserializeJson(d, s)) cfg_from_json(d.as<JsonObjectConst>(), s_all);
  }
  setup_all();
}

void sat_sens_loop() {
  uint32_t now = millis();
  bool any = false;
  for (auto &c : s_all.s) any |= c.type != ST_NONE;
  if (!any) return;
  // alle 10 s messen (Schwimmerschalter jede Sekunde)
  if (now - s_fast_ms >= 1000) {
    s_fast_ms = now;
    Lock l;
    for (int i = 0; i < SAT_SENS_MAX; i++)
      if (s_all.s[i].type == ST_FSW) {
        float old = s_st[i].v[0];
        measure(i);
        if (!(old == s_st[i].v[0])) s_ver++;
      }
  }
  if (s_meas_ms && now - s_meas_ms < 10000) return;
  s_meas_ms = now ? now : 1;
  Lock l;
  for (int i = 0; i < SAT_SENS_MAX; i++)
    if (s_all.s[i].type && s_all.s[i].type != ST_FSW) measure(i);
  s_ver++;
#if SENS_FLOW
  flow_save(false);
#endif
}

uint32_t sat_sens_version() { return s_ver; }

void sat_sens_points(std::vector<SatPoint> &out) {
  Lock l;
  for (int i = 0; i < SAT_SENS_MAX; i++) {
    const Cfg &c = s_all.s[i];
    const State &s = s_st[i];
    if (!c.type || !type_ok(c.type)) continue;
    String tn = type_info(c.type)->name;
    String base = c.name.length() ? c.name : is_tank(c.type) ? "Tank " + String(SENS_TANK_BASE + 1 + i) : tn.substring(0, tn.indexOf(' '));
    int32_t age = s.ms ? (int32_t)((millis() - s.ms) / 1000) : -1;
    if (is_tank(c.type)) {
      SatPoint p;
      p.kind = SP_TANK;
      p.idx = SENS_TANK_BASE + 1 + i;
      p.id = sat_point_id(p.kind, p.idx);
      p.name = base.c_str();
      p.cls = SC_LEVEL;
      p.unit = "%";
      p.ok = s.ok && !isnan(s.v[0]);
      p.value = p.ok ? roundf(s.v[0]) : NAN;
      p.status = s.status.c_str();
      p.age_s = age;
      out.push_back(p);
      continue;
    }
    Val v[4];
    int n = values_of(c, v);
    for (int j = 0; j < n; j++) {
      if (!v[j].suffix) continue;
      SatPoint p;
      p.kind = SP_ANALOG;
      p.idx = SENS_IDX_BASE + 1 + i * 4 + j;
      p.id = sat_point_id(p.kind, p.idx);
      p.name = (base + " " + v[j].suffix).c_str();
      p.cls = v[j].cls;
      p.unit = v[j].unit;
      p.ok = s.ok && !isnan(s.v[j]);
      float st = powf(10, -(int)v[j].dec);
      p.value = p.ok ? roundf(s.v[j] / st) * st : NAN;
      if (!p.ok) p.status = s.status.c_str();
      p.age_s = age;
      out.push_back(p);
    }
  }
}

void sat_sens_json(JsonObject o) {
  Lock l;
  cfg_to_json(o, s_all);
  o["oled"] = (bool)SAT_OLED;
  o["bus_sda"] = bus_sda();
  o["bus_scl"] = bus_scl();
  JsonArray t = o["types"].to<JsonArray>();
  for (auto &x : TYPES) {
    if (!x.on) continue;
    JsonObject y = t.add<JsonObject>();
    y["id"] = x.id;
    y["name"] = x.name;
    y["i2c"] = x.i2c;
    y["addr"] = x.addr;
  }
  JsonArray live = o["live"].to<JsonArray>();
  for (int i = 0; i < SAT_SENS_MAX; i++) {
    JsonObject y = live.add<JsonObject>();
    const State &s = s_st[i];
    y["ok"] = s.ok;
    if (!s.status.isEmpty()) y["status"] = s.status;
    if (!isnan(s.raw)) y["raw"] = s.raw;
    JsonArray v = y["v"].to<JsonArray>();
    for (float x : s.v) {
      if (isnan(x)) v.add(nullptr);
      else v.add(roundf(x * 100) / 100);
    }
  }
}

String sat_sens_set(JsonObjectConst o) {
  All a;
  cfg_from_json(o, a);
  String err = validate(a);
  if (!err.isEmpty()) return err;
  Lock l;
#if SENS_FLOW
  flow_save(true);
#endif
  detach_all();
  s_all = a;
  cfg_save();
  setup_all();
  s_meas_ms = 0;   // gleich messen
  s_ver++;
  return "";
}

// {"slot":n,"act":"tare"|"cal"|"full"|"empty"|"reset","kg":x}
String sat_sens_action(JsonObjectConst o) {
  int i = o["slot"] | -1;
  String act = o["act"] | "";
  if (i < 0 || i >= SAT_SENS_MAX || !s_all.s[i].type) return "Sensor unbekannt";
  Lock l;
  Cfg &c = s_all.s[i];
  State &s = s_st[i];
  measure(i);
  if (act == "reset") {
#if SENS_FLOW
    s_liters[i] = 0;
    flow_save(true);
#endif
  } else if (isnan(s.raw)) {
    return "Noch kein Messwert – Anschluss prüfen";
  } else if (act == "tare") {
    c.b = s.raw;   // HX711: leer = 0 kg
  } else if (act == "cal") {
    float kg = o["kg"] | 0.0f;
    if (kg <= 0) return "Bitte das bekannte Gewicht in kg angeben";
    if (s.raw == c.b) return "Erst „Tara“ ohne Last, dann das Gewicht auflegen";
    c.a = (s.raw - c.b) / kg;
  } else if (act == "full") {
    (c.type == ST_US ? c.a : c.c) = s.raw;   // Ultraschall: Abstand bei voll; Widerstandsgeber: Ohm bei voll
  } else if (act == "empty") {
    c.b = s.raw;   // Abstand bzw. Ohm bei leer
  } else {
    return "Unbekannte Aktion";
  }
  cfg_save();
  measure(i);
  s_ver++;
  return "";
}

void sat_sens_for_pins(const std::function<void(int, const String &, bool, bool)> &f) {
  bool i2c = false;
  for (int i = 0; i < SAT_SENS_MAX; i++) {
    const Cfg &c = s_all.s[i];
    if (!c.type || !type_info(c.type)) continue;
    if (type_info(c.type)->i2c) {
      i2c = true;
      continue;
    }
    String n = c.name.length() ? c.name : "Sensor " + String(i + 1);
    f(c.p1, n, true, c.type == ST_OHM);
    f(c.p2, n, true, false);
  }
#if !SAT_OLED
  int ms, mc;
  if (i2c && !sat_io_mpu_pins(ms, mc)) {   // I2C-Pins, sofern nicht schon der MPU-6050 sie belegt
    f(bus_sda(), "Sensoren I²C SDA", true, false);
    f(bus_scl(), "Sensoren I²C SCL", true, false);
  }
#else
  (void)i2c;
#endif
}
#endif
