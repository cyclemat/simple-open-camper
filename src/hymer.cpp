// Hymer Connect per Bluetooth LE – siehe hymer.h
#include "hymer.h"
#include "i18n/i18n.h"
#include <string.h>
#include <math.h>

// ---------------------------------------------------------------- Werte-Tabelle (immer dabei, siehe hymer_point_name)
// Bus-ID und Sensor-ID wie in den Sensor-Karten der HA-Integration (sensor_maps/base.json, hymer.json).
// Umrechnung: 0 roh, 1 /10, 2 /100, 3 /1000. Schreiben: 0 nur lesen, 1 bool, 2 Text "On"/"Off".
// INV: Zustand umdrehen (Tueren melden "offen" = an, ein Eingang zeigt aber "zu" = an).
enum : uint8_t { HW_NONE = 0, HW_BOOL = 1, HW_STR = 2 };
enum : uint8_t { HF_INV = 1 };
struct HymerDef {
  uint8_t bus, sid;
  uint8_t kind;     // SatPointKind
  uint8_t cls;      // SatClass
  uint8_t xf;       // Umrechnung
  uint8_t wr;       // Schreibart
  uint8_t flags;
  const char *name, *unit;
};

static const HymerDef DEFS[] = {
    // Bordnetz (Bus 3, alle Hymer-Fahrzeuge mit EBL)
    {3, 1, SP_SWITCH, SC_NONE, 0, HW_STR, 0, "Hauptschalter", ""},
    {3, 3, SP_SWITCH, SC_NONE, 0, HW_BOOL, 0, "Wasserpumpe", ""},
    {3, 5, SP_ANALOG, SC_VOLTAGE, 0, 0, 0, "Bordbatterie", "V"},
    {3, 6, SP_ANALOG, SC_CURRENT, 0, 0, 0, "Batteriestrom", "A"},
    {3, 7, SP_ANALOG, SC_VOLTAGE, 0, 0, 0, "Starterbatterie", "V"},
    {3, 10, SP_ANALOG, SC_LEVEL, 0, 0, 0, "Batterie Ladestand", "%"},
    {3, 8, SP_ANALOG, SC_LEVEL, 0, 0, 0, "Frischwasser", "%"},
    {3, 9, SP_ANALOG, SC_LEVEL, 0, 0, 0, "Abwasser", "%"},
    {3, 22, SP_INPUT, SC_ONOFF, 0, 0, 0, "Landstrom", ""},
    {3, 20, SP_INPUT, SC_ONOFF, 0, 0, 0, "Solar angeschlossen", ""},
    {30, 5, SP_ANALOG, SC_VOLTAGE, 0, 0, 0, "SCU-Spannung", "V"},
    // Basisfahrzeug (CAN)
    {1, 9, SP_TEMP, SC_TEMP, 0, 0, 0, "Außentemperatur", "\xC2\xB0" "C"},
    {1, 2, SP_ANALOG, SC_LEVEL, 0, 0, 0, "Kraftstoff", "%"},
    {1, 6, SP_ANALOG, SC_LEVEL, 0, 0, 0, "AdBlue", "%"},
    {1, 12, SP_INPUT, SC_BINARY, 0, 0, HF_INV, "Fahrertür", ""},
    {1, 13, SP_INPUT, SC_BINARY, 0, 0, HF_INV, "Beifahrertür", ""},
    // Solar
    {8, 2, SP_ANALOG, SC_VOLTAGE, 0, 0, 0, "Solarspannung", "V"},
    {8, 3, SP_ANALOG, SC_CURRENT, 0, 0, 0, "Solarstrom", "A"},
    // Lichter (Sensor 1 = an/aus); Busse nach hymer.json, je nach Grundriss vorhanden
    {11, 1, SP_SWITCH, SC_NONE, 0, HW_BOOL, 0, "Deckenlicht Wohnen", ""},
    {12, 1, SP_SWITCH, SC_NONE, 0, HW_BOOL, 0, "Ambiente Wohnen", ""},
    {15, 1, SP_SWITCH, SC_NONE, 0, HW_BOOL, 0, "Ambiente Schlafen", ""},
    {16, 1, SP_SWITCH, SC_NONE, 0, HW_BOOL, 0, "Nachtlicht", ""},
    {19, 1, SP_SWITCH, SC_NONE, 0, HW_BOOL, 0, "Licht Bad", ""},
    {21, 1, SP_SWITCH, SC_NONE, 0, HW_BOOL, 0, "Licht Küche", ""},
    {24, 1, SP_SWITCH, SC_NONE, 0, HW_BOOL, 0, "Lichtgruppe Wohnen", ""},
    {25, 1, SP_SWITCH, SC_NONE, 0, HW_BOOL, 0, "LED-Leiste außen", ""},
    {27, 1, SP_SWITCH, SC_NONE, 0, HW_BOOL, 0, "Lichtgruppe Privat", ""},
    {43, 1, SP_SWITCH, SC_NONE, 0, HW_BOOL, 0, "Licht Sitzgruppe", ""},
    {44, 1, SP_SWITCH, SC_NONE, 0, HW_BOOL, 0, "Leselicht Schlafen", ""},
    // Kuehlschrank (Thetford)
    {34, 2, SP_SWITCH, SC_NONE, 0, HW_BOOL, 0, "Kühlschrank Eco", ""},
    {34, 5, SP_INPUT, SC_BINARY, 0, 0, HF_INV, "Kühlschranktür", ""},
    // Heizung (Truma ueber die SCU)
    {58, 8, SP_TEMP, SC_TEMP, 0, 0, 0, "Heizung Soll", "\xC2\xB0" "C"},
    {58, 7, SP_INPUT, SC_ONOFF, 0, 0, 0, "Heizung", ""},
    // Lithium-Batterie (BMS)
    {99, 1, SP_ANALOG, SC_VOLTAGE, 0, 0, 0, "Lithium Spannung", "V"},
    {99, 2, SP_ANALOG, SC_CURRENT, 0, 0, 0, "Lithium Strom", "A"},
    {99, 3, SP_TEMP, SC_TEMP, 0, 0, 0, "Lithium Temperatur", "\xC2\xB0" "C"},
    {99, 4, SP_ANALOG, SC_LEVEL, 0, 0, 0, "Lithium Ladestand", "%"},
    // Victron-Wechselrichter/Lader an der SCU
    {121, 1, SP_INPUT, SC_ONOFF, 0, 0, 0, "Wechselrichter", ""},
    {121, 15, SP_ANALOG, SC_VOLTAGE, 0, 0, 0, "Netzspannung", "V"},
};
static const size_t N_DEFS = sizeof(DEFS) / sizeof(DEFS[0]);

