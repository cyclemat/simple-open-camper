#include "sat_io.h"
#include "../hymer.h"
#include <Preferences.h>
#include <Wire.h>
#include <math.h>
#include <algorithm>
#include "driver/gpio.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ---------------------------------------------------------------- GPIOs je Chip
// Nur Pins, die am Modul herausgefuehrt und frei sind (ohne Flash/PSRAM/USB). adc = ADC1 (ADC2 geht nicht mit WLAN).
struct PinInfo {
  uint8_t pin;
  bool out, adc, pull;
  const char *note;
};
#define SAT_PIN_MAX 48   // hoechste GPIO-Nummer aller unterstuetzten Chips
#if defined(CONFIG_IDF_TARGET_ESP32C3)
// ESP32-C3 (Super Mini, DevKitM-1). GPIO 11–17 Flash, 18/19 USB.
static const char *CHIP_NAME = "ESP32-C3";
static const PinInfo PINS[] = {
    {0, true, true, true, nullptr},
    {1, true, true, true, nullptr},
    {2, true, true, true, "Startpin – beim Start nicht auf Masse ziehen"},
    {3, true, true, true, nullptr},
    {4, true, true, true, nullptr},
    {5, true, false, true, nullptr},
    {6, true, false, true, nullptr},
    {7, true, false, true, nullptr},
    {8, true, false, true, "Startpin, beim Super Mini die LED – beim Start nicht auf Masse ziehen"},
    {9, true, false, true, "Boot-Taste – beim Start nicht auf Masse ziehen"},
    {10, true, false, true, nullptr},
    {20, true, false, true, "UART0 RX (Log läuft über USB, daher frei)"},
    {21, true, false, true, "UART0 TX (Log läuft über USB, daher frei)"},
};
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
// ESP32-S3 (DevKitC-1). GPIO 19/20 USB, 26–32 Flash, 33–37 bei Octal-PSRAM (N8R8, N16R8) belegt.
static const char *CHIP_NAME = "ESP32-S3";
static const PinInfo PINS[] = {
    {0, true, false, true, "Boot-Taste – beim Start nicht auf Masse ziehen"},
    {1, true, true, true, nullptr},
    {2, true, true, true, nullptr},
    {3, true, true, true, "Startpin (JTAG-Auswahl)"},
    {4, true, true, true, nullptr},
    {5, true, true, true, nullptr},
    {6, true, true, true, nullptr},
    {7, true, true, true, nullptr},
    {8, true, true, true, nullptr},
    {9, true, true, true, nullptr},
    {10, true, true, true, nullptr},
    {11, true, false, true, nullptr},
    {12, true, false, true, nullptr},
    {13, true, false, true, nullptr},
    {14, true, false, true, nullptr},
    {15, true, false, true, nullptr},
    {16, true, false, true, nullptr},
    {17, true, false, true, nullptr},
    {18, true, false, true, nullptr},
    {21, true, false, true, nullptr},
    {38, true, false, true, "beim DevKitC-1 v1.1 die RGB-LED"},
    {39, true, false, true, "JTAG (MTCK)"},
    {40, true, false, true, "JTAG (MTDO)"},
    {41, true, false, true, "JTAG (MTDI)"},
    {42, true, false, true, "JTAG (MTMS)"},
    {43, true, false, true, "UART0 TX – hier läuft das Log"},
    {44, true, false, true, "UART0 RX – hier läuft das Log"},
    {45, true, false, true, "Startpin – beim Start nicht High ziehen"},
    {46, true, false, true, "Startpin – beim Start nicht High ziehen"},
    {47, true, false, true, nullptr},
    {48, true, false, true, "beim DevKitC-1 v1.0 die RGB-LED"},
};
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
// ESP32-C6 (DevKitC-1, Super Mini). GPIO 10/11 nicht am Modul, 12/13 USB, 24–30 Flash.
static const char *CHIP_NAME = "ESP32-C6";
static const PinInfo PINS[] = {
    {0, true, true, true, nullptr},
    {1, true, true, true, nullptr},
    {2, true, true, true, nullptr},
    {3, true, true, true, nullptr},
    {4, true, true, true, "Startpin"},
    {5, true, true, true, "Startpin"},
    {6, true, true, true, nullptr},
    {7, true, false, true, nullptr},
    {8, true, false, true, "Startpin, beim DevKitC-1 die RGB-LED – beim Start nicht auf Masse ziehen"},
    {9, true, false, true, "Boot-Taste – beim Start nicht auf Masse ziehen"},
    {14, true, false, true, nullptr},
    {15, true, false, true, "Startpin"},
    {16, true, false, true, "UART0 TX (Log läuft über USB, daher frei)"},
    {17, true, false, true, "UART0 RX (Log läuft über USB, daher frei)"},
    {18, true, false, true, nullptr},
    {19, true, false, true, nullptr},
    {20, true, false, true, nullptr},
    {21, true, false, true, nullptr},
    {22, true, false, true, nullptr},
    {23, true, false, true, nullptr},
};
#else
// ESP32 klassisch (esp32dev, WROOM). GPIO 6–11 Flash.
static const char *CHIP_NAME = "ESP32";
static const PinInfo PINS[] = {
    {0, true, false, true, "Boot-Taste – beim Start nicht auf Masse ziehen"},
    {2, true, false, true, "Startpin, bei manchen Boards die LED"},
    {4, true, false, true, nullptr},
    {5, true, false, true, "Startpin (beim Start kurz High)"},
    {12, true, false, true, "Startpin – beim Start nicht High ziehen"},
    {13, true, false, true, nullptr},
    {14, true, false, true, "beim Start kurz Takt"},
    {15, true, false, true, "Startpin (beim Start kurz High)"},
    {16, true, false, true, "bei WROVER-Modulen belegt (PSRAM)"},
    {17, true, false, true, "bei WROVER-Modulen belegt (PSRAM)"},
    {18, true, false, true, nullptr},
    {19, true, false, true, nullptr},
    {21, true, false, true, "üblich für I²C SDA"},
    {22, true, false, true, "üblich für I²C SCL"},
    {23, true, false, true, nullptr},
    {25, true, false, true, nullptr},
    {26, true, false, true, nullptr},
    {27, true, false, true, nullptr},
    {32, true, true, true, nullptr},
    {33, true, true, true, nullptr},
    {34, false, true, false, "nur Eingang, kein interner Pull-up"},
    {35, false, true, false, "nur Eingang, kein interner Pull-up"},
    {36, false, true, false, "nur Eingang, kein interner Pull-up (VP)"},
    {39, false, true, false, "nur Eingang, kein interner Pull-up (VN)"},
};
#endif
static const PinInfo *pin_info(int pin) {
  for (auto &p : PINS)
    if (p.pin == pin) return &p;
  return nullptr;
}
// "GPIO 32–39" bzw. "GPIO 0, 1, 2, 3, 4" – fuer Fehlermeldungen
static String adc_pins_text() {
  String s;
  for (auto &p : PINS)
    if (p.adc) s += (s.isEmpty() ? "GPIO " : ", ") + String(p.pin);
  return s;
}

