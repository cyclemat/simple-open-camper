// Votronic per Bluetooth (S-BC) und Kabel (Display Link) – siehe votronic.h.
// Eigene Umsetzung nach Protokollwissen aus syssi/esphome-votronic (Apache-2.0, https://github.com/syssi/esphome-votronic):
//  - Bluetooth: Dienst d0cb6aa7-…, Meldekanal Batteriecomputer 9a082a4e-… (20 Byte), Solarregler 971ccec2-… (19 Byte),
//    Kopplung mit Verschluesselung + PIN (Passkey), Zahlen Little Endian
//  - Display Link: 1000 Baud 8N1, Rahmen 16 Byte "AA <Typ> … <XOR ueber Byte 1..14>", Typ 1A Solarregler, 3A Ladegeraet,
//    7A Ladebooster, CA/DA/FA Batteriecomputer (Smart Shunt)
#include "votronic.h"

#if WITH_VOTRONIC
#include <Arduino.h>
#include <HardwareSerial.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "i18n/i18n.h"

namespace {

const char *SVC_MON = "d0cb6aa7-8548-46d0-99f8-2d02611e5270";
const char *SVC_BOND = "70521e61-022d-f899-d046-4885a76acbd0";
const char *CHR_BAT = "9a082a4e-5bcc-4b1d-9958-a97cfccfa5ec";
const char *CHR_SOLAR = "971ccec2-521d-42fd-b570-cf46fe5ceb65";

const uint32_t STALE_MS = 120000;   // so lange ohne neue Werte = "keine Daten mehr"

// ---------------------------------------------------------------- Werte je Geraet
enum DevId : uint8_t { D_BAT = 0, D_SOLAR, D_CHARGER, D_BOOSTER, D_N };
const char *const DEV_KEY[D_N] = {"vot:bat", "vot:solar", "vot:charger", "vot:booster"};
const char *const DEV_NAME[D_N] = {"Votronic Batterie", "Votronic Solar", "Votronic Ladegerät", "Votronic Ladebooster"};

struct Dev {
  bool has = false;
  uint32_t ms = 0;        // letzte Meldung
  uint8_t src = 0;        // 1 Bluetooth, 2 Kabel
  float v = NAN, v2 = NAN, i = NAN, soc = NAN, rem_ah = NAN, nom_ah = NAN;
  float pv_v = NAN, pv_i = NAN, pv_w = NAN, temp = NAN, load = NAN, chg_ah = NAN, chg_wh = NAN;
  int bst = -1, cst = -1, mode = -1;   // Batteriestatus, Reglerstatus (Bitmasken), Lademodus (DIP-Schalter)
};

SemaphoreHandle_t s_mtx = nullptr;
struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};

VotronicCfg s_cfg;
Dev s_dev[D_N];
uint32_t s_ver = 1;

// Bluetooth
NimBLEClient *s_client = nullptr;
bool s_ble_up = false;
volatile bool s_ble_lost = false;
uint32_t s_next_try = 0, s_ble_rx_ms = 0;
uint8_t s_fails = 0;
std::string s_ble_status;
uint32_t s_search_until = 0;
std::vector<VotronicCand> s_cands;
bool s_pin_asked = false;

// Kabel
HardwareSerial *s_ser = nullptr;
TaskHandle_t s_ser_task = nullptr;
volatile bool s_ser_stop = false;
uint32_t s_ser_rx_ms = 0, s_ser_frames = 0, s_ser_bad = 0;
std::string s_ser_status;
uint8_t s_last_type = 0;

void logf(const char *fmt, ...) {
  char b[200];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(b, sizeof(b), fmt, ap);
  va_end(ap);
  log_printf("[votronic] %s\n", b);
}

uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
int32_t le24s(const uint8_t *p) {
  int32_t v = p[0] | p[1] << 8 | p[2] << 16;
  if (v & 0x800000) v |= ~0xFFFFFF;
  return v;
}