const char *hymer_point_name(uint8_t idx) {
  if (idx < HYMER_IDX_BASE || idx - HYMER_IDX_BASE >= (int)N_DEFS) return nullptr;
  return DEFS[idx - HYMER_IDX_BASE].name;
}

#if !WITH_HYMER
void hymer_begin() {}
uint32_t hymer_version() { return 0; }
std::vector<SatPoint> hymer_points() { return {}; }
bool hymer_has_switches() { return false; }
bool hymer_switch(uint8_t, uint8_t) { return false; }
void hymer_pair_start() {}
bool hymer_pairing() { return false; }
bool hymer_paired() { return false; }
bool hymer_connected() { return false; }
void hymer_forget() {}
void hymer_disconnect() {}
std::string hymer_status() { return ""; }
void hymer_ble_setup() {}
bool hymer_on_adv(const NimBLEAdvertisedDevice *) { return false; }
void hymer_scan_wanted(bool &, bool &) {}
bool hymer_want_connect() { return false; }
void hymer_connect_now() {}
#else

#include <Arduino.h>
#include <atomic>
#include <map>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "mbedtls/base64.h"
#include "hymer_tls.h"

static const char *UUID_NUS = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char *UUID_NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";   // Panel -> SCU
static const char *UUID_NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";   // SCU -> Panel (Notify, SIU: Indicate)
static const char *UUID_POWER = "fff40001-13c9-42f3-9d46-e1d1aa2a7232";
static const char *UUID_POWER_CTRL = "fff40003-13c9-42f3-9d46-e1d1aa2a7232";
static const char *PIA_VERSION = "v0.32.0";
static const uint32_t PAIR_MS = 3 * 60 * 1000;
static const uint32_t TLS_TIMEOUT_MS = 25000;
#if CONFIG_SPIRAM
static const size_t FRAME_MAX = 65536;   // groesste PIA-Nachricht, die ausgewertet wird
#else
static const size_t FRAME_MAX = 24576;
#endif

