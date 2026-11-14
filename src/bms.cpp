// Akkus mit BMS per Bluetooth – siehe bms.h
#include "bms.h"

const char *bms_model_text(BmsModel m) {
  switch (m) {
    case BMS_SUPERB: return "Super B";
    case BMS_PQ_HEX:
    case BMS_PQ_ASCII: return "Power Quarry";
    case BMS_BULLTRON: return "Daly/Bulltron";
    case BMS_RENOGY: return "Renogy";
    case BMS_ECOWORTHY: return "Eco-Worthy";
    case BMS_JBD: return "JBD";
    case BMS_JK: return "JK-BMS";
    case BMS_LITIME: return "LiTime";
    default: return "Akku";
  }
}

#if WITH_BMS
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <atomic>
#include <ctype.h>
#include <deque>
#include <stdarg.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {   // dateilokal: gleichnamige Typen anderer Dateien duerfen sich nicht vermischen

// ---------------------------------------------------------------- Super B (App "Be in Charge" 2.1.1)
const char *SB_SERVICE = "74b9c2d1-dc6d-42cf-a2e9-7398b8fc2e70";
const char *SB_STATUS = "6edadbe4-4f53-4a5a-96ed-02f93db93790";   // Notify: Statuspaket
const char *SB_RPC = "93618162-0634-49ad-8510-798a91645056";      // Textkonsole (nur zum Entsperren benutzt)
const char *SB_UNLOCK = "SuperB";
const char *SB_HINTS[] = {"epsilon", "super b", "superb"};
const size_t SB_CHUNK = 20;

// ---------------------------------------------------------------- Power Quarry (App "Power Quarry" 3.5.2, CBT-BMS)
// Schreiben 0xFFE5/0xFFE9 (ohne Antwort, 20-Byte-Stuecke, 30 ms Abstand), Empfang 0xFFE0/0xFFE4 (Notify).
// Kein Pairing. Nur Messwert-Abfragen, keine Parameter-Befehle.
const uint32_t PQ_POLL_MS = 20000;   // Spannung/Strom; Ladezustand und Seltenes jede 3. Abfrage (1 min)
struct PqProduct {
  const char *code, *name;
  uint16_t ah;
  uint8_t cells;
};
const PqProduct PQ_PRODUCTS[] = {
    {"0A", "VB101", 172, 4}, {"0B", "VB201", 172, 4}, {"0C", "VB001", 172, 4}, {"0D", "VB002", 258, 4},
    {"0E", "VB007", 120, 4}, {"0F", "VB102", 258, 4}, {"09", "VB016", 200, 4}, {"08", "VB103", 240, 4},
    {"07", "VB104", 480, 4}, {"10", "VB029", 40, 4},  {"11", "VB030", 100, 4}, {"12", "VB031", 750, 4},
    {"16", "VB028", 300, 4}, {"18", "VB107", 135, 4}, {"19", "VB038", 50, 4},  {"1C", "VB042", 60, 4},
    {"1A", "VB043", 150, 4}, {"CBT001", "CBT001", 480, 4}, {"VB106", "VB106", 42, 4}, {"13", "VB032", 100, 8},
    {"A0", "VB032", 100, 8}, {"A1", "VB017", 135, 8}, {"A2", "VB033", 200, 8}, {"C0", "VB039", 50, 16},
    {"20", "VB046", 175, 4}, {"17", "VB024", 200, 4}, {"14", "VB037", 135, 4}, {"15", "VB035", 270, 4},
};

const uint32_t DISCOVERY_MS = 60000;
const uint32_t RETRY_MS = 20000;        // nach fehlgeschlagener Verbindung
const uint32_t RECONNECT_MS = 8000;     // nach Abbruch einer bestehenden Verbindung
const uint32_t STALE_MS = 90000;        // so lange ohne Statuspaket: Verbindung neu aufbauen

struct Bat {
  std::string mac, name;
  uint8_t type = 0;
  BmsModel model = BMS_SUPERB;
  NimBLEClient *client = nullptr;
  volatile bool connected = false;
  uint32_t next_try = 0, last_ms = 0, conn_ms = 0;
  bool has = false;
  int soc = -1, soh = -1;
  float volt = NAN, amp = NAN;
  int32_t ttg = -1;
  uint8_t flags = 0;
  std::string model_text, serial, firmware, status = "Warte …";
  bool remove = false;
  // Power Quarry
  int cap_ah = 0;
  uint8_t cells = 4;
  NimBLERemoteCharacteristic *wr = nullptr;
  uint32_t next_poll = 0;
  float temp = NAN;
  int cycles = -1;
  std::vector<uint16_t> cell_mv;
  uint32_t alarms = 0;
  bool chg_on = true, dis_on = true;
  std::string rx;   // Empfangspuffer (Notify)
  bool wr_resp = false;   // mit Bestaetigung schreiben (falls ohne nichts ankommt)
  int mos_raw = -1;       // Power Quarry: Rohwert MOS-Status (0x0A, Byte 2) – Bedeutung noch ungeklaert
  uint32_t polls = 0;     // Zaehler: nur jede 3. Abfrage (1 min) auch Ladezustand/Temperatur/Kapazitaet/Alarme
  int rssi = 0;           // Signal zuletzt (fuers Protokoll bei Trennung)
  uint8_t silent = 0;     // Abfragen hintereinander ganz ohne Empfang
  // Abgefragte Akkus (Power Quarry, Bulltron) nur kurz verbinden: verbinden, abfragen, trennen. Die Dauer-
  // verbindung riss beim Power Quarry immer nach ~13 s ab ("Grund 520", Funkverbindung verloren).
  bool polled() const { return model != BMS_SUPERB && model != BMS_UNKNOWN; }
  int8_t daly_x81 = -1;    // Daly: -1 noch unbekannt, 0 Protokoll D2, 1 Protokoll 0x81 (Antwort 0x51)
  int jk_sw = -1;          // JK: Firmware-Hauptversion (ab 11 Aufbau 32S)
  std::string eco_a1, eco_a2;   // Eco-Worthy: letzte Rahmen A1/A2 (je ein Notify)
};

class ClientCB : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient *c, int reason) override;
};

}  // namespace

static SemaphoreHandle_t s_mtx = nullptr;
static std::vector<Bat> s_bats;
static std::vector<BmsFound> s_found;
static uint32_t s_disc_until = 0;
static std::atomic<uint32_t> s_ver{1};
static std::deque<std::string> s_log;
static ClientCB s_ccb;
static std::string s_rpc;               // Antwort der Textkonsole (Notify), nur waehrend des Entsperrens

namespace {
struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};
}  // namespace

static std::string lower(std::string s) {
  for (auto &c : s) c = tolower((unsigned char)c);
  return s;
}

static void logf(const char *fmt, ...) {
  char b[240];
  int n = snprintf(b, sizeof(b), "%6lus ", (unsigned long)(millis() / 1000));
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(b + n, sizeof(b) - n, fmt, ap);
  va_end(ap);
  log_i("BMS %s", b);
  Lock l;
  s_log.push_back(b);
  while (s_log.size() > 14) s_log.pop_front();
  s_ver++;
}

static Bat *find_mac(const std::string &mac) {
  for (auto &b : s_bats)
    if (b.mac == mac) return &b;
  return nullptr;
}

static void set_status(Bat &b, const char *s) {
  b.status = s;
  s_ver++;
}

// ---------------------------------------------------------------- Speichern
static void save() {
  Preferences p;
  p.begin("bms", false);
  p.clear();
  uint8_t n = 0;
  for (auto &b : s_bats) {
    if (b.remove) continue;
    char k[4];
    snprintf(k, sizeof(k), "m%u", n); p.putString(k, b.mac.c_str());
    snprintf(k, sizeof(k), "t%u", n); p.putUChar(k, b.type);
    snprintf(k, sizeof(k), "k%u", n); p.putUChar(k, b.model);
    snprintf(k, sizeof(k), "a%u", n); p.putString(k, b.name.c_str());
    snprintf(k, sizeof(k), "c%u", n); p.putUShort(k, b.cap_ah);
    snprintf(k, sizeof(k), "z%u", n); p.putUChar(k, b.cells);
    n++;
  }
  p.putUChar("n", n);
  p.end();
}

static void load() {
  Preferences p;
  if (!p.begin("bms", true)) return;
  uint8_t n = p.getUChar("n", 0);
  for (uint8_t i = 0; i < n && i < BMS_MAX; i++) {
    char k[4];
    Bat b;
    snprintf(k, sizeof(k), "m%u", i); b.mac = p.getString(k, "").c_str();
    snprintf(k, sizeof(k), "t%u", i); b.type = p.getUChar(k, 0);
    snprintf(k, sizeof(k), "k%u", i); b.model = (BmsModel)p.getUChar(k, BMS_SUPERB);
    snprintf(k, sizeof(k), "a%u", i); b.name = p.getString(k, "Akku").c_str();
    snprintf(k, sizeof(k), "c%u", i); b.cap_ah = p.getUShort(k, 0);
    snprintf(k, sizeof(k), "z%u", i); b.cells = p.getUChar(k, 4);
    if (!b.mac.empty()) s_bats.push_back(b);
  }
  p.end();
}

// ---------------------------------------------------------------- Super B: Statuspaket
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static float bef(const uint8_t *p) {
  uint32_t u = be32(p);
  float f;
  memcpy(&f, &u, 4);
  return f;
}