// ---------------------------------------------------------------- Einstellungen
void load_cfg() {
  Preferences p;
  if (!p.begin("votronic", true)) return;
  s_cfg.ble_on = p.getBool("ble", false);
  s_cfg.mac = p.getString("mac", "").c_str();
  s_cfg.mac_type = p.getUChar("at", 0);
  s_cfg.pin = p.getString("pin", VOTRONIC_PIN_DEFAULT).c_str();
  s_cfg.uart_on = p.getBool("uon", false);
  s_cfg.uart = p.getChar("un", 1);
  s_cfg.rx = p.getChar("rx", -1);
  p.end();
}
void save_cfg() {
  Preferences p;
  if (!p.begin("votronic", false)) return;
  p.putBool("ble", s_cfg.ble_on);
  p.putString("mac", s_cfg.mac.c_str());
  p.putUChar("at", s_cfg.mac_type);
  p.putString("pin", s_cfg.pin.c_str());
  p.putBool("uon", s_cfg.uart_on);
  p.putChar("un", s_cfg.uart);
  p.putChar("rx", s_cfg.rx);
  p.end();
}

// ---------------------------------------------------------------- Auswertung
// Bluetooth: Batteriecomputer (20 Byte)
void ble_bat(const uint8_t *d, size_t n) {
  if (n != 20) return;
  Lock l;
  Dev &b = s_dev[D_BAT];
  b.v = le16(d) * 0.01f;
  b.v2 = le16(d + 2) * 0.01f;
  b.rem_ah = le16(d + 4);
  b.soc = d[8];
  b.i = le24s(d + 10) * 0.001f;
  b.nom_ah = le16(d + 13) * 0.1f;
  b.has = true;
  b.src = 1;
  b.ms = millis();
  s_ver++;
}

// Bluetooth: Solarregler (19 Byte)
void ble_solar(const uint8_t *d, size_t n) {
  if (n != 19) return;
  Lock l;
  Dev &s = s_dev[D_SOLAR];
  s.v = le16(d) * 0.01f;
  s.pv_v = le16(d + 2) * 0.01f;
  s.pv_i = le16(d + 4) * 0.1f;
  s.bst = d[8];
  s.cst = d[12];
  s.temp = d[12] ? (float)d[9] : NAN;
  s.chg_ah = le16(d + 13);
  s.chg_wh = le16(d + 15) * 10.0f;
  s.pv_w = le16(d + 17) * 0.1f;
  s.has = true;
  s.src = 1;
  s.ms = millis();
  s_ver++;
}

// Kabel: ein vollstaendiger Rahmen (16 Byte, Pruefsumme schon geprueft)
void cable_frame(const uint8_t *d) {
  Lock l;
  uint32_t now = millis();
  s_last_type = d[1];
  auto common = [&](Dev &x) {
    x.has = true;
    x.src = 2;
    x.ms = now;
    s_ver++;
  };
  switch (d[1]) {
    case 0x1A: {   // Solarregler
      Dev &s = s_dev[D_SOLAR];
      s.v = le16(d + 2) * 0.01f;
      s.pv_v = le16(d + 4) * 0.01f;
      s.pv_i = (int16_t)le16(d + 6) * 0.1f;
      s.pv_w = s.pv_v * s.pv_i;
      s.temp = d[11];
      s.mode = d[12];
      s.bst = d[13];
      s.cst = d[14];
      common(s);
      break;
    }
    case 0x3A:     // Ladegeraet (VBCS/VAC)
    case 0x7A: {   // Ladebooster (VCC)
      Dev &c = s_dev[d[1] == 0x3A ? D_CHARGER : D_BOOSTER];
      c.v = le16(d + 2) * 0.01f;
      c.v2 = le16(d + 4) * 0.01f;
      c.i = (int16_t)le16(d + 6) * 0.1f;
      c.load = d[10];
      c.temp = d[11] * 0.1f;
      c.mode = d[12];
      c.bst = d[13];
      c.cst = d[14];
      common(c);
      break;
    }
    case 0xCA: {   // Batteriecomputer Teil 1
      Dev &b = s_dev[D_BAT];
      b.v = le16(d + 2) * 0.01f;
      b.v2 = le16(d + 4) * 0.01f;
      b.rem_ah = le16(d + 6);
      b.soc = d[10];
      b.i = le24s(d + 12) * 0.001f;
      common(b);
      break;
    }
    case 0xDA: {   // Batteriecomputer Teil 2
      Dev &b = s_dev[D_BAT];
      b.nom_ah = le16(d + 6) * 0.1f;
      b.bst = d[13];
      if (b.has) b.ms = now;
      s_ver++;
      break;
    }
    default:       // FA (Batteriecomputer Teil 3), 4A (leer) und unbekannte: nur zaehlen
      break;
  }
}