// Abo-Nachrichten der Hymer-App (aus der HA-Integration): schalten alle Sensorgruppen frei. Reihenfolge wichtig.
static const char *const SUBSCRIBE[] = {
    "EhcI/4kTEgd2MC4zMi4wGNr5ws4GIgIKAA==",
    "ErUKCMO2AhIHdjAuMzIuMBja+cLOBiKfChqcCgoKCAEQAVIEY2FuMAoKCAIQAVIEY2FuMAoKCAMQAVIEY2FuMAoKCAQQAVIEY2FuMAoKCAUQAVIEY2FuMAoKCAYQAVIEY2FuMAoKCAcQAVIEY2FuMAoKCAgQAVIEY2FuMAoKCAkQAVIEY2FuMAoKCAoQAVIEY2FuMAoKCAsQAVIEY2FuMAoKCAwQAVIEY2FuMAoKCA0QAVIEY2FuMAoKCA4QAVIEY2FuMAoKCA8QAVIEY2FuMAoKCBAQAVIEY2FuMAoKCBEQAVIEY2FuMAoKCBIQAVIEY2FuMAoKCBMQAVIEY2FuMAoKCBQQAVIEY2FuMAoKCBUQAVIEY2FuMAoKCBYQAVIEY2FuMAoKCBcQAVIEY2FuMAoKCAEQA1IEbGluMQoKCAIQA1IEbGluMQoKCAMQA1IEbGluMQoKCAQQA1IEbGluMQoKCAUQA1IEbGluMQoKCAYQA1IEbGluMQoKCAcQA1IEbGluMQoKCAgQA1IEbGluMQoKCAkQA1IEbGluMQoKCAoQA1IEbGluMQoKCAsQA1IEbGluMQoKCAwQA1IEbGluMQoKCA0QA1IEbGluMQoKCA4QA1IEbGluMQoKCA8QA1IEbGluMQoKCBAQA1IEbGluMQoKCBEQA1IEbGluMQoKCBIQA1IEbGluMQoKCBMQA1IEbGluMQoKCBQQA1IEbGluMQoKCBUQA1IEbGluMQoKCBYQA1IEbGluMQoKCAEQCFIEbGluMgoKCAIQCFIEbGluMgoKCAMQCFIEbGluMgoKCAQQCFIEbGluMgoKCAUQCFIEbGluMgoKCAYQCFIEbGluMgoKCAcQCFIEbGluMgoECAEQCwoECAIQCwoECAEQDAoECAIQDAoECAMQDAoECAEQDwoECAIQDwoECAMQDwoECAEQEAoECAIQEAoECAEQEwoECAIQEwoECAEQFQoECAIQFQoECAEQFgoECAIQFgoECAEQGAoECAIQGAoECAMQGAoECAEQGQoECAIQGQoECAEQGwoECAIQGwoECAMQGwoECAEQHgoECAIQHgoECAMQHgoECAQQHgoECAUQHgoECAYQHgoECAcQHgoECAgQHgoECAkQHgoECAoQHgoECAsQHgoECAwQHgoECA0QHgoECA4QHgoKCAEQIlIEbGluMQoKCAIQIlIEbGluMQoKCAMQIlIEbGluMQoKCAQQIlIEbGluMQoKCAUQIlIEbGluMQoKCAYQIlIEbGluMQoKCAcQIlIEbGluMQoECAEQJQoECAIQJQoECAEQKwoECAIQKwoECAEQLAoECAIQLAoKCAgQLVIEbGluMQoKCAkQLVIEbGluMQoKCAoQLVIEbGluMQoKCAsQLVIEbGluMQoKCAgQMVIEbGluMQoKCAoQMVIEbGluMQoKCAsQMVIEbGluMQoKCAQQOlIEbGluMQoKCAUQOlIEbGluMQoKCAYQOlIEbGluMQoKCAcQOlIEbGluMQoKCAgQOlIEbGluMQoKCAkQOlIEbGluMQoKCAoQOlIEbGluMQoKCAsQOlIEbGluMQoKCAwQOlIEbGluMQoKCA0QOlIEbGluMQoKCA4QOlIEbGluMQoKCAEQY1IEY2FuMgoKCAIQY1IEY2FuMgoKCAMQY1IEY2FuMgoKCAQQY1IEY2FuMgoKCAUQY1IEY2FuMgoKCAYQY1IEY2FuMgoKCAcQY1IEY2FuMgoKCAgQY1IEY2FuMgoKCAkQY1IEY2FuMgoKCAoQY1IEY2FuMg==",
    "EhsIqdQjEgd2MC4zMi4wGNr5ws4GIgZKBAoCCAA=",
    "EhcIn7UFEgd2MC4zMi4wGNv5ws4GKgIaAA==",
    "EhcItPYkEgd2MC4zMi4wGNv5ws4GYgIKAA==",
    "EhcIjI8GEgd2MC4zMi4wGNv5ws4GSgIKAA==",
    "EhUIjekiEgd2MC4zMi4wGNz5ws4GegA=",
};

static SemaphoreHandle_t s_mtx;
struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};
static std::atomic<uint32_t> s_ver{1};
static Preferences s_prefs;

// Werte
static float s_val[N_DEFS];
static bool s_on[N_DEFS], s_has[N_DEFS];
static uint32_t s_pend_ms[N_DEFS];      // Schaltbefehl unterwegs: Anzeige vorab umgestellt
static uint32_t s_last_rx_ms = 0;

// Kopplung und Verbindung
static bool s_paired = false;
static std::string s_peer_id;           // Identitaetsadresse der gekoppelten SCU
static volatile bool s_pairing = false;
static uint32_t s_pair_until = 0;
static std::string s_cand;              // SCU aus dem letzten Werbepaket
static uint8_t s_cand_type = 0;
static int s_cand_rssi = -127;
static uint32_t s_cand_ms = 0;
static std::map<std::string, uint32_t> s_block;   // Adressen, die gerade nicht klappen -> bis wann meiden
static uint32_t s_next_try_ms = 0, s_backoff_ms = 15000;

static NimBLEClient *s_client = nullptr;
static NimBLERemoteCharacteristic *s_ch_rx = nullptr, *s_ch_tx = nullptr;
static volatile bool s_connected = false, s_ready = false;
static volatile bool s_up = false;      // neue Verbindung: Task startet TLS
static char s_status[80] = "Nicht gekoppelt";
static StreamBufferHandle_t s_sb = nullptr;
struct WriteReq { uint8_t i; uint8_t on; };
static QueueHandle_t s_wq;
static bool s_task_started = false;

static void set_status(const char *s) {
  Lock l;
  if (strcmp(s_status, s)) {
    strncpy(s_status, s, sizeof(s_status) - 1);
    s_ver++;
  }
}

static int def_index(uint8_t bus, uint8_t sid) {
  for (size_t i = 0; i < N_DEFS; i++)
    if (DEFS[i].bus == bus && DEFS[i].sid == sid) return (int)i;
  return -1;
}