// [1] Flags, [2] SoC, [3] SoH, [4..7] Restzeit s, [8..11] Strom A, [12..15] Spannung V (Big Endian)
static void sb_status(const std::string &mac, const uint8_t *d, size_t n) {
  if (n < 16) return;
  Lock l;
  Bat *b = find_mac(mac);
  if (!b) return;
  b->flags = d[1];
  b->soc = d[2];
  b->soh = d[3];
  b->ttg = (int32_t)be32(d + 4);
  float a = bef(d + 8), v = bef(d + 12);
  b->amp = isfinite(a) ? a : NAN;
  b->volt = isfinite(v) ? v : NAN;
  if (!b->has) {
    char raw[3 * 16 + 1] = "";
    for (int i = 0; i < 16; i++) snprintf(raw + 3 * i, 4, "%02X ", d[i]);
    logf("%s: erstes Statuspaket %s", b->name.c_str(), raw);
  }
  b->has = true;
  b->last_ms = millis();
  b->status = "Verbunden";
  s_ver++;
}

// Textkonsole: alles einsammeln (nur fuers Entsperren)
static void sb_rpc(const uint8_t *d, size_t n) {
  Lock l;
  if (s_rpc.size() < 256) s_rpc.append((const char *)d, n);
}

static bool sb_write(NimBLERemoteCharacteristic *c, const char *txt) {
  size_t len = strlen(txt);
  for (size_t off = 0; off < len; off += SB_CHUNK) {
    size_t k = len - off < SB_CHUNK ? len - off : SB_CHUNK;
    if (!c->writeValue((const uint8_t *)txt + off, k, false)) return false;   // ohne Antwort, wie die App
    delay(20);
  }
  return true;
}

// Ablauf wie die App: verbinden, Konsole mit "SuperB" entsperren, Geraete-Info lesen, Status abonnieren
static bool sb_connect(Bat &b, NimBLEClient *c) {
  NimBLERemoteService *svc = c->getService(NimBLEUUID(SB_SERVICE));
  NimBLERemoteCharacteristic *st = svc ? svc->getCharacteristic(NimBLEUUID(SB_STATUS)) : nullptr;
  NimBLERemoteCharacteristic *rpc = svc ? svc->getCharacteristic(NimBLEUUID(SB_RPC)) : nullptr;
  if (!svc || !st || !rpc) {
    logf("%s: kein Super-B-Dienst gefunden", b.name.c_str());
    return false;
  }
  {
    Lock l;
    s_rpc.clear();
  }
  if (!rpc->subscribe(true, [](NimBLERemoteCharacteristic *, uint8_t *d, size_t n, bool) { sb_rpc(d, n); }, true))
    logf("%s: Konsole nicht abonnierbar", b.name.c_str());
  bool unlocked = false;
  for (int attempt = 1; attempt <= 3 && !unlocked && c->isConnected(); attempt++) {
    delay(500);
    sb_write(rpc, "\n");   // Konsole zuruecksetzen
    delay(150);
    {
      Lock l;
      s_rpc.clear();
    }
    std::string line = std::string(SB_UNLOCK) + "\n";
    if (!sb_write(rpc, line.c_str())) {
      logf("%s: Entsperren nicht gesendet (Versuch %d)", b.name.c_str(), attempt);
      continue;
    }
    uint32_t t0 = millis();
    while (millis() - t0 < 2000) {
      {
        Lock l;
        if (s_rpc.find('\n') != std::string::npos) break;
      }
      delay(20);
    }
    Lock l;
    std::string ans = s_rpc.substr(0, s_rpc.find('\n'));
    while (!ans.empty() && (ans.back() == '\r' || ans.back() == '\n')) ans.pop_back();
    if (s_rpc.find('\n') != std::string::npos) {
      unlocked = true;
      logf("%s: entsperrt, Antwort \"%.40s\"", b.name.c_str(), ans.c_str());
    } else {
      logf("%s: keine Antwort beim Entsperren (Versuch %d)", b.name.c_str(), attempt);
    }
  }
  delay(500);
  rpc->unsubscribe();   // Konsole wird danach nicht mehr gebraucht
  // Geraete-Info (0x180A): Modell, Seriennummer, Firmware
  NimBLERemoteService *dis = c->getService(NimBLEUUID((uint16_t)0x180A));
  auto rd = [&](uint16_t u) -> std::string {
    NimBLERemoteCharacteristic *ch = dis ? dis->getCharacteristic(NimBLEUUID(u)) : nullptr;
    if (!ch || !ch->canRead()) return "";
    NimBLEAttValue v = ch->readValue();
    std::string s((const char *)v.data(), v.length());
    while (!s.empty() && (s.back() == 0 || s.back() == ' ')) s.pop_back();
    return s;
  };
  std::string model = rd(0x2A24), serial = rd(0x2A25), fw = rd(0x2A26);
  {
    Lock l;
    b.model_text = model.empty() ? std::string("Super B") : "Super B " + model;
    b.serial = serial;
    b.firmware = fw;
  }
  logf("%s: %s, Seriennr. %s, Firmware %s", b.name.c_str(), model.c_str(), serial.c_str(), fw.c_str());
  std::string mac = b.mac;
  if (!st->subscribe(true, [mac](NimBLERemoteCharacteristic *, uint8_t *d, size_t n, bool) { sb_status(mac, d, n); },
                     true)) {
    logf("%s: Statusmeldungen nicht abonnierbar", b.name.c_str());
    return false;
  }
  return true;
}

// ---------------------------------------------------------------- Power Quarry
static const PqProduct *pq_by_code(const std::string &code) {
  for (auto &p : PQ_PRODUCTS)
    if (code == p.code) return &p;
  return nullptr;
}
static const PqProduct *pq_by_prefix(const std::string &name) {
  for (auto &p : PQ_PRODUCTS)
    if (name.compare(0, strlen(p.code), p.code) == 0) return &p;
  for (auto &p : PQ_PRODUCTS)
    if (name.compare(0, strlen(p.name), p.name) == 0) return &p;
  return nullptr;
}
static std::string upper(std::string s) {
  for (auto &c : s) c = toupper((unsigned char)c);
  return s;
}

// Erkennung wie die App (FilterDev): Herstellerdaten "CBT^" (Version > 1, 2 = ASCII), sonst Name mit Produktcode
// (10 Zeichen) oder Name beginnt mit "VB"/"CBT" (binaer). model = BMS_UNKNOWN: kein Power-Quarry-Akku.
static BmsModel pq_detect(const std::string &raw_name, const std::string &mfr, const PqProduct **prod) {
  *prod = nullptr;
  if (raw_name.empty()) return BMS_UNKNOWN;
  std::string name = upper(raw_name);
  while (!name.empty() && name.back() == ' ') name.pop_back();
  if (mfr.size() >= 6 && mfr.compare(0, 4, "CBT^") == 0 && (uint8_t)mfr[4] > 1) {
    *prod = pq_by_prefix(name);
    return (uint8_t)mfr[4] == 2 ? BMS_PQ_ASCII : BMS_UNKNOWN;   // andere Versionen kennt auch die App nicht
  }
  if (name.size() == 10 && (*prod = pq_by_code(name.substr(0, 2))) != nullptr) return BMS_PQ_HEX;
  if (name.compare(0, 2, "VB") == 0 || name.compare(0, 3, "CBT") == 0) {
    *prod = pq_by_prefix(name);
    return BMS_PQ_HEX;
  }
  return BMS_UNKNOWN;
}

static void pq_rx(const std::string &mac, const uint8_t *d, size_t n) {
  Lock l;
  Bat *b = find_mac(mac);
  if (b && b->rx.size() < 600) b->rx.append((const char *)d, n);
}

static bool pq_write(NimBLERemoteCharacteristic *c, const uint8_t *p, size_t n, bool resp) {
  for (size_t off = 0; off < n; off += 20) {
    size_t k = n - off < 20 ? n - off : 20;
    if (!c->writeValue(p + off, k, resp)) return false;
    delay(30);
  }
  return true;
}

// Diagnose der letzten fehlgeschlagenen Abfrage (fuers Protokoll): gesendet, empfangen (Laenge + Hex/Text)
static std::string s_pq_diag;
static size_t s_pq_rxlen = 0;
static void pq_note(const uint8_t *req, size_t n, bool wrote, const std::string &rx) {
  char b[200];
  int k = snprintf(b, sizeof(b), "gesendet");
  for (size_t i = 0; i < n && i < 8; i++) k += snprintf(b + k, sizeof(b) - k, " %02X", req[i]);
  if (!wrote) {
    snprintf(b + k, sizeof(b) - k, " – Schreiben abgelehnt");
  } else if (rx.empty()) {
    snprintf(b + k, sizeof(b) - k, " – nichts empfangen");
  } else {
    k += snprintf(b + k, sizeof(b) - k, " – empfangen %u B:", (unsigned)rx.size());
    for (size_t i = 0; i < rx.size() && i < 24 && k < (int)sizeof(b) - 4; i++)
      k += snprintf(b + k, sizeof(b) - k, " %02X", (uint8_t)rx[i]);
  }
  s_pq_diag = b;
  s_pq_rxlen = rx.size();
}