// Ladephase aus dem Batteriestatus (I/U1/U2/U3) als Victron-Zustand (victron_state_text)
int phase_state(const Dev &d) {
  if (d.bst < 0) return -1;
  if (d.bst == 0) return d.cst == 0 ? 0 : -1;
  if (d.bst & 0x08) return 5;   // U3: Erhaltung
  if (d.bst & 0x06) return 4;   // U1/U2: Absorption
  if (d.bst & 0x01) return 3;   // I: Hauptladung
  return -1;
}

const char *phase_text(int bst) {
  if (bst < 0) return "–";
  if (bst == 0) return "Standby";
  if (bst & 0x08) return "U3 (Erhaltung)";
  if (bst & 0x04) return "U2";
  if (bst & 0x02) return "U1";
  if (bst & 0x01) return "I (Hauptladung)";
  return "–";
}

std::string mode_text(int m) {
  switch (m) {
    case 24: case 124: return "Blei-Säure";
    case 53: case 153: return "Blei-Gel";
    case 34: case 134: return "AGM 14,4 V";
    case 47: case 147: return "AGM 14,7–14,8 V";
    case 89: case 189: return "LiFePO4 13,9 V";
    case 82: case 182: return "LiFePO4 14,2 V";
    case 84: case 184: return "LiFePO4 14,4 V";
    case 86: case 186: return "LiFePO4 14,6 V";
    case 88: case 188: return "LiFePO4 14,8 V";
    default: break;
  }
  if (m < 0) return "–";
  char b[16];
  snprintf(b, sizeof(b), "%d", m);
  return b;
}

std::string ctl_text(int c, bool charger) {
  if (c < 0) return "–";
  if (c == 0) return "Standby";
  std::string r;
  auto add = [&](const char *t) { r += (r.empty() ? "" : ", ") + std::string(tr(t)); };
  if (charger) {
    if (c & 0x04) add("lädt Batterie 1");
    if (c & 0x08) add("lädt Batterie 2");
    if (c & 0x10) add("reduziert");
    if (c & 0x20) add("Netzleistung begrenzt");
  } else {
    if (c & 0x08) add("aktiv");
    if (c & 0x10) add("reduziert");
    if (c & 0x20) add("AES aktiv");
  }
  return r.empty() ? std::to_string(c) : r;
}