// ---------------------------------------------------------------- protobuf
static void pb_varint(std::vector<uint8_t> &o, uint64_t v) {
  do {
    uint8_t b = v & 0x7F;
    v >>= 7;
    o.push_back(b | (v ? 0x80 : 0));
  } while (v);
}
static void pb_key(std::vector<uint8_t> &o, uint32_t fn, uint8_t wt) { pb_varint(o, (fn << 3) | wt); }
static void pb_uint(std::vector<uint8_t> &o, uint32_t fn, uint64_t v) {
  pb_key(o, fn, 0);
  pb_varint(o, v);
}
static void pb_bytes(std::vector<uint8_t> &o, uint32_t fn, const uint8_t *d, size_t n) {
  pb_key(o, fn, 2);
  pb_varint(o, n);
  o.insert(o.end(), d, d + n);
}
static void pb_bytes(std::vector<uint8_t> &o, uint32_t fn, const std::vector<uint8_t> &d) { pb_bytes(o, fn, d.data(), d.size()); }
static void pb_str(std::vector<uint8_t> &o, uint32_t fn, const char *s) { pb_bytes(o, fn, (const uint8_t *)s, strlen(s)); }

struct PbField {
  uint32_t fn;
  uint8_t wt;
  uint64_t v;          // Varint
  float f;             // fixed32
  const uint8_t *p;    // laengenbegrenzt
  size_t n;
};

static bool pb_read_varint(const uint8_t *&p, const uint8_t *e, uint64_t &v) {
  v = 0;
  for (int sh = 0; p < e && sh < 64; sh += 7) {
    uint8_t b = *p++;
    v |= (uint64_t)(b & 0x7F) << sh;
    if (!(b & 0x80)) return true;
  }
  return false;
}

static void pb_parse(const uint8_t *d, size_t n, std::vector<PbField> &out) {
  out.clear();
  const uint8_t *p = d, *e = d + n;
  while (p < e) {
    uint64_t tag;
    if (!pb_read_varint(p, e, tag)) return;
    PbField f{(uint32_t)(tag >> 3), (uint8_t)(tag & 7), 0, 0, nullptr, 0};
    switch (f.wt) {
      case 0:
        if (!pb_read_varint(p, e, f.v)) return;
        break;
      case 1:
        if (e - p < 8) return;
        p += 8;
        break;
      case 5:
        if (e - p < 4) return;
        memcpy(&f.f, p, 4);
        p += 4;
        break;
      case 2: {
        uint64_t len;
        if (!pb_read_varint(p, e, len) || len > (uint64_t)(e - p)) return;
        f.p = p;
        f.n = len;
        p += len;
        break;
      }
      default:
        return;
    }
    out.push_back(f);
  }
}

static bool printable(const uint8_t *p, size_t n) {
  if (!n || n > 64) return false;
  for (size_t i = 0; i < n; i++)
    if (p[i] < 0x20 && p[i] != '\t') return false;
  return true;
}

// "On"/"OPN"/"True" -> 1, "Off"/"CLS"/"False" -> 0, sonst -1
static int text_on(const std::string &s) {
  std::string u;
  for (char c : s) u += toupper((unsigned char)c);
  if (u == "ON" || u == "OPN" || u == "OPEN" || u == "TRUE" || u == "1" || u == "IGN_ON") return 1;
  if (u == "OFF" || u == "CLS" || u == "CLOSED" || u == "FALSE" || u == "0" || u == "IGN_OFF" || u == "IGN_LOCK") return 0;
  return -1;
}

// Ein Sensoreintrag: 1 Sensor-ID, 2 Bus-ID, Wert in 3 (uint), 4 (Text), 5 (bool), 6 (float), 7 (int)
static void store_entry(const std::vector<PbField> &fs) {
  uint32_t sid = 0, bus = 0;
  bool has3 = false, has4 = false, has5 = false, has6 = false, has7 = false;
  uint64_t v3 = 0, v5 = 0, v7 = 0;
  float v6 = 0;
  std::string v4;
  for (auto &f : fs) {
    if (f.fn == 1 && f.wt == 0) sid = f.v;
    else if (f.fn == 2 && f.wt == 0) bus = f.v;
    else if (f.fn == 3 && f.wt == 0) { has3 = true; v3 = f.v; }
    else if (f.fn == 4 && f.wt == 2 && printable(f.p, f.n)) { has4 = true; v4.assign((const char *)f.p, f.n); }
    else if (f.fn == 5 && f.wt == 0) { has5 = true; v5 = f.v; }
    else if (f.fn == 6 && f.wt == 5) { has6 = true; v6 = f.f; }
    else if (f.fn == 7 && f.wt == 0) { has7 = true; v7 = f.v; }
  }
  int i = def_index(bus, sid);
  if (i < 0) return;
  const HymerDef &d = DEFS[i];
  // Vorrang wie in der HA-Integration: Text, float, uint, int, bool
  float num = NAN;
  int on = -1;
  if (has4) {
    on = text_on(v4);
    char *end = nullptr;
    float x = strtof(v4.c_str(), &end);
    if (end && end != v4.c_str()) num = x;
  } else if (has6) num = v6;
  else if (has3) num = (float)v3;
  else if (has7) num = (float)(int64_t)v7;
  else if (has5) num = v5 ? 1 : 0;
  else return;
  if (!isnan(num)) {
    // 3276.8 / 32768 / 65535 / 6553.5: "kein Wert" vom CAN-Bus
    if (fabsf(num - 3276.8f) < 0.01f || num == 32768.0f || num == 65535.0f || fabsf(num - 6553.5f) < 0.01f) return;
    if (d.xf == 1) num /= 10;
    else if (d.xf == 2) num /= 100;
    else if (d.xf == 3) num /= 1000;
    if (on < 0) on = num != 0;
  }
  if (d.flags & HF_INV && on >= 0) on = !on;
  Lock l;
  bool changed = !s_has[i] || (on >= 0 && s_on[i] != (on == 1)) || (!isnan(num) && s_val[i] != num);
  // Waehrend ein eigener Schaltbefehl unterwegs ist, keine alte Rueckmeldung uebernehmen
  if (d.kind == SP_SWITCH && s_pend_ms[i] && millis() - s_pend_ms[i] < 3000) return;
  s_pend_ms[i] = 0;
  s_has[i] = true;
  if (on >= 0) s_on[i] = on == 1;
  if (!isnan(num)) s_val[i] = num;
  if (changed) s_ver++;
}