// ---------------------------------------------------------------- Einstellungen
struct TankCfg {
  String name;
  uint8_t mode = 0;          // 0 aus, 1 je Sonde ein GPIO, 2 Widerstandsleiter an einem Analog-Pin
  int8_t pins[SAT_IO_PROBES];
  uint8_t pct[SAT_IO_PROBES];
  uint16_t mv[SAT_IO_PROBES];
  int8_t gnd = -1;           // optional: GPIO an der gemeinsamen Stange (nur beim Messen auf Masse)
  bool inv = false;          // Kontakt = High (Module mit Transistor)
  int8_t apin = -1;          // Analog-Pin (Modus 2)
  uint16_t mv_empty = 0;     // Modus 2: Spannung ohne Kontakt
  TankCfg() {
    for (int i = 0; i < SAT_IO_PROBES; i++) {
      pins[i] = -1;
      pct[i] = (uint8_t)((i + 1) * 20);
      mv[i] = 0;
    }
  }
};
struct DsCfg {
  String addr;   // 16 Hex-Zeichen, leer = frei
  String name;
  float off = 0;
};
struct AnCfg {
  String name, unit;
  int8_t pin = -1;
  uint8_t cls = SC_NONE;
  uint16_t mv0 = 0, mv1 = 3000;
  float v0 = 0, v1 = 100;
  uint8_t dec = 1;
};
struct SwCfg {
  String name;
  int8_t pin = -1;
  bool low = false;     // aktiv Low (viele Relaismodule)
  uint8_t boot = 0;     // nach dem Start: 0 aus, 1 an, 2 wie zuletzt
};
struct InCfg {
  String name;
  int8_t pin = -1;
  bool pull = true;
  bool inv = false;
};
struct MpuCfg {
  bool on = false;
  int8_t sda = SDA, scl = SCL;   // Standard-I2C-Pins des Boards (esp32dev 21/22, C3 und S3 8/9)
  uint8_t addr = 0x68;
  bool swap = false, inv_x = false, inv_y = false;
  float zx = 0, zy = 0;
  // gegen Wackeln: Zeitfilter, Totband, nur bei Stillstand messen
  float smooth = 2.0f;     // Sekunden (0 = aus)
  float dead = 0.2f;       // Grad: kleinere Aenderungen werden nicht gemeldet
  bool still = true;       // waehrend Bewegung (Gyro) den letzten Wert halten
  float still_dps = 3.0f;  // ab dieser Drehrate (Grad/s) gilt "bewegt sich"
};
struct IoCfg {
  uint16_t interval = 10;      // Sensoren (s)
  uint16_t tank_interval = 60; // Tanksonden (s)
  TankCfg tanks[SAT_IO_TANKS];
  int8_t ow_pin = -1;
  DsCfg ds[SAT_IO_DS];
  AnCfg an[SAT_IO_ANALOG];
  MpuCfg mpu;
  SwCfg sw[SAT_IO_SWITCHES];
  InCfg in[SAT_IO_INPUTS];
};
static IoCfg s_cfg;

// ---------------------------------------------------------------- Zustand
struct TankState {
  bool ok = false;
  int pct = 0;
  uint8_t probes = 0, n = 0;
  bool odd = false;
  int mv = -1;
  uint32_t ms = 0;
};
struct DsState {
  bool ok = false;
  float t = NAN;
  uint32_t ms = 0;
};
struct AnState {
  bool ok = false;
  int mv = -1;
  float v = NAN;
  uint32_t ms = 0;
};
static TankState s_tank[SAT_IO_TANKS];
static DsState s_ds[SAT_IO_DS];
static AnState s_an[SAT_IO_ANALOG];
static bool s_mpu_ok = false;
static float s_raw_x = NAN, s_raw_y = NAN, s_tilt_x = NAN, s_tilt_y = NAN;
static float s_med_x[5], s_med_y[5];   // letzte Rohwerte fuer den Median
static uint8_t s_med_n = 0, s_med_i = 0;
static bool s_moving = false;
static uint32_t s_moving_ms = 0;
static float s_gyro = 0;
static uint32_t s_mpu_ms = 0;
static bool s_sw[SAT_IO_SWITCHES];
static bool s_in[SAT_IO_INPUTS];
static uint8_t s_in_cnt[SAT_IO_INPUTS];
static uint32_t s_nonce = 0, s_sw_ms = 0;
static volatile uint32_t s_ver = 1;
static std::vector<String> s_ds_found;
static String s_mpu_err;

static SemaphoreHandle_t s_mtx = nullptr;
struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};

// ---------------------------------------------------------------- 1-Wire (DS18B20), eigene Umsetzung ohne Bibliothek
static int s_ow = -1;
static portMUX_TYPE s_ow_mux = portMUX_INITIALIZER_UNLOCKED;

static inline void ow_low() { gpio_set_level((gpio_num_t)s_ow, 0); }
static inline void ow_rel() { gpio_set_level((gpio_num_t)s_ow, 1); }
static inline int ow_rd() { return gpio_get_level((gpio_num_t)s_ow); }

static void ow_setup(int pin) {
  s_ow = pin;
  if (pin < 0) return;
  gpio_reset_pin((gpio_num_t)pin);
  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT_OUTPUT_OD);
  gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_ONLY);
  gpio_set_level((gpio_num_t)pin, 1);
}

static bool ow_reset() {
  if (s_ow < 0) return false;
  ow_low();
  delayMicroseconds(480);
  portENTER_CRITICAL(&s_ow_mux);
  ow_rel();
  delayMicroseconds(70);
  bool present = !ow_rd();
  portEXIT_CRITICAL(&s_ow_mux);
  delayMicroseconds(410);
  return present;
}

static void ow_wbit(bool b) {
  portENTER_CRITICAL(&s_ow_mux);
  ow_low();
  if (b) {
    delayMicroseconds(6);
    ow_rel();
    delayMicroseconds(64);
  } else {
    delayMicroseconds(60);
    ow_rel();
    delayMicroseconds(10);
  }
  portEXIT_CRITICAL(&s_ow_mux);
}

static bool ow_rbit() {
  portENTER_CRITICAL(&s_ow_mux);
  ow_low();
  delayMicroseconds(3);
  ow_rel();
  delayMicroseconds(10);
  bool r = ow_rd();
  portEXIT_CRITICAL(&s_ow_mux);
  delayMicroseconds(53);
  return r;
}