// Geraet -> Victron-artige Kachel
bool fill(int id, VictronInfo &v) {
  const Dev &d = s_dev[id];
  if (!d.has) return false;
  v = VictronInfo();
  v.key = DEV_KEY[id];
  v.mac = "Votronic";
  v.name = tr(DEV_NAME[id]);
  v.has = true;
  v.rssi = 0;   // kein Funksignal anzeigen (Kabel bzw. feste Verbindung)
  uint32_t age = (millis() - d.ms) / 1000;
  v.age_s = (int32_t)age;
  v.status = age * 1000 < STALE_MS ? std::string("OK") : std::string(tr("keine Daten mehr"));
  v.bat_v = d.v;
  v.bat_i = NAN;
  v.soc = NAN;
  v.consumed_ah = NAN;
  v.yield_kwh = NAN;
  v.load_i = NAN;
  v.in_v = NAN;
  v.in_i = NAN;
  v.power_w = NAN;
  v.ac_i = NAN;
  v.aux = NAN;
  switch (id) {
    case D_BAT:
      v.kind = VIC_SHUNT;
      v.record = 0x02;
      v.bat_i = d.i;
      v.soc = d.soc;
      v.power_w = isnan(d.v) || isnan(d.i) ? NAN : d.v * d.i;
      v.consumed_ah = isnan(d.rem_ah) || isnan(d.nom_ah) || d.nom_ah <= 0 ? NAN : d.rem_ah - d.nom_ah;
      if (!isnan(d.v2) && d.v2 > 1) {
        v.aux_mode = 0;   // zweite Batterie = Starterbatterie
        v.aux = d.v2;
      }
      break;
    case D_SOLAR:
      v.kind = VIC_SOLAR;
      v.record = 0x01;
      v.pv_w = d.pv_w;
      v.bat_i = isnan(d.pv_w) || isnan(d.v) || d.v < 1 ? NAN : d.pv_w / d.v;   // Ladestrom ≈ Leistung / Spannung
      v.state = phase_state(d);
      break;
    case D_CHARGER:
      v.kind = VIC_CHARGER;
      v.record = 0x08;
      v.bat_i = d.i;
      v.power_w = isnan(d.v) || isnan(d.i) ? NAN : d.v * d.i;
      v.aux = d.temp;
      v.state = phase_state(d);
      break;
    case D_BOOSTER:
      v.kind = VIC_DCDC;
      v.record = 0x04;
      v.in_v = d.v2;     // Starterbatterie (Eingang)
      v.bat_i = d.i;     // Ladestrom in die Bordbatterie
      v.state = phase_state(d);
      break;
  }
  return true;
}

std::string num(float x, int dec, const char *unit) {
  if (isnan(x)) return "–";
  char b[32];
  snprintf(b, sizeof(b), "%.*f %s", dec, x, unit);
  std::string r = b;
#if UI_DECIMAL_COMMA
  for (auto &c : r)
    if (c == '.') c = ',';
#endif
  return r;
}

void set_ser_status(const std::string &t) {
  Lock l;
  if (s_ser_status != t) {
    s_ser_status = t;
    s_ver++;
  }
}

// ---------------------------------------------------------------- Kabel (Display Link)
void ser_task(void *) {
  VotronicCfg c;
  {
    Lock l;
    c = s_cfg;
  }
  s_ser = new HardwareSerial(c.uart);
  s_ser->setRxBufferSize(256);
  s_ser->begin(1000, SERIAL_8N1, c.rx, -1);
  logf("Kabel: UART%d RX %d, 1000 Baud", c.uart, c.rx);
  set_ser_status(tr("Warte auf Daten …"));
  uint8_t buf[16];
  size_t at = 0;
  uint32_t last_byte = 0;
  while (!s_ser_stop) {
    if (!s_ser->available()) {
      vTaskDelay(pdMS_TO_TICKS(20));
      uint32_t now = millis();
      if (at && now - last_byte > 150) at = 0;   // Luecke: neuer Rahmen
      if (s_ser_rx_ms && now - s_ser_rx_ms > STALE_MS) set_ser_status(tr("Keine Daten mehr – Kabel prüfen"));
      else if (!s_ser_rx_ms && s_ser_bad > 20) set_ser_status(tr("Empfange Daten, aber keine gültigen Rahmen – RX-Pin/Pegel prüfen"));
      continue;
    }
    int ch = s_ser->read();
    if (ch < 0) continue;
    last_byte = millis();
    if (at == 0 && ch != 0xAA) {
      s_ser_bad++;
      continue;
    }
    buf[at++] = (uint8_t)ch;
    if (at < 16) continue;
    at = 0;
    uint8_t x = 0;
    for (int k = 1; k < 15; k++) x ^= buf[k];
    if (x != buf[15]) {
      s_ser_bad++;
      continue;
    }
    s_ser_frames++;
    s_ser_rx_ms = millis();
    cable_frame(buf);
    set_ser_status(tr("Verbunden"));
  }
  s_ser->end();
  delete s_ser;
  s_ser = nullptr;
  set_ser_status("");
  s_ser_task = nullptr;
  vTaskDelete(nullptr);
}