static void extract(const uint8_t *d, size_t n, int depth) {
  if (depth > 5) return;
  std::vector<PbField> fs;
  pb_parse(d, n, fs);
  bool has_sid = false, has_bus = false, has_val = false;
  uint64_t sid = 0, bus = 0;
  for (auto &f : fs) {
    if (f.fn == 1 && f.wt == 0) { has_sid = true; sid = f.v; }
    else if (f.fn == 2 && f.wt == 0) { has_bus = true; bus = f.v; }
    else if (f.fn >= 3 && f.fn <= 7 && (f.wt == 0 || f.wt == 2 || f.wt == 5)) has_val = true;
  }
  if (has_sid && has_bus && has_val) {
    // Huellen mit Nachrichten-ID/Zeitstempel sehen aus wie Sensoren – echte IDs sind klein, die Tiefe 2–4
    bool known = sid < 256 && bus < 256 && def_index(bus, sid) >= 0;
    if (sid < 1000 && bus < 1000 && (depth <= 3 || (depth == 4 && known))) {
      store_entry(fs);
      return;
    }
  }
  for (auto &f : fs)
    if (f.wt == 2 && f.n > 2) extract(f.p, f.n, depth + 1);
}

static void handle_payload(const uint8_t *d, size_t n) {
  std::vector<PbField> top;
  pb_parse(d, n, top);
  for (auto &f : top)
    if (f.wt == 2) extract(f.p, f.n, 0);
  Lock l;
  s_last_rx_ms = millis();
}

// ---------------------------------------------------------------- PIA-Rahmen: A0 CB, Laenge (4, BE), CRC-32 (4, BE), Inhalt
static uint32_t crc32(uint32_t c, const uint8_t *d, size_t n) {
  c = ~c;
  while (n--) {
    c ^= *d++;
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
  }
  return ~c;
}

static std::vector<uint8_t> pia_frame(const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> f = {0xA0, 0xCB};
  uint32_t n = payload.size();
  f.insert(f.end(), {(uint8_t)(n >> 24), (uint8_t)(n >> 16), (uint8_t)(n >> 8), (uint8_t)n, 0, 0, 0, 0});
  f.insert(f.end(), payload.begin(), payload.end());
  uint32_t c = crc32(0, f.data(), f.size());
  f[6] = c >> 24; f[7] = c >> 16; f[8] = c >> 8; f[9] = c;
  return f;
}

static std::vector<uint8_t> s_acc;   // Klartext, bis ein Rahmen vollstaendig ist

static void on_plain(const uint8_t *d, size_t n) {
  if (s_acc.size() + n > FRAME_MAX + 10) {
    log_w("Hymer: Nachricht zu groß, verworfen");
    s_acc.clear();
    return;
  }
  s_acc.insert(s_acc.end(), d, d + n);
  for (;;) {
    size_t i = 0;
    while (i + 1 < s_acc.size() && !(s_acc[i] == 0xA0 && s_acc[i + 1] == 0xCB)) i++;
    if (i) s_acc.erase(s_acc.begin(), s_acc.begin() + i);
    if (s_acc.size() < 10) return;
    uint32_t len = ((uint32_t)s_acc[2] << 24) | (s_acc[3] << 16) | (s_acc[4] << 8) | s_acc[5];
    if (len > FRAME_MAX) {   // zu gross oder kaputt: Kennung ueberspringen und weitersuchen
      s_acc.erase(s_acc.begin(), s_acc.begin() + 2);
      continue;
    }
    if (s_acc.size() < 10 + len) return;
    handle_payload(s_acc.data() + 10, len);
    s_acc.erase(s_acc.begin(), s_acc.begin() + 10 + len);
  }
}

// Huelle einer Anfrage: Feld 2 { 1 ID, 2 Version, 3 Zeit, <feld> Inhalt }
static std::vector<uint8_t> request(uint32_t field, const std::vector<uint8_t> &body) {
  time_t now = time(nullptr);
  std::vector<uint8_t> w;
  pb_uint(w, 1, 1 + esp_random() % 10000000);
  pb_str(w, 2, PIA_VERSION);
  pb_uint(w, 3, now > 1700000000 ? (uint64_t)now : 1767225600ULL);
  pb_bytes(w, field, body);
  std::vector<uint8_t> p;
  pb_bytes(p, 2, w);
  return p;
}

static std::vector<uint8_t> switch_cmd(const HymerDef &d, bool on) {
  std::vector<uint8_t> e;
  pb_uint(e, 1, d.sid);
  pb_uint(e, 2, d.bus);
  if (d.wr == HW_STR) pb_str(e, 4, on ? "On" : "Off");
  else pb_uint(e, 5, on ? 1 : 0);
  std::vector<uint8_t> sub2, inner;
  pb_bytes(sub2, 1, e);
  pb_bytes(inner, 2, sub2);
  return request(4, inner);
}