static void ow_write(uint8_t v) {
  for (int i = 0; i < 8; i++) ow_wbit((v >> i) & 1);
}
static uint8_t ow_read() {
  uint8_t v = 0;
  for (int i = 0; i < 8; i++)
    if (ow_rbit()) v |= 1 << i;
  return v;
}

static uint8_t crc8(const uint8_t *d, int n) {
  uint8_t c = 0;
  while (n--) {
    uint8_t b = *d++;
    for (int i = 0; i < 8; i++) {
      uint8_t mix = (c ^ b) & 1;
      c >>= 1;
      if (mix) c ^= 0x8C;
      b >>= 1;
    }
  }
  return c;
}

static String rom_hex(const uint8_t *r) {
  char b[17];
  for (int i = 0; i < 8; i++) snprintf(b + 2 * i, 3, "%02x", r[i]);
  return b;
}
static bool rom_parse(const String &s, uint8_t *r) {
  if (s.length() != 16) return false;
  for (int i = 0; i < 8; i++) r[i] = (uint8_t)strtoul(s.substring(2 * i, 2 * i + 2).c_str(), nullptr, 16);
  return crc8(r, 7) == r[7];
}

// Alle Fuehler am Bus suchen (Maxim-Suchverfahren)
static std::vector<String> ow_search() {
  std::vector<String> out;
  uint8_t rom[8] = {0};
  int last_disc = 0;
  for (int round = 0; round < 16; round++) {
    if (!ow_reset()) break;
    ow_write(0xF0);
    int last_zero = 0;
    bool fail = false;
    for (int bit = 1; bit <= 64; bit++) {
      bool a = ow_rbit(), b = ow_rbit();
      if (a && b) {
        fail = true;
        break;
      }
      bool dir;
      if (a != b) dir = a;
      else {
        if (bit < last_disc) dir = (rom[(bit - 1) / 8] >> ((bit - 1) % 8)) & 1;
        else dir = bit == last_disc;
        if (!dir) last_zero = bit;
      }
      if (dir) rom[(bit - 1) / 8] |= 1 << ((bit - 1) % 8);
      else rom[(bit - 1) / 8] &= ~(1 << ((bit - 1) % 8));
      ow_wbit(dir);
    }
    if (fail) break;
    if (crc8(rom, 7) == rom[7] && (rom[0] == 0x28 || rom[0] == 0x10 || rom[0] == 0x22)) out.push_back(rom_hex(rom));
    last_disc = last_zero;
    if (!last_disc) break;
  }
  return out;
}

static void ds_convert_all() {
  if (!ow_reset()) return;
  ow_write(0xCC);   // alle
  ow_write(0x44);   // messen
}

static bool ds_read(const uint8_t *rom, float &t) {
  if (!ow_reset()) return false;
  ow_write(0x55);
  for (int i = 0; i < 8; i++) ow_write(rom[i]);
  ow_write(0xBE);
  uint8_t b[9];
  for (int i = 0; i < 9; i++) b[i] = ow_read();
  if (crc8(b, 8) != b[8]) return false;
  int16_t raw = (int16_t)(b[1] << 8 | b[0]);
  t = rom[0] == 0x10 ? raw / 2.0f : raw / 16.0f;
  return t > -56 && t < 126;
}

// ---------------------------------------------------------------- MPU-6050 (GY-521)
static bool s_wire = false;

static bool mpu_write(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(s_cfg.mpu.addr);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

static void mpu_setup() {
  if (s_wire) {
    Wire.end();
    s_wire = false;
  }
  s_mpu_ok = false;
  s_raw_x = s_raw_y = s_tilt_x = s_tilt_y = NAN;
  s_med_n = s_med_i = 0;
  s_moving = false;
  if (!s_cfg.mpu.on || s_cfg.mpu.sda < 0 || s_cfg.mpu.scl < 0) return;
  s_wire = Wire.begin(s_cfg.mpu.sda, s_cfg.mpu.scl, 100000);
  if (!s_wire) {
    s_mpu_err = "I²C startet nicht";
    return;
  }
  // aufwecken (Takt vom Gyro), Tiefpass 5 Hz gegen Vibration, +-2 g, Gyro +-250 Grad/s
  s_mpu_ok = mpu_write(0x6B, 0x01) && mpu_write(0x1A, 0x06) && mpu_write(0x1C, 0x00) && mpu_write(0x1B, 0x00);
  s_mpu_err = s_mpu_ok ? "" : "MPU-6050 antwortet nicht – Verdrahtung und Adresse prüfen";
}

// Neigung aus der Schwerkraft und Drehrate (Grad/s) aus dem Gyro
static bool mpu_read(float &x, float &y, float &dps) {
  Wire.beginTransmission(s_cfg.mpu.addr);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)s_cfg.mpu.addr, 14) != 14) return false;
  uint8_t b[14];
  for (int i = 0; i < 14; i++) b[i] = Wire.read();
  auto w = [&](int i) { return (int16_t)((b[i] << 8) | b[i + 1]); };
  float fx = w(0), fy = w(2), fz = w(4);
  float gx = w(8) / 131.0f, gy = w(10) / 131.0f, gz = w(12) / 131.0f;
  dps = sqrtf(gx * gx + gy * gy + gz * gz);
  if (fx == 0 && fy == 0 && fz == 0) return false;
  // Neigung aus der Schwerkraft: laengs um die Y-Achse, quer um die X-Achse
  x = atan2f(fx, sqrtf(fy * fy + fz * fz)) * 57.2958f;
  y = atan2f(fy, sqrtf(fx * fx + fz * fz)) * 57.2958f;
  return true;
}

// ---------------------------------------------------------------- Messen
static int adc_mv(int pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogReadMilliVolts(pin);
  return (int)(sum / 16);
}

static bool tank_used(const TankCfg &t) {
  if (t.mode == 1)
    for (int i = 0; i < SAT_IO_PROBES; i++)
      if (t.pins[i] >= 0) return true;
  return t.mode == 2 && t.apin >= 0;
}