void ser_stop() {
  if (!s_ser_task) return;
  s_ser_stop = true;
  for (int i = 0; i < 100 && s_ser_task; i++) vTaskDelay(pdMS_TO_TICKS(20));
}

void ser_apply() {
  ser_stop();
  if (!s_cfg.uart_on) return;
  if (s_cfg.rx < 0 || s_cfg.uart < 1 || s_cfg.uart >= votronic_uart_count()) {
    set_ser_status(tr("RX-Pin und UART einstellen"));
    return;
  }
  s_ser_stop = false;
  s_ser_rx_ms = 0;
  s_ser_bad = 0;
  xTaskCreatePinnedToCore(ser_task, "votronic", 3072, nullptr, 2, &s_ser_task, 1);
}

// ---------------------------------------------------------------- Bluetooth (S-BC)
void set_ble_status(const std::string &t) {
  Lock l;
  if (s_ble_status != t) {
    s_ble_status = t;
    s_ver++;
  }
}

class ClientCB : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient *, int reason) override {
    s_ble_lost = true;
    logf("S-BC getrennt (Grund %d)", reason);
    if (reason == 0x205 || reason == 0x206) set_ble_status(tr("Kopplung abgelehnt – PIN prüfen, Taste am S-BC kurz drücken"));
  }
  void onPassKeyEntry(NimBLEConnInfo &ci) override {
    std::string pin;
    {
      Lock l;
      pin = s_cfg.pin;
      s_pin_asked = true;
    }
    bool ok = NimBLEDevice::injectPassKey(ci, (uint32_t)strtoul(pin.c_str(), nullptr, 10));
    logf("S-BC verlangt PIN – gebe sie ein (%s)", ok ? "angenommen" : "vom Bluetooth-Stack abgelehnt");
  }
  void onConfirmPasskey(NimBLEConnInfo &ci, uint32_t) override { NimBLEDevice::injectConfirmPasskey(ci, true); }
  void onAuthenticationComplete(NimBLEConnInfo &ci) override {
    logf("Kopplung %s (authentifiziert %d, gespeichert %d)", ci.isEncrypted() ? "ok" : "FEHLGESCHLAGEN", ci.isAuthenticated(),
         ci.isBonded());
  }
};
ClientCB s_ccb;

void on_bat(NimBLERemoteCharacteristic *, uint8_t *d, size_t n, bool) {
  ble_bat(d, n);
  Lock l;
  s_ble_rx_ms = millis();
}
void on_solar(NimBLERemoteCharacteristic *, uint8_t *d, size_t n, bool) {
  ble_solar(d, n);
  Lock l;
  s_ble_rx_ms = millis();
}

void ble_drop(uint32_t retry_ms) {
  NimBLEClient *c;
  {
    Lock l;
    c = s_client;
    s_client = nullptr;
    if (s_ble_up) s_ver++;
    s_ble_up = false;
    s_next_try = millis() + retry_ms;
  }
  if (c) {
    if (c->isConnected()) c->disconnect();
    NimBLEDevice::deleteClient(c);
  }
}

// Waehrend der Verbindung: PIN-Eingabe und MITM erlauben (S-BC verlangt Passkey), danach wieder wie ueberall
struct SecGuard {
  SecGuard() {
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
  }
  ~SecGuard() {
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityAuth(true, false, true);
  }
};

}   // namespace

// ---------------------------------------------------------------- oeffentlich
void votronic_begin() {
  s_mtx = xSemaphoreCreateRecursiveMutex();
  load_cfg();
  ser_apply();
}

VotronicCfg votronic_cfg() {
  if (!s_mtx) return VotronicCfg();
  Lock l;
  return s_cfg;
}