// Binaer: AA 55 CMD LEN DATA SUM 0A 0D (Antwort: 0D 0A), SUM = (CMD + LEN + DATA) & 0xFF
static bool pq_hex_frame(const std::string &rx, uint8_t cmd, std::string &data) {
  const uint8_t *b = (const uint8_t *)rx.data();
  size_t n = rx.size();
  for (size_t i = 0; i + 4 <= n; i++) {
    if (b[i] != 0xAA || b[i + 1] != 0x55) continue;
    size_t total = (size_t)b[i + 3] + 7;
    if (i + total > n) return false;
    unsigned sum = 0;
    for (size_t k = 2; k < total - 3; k++) sum += b[i + k];
    // Ende laut App 0A 0D – der Akku (VB037, getestet 2026-11-13) schickt 0D 0A: beides zulassen
    bool tail = (b[i + total - 2] == 0x0A && b[i + total - 1] == 0x0D) || (b[i + total - 2] == 0x0D && b[i + total - 1] == 0x0A);
    if ((sum & 0xFF) == b[i + total - 3] && tail && b[i + 2] == cmd) {
      data.assign((const char *)b + i + 4, b[i + 3]);
      return true;
    }
  }
  return false;
}

// ASCII: "~" … "\r", Pruefsumme und Laenge pruefen, INFO als Bytes
static bool pq_ascii_frame(const std::string &rx, std::string &info) {
  size_t a = rx.find('~');
  if (a == std::string::npos) return false;
  size_t e = rx.find('\r', a);
  if (e == std::string::npos) return false;
  std::string s = rx.substr(a + 1, e - a - 1);
  if (s.size() < 16 || s.size() % 2) return false;
  unsigned sum = 0;
  for (size_t i = 0; i + 4 < s.size(); i++) sum += (unsigned char)s[i];
  std::string b;
  for (size_t i = 0; i < s.size(); i += 2) {
    int h = isxdigit((unsigned char)s[i]) ? (isdigit((unsigned char)s[i]) ? s[i] - '0' : (toupper(s[i]) - 'A' + 10)) : -1;
    int l = isxdigit((unsigned char)s[i + 1]) ? (isdigit((unsigned char)s[i + 1]) ? s[i + 1] - '0' : (toupper(s[i + 1]) - 'A' + 10)) : -1;
    if (h < 0 || l < 0) return false;
    b.push_back((char)(h << 4 | l));
  }
  unsigned cs = (uint8_t)b[b.size() - 2] << 8 | (uint8_t)b[b.size() - 1];
  if (((sum + cs) & 0xFFFF) != 0) return false;
  unsigned lenid = ((uint8_t)b[4] << 8 | (uint8_t)b[5]) & 0xFFF;
  if (lenid != s.size() - 16) return false;
  info = b.substr(6, b.size() - 8);
  return true;
}

// Modbus-RTU (Bulltron): D2 03 LEN DATEN CRClo CRChi, CRC-16/Modbus (0xFFFF, 0xA001)
static uint16_t mb_crc(const uint8_t *d, size_t n) {
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= d[i];
    for (int k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xA001 : (c >> 1);
  }
  return c;
}
static uint8_t s_mb_addr = 0xD2;   // erwartete Antwortadresse (Daly D2: 0xD2, Daly 0x81: 0x51, Renogy: 0x30)
static bool mb_frame(const std::string &rx, uint8_t len, std::string &data) {
  const uint8_t *b = (const uint8_t *)rx.data();
  size_t n = rx.size();
  for (size_t i = 0; i + 2 < n; i++) {
    if (b[i] != s_mb_addr || b[i + 1] != 0x03 || b[i + 2] != len) continue;
    size_t total = (size_t)len + 5;
    if (i + total > n) continue;
    uint16_t c = mb_crc(b + i, total - 2);
    if (b[i + total - 2] == (c & 0xFF) && b[i + total - 1] == (c >> 8)) {
      data.assign((const char *)b + i + 3, len);
      return true;
    }
  }
  return false;
}

// JBD: DD CMD STATUS LEN DATEN SUM(2, BE) 77, SUM = 0x10000 - Summe(STATUS..DATEN)
static bool jbd_frame(const std::string &rx, uint8_t cmd, std::string &data) {
  const uint8_t *b = (const uint8_t *)rx.data();
  size_t n = rx.size();
  for (size_t i = 0; i + 4 <= n; i++) {
    if (b[i] != 0xDD || b[i + 1] != cmd) continue;
    size_t len = b[i + 3], total = len + 7;
    if (i + total > n) return false;
    if (b[i + total - 1] != 0x77 || b[i + 2] != 0) continue;
    unsigned sum = 0;
    for (size_t k = 2; k < 4 + len; k++) sum += b[i + k];
    if (((0x10000 - sum) & 0xFFFF) != (unsigned)(b[i + 4 + len] << 8 | b[i + 5 + len])) continue;
    data.assign((const char *)b + i + 4, len);
    return true;
  }
  return false;
}

// JK02: 55 AA EB 90 TYP …, 300 Byte, Byte 299 = Summe der Bytes 0..298 (unteres Byte).
// TYP 2 = Zelldaten, 3 = Geraeteinfo (Firmware ab Byte 30)
static uint8_t s_jk_type = 0x02;
static bool jk_frame(const std::string &rx, std::string &data) {
  const uint8_t *b = (const uint8_t *)rx.data();
  size_t n = rx.size();
  for (size_t i = 0; i + 300 <= n; i++) {
    if (b[i] != 0x55 || b[i + 1] != 0xAA || b[i + 2] != 0xEB || b[i + 3] != 0x90 || b[i + 4] != s_jk_type) continue;
    uint8_t sum = 0;
    for (size_t k = 0; k < 299; k++) sum += b[i + k];
    if (sum != b[i + 299]) continue;
    data.assign((const char *)b + i, 300);
    return true;
  }
  return false;
}

// Anfrage senden, auf passende Antwort warten (Puffer gehoert dem Akku, gefuellt per Notify).
// kind: 0 Power Quarry binaer (cmd = Befehl), 1 Power Quarry ASCII, 2 Modbus (cmd = erwartete Datenlaenge),
//       3 JBD (cmd), 4 JK (Zelldaten-Rahmen), 5 LiTime (Antwort ab cmd*2 Byte, dann kurz ruhig)
static bool pq_ask(const std::string &mac, NimBLERemoteCharacteristic *wr, const uint8_t *req, size_t n, int kind,
                   uint8_t cmd, std::string &out, uint32_t timeout_ms) {
  {
    Lock l;
    Bat *b = find_mac(mac);
    if (!b) return false;
    b->rx.clear();
  }
  bool resp = false;
  {
    Lock l;
    Bat *b = find_mac(mac);
    resp = b && b->wr_resp;
  }
  if (!pq_write(wr, req, n, resp)) {
    pq_note(req, n, false, "");
    return false;
  }
  uint32_t t0 = millis();
  while (millis() - t0 < timeout_ms) {
    delay(20);
    Lock l;
    Bat *b = find_mac(mac);
    if (!b) return false;
    bool got = false;
    switch (kind) {
      case 1: got = pq_ascii_frame(b->rx, out); break;
      case 2: got = mb_frame(b->rx, cmd, out); break;
      case 3: got = jbd_frame(b->rx, cmd, out); break;
      case 4: got = jk_frame(b->rx, out); break;
      case 5: {   // LiTime: kein Rahmen, feste Laenge – warten bis genug da ist und 150 ms nichts mehr kommt
        static size_t last_n = 0;
        static uint32_t last_ms = 0;
        if (b->rx.size() != last_n) {
          last_n = b->rx.size();
          last_ms = millis();
        } else if (last_n >= (size_t)cmd * 2 && millis() - last_ms > 150) {
          out = b->rx;
          last_n = 0;
          got = true;
        }
        break;
      }
      default: got = pq_hex_frame(b->rx, cmd, out);
    }
    if (got) return true;
  }
  Lock l;
  Bat *b = find_mac(mac);
  pq_note(req, n, true, b ? b->rx : std::string());
  return false;
}

static bool pq_connect(Bat &b, NimBLEClient *c) {
  NimBLERemoteService *ws = c->getService(NimBLEUUID((uint16_t)0xFFE5));
  NimBLERemoteService *ns = c->getService(NimBLEUUID((uint16_t)0xFFE0));
  NimBLERemoteCharacteristic *wr = ws ? ws->getCharacteristic(NimBLEUUID((uint16_t)0xFFE9)) : nullptr;
  NimBLERemoteCharacteristic *nt = ns ? ns->getCharacteristic(NimBLEUUID((uint16_t)0xFFE4)) : nullptr;
  if (!wr || !nt) {
    logf("%s: Dienste FFE5/FFE9 bzw. FFE0/FFE4 fehlen", b.name.c_str());
    return false;
  }
  if (!b.has)
    logf("%s: FFE9 %s%s, FFE4 %s%s", b.name.c_str(), wr->canWriteNoResponse() ? "ohne-Antw " : "",
         wr->canWrite() ? "mit-Antw" : "", nt->canNotify() ? "notify " : "", nt->canIndicate() ? "indicate" : "");
  b.wr_resp = !wr->canWriteNoResponse() && wr->canWrite();
  std::string mac = b.mac;
  bool use_notify = nt->canNotify() || !nt->canIndicate();
  if (!nt->subscribe(use_notify, [mac](NimBLERemoteCharacteristic *, uint8_t *d, size_t n, bool) { pq_rx(mac, d, n); }, true)) {
    logf("%s: Notify nicht abonnierbar", b.name.c_str());
    return false;
  }
  b.wr = wr;
  b.next_poll = millis() + 300;
  if (!b.has) logf("%s: verbunden, Protokoll %s", b.name.c_str(), b.model == BMS_PQ_ASCII ? "ASCII" : "binaer");
  return true;
}