// ---------------------------------------------------------------- Uebertragung
static HymerTls s_tls;

static void on_notify(NimBLERemoteCharacteristic *, uint8_t *d, size_t n, bool) {
  if (s_sb && xStreamBufferSend(s_sb, d, n, 0) != n) log_w("Hymer: Empfangspuffer voll");
}

class ClientCB : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient *, int reason) override {
    s_connected = false;
    s_ready = false;
    log_i("Hymer: getrennt (%d)", reason);
  }
};
static ClientCB s_ccb;

static bool write_scu(const std::vector<uint8_t> &d) {
  if (!s_connected || !s_ch_rx || d.empty()) return d.empty();
  size_t chunk = s_client->getMTU() > 3 ? s_client->getMTU() - 3 : 20;
  if (chunk > 244) chunk = 244;
  for (size_t pos = 0; pos < d.size(); pos += chunk) {
    size_t k = d.size() - pos < chunk ? d.size() - pos : chunk;
    // Mit Antwort: die SCU hat nur einen kleinen Empfangspuffer, so laeuft nichts ueber
    if (!s_ch_rx->writeValue(d.data() + pos, k, true)) return false;
  }
  return true;
}

static bool send_pia(const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> f = pia_frame(payload), out;
  if (!s_tls.send(f.data(), f.size(), out)) return false;
  return write_scu(out);
}

// Empfangene Bytes durch TLS schicken; false = Fehler
static bool pump(uint32_t wait_ms) {
  uint8_t buf[256];
  size_t n = s_sb ? xStreamBufferReceive(s_sb, buf, sizeof(buf), pdMS_TO_TICKS(wait_ms)) : 0;
  if (!n) return true;
  std::vector<uint8_t> out;
  bool ok = s_tls.feed(buf, n, out, on_plain);
  if (!out.empty() && !write_scu(out)) return false;
  if (!ok) log_w("Hymer: %s", s_tls.err().c_str());
  return ok;
}

static bool session() {
  set_status("TLS-Verbindung …");
  s_acc.clear();
  xStreamBufferReset(s_sb);
  std::vector<uint8_t> out;
  if (!s_tls.start(out) || !write_scu(out)) return false;
  uint32_t until = millis() + TLS_TIMEOUT_MS;
  while (!s_tls.ready()) {
    if (!s_connected || (int32_t)(millis() - until) > 0) {
      if (s_connected) set_status("SCU antwortet nicht – CONNECTION gedrückt?");
      return false;
    }
    if (!pump(100)) {
      set_status(s_tls.err().c_str());
      return false;
    }
  }
  set_status("Abonniere Werte …");
  for (auto b64 : SUBSCRIBE) {
    std::vector<uint8_t> p(strlen(b64));
    size_t olen = 0;
    if (mbedtls_base64_decode(p.data(), p.size(), &olen, (const uint8_t *)b64, strlen(b64)) != 0) continue;
    p.resize(olen);
    if (!send_pia(p)) return false;
    // Antworten zwischendurch abholen, damit der Empfangspuffer nicht ueberlaeuft
    for (int i = 0; i < 3; i++) pump(20);
  }
  if (!send_pia(request(9, {}))) return false;   // alles neu melden lassen
  {
    Lock l;
    s_last_rx_ms = millis();
  }
  return true;
}

static void drop_client() {
  if (s_client) {
    if (s_client->isConnected()) s_client->disconnect();
    NimBLEDevice::deleteClient(s_client);
  }
  s_client = nullptr;
  s_ch_rx = s_ch_tx = nullptr;
  s_connected = false;
  s_ready = false;
}