void votronic_set_cfg(const VotronicCfg &c) {
  if (!s_mtx) return;
  bool ser_changed, ble_changed;
  {
    Lock l;
    ser_changed = c.uart_on != s_cfg.uart_on || c.uart != s_cfg.uart || c.rx != s_cfg.rx;
    ble_changed = c.ble_on != s_cfg.ble_on || c.mac != s_cfg.mac || c.pin != s_cfg.pin;
    s_cfg = c;
    if (s_cfg.pin.empty()) s_cfg.pin = VOTRONIC_PIN_DEFAULT;
    if (!c.ble_on) s_search_until = 0;
    s_fails = 0;
    s_next_try = 0;
    s_ver++;
  }
  save_cfg();
  if (ser_changed) ser_apply();
  if (ble_changed) s_ble_lost = true;   // Bluetooth-Task trennt die alte Verbindung
}

bool votronic_active() {
  if (!s_mtx) return false;
  Lock l;
  return s_cfg.ble_on || s_cfg.uart_on;
}

std::string votronic_status() {
  if (!s_mtx) return "";
  Lock l;
  if (!s_cfg.ble_on) return tr("Aus");
  if (s_cfg.mac.empty()) return tr("Kein S-BC gewählt");
  if (s_ble_up) return s_ble_rx_ms ? tr("Verbunden") : tr("Verbunden, warte auf Daten …");
  return s_ble_status.empty() ? tr("Suche S-BC …") : s_ble_status;
}

std::string votronic_uart_status() {
  if (!s_mtx) return "";
  Lock l;
  if (!s_cfg.uart_on) return tr("Aus");
  return s_ser_status;
}

uint32_t votronic_version() {
  if (!s_mtx) return 0;
  Lock l;
  return s_ver;
}

bool votronic_get(const std::string &key, VictronInfo &v) {
  if (!s_mtx || key.rfind("vot:", 0) != 0) return false;
  Lock l;
  if (!s_cfg.ble_on && !s_cfg.uart_on) return false;
  for (int i = 0; i < D_N; i++)
    if (key == DEV_KEY[i]) return fill(i, v);
  return false;
}

std::vector<VictronInfo> votronic_list() {
  std::vector<VictronInfo> r;
  if (!s_mtx) return r;
  Lock l;
  if (!s_cfg.ble_on && !s_cfg.uart_on) return r;
  for (int i = 0; i < D_N; i++) {
    VictronInfo v;
    if (fill(i, v)) r.push_back(v);
  }
  return r;
}

std::vector<VotronicRow> votronic_rows() {
  std::vector<VotronicRow> r;
  if (!s_mtx) return r;
  Lock l;
  auto add = [&](int id, const char *n, const std::string &val) {
    if (val != "–") r.push_back({tr(DEV_NAME[id]), tr(n), val});
  };
  for (int id = 0; id < D_N; id++) {
    const Dev &d = s_dev[id];
    if (!d.has) continue;
    switch (id) {
      case D_BAT:
        add(id, "Ladezustand", num(d.soc, 0, "%"));
        add(id, "Spannung", num(d.v, 2, "V"));
        add(id, "Strom", num(d.i, 2, "A"));
        add(id, "Leistung", isnan(d.v) || isnan(d.i) ? "–" : num(d.v * d.i, 0, "W"));
        add(id, "Restkapazität", num(d.rem_ah, 0, "Ah"));
        add(id, "Nennkapazität", num(d.nom_ah, 0, "Ah"));
        add(id, "Starterbatterie", d.v2 > 1 ? num(d.v2, 2, "V") : "–");
        break;
      case D_SOLAR:
        add(id, "PV-Leistung", num(d.pv_w, 0, "W"));
        add(id, "PV-Spannung", num(d.pv_v, 1, "V"));
        add(id, "PV-Strom", num(d.pv_i, 1, "A"));
        add(id, "Batteriespannung", num(d.v, 2, "V"));
        add(id, "Ladephase", tr(phase_text(d.bst)));
        add(id, "Regler", ctl_text(d.cst, false));
        add(id, "Reglertemperatur", num(d.temp, 0, "°C"));
        add(id, "Geladen (seit Zurücksetzen)", num(d.chg_ah, 0, "Ah"));
        add(id, "Energie (seit Zurücksetzen)", isnan(d.chg_wh) ? "–" : num(d.chg_wh / 1000.0f, 2, "kWh"));
        add(id, "Lademodus", d.mode < 0 ? "–" : tr(mode_text(d.mode).c_str()));
        break;
      default:
        add(id, "Batteriespannung", num(d.v, 2, "V"));
        add(id, id == D_BOOSTER ? "Starterbatterie" : "Batterie 2", num(d.v2, 2, "V"));
        add(id, "Ladestrom", num(d.i, 1, "A"));
        add(id, "Leistung", isnan(d.v) || isnan(d.i) ? "–" : num(d.v * d.i, 0, "W"));
        add(id, "Auslastung", num(d.load, 0, "%"));
        add(id, "Temperatur", num(d.temp, 1, "°C"));
        add(id, "Ladephase", tr(phase_text(d.bst)));
        add(id, "Regler", ctl_text(d.cst, true));
        add(id, "Lademodus", d.mode < 0 ? "–" : tr(mode_text(d.mode).c_str()));
        break;
    }
    uint32_t age = (millis() - d.ms) / 1000;
    char b[48];
    snprintf(b, sizeof(b), tr("%s, vor %lu s"), d.src == 1 ? tr("Bluetooth") : tr("Kabel"), (unsigned long)age);
    add(id, "Empfang", b);
  }
  return r;
}