static void tank_measure(int i) {
  const TankCfg &c = s_cfg.tanks[i];
  TankState st;
  st.ms = millis();
  if (c.mode == 1) {
    // Sonden nur jetzt bestromen: Masse-Stange auf Low, Sonden mit Pull-up (bzw. Pull-down bei "Kontakt = High")
    if (c.gnd >= 0) {
      pinMode(c.gnd, OUTPUT);
      digitalWrite(c.gnd, LOW);
    }
    for (int p = 0; p < SAT_IO_PROBES; p++)
      if (c.pins[p] >= 0) pinMode(c.pins[p], c.inv ? INPUT_PULLDOWN : INPUT_PULLUP);
    delay(5);
    uint8_t votes[SAT_IO_PROBES] = {0};
    for (int r = 0; r < 3; r++) {
      for (int p = 0; p < SAT_IO_PROBES; p++)
        if (c.pins[p] >= 0 && (digitalRead(c.pins[p]) == (c.inv ? HIGH : LOW))) votes[p]++;
      delay(2);
    }
    for (int p = 0; p < SAT_IO_PROBES; p++)
      if (c.pins[p] >= 0) pinMode(c.pins[p], INPUT);   // stromlos bis zur naechsten Messung
    if (c.gnd >= 0) pinMode(c.gnd, INPUT);
    for (int p = 0; p < SAT_IO_PROBES; p++) {
      if (c.pins[p] < 0) continue;
      st.n++;
      if (votes[p] >= 2) st.probes |= 1 << p;
    }
    st.ok = st.n > 0;
  } else if (c.mode == 2 && c.apin >= 0) {
    st.mv = adc_mv(c.apin);
    // naechstliegende Stufe der Widerstandsleiter
    int best = -1, best_d = abs(st.mv - (int)c.mv_empty);
    for (int p = 0; p < SAT_IO_PROBES; p++) {
      if (!c.pct[p] || !c.mv[p]) continue;
      st.n++;
      int d = abs(st.mv - (int)c.mv[p]);
      if (d < best_d) {
        best_d = d;
        best = p;
      }
    }
    // Kontakt an der getroffenen Stufe und allen niedrigeren
    if (best >= 0)
      for (int p = 0; p < SAT_IO_PROBES; p++)
        if (c.pct[p] && c.mv[p] && c.pct[p] <= c.pct[best]) st.probes |= 1 << p;
    st.ok = st.n > 0;
  }
  // hoechste Stufe mit Kontakt; unplausibel, wenn eine hoehere Sonde nass und eine niedrigere trocken ist
  for (int p = 0; p < SAT_IO_PROBES; p++)
    if (st.probes & (1 << p)) st.pct = max(st.pct, (int)c.pct[p]);
  for (int a = 0; a < SAT_IO_PROBES; a++)
    for (int b = 0; b < SAT_IO_PROBES; b++) {
      bool used_b = c.mode == 1 ? c.pins[b] >= 0 : (c.pct[b] && c.mv[b]);
      if ((st.probes & (1 << a)) && used_b && !(st.probes & (1 << b)) && c.pct[b] < c.pct[a]) st.odd = true;
    }
  Lock l;
  bool changed = st.ok != s_tank[i].ok || st.pct != s_tank[i].pct || st.probes != s_tank[i].probes || st.odd != s_tank[i].odd;
  s_tank[i] = st;
  if (changed) s_ver++;
}

static void sensors_measure_analog() {
  for (int i = 0; i < SAT_IO_ANALOG; i++) {
    const AnCfg &c = s_cfg.an[i];
    if (c.pin < 0) continue;
    int mv = adc_mv(c.pin);
    float v = c.mv1 != c.mv0 ? c.v0 + (mv - (int)c.mv0) * (c.v1 - c.v0) / ((int)c.mv1 - (int)c.mv0) : NAN;
    Lock l;
    AnState &s = s_an[i];
    float old = s.v;
    if (!isnan(v) && !isnan(s.v)) v = s.v * 0.5f + v * 0.5f;   // etwas glaetten
    s.ok = !isnan(v);
    s.mv = mv;
    s.v = v;
    s.ms = millis();
    float step = powf(10, -(int)c.dec);
    if (isnan(old) != isnan(v) || fabsf(roundf(old / step) - roundf(v / step)) >= 1) s_ver++;
  }
}

// ---------------------------------------------------------------- Pins anwenden
static void apply_pins() {
  for (int i = 0; i < SAT_IO_SWITCHES; i++) {
    const SwCfg &c = s_cfg.sw[i];
    if (c.pin < 0) continue;
    pinMode(c.pin, OUTPUT);
    digitalWrite(c.pin, (s_sw[i] != c.low) ? HIGH : LOW);
  }
  for (int i = 0; i < SAT_IO_INPUTS; i++) {
    const InCfg &c = s_cfg.in[i];
    if (c.pin < 0) continue;
    const PinInfo *pi = pin_info(c.pin);
    pinMode(c.pin, c.pull && pi && pi->pull ? INPUT_PULLUP : INPUT);
    s_in[i] = (digitalRead(c.pin) == LOW) != c.inv;   // Kontakt gegen Masse = an
    s_in_cnt[i] = 0;
  }
  for (int i = 0; i < SAT_IO_TANKS; i++) {
    const TankCfg &c = s_cfg.tanks[i];
    if (c.mode == 1)
      for (int p = 0; p < SAT_IO_PROBES; p++)
        if (c.pins[p] >= 0) pinMode(c.pins[p], INPUT);
    if (c.gnd >= 0) pinMode(c.gnd, INPUT);
    s_tank[i] = TankState();
  }
  ow_setup(s_cfg.ow_pin);
  for (auto &d : s_ds) d = DsState();
  for (auto &a : s_an) a = AnState();
  mpu_setup();
  s_ver++;
}

static void save_switch_states() {
  uint8_t bits = 0;
  for (int i = 0; i < SAT_IO_SWITCHES; i++)
    if (s_sw[i]) bits |= 1 << i;
  Preferences p;
  p.begin("satio", false);
  if (p.getUChar("sw", 0) != bits) p.putUChar("sw", bits);
  p.end();
}