static void task(void *) {
  uint32_t refresh_ms = 0;
  for (;;) {
    if (s_up) {
      s_up = false;
      bool ok = session();
      s_ready = ok && s_connected;
      if (s_ready) {
        set_status("OK");
        s_backoff_ms = 15000;
        refresh_ms = millis();
      } else {
        s_tls.reset();
        if (s_connected && s_client) s_client->disconnect();
      }
      s_ver++;
    }
    if (!s_ready) {
      if (s_tls.ready() || !s_acc.empty()) {
        s_tls.reset();
        s_acc.clear();
        s_acc.shrink_to_fit();
        if (!s_connected) set_status(s_paired ? "Getrennt – suche SCU …" : "Nicht gekoppelt");
      }
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    if (!pump(100)) {
      set_status("Verbindungsfehler");
      if (s_client) s_client->disconnect();
      continue;
    }
    WriteReq w;
    while (s_ready && xQueueReceive(s_wq, &w, 0) == pdTRUE) {
      if (!send_pia(switch_cmd(DEFS[w.i], w.on))) set_status("Befehl nicht gesendet");
    }
    // Lange still: alles neu melden lassen; bleibt es still, ist die Verbindung tot
    uint32_t last;
    {
      Lock l;
      last = s_last_rx_ms;
    }
    if (millis() - last > 3UL * 60 * 1000 && millis() - refresh_ms > 3UL * 60 * 1000) {
      refresh_ms = millis();
      send_pia(request(9, {}));
    }
    if (millis() - last > 6UL * 60 * 1000) {
      set_status("Keine Werte mehr – verbinde neu");
      if (s_client) s_client->disconnect();
      Lock l;
      s_last_rx_ms = millis();
    }
  }
}

static void ensure_task() {
  if (s_task_started) return;
  s_task_started = true;
  // Puffer und Task erst, wenn Hymer eingeschaltet ist (spart internen RAM)
#if CONFIG_SPIRAM
  s_sb = xStreamBufferCreateWithCaps(8192, 1, MALLOC_CAP_SPIRAM);
#endif
  if (!s_sb) s_sb = xStreamBufferCreate(6144, 1);
  xTaskCreatePinnedToCore(task, "hymer", 7168, nullptr, 3, nullptr, 0);   // RSA/X.509 brauchen etwas Stack
}

// ---------------------------------------------------------------- Bluetooth-Anbindung (aus truma.cpp)
void hymer_ble_setup() {
  NimBLEDevice::setSecurityAuth(true, false, true);   // Bonding, Just Works, Secure Connections
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setMTU(247);
}

static bool looks_like_scu(const NimBLEAdvertisedDevice *dev) {
  for (int i = 0; i < (int)dev->getServiceUUIDCount(); i++) {
    std::string u = dev->getServiceUUID(i).toString();
    if (u == UUID_NUS || u == UUID_POWER) return true;
  }
  if (dev->haveName()) {
    std::string n;
    for (char c : dev->getName()) n += tolower((unsigned char)c);
    if (n.find("hymer") != std::string::npos || n.find("scu") != std::string::npos || n.find("siu") != std::string::npos)
      return true;
  }
  return false;
}

bool hymer_on_adv(const NimBLEAdvertisedDevice *dev) {
  std::string mac = dev->getAddress().toString();
  bool mine = s_paired && mac == s_peer_id;
  if (!mine && !looks_like_scu(dev)) return false;
  Lock l;
  auto b = s_block.find(mac);
  if (b != s_block.end() && (int32_t)(millis() - b->second) < 0) return true;
  if (!s_pairing && !s_paired) return true;
  // Gekoppelt: bevorzugt die eigene SCU; beim Koppeln die mit dem staerksten Signal
  bool fresh = millis() - s_cand_ms > 5000;
  if (s_cand.empty() || mine || fresh || (s_pairing && dev->getRSSI() > s_cand_rssi && s_cand != s_peer_id)) {
    s_cand = mac;
    s_cand_type = dev->getAddress().getType();
    s_cand_rssi = dev->getRSSI();
    s_cand_ms = millis();
  }
  return true;
}

void hymer_scan_wanted(bool &passive, bool &active) {
  ensure_task();   // wird nur aufgerufen, wenn Hymer in den Einstellungen eingeschaltet ist
  if (s_pairing && (int32_t)(millis() - s_pair_until) >= 0) {
    s_pairing = false;
    set_status(s_paired ? "Getrennt – suche SCU …" : "Nicht gekoppelt");
  }
  if (s_pairing) active = true;
  else if (s_paired && !s_connected) passive = true;
}

bool hymer_want_connect() {
  if (s_connected || (!s_paired && !s_pairing)) return false;
  if ((int32_t)(millis() - s_next_try_ms) < 0) return false;
  Lock l;
  return !s_cand.empty() && millis() - s_cand_ms < 10000;
}

void hymer_connect_now() {
  std::string mac;
  uint8_t type;
  bool pairing = s_pairing;
  {
    Lock l;
    mac = s_cand;
    type = s_cand_type;
    s_cand.clear();
    s_cand_rssi = -127;
  }
  if (mac.empty()) return;
  set_status(pairing ? "Kopple …" : "Verbinde …");
  drop_client();
  s_client = NimBLEDevice::createClient();
  s_client->setClientCallbacks(&s_ccb, false);
  s_client->setConnectTimeout(10000);
  auto fail = [&](const char *why, uint32_t block_ms) {
    log_w("Hymer %s: %s", mac.c_str(), why);
    drop_client();
    {
      Lock l;
      s_block[mac] = millis() + block_ms;
    }
    s_next_try_ms = millis() + s_backoff_ms;
    if (s_backoff_ms < 60000) s_backoff_ms *= 2;
    set_status(why);
  };
  if (!s_client->connect(NimBLEAddress(mac, type), true)) return fail("Verbindung fehlgeschlagen", 20000);
  // SCU erkennen (Power-Dienst), Nordic UART suchen
  NimBLERemoteService *pw = s_client->getService(NimBLEUUID(UUID_POWER));
  NimBLERemoteService *nus = s_client->getService(NimBLEUUID(UUID_NUS));
  if (!nus) return fail("Kein Hymer-Gerät (Dienst fehlt)", 10 * 60 * 1000);
  if (!pw && !s_paired && !pairing) return fail("Kein Hymer-Gerät", 10 * 60 * 1000);
  // Ohne Kopplung ignoriert die SCU den TLS-Aufbau; beim ersten Mal muss CONNECTION gedrueckt sein
  if (!s_client->secureConnection())
    return fail(pairing ? "Kopplung abgelehnt – an der SCU „CONNECTION“ drücken" : "Verschlüsselung fehlgeschlagen – neu koppeln?", 20000);
  s_ch_rx = nus->getCharacteristic(NimBLEUUID(UUID_NUS_RX));
  s_ch_tx = nus->getCharacteristic(NimBLEUUID(UUID_NUS_TX));
  if (!s_ch_rx || !s_ch_tx) return fail("Kein Hymer-Gerät (UART fehlt)", 10 * 60 * 1000);
  // Aufwecken (wie die App), falls die SCU schlaeft
  if (pw) {
    NimBLERemoteCharacteristic *ctl = pw->getCharacteristic(NimBLEUUID(UUID_POWER_CTRL));
    static const uint8_t wake = 0x0A;
    if (ctl) ctl->writeValue(&wake, 1, true);
  }
  bool notify = s_ch_tx->canNotify();   // SCU: Notify, SIU: Indicate
  if (!s_ch_tx->subscribe(notify, on_notify, true)) return fail("Abonnieren abgelehnt – neu koppeln?", 20000);
  s_connected = true;
  if (pairing) {
    std::string id = s_client->getConnInfo().getIdAddress().toString();
    {
      Lock l;
      s_paired = true;
      s_pairing = false;
      s_peer_id = id;
    }
    s_prefs.begin("hymer", false);
    s_prefs.putBool("paired", true);
    s_prefs.putString("peer", id.c_str());
    s_prefs.end();
    log_i("Hymer gekoppelt: %s", id.c_str());
  }
  s_up = true;
}

// ---------------------------------------------------------------- oeffentliche API
void hymer_begin() {
  s_mtx = xSemaphoreCreateRecursiveMutex();
  s_wq = xQueueCreate(8, sizeof(WriteReq));
  s_prefs.begin("hymer", false);
  s_paired = s_prefs.getBool("paired", false);
  s_peer_id = s_prefs.getString("peer", "").c_str();
  s_prefs.end();
  strncpy(s_status, s_paired ? "Suche SCU …" : "Nicht gekoppelt", sizeof(s_status) - 1);
  log_i("Hymer: %s", s_paired ? "gekoppelt" : "nicht gekoppelt");
}

uint32_t hymer_version() { return s_ver; }

std::vector<SatPoint> hymer_points() {
  std::vector<SatPoint> r;
  if (!s_mtx) return r;
  Lock l;
  int32_t age = s_last_rx_ms ? (int32_t)((millis() - s_last_rx_ms) / 1000) : -1;
  for (size_t i = 0; i < N_DEFS; i++) {
    if (!s_has[i]) continue;
    const HymerDef &d = DEFS[i];
    SatPoint p;
    p.kind = (SatPointKind)d.kind;
    p.idx = HYMER_IDX_BASE + i;
    p.id = sat_point_id(p.kind, p.idx);
    p.name = tr(d.name);
    p.cls = d.cls;
    p.unit = d.unit;
    p.ok = s_ready;
    p.on = s_on[i];
    if (d.kind != SP_SWITCH && d.kind != SP_INPUT) p.value = s_val[i];
    p.age_s = age;
    if (!s_ready) p.status = tr("Hymer getrennt");
    r.push_back(p);
  }
  return r;
}

bool hymer_has_switches() {
  if (!s_mtx) return false;
  Lock l;
  for (size_t i = 0; i < N_DEFS; i++)
    if (s_has[i] && DEFS[i].wr) return true;
  return false;
}

bool hymer_switch(uint8_t idx, uint8_t val) {
  if (!s_mtx || idx < HYMER_IDX_BASE || idx - HYMER_IDX_BASE >= (int)N_DEFS) return false;
  size_t i = idx - HYMER_IDX_BASE;
  if (!DEFS[i].wr || !s_ready) return false;
  WriteReq w;
  {
    Lock l;
    bool on = val == 2 ? !s_on[i] : val == 1;
    w = {(uint8_t)i, (uint8_t)on};
    s_on[i] = on;              // sofort anzeigen, die SCU bestaetigt
    s_pend_ms[i] = millis();
  }
  if (xQueueSend(s_wq, &w, 0) != pdTRUE) return false;
  s_ver++;
  return true;
}

void hymer_pair_start() {
  {
    Lock l;
    s_pairing = true;
    s_pair_until = millis() + PAIR_MS;
    s_block.clear();
    s_cand.clear();
    s_cand_rssi = -127;
  }
  s_next_try_ms = 0;
  s_backoff_ms = 15000;
  if (s_client && s_client->isConnected()) s_client->disconnect();
  set_status("Kopplung läuft – an der SCU „CONNECTION“ drücken");
}

bool hymer_pairing() { return s_pairing; }
bool hymer_paired() { return s_paired; }
bool hymer_connected() { return s_ready; }

std::string hymer_status() {
  if (!s_mtx) return "";
  Lock l;
  return s_status;
}

void hymer_disconnect() {
  s_pairing = false;
  if (s_client && s_client->isConnected()) s_client->disconnect();
  set_status("Ausgeschaltet");
}

void hymer_forget() {
  std::string id;
  {
    Lock l;
    id = s_peer_id;
    s_paired = false;
    s_pairing = false;
    s_peer_id.clear();
    for (size_t i = 0; i < N_DEFS; i++) s_has[i] = false;
  }
  if (s_client && s_client->isConnected()) s_client->disconnect();
  // Nur die Kopplung mit der SCU loeschen (das iNet X ist ebenfalls gekoppelt)
  for (int i = NimBLEDevice::getNumBonds() - 1; i >= 0; i--) {
    NimBLEAddress a = NimBLEDevice::getBondedAddress(i);
    if (!id.empty() && a.toString() == id) NimBLEDevice::deleteBond(a);
  }
  s_prefs.begin("hymer", false);
  s_prefs.putBool("paired", false);
  s_prefs.remove("peer");
  s_prefs.end();
  set_status("Nicht gekoppelt");
  s_ver++;
}

#endif