// Bulltron: Dienst 0xFFF0, Antworten 0xFFF1 (Notify), Anfragen 0xFFF2 (die App schreibt mit Bestaetigung und
// abonniert auch dort Notify). Ohne Pairing.
// Gemeinsam fuer JBD/JK/LiTime: Dienst + Notify-/Schreib-Characteristic suchen, Notify abonnieren
static bool generic_connect(Bat &b, NimBLEClient *c, uint16_t svc_id, uint16_t rx_id, uint16_t tx_id, const char *what) {
  NimBLERemoteService *svc = c->getService(NimBLEUUID(svc_id));
  NimBLERemoteCharacteristic *nt = svc ? svc->getCharacteristic(NimBLEUUID(rx_id)) : nullptr;
  NimBLERemoteCharacteristic *wr = svc ? svc->getCharacteristic(NimBLEUUID(tx_id)) : nullptr;
  if (!nt || !wr) return false;
  std::string mac = b.mac;
  if (!nt->subscribe(true, [mac](NimBLERemoteCharacteristic *, uint8_t *d, size_t n, bool) { pq_rx(mac, d, n); }, true)) {
    logf("%s: Notify nicht abonnierbar", b.name.c_str());
    return false;
  }
  b.wr = wr;
  b.wr_resp = !wr->canWriteNoResponse() && wr->canWrite();
  b.next_poll = millis() + 300;
  if (!b.has) logf("%s: verbunden (%s)", b.name.c_str(), what);
  return true;
}

static bool jbd_connect(Bat &b, NimBLEClient *c) {
  if (generic_connect(b, c, 0xFF00, 0xFF01, 0xFF02, "JBD")) return true;
  if (generic_connect(b, c, 0xFFF0, 0xFFF1, 0xFFF2, "JBD, Variante FFF0")) return true;   // z. B. DH04
  logf("%s: JBD-Dienst FF00/FFF0 fehlt", b.name.c_str());
  return false;
}

static bool jk_connect(Bat &b, NimBLEClient *c) {
  if (generic_connect(b, c, 0xFFE0, 0xFFE1, 0xFFE1, "JK02")) return true;
  logf("%s: JK-Dienst FFE0/FFE1 fehlt", b.name.c_str());
  return false;
}

// Renogy: Antworten auf 0xFFF1 (Dienst 0xFFF0), Anfragen auf 0xFFD1 (Dienst 0xFFD0)
static bool renogy_connect(Bat &b, NimBLEClient *c) {
  NimBLERemoteService *rs = c->getService(NimBLEUUID((uint16_t)0xFFF0));
  NimBLERemoteService *ts = c->getService(NimBLEUUID((uint16_t)0xFFD0));
  NimBLERemoteCharacteristic *nt = rs ? rs->getCharacteristic(NimBLEUUID((uint16_t)0xFFF1)) : nullptr;
  NimBLERemoteCharacteristic *wr = ts ? ts->getCharacteristic(NimBLEUUID((uint16_t)0xFFD1)) : nullptr;
  if (!nt || !wr) {
    logf("%s: Renogy-Dienste FFF0/FFF1 bzw. FFD0/FFD1 fehlen", b.name.c_str());
    return false;
  }
  std::string mac = b.mac;
  if (!nt->subscribe(true, [mac](NimBLERemoteCharacteristic *, uint8_t *d, size_t n, bool) { pq_rx(mac, d, n); }, true))
    return false;
  b.wr = wr;
  b.wr_resp = !wr->canWriteNoResponse() && wr->canWrite();
  b.next_poll = millis() + 300;
  if (!b.has) logf("%s: verbunden (Renogy)", b.name.c_str());
  return true;
}