// ---------------------------------------------------------------- Einstellungen <-> JSON
static void cfg_to_json(JsonObject o) {
  o["interval"] = s_cfg.interval;
  o["tank_interval"] = s_cfg.tank_interval;
  JsonArray ta = o["tanks"].to<JsonArray>();
  for (auto &t : s_cfg.tanks) {
    JsonObject x = ta.add<JsonObject>();
    x["name"] = t.name;
    x["mode"] = t.mode;
    JsonArray pins = x["pins"].to<JsonArray>(), pct = x["pct"].to<JsonArray>(), mv = x["mv"].to<JsonArray>();
    for (int i = 0; i < SAT_IO_PROBES; i++) {
      pins.add(t.pins[i]);
      pct.add(t.pct[i]);
      mv.add(t.mv[i]);
    }
    x["gnd"] = t.gnd;
    x["inv"] = t.inv;
    x["apin"] = t.apin;
    x["mv_empty"] = t.mv_empty;
  }
  o["ow_pin"] = s_cfg.ow_pin;
  JsonArray da = o["ds"].to<JsonArray>();
  for (auto &d : s_cfg.ds) {
    JsonObject x = da.add<JsonObject>();
    x["addr"] = d.addr;
    x["name"] = d.name;
    x["off"] = d.off;
  }
  JsonArray aa = o["analog"].to<JsonArray>();
  for (auto &a : s_cfg.an) {
    JsonObject x = aa.add<JsonObject>();
    x["name"] = a.name;
    x["pin"] = a.pin;
    x["class"] = sat_class_name(a.cls);
    x["unit"] = a.unit;
    x["mv0"] = a.mv0;
    x["v0"] = a.v0;
    x["mv1"] = a.mv1;
    x["v1"] = a.v1;
    x["dec"] = a.dec;
  }
  JsonObject m = o["mpu"].to<JsonObject>();
  m["on"] = s_cfg.mpu.on;
  m["sda"] = s_cfg.mpu.sda;
  m["scl"] = s_cfg.mpu.scl;
  m["addr"] = s_cfg.mpu.addr;
  m["swap"] = s_cfg.mpu.swap;
  m["inv_x"] = s_cfg.mpu.inv_x;
  m["inv_y"] = s_cfg.mpu.inv_y;
  m["zx"] = s_cfg.mpu.zx;
  m["zy"] = s_cfg.mpu.zy;
  m["smooth"] = s_cfg.mpu.smooth;
  m["dead"] = s_cfg.mpu.dead;
  m["still"] = s_cfg.mpu.still;
  m["still_dps"] = s_cfg.mpu.still_dps;
  JsonArray sa = o["switches"].to<JsonArray>();
  for (auto &s : s_cfg.sw) {
    JsonObject x = sa.add<JsonObject>();
    x["name"] = s.name;
    x["pin"] = s.pin;
    x["low"] = s.low;
    x["boot"] = s.boot;
  }
  JsonArray ia = o["inputs"].to<JsonArray>();
  for (auto &s : s_cfg.in) {
    JsonObject x = ia.add<JsonObject>();
    x["name"] = s.name;
    x["pin"] = s.pin;
    x["pull"] = s.pull;
    x["inv"] = s.inv;
  }
}

static int8_t pin_of(JsonVariantConst v) {
  int p = v | -1;
  return p < 0 || p > SAT_PIN_MAX ? -1 : (int8_t)p;
}

static void cfg_from_json(JsonObjectConst o, IoCfg &c) {
  c.interval = constrain((int)(o["interval"] | 10), 2, 3600);
  c.tank_interval = constrain((int)(o["tank_interval"] | 60), 2, 65535);
  JsonArrayConst ta = o["tanks"];
  for (int i = 0; i < SAT_IO_TANKS && i < (int)ta.size(); i++) {
    JsonObjectConst x = ta[i];
    TankCfg &t = c.tanks[i];
    t.name = String((const char *)(x["name"] | "")).substring(0, 32);
    t.mode = constrain((int)(x["mode"] | 0), 0, 2);
    for (int p = 0; p < SAT_IO_PROBES; p++) {
      t.pins[p] = pin_of(x["pins"][p]);
      t.pct[p] = constrain((int)(x["pct"][p] | 0), 0, 100);
      t.mv[p] = constrain((int)(x["mv"][p] | 0), 0, 3300);
    }
    t.gnd = pin_of(x["gnd"]);
    t.inv = x["inv"] | false;
    t.apin = pin_of(x["apin"]);
    t.mv_empty = constrain((int)(x["mv_empty"] | 0), 0, 3300);
  }
  c.ow_pin = pin_of(o["ow_pin"]);
  JsonArrayConst da = o["ds"];
  for (int i = 0; i < SAT_IO_DS && i < (int)da.size(); i++) {
    c.ds[i].addr = (const char *)(da[i]["addr"] | "");
    c.ds[i].addr.toLowerCase();
    c.ds[i].name = String((const char *)(da[i]["name"] | "")).substring(0, 32);
    c.ds[i].off = constrain(da[i]["off"] | 0.0f, -10.0f, 10.0f);
  }
  JsonArrayConst aa = o["analog"];
  for (int i = 0; i < SAT_IO_ANALOG && i < (int)aa.size(); i++) {
    JsonObjectConst x = aa[i];
    AnCfg &a = c.an[i];
    a.name = String((const char *)(x["name"] | "")).substring(0, 32);
    a.pin = pin_of(x["pin"]);
    a.cls = sat_class_of(x["class"] | "");
    a.unit = String((const char *)(x["unit"] | "")).substring(0, 8);
    a.mv0 = constrain((int)(x["mv0"] | 0), 0, 3300);
    a.mv1 = constrain((int)(x["mv1"] | 3000), 0, 3300);
    a.v0 = x["v0"] | 0.0f;
    a.v1 = x["v1"] | 100.0f;
    a.dec = constrain((int)(x["dec"] | 1), 0, 3);
  }
  JsonObjectConst m = o["mpu"];
  c.mpu.on = m["on"] | false;
  c.mpu.sda = pin_of(m["sda"]);
  c.mpu.scl = pin_of(m["scl"]);
  c.mpu.addr = (m["addr"] | 0x68) == 0x69 ? 0x69 : 0x68;
  c.mpu.swap = m["swap"] | false;
  c.mpu.inv_x = m["inv_x"] | false;
  c.mpu.inv_y = m["inv_y"] | false;
  c.mpu.zx = m["zx"] | 0.0f;
  c.mpu.zy = m["zy"] | 0.0f;
  c.mpu.smooth = constrain(m["smooth"] | 2.0f, 0.0f, 60.0f);
  c.mpu.dead = constrain(m["dead"] | 0.2f, 0.0f, 5.0f);
  c.mpu.still = m["still"] | true;
  c.mpu.still_dps = constrain(m["still_dps"] | 3.0f, 0.5f, 50.0f);
  JsonArrayConst sa = o["switches"];
  for (int i = 0; i < SAT_IO_SWITCHES && i < (int)sa.size(); i++) {
    c.sw[i].name = String((const char *)(sa[i]["name"] | "")).substring(0, 32);
    c.sw[i].pin = pin_of(sa[i]["pin"]);
    c.sw[i].low = sa[i]["low"] | false;
    c.sw[i].boot = constrain((int)(sa[i]["boot"] | 0), 0, 2);
  }
  JsonArrayConst ia = o["inputs"];
  for (int i = 0; i < SAT_IO_INPUTS && i < (int)ia.size(); i++) {
    c.in[i].name = String((const char *)(ia[i]["name"] | "")).substring(0, 32);
    c.in[i].pin = pin_of(ia[i]["pin"]);
    c.in[i].pull = ia[i]["pull"] | true;
    c.in[i].inv = ia[i]["inv"] | false;
  }
}