void votronic_search(uint32_t ms) {
  if (!s_mtx) return;
  Lock l;
  s_search_until = (millis() + ms) | 1;
}

std::vector<VotronicCand> votronic_candidates() {
  if (!s_mtx) return {};
  Lock l;
  return s_cands;
}

int votronic_uart_count() { return SOC_UART_HP_NUM; }

void votronic_print_debug() {
  if (!s_mtx) {
    log_printf("Votronic: nicht gestartet\n");
    return;
  }
  Lock l;
  log_printf("Votronic: Bluetooth %s (S-BC %s) – %s\n", s_cfg.ble_on ? "an" : "aus", s_cfg.mac.empty() ? "-" : s_cfg.mac.c_str(),
             votronic_status().c_str());
  log_printf("Votronic: Kabel %s (UART%d RX %d) – %s, %lu Rahmen, %lu verworfen, letzter Typ %02X\n", s_cfg.uart_on ? "an" : "aus",
             s_cfg.uart, s_cfg.rx, s_ser_status.c_str(), (unsigned long)s_ser_frames, (unsigned long)s_ser_bad, s_last_type);
  for (auto &row : votronic_rows()) log_printf("  %-22s %-28s %s\n", row.dev.c_str(), row.name.c_str(), row.value.c_str());
}

// ---- Bluetooth-Verteiler (truma.cpp, Bluetooth-Task)
bool votronic_ble_needed() {
  if (!s_mtx) return false;
  Lock l;
  return s_cfg.ble_on && (!s_cfg.mac.empty() || s_search_until);
}

bool votronic_on_adv(const NimBLEAdvertisedDevice *dev) {
  if (!s_mtx) return false;
  std::string name = dev->haveName() ? dev->getName() : "";
  std::string low = name;
  for (auto &c : low) c = (char)tolower((unsigned char)c);
  std::string mac = dev->getAddress().toString();
  for (auto &c : mac) c = (char)tolower((unsigned char)c);
  bool hit = low.find("votronic") != std::string::npos || low.find("s-bc") != std::string::npos;
  if (!hit && dev->haveServiceUUID())
    hit = dev->isAdvertisingService(NimBLEUUID(SVC_MON)) || dev->isAdvertisingService(NimBLEUUID(SVC_BOND));
  Lock l;
  if (mac == s_cfg.mac) {
    s_cfg.mac_type = dev->getAddress().getType();
    hit = true;
  }
  if (!hit) return false;
  if (s_search_until) {
    for (auto &c : s_cands)
      if (c.mac == mac) {
        c.rssi = dev->getRSSI();
        return true;
      }
    if (s_cands.size() < 8) {
      VotronicCand c;
      c.mac = mac;
      c.name = name;
      c.type = dev->getAddress().getType();
      c.rssi = dev->getRSSI();
      s_cands.push_back(c);
      s_ver++;
    }
  }
  return true;
}