// Eco-Worthy: jeder Notify ist ein ganzer Rahmen, beginnend mit A1/A2 (Version 1) oder MAC + A1/A2 (Version 2),
// am Ende CRC-16/Modbus (Little Endian). Version 2 wird um 2 Byte vorne aufgefuellt, dann gelten dieselben Positionen.
static void eco_rx(const std::string &mac, const uint8_t *d, size_t n) {
  if (n < 10) return;
  uint16_t c = mb_crc(d, n - 2);
  if (d[n - 2] != (c & 0xFF) || d[n - 1] != (c >> 8)) return;
  uint8_t m[6];
  if (sscanf(mac.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6) return;
  bool v2 = n > 7 && memcmp(d, m, 6) == 0 && (d[6] == 0xA1 || d[6] == 0xA2);
  bool v1 = d[0] == 0xA1 || d[0] == 0xA2;
  if (!v1 && !v2) return;
  std::string f = v2 ? std::string(2, '\0') + std::string((const char *)d, n) : std::string((const char *)d, n);
  uint8_t typ = v2 ? d[6] : d[0];
  Lock l;
  Bat *b = find_mac(mac);
  if (!b) return;
  (typ == 0xA1 ? b->eco_a1 : b->eco_a2) = f;
}

static bool eco_connect(Bat &b, NimBLEClient *c) {
  NimBLERemoteService *svc = c->getService(NimBLEUUID((uint16_t)0xFFF0));
  NimBLERemoteCharacteristic *nt = svc ? svc->getCharacteristic(NimBLEUUID((uint16_t)0xFFF1)) : nullptr;
  NimBLERemoteCharacteristic *wr = svc ? svc->getCharacteristic(NimBLEUUID((uint16_t)0xFFF2)) : nullptr;
  if (!nt || !wr) {
    logf("%s: Eco-Worthy-Dienst FFF0 mit FFF1/FFF2 fehlt", b.name.c_str());
    return false;
  }
  std::string mac = b.mac;
  if (!nt->subscribe(true, [mac](NimBLERemoteCharacteristic *, uint8_t *d, size_t n, bool) { eco_rx(mac, d, n); }, true))
    return false;
  b.wr = wr;
  b.wr_resp = !wr->canWriteNoResponse() && wr->canWrite();
  b.next_poll = millis() + 300;
  if (!b.has) logf("%s: verbunden (Eco-Worthy)", b.name.c_str());
  return true;
}

static bool litime_connect(Bat &b, NimBLEClient *c) {
  if (generic_connect(b, c, 0xFFE0, 0xFFE1, 0xFFE2, "LiTime")) return true;
  logf("%s: LiTime-Dienst FFE0 mit FFE1/FFE2 fehlt", b.name.c_str());
  return false;
}

static bool bt_connect(Bat &b, NimBLEClient *c) {
  NimBLERemoteService *svc = c->getService(NimBLEUUID((uint16_t)0xFFF0));
  NimBLERemoteCharacteristic *nt = svc ? svc->getCharacteristic(NimBLEUUID((uint16_t)0xFFF1)) : nullptr;
  NimBLERemoteCharacteristic *wr = svc ? svc->getCharacteristic(NimBLEUUID((uint16_t)0xFFF2)) : nullptr;
  if (!nt || !wr) {
    logf("%s: Dienst FFF0 mit FFF1/FFF2 fehlt", b.name.c_str());
    return false;
  }
  std::string mac = b.mac;
  auto cb = [mac](NimBLERemoteCharacteristic *, uint8_t *d, size_t n, bool) { pq_rx(mac, d, n); };
  if (!nt->subscribe(true, cb, true)) {
    logf("%s: Notify auf FFF1 nicht abonnierbar", b.name.c_str());
    return false;
  }
  if (wr->canNotify()) wr->subscribe(true, cb, true);
  b.wr = wr;
  b.wr_resp = wr->canWrite();
  b.next_poll = millis() + 300;
  if (!b.has) logf("%s: verbunden (Modbus)", b.name.c_str());
  return true;
}

static int16_t le16s(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static int16_t be16s(const uint8_t *p) { return (int16_t)(p[0] << 8 | p[1]); }

void ClientCB::onDisconnect(NimBLEClient *c, int reason) {
  Lock l;
  for (auto &b : s_bats)
    if (b.client == c) {
      b.connected = false;
      b.next_try = millis() + RECONNECT_MS;
      b.status = "Verbindung getrennt";
      s_ver++;
      logf("%s: getrennt (Grund %d) nach %lu s, Signal zuletzt %d dBm", b.name.c_str(), reason,
           (unsigned long)((millis() - b.conn_ms) / 1000), b.rssi);
    }
}

// Client vom Akku loesen (unter der Sperre) und erst danach ausserhalb der Sperre trennen/loeschen:
// die Trennmeldung von NimBLE (onDisconnect) braucht dieselbe Sperre.
static NimBLEClient *detach(Bat &b) {
  NimBLEClient *c = b.client;
  b.client = nullptr;
  b.connected = false;
  b.wr = nullptr;
  return c;
}
static void kill(NimBLEClient *c) {
  if (!c) return;
  if (c->isConnected()) c->disconnect();
  NimBLEDevice::deleteClient(c);
}

// ---------------------------------------------------------------- Bluetooth-Ablauf (Scan-Task von truma.cpp)
bool bms_on_adv(const NimBLEAdvertisedDevice *dev) {
  if (!s_disc_until) return false;
  bool svc = dev->isAdvertisingService(NimBLEUUID(SB_SERVICE));
  std::string name = dev->haveName() ? dev->getName() : "";
  std::string ln = lower(name);
  bool hint = false;
  for (const char *h : SB_HINTS) hint |= ln.find(h) != std::string::npos;
  BmsModel model = BMS_SUPERB;
  const PqProduct *prod = nullptr;
  if (!svc && !hint) {
    model = pq_detect(name, dev->haveManufacturerData() ? dev->getManufacturerData() : std::string(), &prod);
    std::string un = name;
    for (auto &ch : un) ch = toupper((unsigned char)ch);
    bool ff00 = dev->isAdvertisingService(NimBLEUUID((uint16_t)0xFF00));
    // eindeutige Merkmale nach aiobmsble zuerst: Hersteller-IDs und Namensanfaenge
    uint16_t mid = 0;
    if (dev->haveManufacturerData()) {
      std::string md = dev->getManufacturerData();
      if (md.size() >= 2) mid = (uint8_t)md[0] | (uint8_t)md[1] << 8;
    }
    if (model == BMS_UNKNOWN && (un.rfind("DL-", 0) == 0 || mid == 0x102 || mid == 0x104 || mid == 0x107 ||
                                 mid == 0x302 || mid == 0x303 || mid == 0x402))
      model = BMS_BULLTRON;   // Daly
    if (model == BMS_UNKNOWN && mid == 0x585A) model = BMS_LITIME;   // LiTime/Redodo
    if (model == BMS_UNKNOWN && un.rfind("BT-TH-", 0) == 0) model = BMS_RENOGY;
    if (model == BMS_UNKNOWN && (un.rfind("ECO-WORTHY", 0) == 0 || un.rfind("DCHOUSE", 0) == 0)) model = BMS_ECOWORTHY;
    if (model == BMS_UNKNOWN && un.rfind("JK", 0) == 0) model = BMS_JK;   // "JK-B2A24S…", "JK_…"
    // LiTime / Redodo / PowerQueen: "L-12100…", "R-12…", "P-12…"
    if (model == BMS_UNKNOWN && un.size() >= 4 && (un[0] == 'L' || un[0] == 'R' || un[0] == 'P') && un[1] == '-' &&
        isdigit((unsigned char)un[2]) && isdigit((unsigned char)un[3]))
      model = BMS_LITIME;
    // JBD / Xiaoxiang: Dienst 0xFF00 oder typische Namen
    if (model == BMS_UNKNOWN && (ff00 || un.rfind("JBD", 0) == 0 || un.find("XIAOXIANG") != std::string::npos ||
                                 ((un.rfind("SP", 0) == 0 || un.rfind("DP", 0) == 0) && un.size() > 2 &&
                                  isdigit((unsigned char)un[2]))))
      model = BMS_JBD;
    if (model == BMS_UNKNOWN) {
      // Bulltron: "DL"/"JHB" im Namen; in den rohen Werbedaten "JHB" oder "DL" zusammen mit Dienst 0xFFF0
      // (zwei Buchstaben allein treffen zufaellig auch fremde Geraete)
      const std::vector<uint8_t> &pl = dev->getPayload();
      std::string raw(pl.begin(), pl.end());
      bool fff0 = dev->isAdvertisingService(NimBLEUUID((uint16_t)0xFFF0));
      if (name.find("DL") != std::string::npos || name.find("JHB") != std::string::npos ||
          raw.find("JHB") != std::string::npos || (fff0 && raw.find("DL") != std::string::npos))
        model = BMS_BULLTRON;
      else
        return false;
    }
  }
  std::string mac = lower(dev->getAddress().toString());
  Lock l;
  for (auto &f : s_found)
    if (f.mac == mac) {
      f.rssi = dev->getRSSI();
      if (!name.empty()) f.name = name;
      f.service |= svc;
      return true;
    }
  if (s_found.size() >= 12) return true;
  BmsFound f;
  f.mac = mac;
  f.name = name;
  f.addr_type = dev->getAddress().getType();
  f.rssi = dev->getRSSI();
  f.model = model;
  f.service = svc;
  if (prod) {
    f.product = prod->name;
    f.capacity_ah = prod->ah;
    f.cells = prod->cells;
  }
  f.configured = find_mac(mac) != nullptr;
  s_found.push_back(f);
  s_ver++;
  logf("Gefunden: %s \"%s\" %d dBm, %s%s%s", mac.c_str(), name.c_str(), f.rssi, bms_model_text(model),
       svc ? " (Super-B-Dienst)" : "", model == BMS_PQ_ASCII ? " ASCII" : model == BMS_PQ_HEX ? " binaer" : "");
  if (model == BMS_BULLTRON) f.service = false;
  return true;
}

void bms_scan_wanted(bool &, bool &active) {
  if (!s_disc_until) return;
  if ((int32_t)(millis() - s_disc_until) >= 0) {
    s_disc_until = 0;
    s_ver++;
    return;
  }
  active = true;
}

bool bms_want_connect() {
  std::vector<NimBLEClient *> dead;
  bool want = false;
  {
    Lock l;
    uint32_t now = millis();
    // Entfernte Akkus hier aufraeumen (Clients gehoeren dem Scan-Task)
    for (size_t i = 0; i < s_bats.size();) {
      if (s_bats[i].remove) {
        dead.push_back(detach(s_bats[i]));
        s_bats.erase(s_bats.begin() + i);
        s_ver++;
      } else {
        i++;
      }
    }
    for (auto &b : s_bats) {
      bool stale = b.connected && (b.last_ms ? now - b.last_ms > STALE_MS : now - b.conn_ms > STALE_MS);
      if (stale) {   // verbunden, aber stumm
        logf("%s: keine Werte mehr – verbinde neu", b.name.c_str());
        dead.push_back(detach(b));
        b.next_try = now;
      }
      if (!b.connected && (int32_t)(now - b.next_try) >= 0) want = true;
    }
    if (s_disc_until) want = false;   // waehrend der Suche nicht verbinden
  }
  for (auto *c : dead) kill(c);
  return want;
}

void bms_connect_now() {
  std::string mac;
  uint8_t type = 0;
  NimBLEClient *old = nullptr;
  {
    Lock l;
    uint32_t now = millis();
    for (auto &b : s_bats)
      if (!b.connected && (int32_t)(now - b.next_try) >= 0) {
        mac = b.mac;
        type = b.type;
        b.next_try = now + RETRY_MS;   // bis zum Ergebnis nicht erneut
        set_status(b, "Verbinde …");
        old = detach(b);
        break;
      }
  }
  kill(old);
  if (mac.empty()) return;
  NimBLEClient *c = NimBLEDevice::createClient();
  c->setClientCallbacks(&s_ccb, false);
  c->setConnectTimeout(8000);
  // Dauerverbindung neben WLAN (gemeinsames Funkteil): Takt 30–50 ms, Zeitgrenze 6 s statt knapper Vorgabe
  // (Power Quarry trennte sonst nach ~10 s mit "Grund 520" = Connection Timeout)
  c->setConnectionParams(24, 40, 0, 600);
  {
    Lock l;
    Bat *b = find_mac(mac);
    if (!b) {
      NimBLEDevice::deleteClient(c);
      return;
    }
    b->client = c;
  }
  bool quiet = false;
  {
    Lock l;
    Bat *b = find_mac(mac);
    quiet = b && b->polled() && b->has;   // Kurzverbindungen: nur beim ersten Mal und bei Fehlern ins Protokoll
  }
  if (!quiet) logf("Verbinde %s …", mac.c_str());
  bool ok = c->connect(NimBLEAddress(mac, type), true);
  if (!ok) {
    {
      Lock l;
      Bat *b = find_mac(mac);
      if (b) {
        detach(*b);
        b->next_try = millis() + RETRY_MS;
        set_status(*b, "Nicht erreichbar – Akku in Reichweite? App geschlossen?");
      }
    }
    kill(c);
    logf("%s: Verbindung fehlgeschlagen", mac.c_str());
    return;
  }
  Bat copy;
  {
    Lock l;
    Bat *b = find_mac(mac);
    if (!b) return;
    b->connected = true;
    b->conn_ms = millis();
    b->last_ms = 0;
    copy = *b;
  }
  bool good = false;
  if (copy.model == BMS_SUPERB) good = sb_connect(copy, c);
  else if (copy.model == BMS_PQ_HEX || copy.model == BMS_PQ_ASCII) good = pq_connect(copy, c);
  else if (copy.model == BMS_BULLTRON) good = bt_connect(copy, c);
  else if (copy.model == BMS_JBD) good = jbd_connect(copy, c);
  else if (copy.model == BMS_JK) good = jk_connect(copy, c);
  else if (copy.model == BMS_LITIME) good = litime_connect(copy, c);
  else if (copy.model == BMS_RENOGY) good = renogy_connect(copy, c);
  else if (copy.model == BMS_ECOWORTHY) good = eco_connect(copy, c);
  NimBLEClient *dead = nullptr;
  {
    Lock l;
    Bat *b = find_mac(mac);
    if (!b) {
      dead = c;   // waehrenddessen entfernt
    } else {
      b->model_text = copy.model_text;
      b->serial = copy.serial;
      b->firmware = copy.firmware;
      b->wr = copy.wr;
      b->next_poll = copy.next_poll;
      b->wr_resp = copy.wr_resp;
      if (!good || !c->isConnected()) {
        dead = detach(*b);
        b->next_try = millis() + RETRY_MS;
        set_status(*b, "Verbunden, aber keine Werte – siehe Protokoll");
      } else if (!b->has) {
        set_status(*b, "Verbunden, warte auf Werte …");
      }
    }
  }
  kill(dead);
}

bool bms_want_poll() {
  if (!s_mtx) return false;
  Lock l;
  uint32_t now = millis();
  for (auto &b : s_bats)
    if (b.connected && b.wr && (int32_t)(now - b.next_poll) >= 0) return true;
  return false;
}

void bms_poll_now() {
  std::string mac;
  NimBLERemoteCharacteristic *wr = nullptr;
  BmsModel model = BMS_UNKNOWN;
  uint8_t cells = 4;
  bool full = true;   // auch die selten noetigen Werte (Temperatur, Kapazitaet, Zyklen, Alarme)
  NimBLEClient *cl = nullptr;
  {
    Lock l;
    uint32_t now = millis();
    for (auto &b : s_bats)
      if (b.connected && b.wr && (int32_t)(now - b.next_poll) >= 0) {
        mac = b.mac;
        wr = b.wr;
        model = b.model;
        cells = b.cells ? b.cells : 4;
        b.next_poll = now + PQ_POLL_MS;
        full = !b.has || b.polls % 3 == 0;
        b.polls++;
        cl = b.client;
        break;
      }
  }
  if (mac.empty()) return;
  // Abgefragte Akkus nach der Abfrage trennen (auch bei Fehlern), naechste Verbindung in PQ_POLL_MS
  struct AfterPoll {
    std::string mac;
    ~AfterPoll() {
      NimBLEClient *c = nullptr;
      {
        Lock l;
        Bat *b = find_mac(mac);
        if (!b || !b->polled()) return;
        c = detach(*b);
        b->next_try = millis() + PQ_POLL_MS;
      }
      kill(c);
    }
  } after_poll{mac};
  bool ok = false;
  float soc = NAN, v = NAN, a = NAN, temp = NAN;
  int cyc = -1, cap = 0;
  std::vector<uint16_t> cmv;
  uint32_t alarms = 0;
  bool chg_on = true, dis_on = true;
  int mos = -1;
  std::string d;
  if (cl && cl->isConnected()) {
    int r = cl->getRssi();
    Lock l;
    Bat *b = find_mac(mac);
    if (b && r) b->rssi = r;
  }
  int state = 0;   // Bulltron: 1 laedt, 2 entlaedt
  float rem_ah = NAN;
  if (model == BMS_JBD) {
    // Grunddaten: DD A5 03 00 FF FD 77 -> V u16/100, I s16/100 (+ laden), Rest u16/100 Ah, Nenn u16/100 Ah, Zyklen,
    // Datum, Balance(2x2), Schutz u16, Version, SoC, FET (Bit0 Laden, Bit1 Entladen), Zellen, NTC, NTC u16 0,1 K
    static const uint8_t REQ[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
    for (int attempt = 0; attempt < 2 && !ok; attempt++) {
      if (!pq_ask(mac, wr, REQ, 7, 3, 0x03, d, 2000) || d.size() < 23) continue;
      const uint8_t *q = (const uint8_t *)d.data();
      auto be = [&](int o) -> uint16_t { return q[o] << 8 | q[o + 1]; };
      v = be(0) / 100.0f;
      a = (int16_t)be(2) / 100.0f;
      cap = (int)lroundf(be(6) / 100.0f);
      cyc = be(8);
      alarms = be(16);
      soc = q[19];
      chg_on = q[20] & 0x01;
      dis_on = q[20] & 0x02;
      int nt = q[22];
      float sum = 0;
      int cnt = 0;
      for (int t = 0; t < nt && 23 + 2 * t + 1 < (int)d.size(); t++) {
        sum += be(23 + 2 * t) / 10.0f - 273.15f;
        cnt++;
      }
      if (cnt) temp = sum / cnt;
      ok = true;
    }
  } else if (model == BMS_JK) {
    // JK02: Geraete-Info (0x97) und Zelldaten (0x96) anfordern, dann kommt der 300-Byte-Rahmen Typ 2
    auto jk_req = [&](uint8_t cmd) {
      uint8_t f[20] = {0xAA, 0x55, 0x90, 0xEB, cmd, 0x00};
      uint8_t sum = 0;
      for (int i = 0; i < 19; i++) sum += f[i];
      f[19] = sum;
      return std::string((const char *)f, 20);
    };
    std::string r97 = jk_req(0x97), r96 = jk_req(0x96);
    int sw = -1;
    {
      Lock l;
      Bat *b = find_mac(mac);
      if (b) sw = b->jk_sw;
    }
    // Geraeteinfo (Typ 3): Firmware-Version ab Byte 30 ("11.XW" …) – ab Version 11 Aufbau 32S (wie aiobmsble)
    s_jk_type = 0x03;
    if (pq_ask(mac, wr, (const uint8_t *)r97.data(), 20, 4, 0, d, 3000) && d.size() >= 38) {
      sw = atoi(d.substr(30, 8).c_str());
      Lock l;
      Bat *b = find_mac(mac);
      if (b && b->jk_sw != sw) {
        b->jk_sw = sw;
        logf("%s: JK-Firmware %d, Aufbau %s", b->name.c_str(), sw, sw >= 11 ? "32S" : "24S");
      }
    }
    s_jk_type = 0x02;
    for (int attempt = 0; attempt < 2 && !ok; attempt++) {
      if (!pq_ask(mac, wr, (const uint8_t *)r96.data(), 20, 4, 0, d, 5000) || d.size() < 300) continue;
      const uint8_t *q = (const uint8_t *)d.data();
      // 24S-/32S-Aufbau: nach Firmware (ab 11 = 32S); ohne Info wie bisher an Byte 287 erkennen
      int o = sw >= 0 ? (sw >= 11 ? 32 : 0) : (q[287] > 0 ? 32 : 0);
      auto u16 = [&](int x) -> uint16_t { return q[x] | q[x + 1] << 8; };
      auto u32 = [&](int x) -> uint32_t { return q[x] | q[x + 1] << 8 | q[x + 2] << 16 | (uint32_t)q[x + 3] << 24; };
      v = u16(118 + o) / 1000.0f;
      a = (int32_t)u32(126 + o) / 1000.0f;
      float t1 = (int16_t)u16(130 + o) / 10.0f, t2 = (int16_t)u16(132 + o) / 10.0f;
      temp = (t1 + t2) / 2;
      soc = q[141 + o];
      cap = (int)lroundf(u32(146 + o) / 1000.0f);
      cyc = (int)u32(150 + o);
      chg_on = q[166 + o];
      dis_on = q[167 + o];
      alarms = o ? (u32(166) >> 16) : (u32(134) & 0xFFFF);   // wie aiobmsble
      ok = true;
    }
  } else if (model == BMS_LITIME) {
    // LiTime: 00 00 04 01 13 55 AA 17 -> Antwort (Little Endian): Gesamt-mV u32 @8, Strom mA i32 @48, Zelltemp i16 @52,
    // Rest u16/100 Ah @62, Voll u16/100 Ah @64, Schutz u32 @76, Zustand u16 @88, SoC u16 @90, Entladungen u32 @96
    static const uint8_t REQ[8] = {0x00, 0x00, 0x04, 0x01, 0x13, 0x55, 0xAA, 0x17};
    for (int attempt = 0; attempt < 2 && !ok; attempt++) {
      if (!pq_ask(mac, wr, REQ, 8, 5, 52, d, 3000) || d.size() < 100) continue;   // 52*2 = 104 Byte
      const uint8_t *q = (const uint8_t *)d.data();
      auto u16 = [&](int x) -> uint16_t { return q[x] | q[x + 1] << 8; };
      auto u32 = [&](int x) -> uint32_t { return q[x] | q[x + 1] << 8 | q[x + 2] << 16 | (uint32_t)q[x + 3] << 24; };
      v = u32(8) / 1000.0f;
      a = (int32_t)u32(48) / 1000.0f;
      temp = (int16_t)u16(52);
      cap = (int)lroundf(u16(64) / 100.0f);
      uint32_t prot = u32(76);
      uint16_t st = u16(88);
      soc = u16(90);
      cyc = (int)u32(96);
      chg_on = st != 4;
      dis_on = !(u32(68) == 0x80 || prot == 0x20 || prot == 0x80);
      alarms = prot;
      ok = true;
    }
  } else if (model == BMS_BULLTRON) {
    int8_t x81 = -1;
    {
      Lock l;
      Bat *b = find_mac(mac);
      if (b) x81 = b->daly_x81;
    }
    uint8_t dev = 0xD2;
    auto rd = [&](uint16_t reg, uint16_t cnt) -> bool {
      s_mb_addr = dev == 0x81 ? 0x51 : dev;   // Daly 0x81 antwortet mit 0x51
      uint8_t req[8] = {dev, 0x03, (uint8_t)(reg >> 8), (uint8_t)reg, (uint8_t)(cnt >> 8), (uint8_t)cnt, 0, 0};
      uint16_t c = mb_crc(req, 6);
      req[6] = c & 0xFF;
      req[7] = c >> 8;
      for (int attempt = 0; attempt < 2; attempt++)
        if (pq_ask(mac, wr, req, 8, 2, (uint8_t)(cnt * 2), d, 2500)) return true;
      return false;
    };
    int nominal = 0;
    {
      Lock l;
      Bat *b = find_mac(mac);
      if (b) nominal = b->cap_ah;
    }
    if (x81 != 1 && nominal <= 0 && rd(0x0080, 41) && d.size() >= 2)   // Einstellungen: S0 = Nennkapazitaet x 0,1 Ah
      cap = (int)lroundf(((uint8_t)d[0] << 8 | (uint8_t)d[1]) / 10.0f);
    bool d2ok = x81 != 1 && rd(0x0000, 62) && d.size() >= 124;   // Laufdaten D2
    if (!d2ok && x81 != 0) {
      // Daly-Variante 0x81 (Antwort 0x51): 64 Register ab 0 und 62 ab 0x41 (Positionen nach aiobmsble)
      dev = 0x81;
      std::string m64;
      if (rd(0x0000, 64) && d.size() >= 128) {
        m64 = d;
        if (rd(0x0041, 62) && d.size() >= 124) {
          auto r64 = [&](int x) -> uint16_t { return (uint8_t)m64[x] << 8 | (uint8_t)m64[x + 1]; };
          auto r62 = [&](int x) -> uint16_t { return (uint8_t)d[x] << 8 | (uint8_t)d[x + 1]; };
          v = r64(112) / 10.0f;
          a = ((int)r64(114) - 30000) / 10.0f;
          soc = r64(116) / 10.0f;
          int nt = r64(122) > 8 ? 8 : r64(122), sum = 0;
          for (int i = 0; i < nt; i++) sum += (int)r64(96 + 2 * i) - 40;
          if (nt) temp = (float)sum / nt;
          cyc = r62(22);
          alarms = (uint32_t)r62(88) << 16 | r62(90);
          chg_on = r62(34);
          dis_on = r62(36);
          state = a > 0.05f ? 1 : a < -0.05f ? 2 : 0;
          ok = true;
          if (x81 != 1) {
            Lock l;
            Bat *b = find_mac(mac);
            if (b) b->daly_x81 = 1;
            logf("%s: Daly-Protokoll 0x81", mac.c_str());
          }
        }
      }
    } else if (d2ok) {   // Laufdaten D2, 62 Register Big Endian
      if (x81 == -1) {
        Lock l;
        Bat *b = find_mac(mac);
        if (b) b->daly_x81 = 0;
      }
      auto r = [&](int i) -> uint16_t { return (uint8_t)d[2 * i] << 8 | (uint8_t)d[2 * i + 1]; };
      for (int i = 0; i < 32; i++)
        if (r(i)) cmv.push_back(r(i));
      int nt = r(50) > 8 ? 8 : r(50), sum = 0;
      for (int i = 0; i < nt; i++) sum += (int)r(32 + i) - 40;
      if (nt) temp = (float)sum / nt;
      v = r(40) * 0.1f;
      a = ((int)r(41) - 30000) * 0.1f;
      soc = r(42) / 10.0f;
      state = r(47);
      rem_ah = r(48) * 0.1f;
      cyc = r(51);
      chg_on = r(53) == 1;
      dis_on = r(54) == 1;
      alarms = (r(58) | r(59) | r(60) | r(61)) ? (uint32_t)(r(58) | r(59) << 16) | 1u : 0;
      ok = true;
    }
  } else if (model == BMS_RENOGY) {
    // Renogy (Modbus, Geraet 0x30): 0x13B2 x7 = Strom/Spannung/Ladung/Kapazitaet/Zyklen, 0x1388 x34 = Zellen/Temperaturen,
    // 0x13EC x8 = Alarme und Schalter (Positionen nach aiobmsble, ohne den 3-Byte-Kopf)
    auto rd = [&](uint16_t reg, uint16_t cnt) -> bool {
      s_mb_addr = 0x30;
      uint8_t req[8] = {0x30, 0x03, (uint8_t)(reg >> 8), (uint8_t)reg, (uint8_t)(cnt >> 8), (uint8_t)cnt, 0, 0};
      uint16_t c = mb_crc(req, 6);
      req[6] = c & 0xFF;
      req[7] = c >> 8;
      for (int attempt = 0; attempt < 2; attempt++)
        if (pq_ask(mac, wr, req, 8, 2, (uint8_t)(cnt * 2), d, 2500)) return true;
      return false;
    };
    auto be16 = [&](int x) -> uint16_t { return (uint8_t)d[x] << 8 | (uint8_t)d[x + 1]; };
    auto be32 = [&](int x) -> uint32_t { return (uint32_t)be16(x) << 16 | be16(x + 2); };
    if (rd(0x13B2, 7) && d.size() >= 14) {
      a = (int16_t)be16(0) / 100.0f;
      v = be16(2) / 10.0f;
      float ah = be32(4) / 1000.0f, full = be32(8) / 1000.0f;
      cap = (int)lroundf(full);
      cyc = be16(12);
      if (full > 0) soc = ah / full * 100.0f;
      ok = !isnan(soc);
      if (rd(0x1388, 0x22) && d.size() >= 38 && be16(34) > 0) temp = (int16_t)be16(36) / 10.0f;
      if (rd(0x13EC, 8) && d.size() >= 14) {
        uint8_t st = (uint8_t)d[13];
        chg_on = st & 0x02;
        dis_on = st & 0x04;
        alarms = 0;
        for (int i = 0; i < 13; i++) alarms |= (uint8_t)d[i];
        alarms |= st & ~0x0E & 0xFF;
      }
    }
  } else if (model == BMS_ECOWORTHY) {
    // Eco-Worthy: zwei Startbefehle, danach schickt der Akku die Rahmen A1 (Werte) und A2 (Zellen, Temperaturen)
    static const uint8_t INIT1[18] = {0xFF, 0x08, 0x02, 0x00, 0x0B, 0x01, 0x00, 0x64, 0x01, 0xFF,
                                      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x2D};
    static const uint8_t INIT2[18] = {0xFF, 0x08, 0x02, 0x00, 0x0B, 0x01, 0x00, 0x14, 0x01, 0xFF,
                                      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x65, 0xEF};
    {
      Lock l;
      Bat *b = find_mac(mac);
      if (b) b->eco_a1.clear(), b->eco_a2.clear();
    }
    pq_write(wr, INIT1, 18, false);
    delay(150);
    pq_write(wr, INIT2, 18, false);
    std::string f1, f2;
    for (uint32_t t0 = millis(); millis() - t0 < 4000 && (f1.empty() || f2.empty());) {
      delay(50);
      Lock l;
      Bat *b = find_mac(mac);
      if (!b) break;
      f1 = b->eco_a1;
      f2 = b->eco_a2;
    }
    if (f1.size() >= 53) {
      const uint8_t *q = (const uint8_t *)f1.data();
      auto be = [&](int x) -> uint16_t { return q[x] << 8 | q[x + 1]; };
      bool v1 = q[0] == 0xA1;   // Version 1: Strom in 0,01 A, Version 2: 0,1 A
      soc = be(16);
      v = be(20) / 100.0f;
      a = (int16_t)be(22) / (v1 ? 100.0f : 10.0f);
      cap = be(26) / 100;
      alarms = be(51);
      ok = true;
      if (f2.size() >= 84) {
        const uint8_t *t = (const uint8_t *)f2.data();
        int nt = t[80] << 8 | t[81];
        if (nt > 0 && f2.size() >= 84) temp = (int16_t)(t[82] << 8 | t[83]) / 10.0f;
      }
    } else {
      pq_note(INIT2, 18, true, f1.empty() ? std::string() : f1);
    }
  } else if (model == BMS_PQ_ASCII) {
    // ~11014642E00201FD35\r : Messwerte, Akku 01
    static const char REQ[] = "~11014642E00201FD35\r";
    for (int attempt = 0; attempt < 2 && !ok; attempt++) {
      if (!pq_ask(mac, wr, (const uint8_t *)REQ, sizeof(REQ) - 1, 1, 0, d, 2500)) continue;
      const uint8_t *p = (const uint8_t *)d.data();
      size_t n = d.size(), i = 0;
      if (n < 1) continue;
      uint8_t m = p[i++];
      if (n < 1 + 2u * m + 1) continue;
      uint8_t tc = p[1 + 2 * m], nmos = tc >> 4, nct = tc & 0x0F;
      if (n != 2u * m + 2u * (nmos + nct) + 23) {
        logf("%s: ASCII-Antwort hat unerwartete Laenge %u", mac.c_str(), (unsigned)n);
        continue;
      }
      for (uint8_t k = 0; k < m; k++, i += 2) cmv.push_back((uint16_t)be16s(p + i));
      i++;
      i += 2 * nmos;
      for (uint8_t k = 0; k < nct; k++, i += 2) {
        float t = be16s(p + i) / 10.0f;
        if (isnan(temp) || t > temp) temp = t;
      }
      a = be16s(p + i) / 10.0f;
      i += 2;
      v = (uint16_t)be16s(p + i) / 10.0f;
      i += 2;
      soc = be16s(p + i);
      i += 3;
      cyc = (uint16_t)be16s(p + i);
      ok = true;
    }
  } else {
    auto ask = [&](uint8_t cmd) -> bool {
      uint8_t req[7] = {0xAA, 0x55, cmd, 0x00, cmd, 0x0A, 0x0D};
      for (int attempt = 0; attempt < 2; attempt++)
        if (pq_ask(mac, wr, req, 7, 0, cmd, d, 1500)) return true;
      return false;
    };
    if (full && ask(0x15) && d.size() >= 4) {   // Kapazitaet Ah, Zyklen
      const uint8_t *p = (const uint8_t *)d.data();
      cap = (uint16_t)le16s(p);
      cyc = (uint16_t)le16s(p + 2);
    }
    bool s1 = !full || (ask(0x0A) && d.size() >= 2);   // SoC, MOS-Status – nur jede Minute
    if (s1 && full) {
      soc = (uint8_t)d[0];
      mos = (uint8_t)d[1];
      // Die Deutung der App (oberes Nibble 0 = Laden an, unteres 2 = Entladen an) passt nicht zum Akku (zeigte
      // bei 98 % in Ruhe beides aus) – vorerst nicht auswerten, Rohwert steht im Protokoll.
    }
    bool s2 = ask(0x0B) && d.size() >= 8;   // Spannung mV, Strom mA (+ laden)
    if (s2) {
      const uint8_t *p = (const uint8_t *)d.data();
      v = le32(p) / 1000.0f;
      a = (int32_t)le32(p + 4) / 1000.0f;
    }
    if (full && ask(0x09) && d.size() >= 2) temp = le16s((const uint8_t *)d.data());
    for (uint8_t g = 0; full && g < 4 && cmv.size() < cells; g++) {
      if (!ask(0x05 + g)) break;
      const uint8_t *p = (const uint8_t *)d.data();
      for (size_t k = 0; k + 1 < d.size() && k < 10; k += 2) cmv.push_back((uint16_t)le16s(p + k));
    }
    if (cmv.size() > cells) cmv.resize(cells);
    if (full && ask(0x21) && d.size() >= 4) {
      const uint8_t *p = (const uint8_t *)d.data();
      alarms = p[1] | p[2] << 8 | (uint32_t)p[3] << 16;
    }
    ok = s1 && s2;
  }
  Lock l;
  Bat *b = find_mac(mac);
  if (!b) return;
  if (!ok) {
    logf("%s: keine gueltige Antwort – %s", b->name.c_str(), s_pq_diag.c_str());
    if (s_pq_rxlen == 0 && ++b->silent >= 2) {   // gar nichts zurueck: andere Schreibart probieren
      b->silent = 0;
      b->wr_resp = !b->wr_resp;
      logf("%s: schreibe jetzt %s Bestaetigung", b->name.c_str(), b->wr_resp ? "mit" : "ohne");
    }
    return;
  }
  b->silent = 0;
  if (!b->has || (mos >= 0 && mos != b->mos_raw))
    logf(mos >= 0 ? "%s: %.0f %% %.2f V %.2f A, MOS-Status 0x%02X" : "%s: %.0f %% %.2f V %.2f A", b->name.c_str(), soc,
         v, a, mos);
  if (mos >= 0) b->mos_raw = mos;
  b->has = true;
  b->last_ms = millis();
  if (!isnan(soc)) b->soc = (int)lroundf(soc);
  soc = b->soc;   // ohne neue Abfrage: letzter Ladezustand (fuer Restzeit und Zustand)
  b->volt = v;
  b->amp = a;
  if (!isnan(temp)) b->temp = temp;   // seltene Werte nur bei der vollen Abfrage
  if (cyc >= 0) b->cycles = cyc;
  if (cap > 0) b->cap_ah = cap;
  if (!cmv.empty()) b->cell_mv = cmv;
  if (full || model != BMS_PQ_HEX) b->alarms = alarms;
  alarms = b->alarms;
  b->chg_on = chg_on;
  b->dis_on = dis_on;
  // Restzeit wie die App: ab 0,5 A, mit Kapazitaet
  b->ttg = -1;
  if (b->cap_ah > 0 && isfinite(a) && a <= -0.5f) b->ttg = (int32_t)(soc / 100.0f * b->cap_ah / -a * 3600.0f);
  if (model == BMS_BULLTRON) {
    // Zustandsregister des Akkus statt Stromschwelle; Lade-/Entlade-MOSFET sind hier eindeutig dokumentiert
    if (state == 1) a = fabsf(a);
    if (state == 2) a = -fabsf(a);
    b->amp = a;
    b->flags = (alarms ? 0 : 0x01) | (state == 2 ? 0x02 : 0) | (state == 1 ? 0x04 : 0) | (soc <= 10 ? 0x08 : 0) |
               (chg_on ? 0x10 : 0) | (dis_on ? 0x20 : 0);
    b->ttg = -1;
    if (b->cap_ah > 0 && state == 2 && fabsf(a) > 0.05f) b->ttg = (int32_t)(soc / 100.0f * b->cap_ah / fabsf(a) * 3600.0f);
    (void)rem_ah;
    b->status = "Verbunden";
    s_ver++;
    return;
  }
  // Zustand in die gemeinsamen Flags (wie Super B): ok, entlaedt, laedt, niedrig, Laden erlaubt, Entladen erlaubt
  b->flags = (alarms ? 0 : 0x01) | (a < -0.2f ? 0x02 : 0) | (a > 0.2f ? 0x04 : 0) | (soc <= 10 ? 0x08 : 0) |
             (chg_on ? 0x10 : 0) | (dis_on ? 0x20 : 0);
  b->status = "Verbunden";
  s_ver++;
}

// ---------------------------------------------------------------- oeffentliche API
void bms_begin() {
  if (s_mtx) return;
  s_mtx = xSemaphoreCreateRecursiveMutex();
  load();
  uint32_t now = millis();
  for (auto &b : s_bats) b.next_try = now + 15000;   // erst nach dem Start von WLAN und Display
}

uint32_t bms_version() { return s_ver; }

bool bms_has_devices() {
  if (!s_mtx) return false;
  Lock l;
  return !s_bats.empty();
}

static BmsInfo info_of(const Bat &b) {
  BmsInfo i;
  i.key = BMS_KEY_PREFIX + b.mac;
  i.mac = b.mac;
  i.name = b.name;
  i.model = b.model;
  i.model_text = b.model_text.empty() ? bms_model_text(b.model) : b.model_text;
  i.serial = b.serial;
  i.firmware = b.firmware;
  i.connected = b.connected || (b.polled() && b.last_ms && millis() - b.last_ms < 3 * PQ_POLL_MS);
  i.has = b.has;
  i.age_s = b.last_ms ? (int32_t)((millis() - b.last_ms) / 1000) : -1;
  i.soc = b.soc;
  i.soh = b.soh;
  i.volt = b.volt;
  i.amp = b.amp;
  i.ttg_s = b.ttg > 0 ? b.ttg : -1;
  i.ok = b.flags & 0x01;
  i.discharging = b.flags & 0x02;
  i.charging = b.flags & 0x04;
  i.low = b.flags & 0x08;
  i.charge_allowed = b.flags & 0x10;
  i.discharge_allowed = b.flags & 0x20;
  i.temp = b.temp;
  i.cycles = b.cycles;
  i.capacity_ah = b.cap_ah;
  i.cells_mv = b.cell_mv;
  i.alarms = b.alarms;
  i.status = b.status;
  return i;
}

std::vector<BmsInfo> bms_list() {
  std::vector<BmsInfo> out;
  if (!s_mtx) return out;
  Lock l;
  for (auto &b : s_bats)
    if (!b.remove) out.push_back(info_of(b));
  return out;
}

bool bms_get(const std::string &key, BmsInfo &out) {
  if (!s_mtx) return false;
  Lock l;
  for (auto &b : s_bats)
    if (!b.remove && BMS_KEY_PREFIX + b.mac == key) {
      out = info_of(b);
      return true;
    }
  return false;
}

void bms_discovery_start() {
  Lock l;
  s_found.clear();
  s_disc_until = (millis() + DISCOVERY_MS) | 1;
  s_ver++;
  logf("Suche gestartet (60 s)");
}

bool bms_discovery_running() { return s_disc_until != 0; }

std::vector<BmsFound> bms_found() {
  Lock l;
  std::vector<BmsFound> out = s_found;
  for (auto &f : out) f.configured = find_mac(f.mac) != nullptr;
  return out;
}

std::string bms_add(const std::string &mac_in, uint8_t addr_type, const std::string &name, BmsModel model,
                    int capacity_ah, int cells) {
  std::string mac = lower(mac_in);
  if (mac.size() != 17) return "Ungültige Adresse";
  Lock l;
  if (find_mac(mac)) return "Dieser Akku ist schon eingerichtet";
  size_t n = 0;
  for (auto &b : s_bats) n += !b.remove;
  if (n >= BMS_MAX) return "Es sind höchstens vier Akkus möglich";
  Bat b;
  b.mac = mac;
  b.type = addr_type;
  b.model = model == BMS_UNKNOWN ? BMS_SUPERB : model;
  b.name = name.empty() ? std::string("Akku") : name.substr(0, 32);
  b.cap_ah = capacity_ah > 0 ? capacity_ah : 0;
  b.cells = cells > 0 && cells <= 32 ? cells : 4;
  b.model_text = bms_model_text(b.model);
  b.next_try = millis() + 2000;
  s_bats.push_back(b);
  save();
  s_disc_until = 0;   // Suche beenden, gleich verbinden
  s_ver++;
  logf("Eingerichtet: %s \"%s\"", mac.c_str(), b.name.c_str());
  return "";
}

void bms_remove(const std::string &mac) {
  Lock l;
  Bat *b = find_mac(lower(mac));
  if (!b) return;
  b->remove = true;   // aufraeumen im Scan-Task (Client)
  save();
  s_ver++;
}

void bms_rename(const std::string &mac, const std::string &name) {
  Lock l;
  Bat *b = find_mac(lower(mac));
  if (!b || name.empty()) return;
  b->name = name.substr(0, 32);
  save();
  s_ver++;
}

std::vector<std::string> bms_log() {
  Lock l;
  return std::vector<std::string>(s_log.begin(), s_log.end());
}
#endif