// Jeder GPIO nur einmal, passende Faehigkeiten
static String validate(const IoCfg &c) {
  String used[SAT_PIN_MAX + 1];
  String err;
  auto use = [&](int pin, const String &what, bool out, bool adc) {
    if (pin < 0 || !err.isEmpty()) return;
    const PinInfo *pi = pin_info(pin);
    if (!pi) {
      err = "GPIO " + String(pin) + " (" + what + ") ist am " + CHIP_NAME + " nicht nutzbar";
      return;
    }
    if (out && !pi->out) {
      err = "GPIO " + String(pin) + " kann nur Eingang sein – nicht für " + what;
      return;
    }
    if (adc && !pi->adc) {
      err = what + ": GPIO " + String(pin) + " hat keinen nutzbaren Analogeingang (nur " + adc_pins_text() + ")";
      return;
    }
    if (!used[pin].isEmpty()) {
      err = "GPIO " + String(pin) + " ist doppelt belegt: " + used[pin] + " und " + what;
      return;
    }
    used[pin] = what;
  };
  for (int i = 0; i < SAT_IO_TANKS; i++) {
    const TankCfg &t = c.tanks[i];
    String n = "Tank " + String(i + 1);
    if (t.mode == 1) {
      for (int p = 0; p < SAT_IO_PROBES; p++) use(t.pins[p], n + " Sonde " + String(p + 1), false, false);
      use(t.gnd, n + " Masse", true, false);
    } else if (t.mode == 2) {
      use(t.apin, n + " Analog", false, true);
    }
  }
  use(c.ow_pin, "DS18B20-Bus", true, false);
  for (int i = 0; i < SAT_IO_ANALOG; i++) use(c.an[i].pin, "Analog " + String(i + 1), false, true);
  if (c.mpu.on) {
    use(c.mpu.sda, "MPU-6050 SDA", true, false);
    use(c.mpu.scl, "MPU-6050 SCL", true, false);
  }
  for (int i = 0; i < SAT_IO_SWITCHES; i++) use(c.sw[i].pin, "Schalter " + String(i + 1), true, false);
  for (int i = 0; i < SAT_IO_INPUTS; i++) use(c.in[i].pin, "Eingang " + String(i + 1), false, false);
  return err;
}

static void cfg_save() {
  JsonDocument d;
  cfg_to_json(d.to<JsonObject>());
  String s;
  serializeJson(d, s);
  Preferences p;
  p.begin("satio", false);
  p.putString("cfg", s);
  p.end();
}

// ---------------------------------------------------------------- oeffentlich
void sat_io_begin() {
  if (!s_mtx) s_mtx = xSemaphoreCreateRecursiveMutex();
  s_nonce = esp_random();
  Preferences p;
  p.begin("satio", true);
  String s = p.getString("cfg", "");
  uint8_t bits = p.getUChar("sw", 0);
  p.end();
  if (!s.isEmpty()) {
    JsonDocument d;
    if (!deserializeJson(d, s)) {
      IoCfg c;
      cfg_from_json(d.as<JsonObjectConst>(), c);
      if (validate(c).isEmpty()) s_cfg = c;
    }
  }
  for (int i = 0; i < SAT_IO_SWITCHES; i++) {
    uint8_t b = s_cfg.sw[i].boot;
    s_sw[i] = b == 1 || (b == 2 && (bits & (1 << i)));
  }
  apply_pins();
}

uint32_t sat_io_version() { return s_ver + hymer_version() * 101; }
uint32_t sat_io_last_switch_ms() { return s_sw_ms; }

bool sat_io_has_switches() {
  if (hymer_has_switches()) return true;
  for (auto &s : s_cfg.sw)
    if (s.pin >= 0) return true;
  return false;
}

bool sat_io_switch(uint8_t idx, uint8_t val) {
  if (hymer_is_idx(idx)) {   // Licht/Schalter im Hymer (per Bluetooth an der SCU)
    if (!hymer_switch(idx, val)) return false;
    Lock l;
    s_nonce = esp_random();
    s_sw_ms = millis();
    s_ver++;
    return true;
  }
  if (idx < 1 || idx > SAT_IO_SWITCHES) return false;
  int i = idx - 1;
  {
    Lock l;
    const SwCfg &c = s_cfg.sw[i];
    if (c.pin < 0) return false;
    bool on = val == 2 ? !s_sw[i] : val == 1;
    s_sw[i] = on;
    digitalWrite(c.pin, (on != c.low) ? HIGH : LOW);
    s_nonce = esp_random();   // jede Schaltung macht alte Befehlspakete wertlos
    s_sw_ms = millis();
    s_ver++;
    log_i("Schalter %d (%s): %s", idx, c.name.c_str(), on ? "an" : "aus");
  }
  return true;
}

bool sat_io_switch_nonce(uint8_t idx, uint8_t val, uint32_t nonce) {
  {
    Lock l;
    if (nonce != s_nonce) return false;
  }
  return sat_io_switch(idx, val);
}

static String default_name(SatPointKind k, int idx) {
  switch (k) {
    case SP_TANK: return "Tank " + String(idx);
    case SP_TEMP: return "Temperatur " + String(idx);
    case SP_ANALOG: return "Analog " + String(idx);
    case SP_TILT: return idx == 1 ? "Neigung längs" : "Neigung quer";
    case SP_INPUT: return "Eingang " + String(idx);
    case SP_SWITCH: return "Schalter " + String(idx);
    default: return "";
  }
}

static int32_t age_of(uint32_t ms) { return ms ? (int32_t)((millis() - ms) / 1000) : -1; }