void votronic_scan_wanted(bool &, bool &active) {
  if (!s_mtx) return;
  Lock l;
  if (s_search_until && (int32_t)(millis() - s_search_until) >= 0) {
    s_search_until = 0;
    s_ver++;
  }
  if (s_cfg.ble_on && s_search_until) active = true;
}

bool votronic_want_connect() {
  if (!s_mtx) return false;
  Lock l;
  return s_cfg.ble_on && !s_cfg.mac.empty() && !s_client && (int32_t)(millis() - s_next_try) >= 0;
}

void votronic_connect_now() {
  std::string mac;
  uint8_t type;
  {
    Lock l;
    mac = s_cfg.mac;
    type = s_cfg.mac_type;
    s_ble_lost = false;
    s_pin_asked = false;
    s_ble_status = tr("Verbinde …");
    s_ver++;
  }
  NimBLEClient *c = NimBLEDevice::createClient();
  c->setClientCallbacks(&s_ccb, false);
  c->setConnectTimeout(8000);
  c->setConnectionParams(24, 40, 0, 600);
  {
    Lock l;
    s_client = c;
  }
  bool subscribed = false, ok;
  {
    SecGuard sg;
    NimBLEAddress addr(mac, type);
    ok = c->connect(addr, true);
    if (ok && !c->getConnInfo().isEncrypted()) {
      bool sec = c->secureConnection();
      if (!sec) logf("S-BC %s: Kopplung fehlgeschlagen, Fehler %d", mac.c_str(), c->getLastError());
    }
  }
  if (ok) {
    NimBLERemoteService *svc = c->getService(NimBLEUUID(SVC_MON));
    if (svc) {
      NimBLERemoteCharacteristic *b = svc->getCharacteristic(NimBLEUUID(CHR_BAT));
      NimBLERemoteCharacteristic *s = svc->getCharacteristic(NimBLEUUID(CHR_SOLAR));
      if (b && b->canNotify() && b->subscribe(true, on_bat, true)) subscribed = true;
      if (s && s->canNotify() && s->subscribe(true, on_solar, true)) subscribed = true;
      if (!b) logf("S-BC %s: kein Batteriecomputer angeschlossen?", mac.c_str());
      if (!s) logf("S-BC %s: kein Solarregler angeschlossen?", mac.c_str());
    } else {
      logf("S-BC %s: Dienst fehlt", mac.c_str());
    }
  }
  if (!subscribed) {
    uint8_t f;
    bool pin_asked;
    {
      Lock l;
      f = s_fails < 8 ? ++s_fails : s_fails;
      pin_asked = s_pin_asked;
    }
    ble_drop(f * 15000UL);
    set_ble_status(!ok ? tr("S-BC nicht erreichbar – in Reichweite? App getrennt?")
                       : pin_asked ? tr("Kopplung abgelehnt – PIN prüfen, Taste am S-BC kurz drücken")
                                   : tr("S-BC antwortet nicht wie erwartet"));
    if (!ok) logf("S-BC %s: Verbindung fehlgeschlagen", mac.c_str());
    return;
  }
  Lock l;
  s_ble_up = true;
  s_fails = 0;
  s_ble_status = "";
  s_ver++;
  logf("S-BC %s verbunden", mac.c_str());
}

void votronic_ble_loop() {
  if (!s_mtx) return;
  bool drop = false, stop = false;
  {
    Lock l;
    if (!s_client) return;
    if (s_ble_lost) {
      drop = true;
      stop = !s_cfg.ble_on || s_cfg.mac.empty();
    } else if (s_ble_up && s_ble_rx_ms && millis() - s_ble_rx_ms > STALE_MS) {
      drop = true;   // verbunden, aber nichts mehr gemeldet: neu verbinden
    }
  }
  if (drop) {
    ble_drop(stop ? 0 : 3000);
    if (stop) set_ble_status("");
  }
}

#endif