std::vector<SatPoint> sat_io_points() {
  std::vector<SatPoint> r;
  Lock l;
  auto add = [&](SatPointKind k, int idx, const String &name) -> SatPoint & {
    SatPoint p;
    p.kind = k;
    p.idx = idx;
    p.id = sat_point_id(k, idx);
    p.name = (name.isEmpty() ? default_name(k, idx) : name).c_str();
    r.push_back(p);
    return r.back();
  };
  for (int i = 0; i < SAT_IO_TANKS; i++) {
    const TankCfg &c = s_cfg.tanks[i];
    if (!tank_used(c)) continue;
    SatPoint &p = add(SP_TANK, i + 1, c.name);
    p.cls = SC_LEVEL;
    p.unit = "%";
    p.ok = s_tank[i].ok;
    p.value = s_tank[i].ok ? s_tank[i].pct : NAN;
    p.probes = s_tank[i].probes;
    p.probes_n = s_tank[i].n;
    p.odd = s_tank[i].odd;
    p.age_s = age_of(s_tank[i].ms);
    p.status = !s_tank[i].ms ? "misst …" : s_tank[i].odd ? "Sonden unplausibel" : "";
  }
  for (int i = 0; i < SAT_IO_DS; i++) {
    const DsCfg &c = s_cfg.ds[i];
    if (c.addr.isEmpty() || s_cfg.ow_pin < 0) continue;
    SatPoint &p = add(SP_TEMP, i + 1, c.name);
    p.cls = SC_TEMP;
    p.unit = sat_class_unit(SC_TEMP);
    p.ok = s_ds[i].ok;
    p.value = s_ds[i].ok ? s_ds[i].t + c.off : NAN;
    p.age_s = age_of(s_ds[i].ms);
    p.status = s_ds[i].ok ? "" : s_ds[i].ms ? "Fühler antwortet nicht" : "misst …";
  }
  for (int i = 0; i < SAT_IO_ANALOG; i++) {
    const AnCfg &c = s_cfg.an[i];
    if (c.pin < 0) continue;
    SatPoint &p = add(SP_ANALOG, i + 1, c.name);
    p.cls = c.cls;
    p.unit = (c.unit.isEmpty() ? String(sat_class_unit(c.cls)) : c.unit).c_str();
    p.ok = s_an[i].ok;
    float step = powf(10, -(int)c.dec);
    p.value = s_an[i].ok ? roundf(s_an[i].v / step) * step : NAN;
    p.age_s = age_of(s_an[i].ms);
  }
  if (s_cfg.mpu.on) {
    for (int a = 0; a < 2; a++) {
      SatPoint &p = add(SP_TILT, a + 1, "");
      p.cls = SC_TILT;
      p.unit = sat_class_unit(SC_TILT);
      float v = a == 0 ? s_tilt_x : s_tilt_y;
      p.ok = s_mpu_ok && !isnan(v);
      p.value = p.ok ? roundf(v * 10) / 10 : NAN;
      p.age_s = age_of(s_mpu_ms);
      if (!s_mpu_ok) p.status = s_mpu_err.c_str();
    }
  }
  for (int i = 0; i < SAT_IO_INPUTS; i++) {
    const InCfg &c = s_cfg.in[i];
    if (c.pin < 0) continue;
    SatPoint &p = add(SP_INPUT, i + 1, c.name);
    p.cls = SC_BINARY;
    p.ok = true;
    p.on = s_in[i];
    p.value = s_in[i] ? 1 : 0;
    p.age_s = 0;
  }
  for (int i = 0; i < SAT_IO_SWITCHES; i++) {
    const SwCfg &c = s_cfg.sw[i];
    if (c.pin < 0) continue;
    SatPoint &p = add(SP_SWITCH, i + 1, c.name);
    p.cls = SC_BINARY;
    p.ok = true;
    p.on = s_sw[i];
    p.value = s_sw[i] ? 1 : 0;
    p.nonce = s_nonce;
    p.age_s = 0;
  }
  // Hymer Connect (Bluetooth zur SCU): Werte und Lichter als eigene Punkte ab Nummer HYMER_IDX_BASE
  for (auto &p : hymer_points()) {
    if (p.kind == SP_SWITCH) p.nonce = s_nonce;
    r.push_back(p);
  }
  return r;
}

// ---------------------------------------------------------------- Ablauf
static uint32_t s_tank_ms = 0, s_sens_ms = 0, s_ds_conv_ms = 0, s_in_ms = 0, s_mpu_poll_ms = 0, s_sw_save_ms = 0;
static bool s_tank_now = false;
static bool s_sw_dirty = false;

void sat_io_loop() {
  uint32_t now = millis();
  // Eingaenge: alle 20 ms, entprellt (3 gleiche Lesungen)
  if (now - s_in_ms >= 20) {
    s_in_ms = now;
    for (int i = 0; i < SAT_IO_INPUTS; i++) {
      const InCfg &c = s_cfg.in[i];
      if (c.pin < 0) continue;
      bool v = (digitalRead(c.pin) == LOW) != c.inv;
      if (v == s_in[i]) {
        s_in_cnt[i] = 0;
        continue;
      }
      if (++s_in_cnt[i] >= 3) {
        Lock l;
        s_in[i] = v;
        s_in_cnt[i] = 0;
        s_ver++;
      }
    }
  }
  // Tanks: eigenes Intervall
  if (s_tank_now || !s_tank_ms || now - s_tank_ms >= s_cfg.tank_interval * 1000UL) {
    s_tank_now = false;
    s_tank_ms = now ? now : 1;
    for (int i = 0; i < SAT_IO_TANKS; i++)
      if (tank_used(s_cfg.tanks[i])) tank_measure(i);
  }
  // MPU: alle 100 ms lesen. Gegen Wackeln: Median aus 5 Werten (Ausreisser weg), dann Zeitfilter;
  // bewegt sich das Fahrzeug (Gyro), bleibt der letzte ruhige Wert stehen (plus 1 s Nachlauf)
  if (s_mpu_ok && now - s_mpu_poll_ms >= 100) {
    float dt = s_mpu_poll_ms ? (now - s_mpu_poll_ms) / 1000.0f : 0.1f;
    s_mpu_poll_ms = now;
    float x, y, dps;
    if (mpu_read(x, y, dps)) {
      if (s_cfg.mpu.swap) std::swap(x, y);
      s_gyro = dps;
      if (s_cfg.mpu.still && dps > s_cfg.mpu.still_dps) s_moving_ms = now;
      s_moving = s_cfg.mpu.still && s_moving_ms && now - s_moving_ms < 1000;
      if (!s_moving || isnan(s_raw_x)) {
        s_med_x[s_med_i] = x;
        s_med_y[s_med_i] = y;
        s_med_i = (s_med_i + 1) % 5;
        if (s_med_n < 5) s_med_n++;
        float mx[5], my[5];
        memcpy(mx, s_med_x, sizeof(mx));
        memcpy(my, s_med_y, sizeof(my));
        std::sort(mx, mx + s_med_n);
        std::sort(my, my + s_med_n);
        x = mx[s_med_n / 2];
        y = my[s_med_n / 2];
        float a = s_cfg.mpu.smooth > 0 ? dt / (s_cfg.mpu.smooth + dt) : 1.0f;
        s_raw_x = isnan(s_raw_x) ? x : s_raw_x + (x - s_raw_x) * a;
        s_raw_y = isnan(s_raw_y) ? y : s_raw_y + (y - s_raw_y) * a;
      }
    }
  }
  // Sensoren: DS18B20 (messen, 800 ms spaeter lesen), Analog, Neigung
  if (!s_sens_ms || now - s_sens_ms >= s_cfg.interval * 1000UL) {
    s_sens_ms = now ? now : 1;
    if (s_cfg.ow_pin >= 0) {
      ds_convert_all();
      s_ds_conv_ms = now ? now : 1;
    }
    sensors_measure_analog();
    if (s_mpu_ok && !isnan(s_raw_x)) {
      float x = (s_raw_x - s_cfg.mpu.zx) * (s_cfg.mpu.inv_x ? -1 : 1);
      float y = (s_raw_y - s_cfg.mpu.zy) * (s_cfg.mpu.inv_y ? -1 : 1);
      Lock l;
      // Totband: erst ab der eingestellten Aenderung einen neuen Wert melden
      float dead = fmaxf(0.05f, s_cfg.mpu.dead);
      bool changed = false;
      if (isnan(s_tilt_x) || fabsf(x - s_tilt_x) >= dead) {
        s_tilt_x = x;
        changed = true;
      }
      if (isnan(s_tilt_y) || fabsf(y - s_tilt_y) >= dead) {
        s_tilt_y = y;
        changed = true;
      }
      s_mpu_ms = now;
      if (changed) s_ver++;
    }
  }
  if (s_ds_conv_ms && now - s_ds_conv_ms >= 800) {
    s_ds_conv_ms = 0;
    for (int i = 0; i < SAT_IO_DS; i++) {
      uint8_t rom[8];
      if (s_cfg.ds[i].addr.isEmpty() || !rom_parse(s_cfg.ds[i].addr, rom)) continue;
      float t;
      bool ok = ds_read(rom, t);
      Lock l;
      DsState &s = s_ds[i];
      bool changed = ok != s.ok || (ok && fabsf(roundf(t * 10) - roundf(s.t * 10)) >= 1);
      s.ok = ok;
      if (ok) s.t = t;
      s.ms = now;
      if (changed) s_ver++;
    }
  }
  // Schaltzustaende fuer "wie zuletzt" sichern (nicht im Bluetooth-Rueckruf, und nicht bei jedem Klick)
  static uint32_t seen_sw = 0;
  if (s_sw_ms != seen_sw) {
    seen_sw = s_sw_ms;
    s_sw_dirty = true;
    s_sw_save_ms = now;
  }
  if (s_sw_dirty && now - s_sw_save_ms > 3000) {
    s_sw_dirty = false;
    save_switch_states();
  }
}

// ---------------------------------------------------------------- Weboberflaeche
void sat_io_config_json(JsonObject o) {
  Lock l;
  cfg_to_json(o);
}

String sat_io_set_config(JsonObjectConst o) {
  IoCfg c;
  cfg_from_json(o, c);
  String err = validate(c);
  if (!err.isEmpty()) return err;
  {
    Lock l;
    // geaenderte Schalter-Pins: alten Ausgang freigeben
    for (int i = 0; i < SAT_IO_SWITCHES; i++)
      if (s_cfg.sw[i].pin >= 0 && s_cfg.sw[i].pin != c.sw[i].pin) pinMode(s_cfg.sw[i].pin, INPUT);
    s_cfg = c;
    apply_pins();
  }
  cfg_save();
  s_tank_ms = 0;
  s_sens_ms = 0;
  return "";
}

void sat_io_state_json(JsonObject o) {
  Lock l;
  JsonArray ta = o["tanks"].to<JsonArray>();
  for (int i = 0; i < SAT_IO_TANKS; i++) {
    JsonObject x = ta.add<JsonObject>();
    x["ok"] = s_tank[i].ok;
    x["pct"] = s_tank[i].pct;
    x["probes"] = s_tank[i].probes;
    x["odd"] = s_tank[i].odd;
    x["mv"] = s_tank[i].mv;
    x["age"] = age_of(s_tank[i].ms);
  }
  JsonArray da = o["ds"].to<JsonArray>();
  for (int i = 0; i < SAT_IO_DS; i++) {
    JsonObject x = da.add<JsonObject>();
    x["ok"] = s_ds[i].ok;
    if (s_ds[i].ok) x["t"] = roundf((s_ds[i].t + s_cfg.ds[i].off) * 10) / 10;
  }
  JsonArray fa = o["ds_found"].to<JsonArray>();
  for (auto &f : s_ds_found) fa.add(f);
  JsonArray aa = o["analog"].to<JsonArray>();
  for (int i = 0; i < SAT_IO_ANALOG; i++) {
    JsonObject x = aa.add<JsonObject>();
    x["mv"] = s_an[i].mv;
    if (s_an[i].ok) x["v"] = s_an[i].v;
  }
  JsonObject m = o["mpu"].to<JsonObject>();
  m["ok"] = s_mpu_ok;
  m["error"] = s_mpu_err;
  m["moving"] = s_moving;
  m["dps"] = roundf(s_gyro * 10) / 10;
  if (!isnan(s_tilt_x)) {
    m["x"] = roundf(s_tilt_x * 10) / 10;
    m["y"] = roundf(s_tilt_y * 10) / 10;
  }
  JsonArray sa = o["switches"].to<JsonArray>();
  for (int i = 0; i < SAT_IO_SWITCHES; i++) sa.add(s_sw[i]);
  JsonArray ia = o["inputs"].to<JsonArray>();
  for (int i = 0; i < SAT_IO_INPUTS; i++) ia.add(s_in[i]);
}

String sat_io_action(JsonObjectConst o) {
  if (o["tanks_now"] | false) s_tank_now = true;
  if (o["switch"].is<int>()) {
    if (!sat_io_switch(o["switch"].as<int>(), o["on"].is<bool>() ? (o["on"].as<bool>() ? 1 : 0) : 2))
      return "Schalter nicht eingerichtet";
  }
  if (o["tilt_zero"] | false) {
    if (!s_mpu_ok || isnan(s_raw_x)) return "Keine Werte vom MPU-6050";
    {
      Lock l;
      s_cfg.mpu.zx = s_raw_x;
      s_cfg.mpu.zy = s_raw_y;
      s_tilt_x = s_tilt_y = NAN;
    }
    cfg_save();
    s_sens_ms = 0;
  }
  if (o["ds_search"] | false) {
    if (s_cfg.ow_pin < 0) return "Erst den GPIO für den DS18B20-Bus wählen und speichern";
    std::vector<String> found = ow_search();
    int added = 0;
    {
      Lock l;
      s_ds_found = found;
      // neue Fuehler in freie Plaetze uebernehmen
      for (auto &f : found) {
        bool known = false;
        for (auto &d : s_cfg.ds) known |= d.addr == f;
        if (known) continue;
        for (auto &d : s_cfg.ds)
          if (d.addr.isEmpty()) {
            d.addr = f;
            d.name = "";
            added++;
            break;
          }
      }
    }
    if (added) cfg_save();
    s_sens_ms = 0;
    if (found.empty()) return "Kein Fühler gefunden – Verdrahtung und 4,7-kΩ-Widerstand prüfen";
  }
  return "";
}

const char *sat_io_chip() { return CHIP_NAME; }

void sat_io_pins_json(JsonArray a) {
  for (auto &p : PINS) {
    JsonObject o = a.add<JsonObject>();
    o["pin"] = p.pin;
    o["out"] = p.out;
    o["adc"] = p.adc;
    o["pull"] = p.pull;
    if (p.note) o["note"] = p.note;
  }
}
