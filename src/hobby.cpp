// Hobby-Wohnwagen/-Reisemobile: HobbyConnect-Box per Bluetooth und LSG-Modul (CI-BUS) per Kabel – siehe hobby.h.
// Eigene Umsetzung nach Protokollwissen aus:
//  - AndreasKirsten79/hobby-caravan-pi (BLE, Doku BLE-PROTOKOLL.md; ohne Lizenzangabe -> nur Protokollwissen)
//  - BastyJuice/Hobby_Caravan_Serial_2_CI-BUS_Connectivity (CI-BUS seriell, MIT)
// Beschreibung: docs/protokolle/hobby_ble_cibus.md
#include "hobby.h"

#if WITH_HOBBY
#include <Arduino.h>
#include <HardwareSerial.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <deque>
#include <map>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "i18n/i18n.h"
#include "settings.h"

namespace {

// ---------------------------------------------------------------- Variablen
enum Wr : uint8_t {
  W_NO = 0,    // nur lesen
  W_TGL = 1,   // Umschalten: BLE "cmd-tgl:KEY", Kabel "!tgl:<id>" (zustandsbewusst: nur senden, wenn Soll != Ist)
  W_DIM = 2,   // Dimmer 0..15 als Schalter: BLE "net-KEY-N", Kabel "!set:<id>=N"
  W_RUN = 3,   // Hauptschalter: nur Kabel "!run:30" (ein) / "!run:31" (aus)
};
enum Conv : uint8_t { C_NUM = 0, C_WATER = 1 };

struct Def {
  const char *key;
  uint16_t cid;        // Nummer am CI-BUS laut BastyJuice-Tabelle (Ersatz, falls "!gvl" nicht auswertbar ist)
  SatPointKind kind;
  uint8_t cls;
  const char *unit;
  const char *name;
  Wr wr;
  Conv conv;
};

// Reihenfolge = Punktnummer (idx = Position + 1) – neue Eintraege nur hinten anfuegen, sonst wandern die Kacheln
const Def DEFS[] = {
    {"LIGHT_DECKE", 61, SP_SWITCH, SC_NONE, "", "Deckenlicht", W_TGL, C_NUM},
    {"LIGHT_WAND", 62, SP_SWITCH, SC_NONE, "", "Wandlicht", W_TGL, C_NUM},
    {"LIGHT_BETTL", 63, SP_SWITCH, SC_NONE, "", "Bettlicht links", W_TGL, C_NUM},
    {"LIGHT_BETTR", 64, SP_SWITCH, SC_NONE, "", "Bettlicht rechts", W_TGL, C_NUM},
    {"LIGHT_DUSCHE", 65, SP_SWITCH, SC_NONE, "", "Licht Dusche", W_TGL, C_NUM},
    {"LIGHT_WASCH", 66, SP_SWITCH, SC_NONE, "", "Licht Waschraum", W_TGL, C_NUM},
    {"LIGHT_AMB1", 67, SP_SWITCH, SC_NONE, "", "Ambientelicht 1", W_TGL, C_NUM},
    {"LIGHT_AMB2", 68, SP_SWITCH, SC_NONE, "", "Ambientelicht 2", W_TGL, C_NUM},
    {"LIGHT_AMB3", 69, SP_SWITCH, SC_NONE, "", "Ambientelicht 3", W_TGL, C_NUM},
    {"LIGHT_ZUSATZL", 70, SP_SWITCH, SC_NONE, "", "Zusatzlicht links", W_TGL, C_NUM},
    {"LIGHT_ZUSATZM", 71, SP_SWITCH, SC_NONE, "", "Zusatzlicht Mitte", W_TGL, C_NUM},
    {"LIGHT_ZUSATZR", 72, SP_SWITCH, SC_NONE, "", "Zusatzlicht rechts", W_TGL, C_NUM},
    {"LIGHT_KUECHE", 73, SP_SWITCH, SC_NONE, "", "Küchenlicht", W_TGL, C_NUM},
    {"LIGHT_AUSSEN", 74, SP_SWITCH, SC_NONE, "", "Außenlicht", W_TGL, C_NUM},
    {"LIGHT_THERME", 75, SP_SWITCH, SC_NONE, "", "Licht Therme", W_TGL, C_NUM},
    {"LIGHT_FUSSB", 76, SP_SWITCH, SC_NONE, "", "Fußbodenlicht", W_TGL, C_NUM},
    {"LIGHT_DIM0", 0, SP_SWITCH, SC_NONE, "", "Dimmer 0", W_DIM, C_NUM},
    {"LIGHT_DIM1", 0, SP_SWITCH, SC_NONE, "", "Dimmer 1", W_DIM, C_NUM},
    {"LIGHT_DIM2", 0, SP_SWITCH, SC_NONE, "", "Dimmer 2", W_DIM, C_NUM},
    {"LIGHT_DIM3", 0, SP_SWITCH, SC_NONE, "", "Dimmer 3", W_DIM, C_NUM},
    {"LIGHT_DIM4", 0, SP_SWITCH, SC_NONE, "", "Dimmer 4", W_DIM, C_NUM},
    {"AC_TRUMA_LIGHT_ON_OFF", 84, SP_SWITCH, SC_NONE, "", "Licht Klimaanlage", W_TGL, C_NUM},
    {"FRIDGE_ON_OFF", 105, SP_SWITCH, SC_NONE, "", "Kühlschrank", W_TGL, C_NUM},
    {"FLOOR_HEATER_ON", 0, SP_SWITCH, SC_NONE, "", "Fußbodenheizung", W_TGL, C_NUM},
    {"THERME_ON", 0, SP_SWITCH, SC_NONE, "", "Therme", W_TGL, C_NUM},
    {"AC_DOM_FJ_ENABLE", 9, SP_SWITCH, SC_NONE, "", "Klimaanlage", W_TGL, C_NUM},
    {"HS_EN", 18, SP_SWITCH, SC_NONE, "", "Hauptschalter", W_RUN, C_NUM},
    // Zustaende (nur lesen)
    {"LINE_EN", 17, SP_INPUT, SC_ONOFF, "", "Landstrom", W_NO, C_NUM},
    {"DP_EN", 19, SP_INPUT, SC_ONOFF, "", "Zündung (D+)", W_NO, C_NUM},
    {"HEATER_ONOFF", 34, SP_INPUT, SC_ONOFF, "", "Heizung", W_NO, C_NUM},
    {"HEATER_WATER", 36, SP_INPUT, SC_ONOFF, "", "Warmwasser", W_NO, C_NUM},
    {"HEATER_EL", 38, SP_INPUT, SC_ONOFF, "", "Heizung elektrisch", W_NO, C_NUM},
    {"HEATER_GAS", 39, SP_INPUT, SC_ONOFF, "", "Heizung Gas", W_NO, C_NUM},
    {"TH_A_EN", 90, SP_INPUT, SC_ONOFF, "", "Truma Heizung", W_NO, C_NUM},
    {"TH_W_EN", 91, SP_INPUT, SC_ONOFF, "", "Truma Warmwasser", W_NO, C_NUM},
    {"AC_TRUMA_ENABLE", 80, SP_INPUT, SC_ONOFF, "", "Klimaanlage (Truma)", W_NO, C_NUM},
    {"ULTRAHEAT_ONOFF", 14, SP_INPUT, SC_ONOFF, "", "Ultraheat", W_NO, C_NUM},
    {"CHARGER0_ACTIVE", 41, SP_INPUT, SC_ONOFF, "", "Ladegerät", W_NO, C_NUM},
    // Temperaturen
    {"TEMP_IN", 23, SP_TEMP, SC_TEMP, "°C", "Innentemperatur", W_NO, C_NUM},
    {"TEMP_OUT", 24, SP_TEMP, SC_TEMP, "°C", "Außentemperatur", W_NO, C_NUM},
    {"IBS0_TEMPERATURE", 5, SP_TEMP, SC_TEMP, "°C", "Batterietemperatur", W_NO, C_NUM},
    {"HEATER_TEMP", 35, SP_TEMP, SC_TEMP, "°C", "Heizung Soll", W_NO, C_NUM},
    {"HEATER_WATER_TEMP", 37, SP_TEMP, SC_TEMP, "°C", "Warmwasser Soll", W_NO, C_NUM},
    {"TH_A_T", 92, SP_TEMP, SC_TEMP, "°C", "Truma Raum Soll", W_NO, C_NUM},
    {"TH_W_T", 93, SP_TEMP, SC_TEMP, "°C", "Truma Wasser Soll", W_NO, C_NUM},
    {"AC_DOM_FJ_TARGETTEMP", 11, SP_TEMP, SC_TEMP, "°C", "Klima Soll", W_NO, C_NUM},
    // Batterie, Bordnetz
    {"IBS0_UBAT", 2, SP_ANALOG, SC_VOLTAGE, "V", "Batterie Spannung", W_NO, C_NUM},
    {"IBS0_IBAT", 1, SP_ANALOG, SC_CURRENT, "A", "Batterie Strom", W_NO, C_NUM},
    {"IBS0_SOC2", 3, SP_ANALOG, SC_LEVEL, "%", "Batterie Ladezustand", W_NO, C_NUM},
    {"IBS0_REMAINING_TIME", 4, SP_ANALOG, SC_NONE, "h", "Batterie Restzeit", W_NO, C_NUM},
    {"UBAT", 25, SP_ANALOG, SC_VOLTAGE, "V", "Bordspannung", W_NO, C_NUM},
    {"CHARGER0_I", 42, SP_ANALOG, SC_CURRENT, "A", "Ladestrom", W_NO, C_NUM},
    {"ULTRAHEAT_POWER", 15, SP_ANALOG, SC_POWER, "W", "Ultraheat Leistung", W_NO, C_NUM},
    {"FRIDGE_TEMP", 107, SP_ANALOG, SC_NONE, "", "Kühlschrank Stufe", W_NO, C_NUM},
    // Wasser: 0..4 Stufen (je 25 %) bzw. Prozent, teils "37:37"
    {"FRESH_WATER_LEVEL", 29, SP_TANK, SC_LEVEL, "%", "Frischwasser", W_NO, C_WATER},
    {"WATER_LEVEL", 0, SP_TANK, SC_LEVEL, "%", "Frischwasser", W_NO, C_WATER},
};
const size_t N_DEFS = sizeof(DEFS) / sizeof(DEFS[0]);

// Baugruppe mit "<PREFIX>AVAILABLE" = 0: ihre Werte nicht anzeigen
const char *const GROUPS[] = {"FRIDGE_", "HEATER_", "AC_DOM_FJ_", "AC_TRUMA_", "ULTRAHEAT_", "IBS0_", "CHARGER0_", "TH_"};

const char *SVC_UUID = "c7841029-fe7c-4894-8532-f97908ef1ae4";
const uint32_t SESSION_MS = 300000;   // Box beendet die Sitzung sonst irgendwann: alle 5 min neu verbinden
const uint32_t STALE_MS = 120000;     // Kabel: so lange ohne Zeile = Verbindung weg

struct Op {
  Wr wr;
  std::string key;
  int val;
};

SemaphoreHandle_t s_mtx = nullptr;
struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};

HobbyCfg s_cfg;
uint32_t s_ver = 1;
std::map<std::string, std::string> s_raw;   // alle empfangenen Variablen (Diagnose "hobby")
bool s_has[N_DEFS] = {};
bool s_on[N_DEFS] = {};
float s_val[N_DEFS];
int s_dim_last[N_DEFS];                     // Dimmer: letzte Stufe > 0 (zum Wiedereinschalten)
std::deque<Op> s_ops;

// Bluetooth
NimBLEClient *s_client = nullptr;
NimBLERemoteCharacteristic *s_chr = nullptr;
bool s_ble_up = false;         // verbunden und Kanal abonniert
volatile bool s_ble_lost = false;
uint32_t s_conn_ms = 0, s_next_try = 0, s_hs_at = 0, s_last_tx = 0, s_ble_rx_ms = 0;
uint8_t s_hs_step = 0, s_fails = 0;
std::string s_frag, s_ble_status;
uint32_t s_search_until = 0;
std::vector<HobbyCand> s_cands;

// Kabel (CI-BUS)
HardwareSerial *s_ser = nullptr;
TaskHandle_t s_ser_task = nullptr;
volatile bool s_ser_stop = false;
bool s_ser_up = false;
uint32_t s_ser_rx_ms = 0;
std::string s_ser_status;
std::map<int, std::string> s_ids;           // CI-BUS-Nummer -> Variablenname (aus "!gvl")

void logf(const char *fmt, ...) {
  char b[200];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(b, sizeof(b), fmt, ap);
  va_end(ap);
  log_printf("[hobby] %s\n", b);
}

int def_of(const std::string &key) {
  for (size_t i = 0; i < N_DEFS; i++)
    if (key == DEFS[i].key) return (int)i;
  return -1;
}

bool group_hidden(const char *key) {
  for (const char *g : GROUPS) {
    size_t n = strlen(g);
    if (strncmp(key, g, n)) continue;
    auto it = s_raw.find(std::string(g) + "AVAILABLE");
    if (it != s_raw.end()) {
      const std::string &v = it->second;
      return v == "0" || v == "Off" || v == "off";
    }
  }
  return false;
}

bool parse_on(const std::string &s) {
  if (s.empty()) return false;
  char *e = nullptr;
  long n = strtol(s.c_str(), &e, 10);
  if (e != s.c_str()) return n != 0;
  std::string l;
  for (char c : s) l += (char)tolower((unsigned char)c);
  return l.rfind("on", 0) == 0 || l == "true" || l == "an" || l == "ein";
}

float parse_num(const std::string &s) {
  std::string t;
  for (char c : s) {
    if (c == ',') c = '.';
    if (c == ':') break;   // "37:37"
    t += c;
  }
  const char *p = t.c_str();
  while (*p == ' ') p++;
  char *e = nullptr;
  float f = strtof(p, &e);
  return e == p ? NAN : f;
}

// Wert einer Variablen uebernehmen (unter Lock)
void set_var(const std::string &key, std::string val) {
  size_t pos;
  while ((pos = val.find("^C")) != std::string::npos) val.replace(pos, 2, "\xC2\xB0" "C");
  while (!val.empty() && (val.back() == ' ' || val.back() == '\r' || val.back() == '\n')) val.pop_back();
  while (!val.empty() && val.front() == ' ') val.erase(0, 1);
  if (key.empty() || key.size() > 32) return;
  auto it = s_raw.find(key);
  if (it != s_raw.end() && it->second == val) return;
  if (s_raw.size() > 200 && it == s_raw.end()) return;   // Schutz vor Muell
  s_raw[key] = val;
  s_ver++;
  int i = def_of(key);
  if (i < 0) return;
  const Def &d = DEFS[i];
  s_has[i] = true;
  if (d.wr == W_DIM) {
    float f = parse_num(val);
    int n = isnan(f) ? 0 : (int)lroundf(f);
    s_on[i] = n > 0;
    s_val[i] = n;
    if (n > 0) s_dim_last[i] = n;
  } else if (d.kind == SP_SWITCH || d.kind == SP_INPUT) {
    s_on[i] = parse_on(val);
  } else {
    float f = parse_num(val);
    if (d.conv == C_WATER && !isnan(f) && f <= 4 && f == floorf(f) && val.find(':') == std::string::npos) f *= 25;
    s_val[i] = f;
  }
}

// ---------------------------------------------------------------- Einstellungen
void load_cfg() {
  Preferences p;
  if (!p.begin("hobby", true)) return;
  s_cfg.ble_on = p.getBool("ble", false);
  s_cfg.mac = p.getString("mac", "").c_str();
  s_cfg.mac_type = p.getUChar("at", 0);
  s_cfg.uart_on = p.getBool("uon", false);
  s_cfg.uart = p.getChar("un", 1);
  s_cfg.rx = p.getChar("rx", -1);
  s_cfg.tx = p.getChar("tx", -1);
  p.end();
}
void save_cfg() {
  Preferences p;
  if (!p.begin("hobby", false)) return;
  p.putBool("ble", s_cfg.ble_on);
  p.putString("mac", s_cfg.mac.c_str());
  p.putUChar("at", s_cfg.mac_type);
  p.putBool("uon", s_cfg.uart_on);
  p.putChar("un", s_cfg.uart);
  p.putChar("rx", s_cfg.rx);
  p.putChar("tx", s_cfg.tx);
  p.end();
}

// Welcher Weg sendet Befehle: Kabel vor Bluetooth
bool use_serial() { return s_ser_up; }

// ---------------------------------------------------------------- Kabel (CI-BUS am LSG)
// Zeile auswerten: "$<id>:<wert>" (Datenstrom), bei "!gvl" Zuordnung Nummer -> Name ("$<id>…NAME…" o. ae.)
bool name_token(const char *p, std::string &out) {
  if (!(*p >= 'A' && *p <= 'Z')) return false;
  std::string t;
  while ((*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_') t += *p++;
  if (t.size() < 3 || t == "GVL" || t == "END" || t == "SYS" || t == "SON" || t == "SOF") return false;
  out = t;
  return true;
}

void ser_line(const std::string &ln) {
  const char *s = ln.c_str();
  while (*s == ' ') s++;
  const char *d = strchr(s, '$');
  int id = -1;
  const char *rest = nullptr;
  if (d && isdigit((unsigned char)d[1])) {
    id = atoi(d + 1);
    rest = d + 1;
    while (isdigit((unsigned char)*rest)) rest++;
  } else if (isdigit((unsigned char)*s)) {   // Variablenliste ohne '$'
    id = atoi(s);
    rest = s;
    while (isdigit((unsigned char)*rest)) rest++;
  }
  if (id < 0 || id > 999) return;
  while (*rest == ':' || *rest == '=' || *rest == ' ' || *rest == '\t' || *rest == ';') rest++;
  std::string name;
  Lock l;
  if (name_token(rest, name)) {
    // "NAME" bzw. "NAME:wert" / "NAME=wert": Zuordnung merken
    s_ids[id] = name;
    const char *v = rest + name.size();
    if (*v == ':' || *v == '=') set_var(name, v + 1);
    return;
  }
  auto it = s_ids.find(id);
  if (it != s_ids.end()) {
    set_var(it->second, rest);
    return;
  }
  for (size_t i = 0; i < N_DEFS; i++)   // Ersatz: Nummern laut Tabelle
    if (DEFS[i].cid == id) {
      set_var(DEFS[i].key, rest);
      return;
    }
  set_var("$" + std::to_string(id), rest);
}

int ser_id_of(const std::string &key) {
  for (auto &kv : s_ids)
    if (kv.second == key) return kv.first;
  int i = def_of(key);
  return i >= 0 ? DEFS[i].cid : 0;
}

void ser_send(const char *cmd) {
  if (!s_ser) return;
  s_ser->print(cmd);
  s_ser->print("\r\n");
  s_ser->flush();
}

bool ser_readline(std::string &out, uint32_t wait_ms) {
  static std::string buf;
  uint32_t t0 = millis();
  for (;;) {
    while (s_ser && s_ser->available()) {
      char c = (char)s_ser->read();
      if (c == '\n' || c == '\r') {
        if (buf.empty()) continue;
        out.swap(buf);
        buf.clear();
        return true;
      }
      if (buf.size() < 160) buf += c;
    }
    if (millis() - t0 >= wait_ms || s_ser_stop) return false;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void set_ser_status(const char *t) {
  Lock l;
  if (s_ser_status != t) {
    s_ser_status = t;
    s_ver++;
  }
}

void ser_task(void *) {
  HobbyCfg c;
  {
    Lock l;
    c = s_cfg;
  }
  s_ser = new HardwareSerial(c.uart);
  s_ser->setRxBufferSize(1024);
  s_ser->begin(115200, SERIAL_8N1, c.rx, c.tx);
  logf("Kabel: UART%d RX %d TX %d, 115200 Baud", c.uart, c.rx, c.tx);
  uint32_t last_init = 0;
  bool init_done = false;
  std::string ln;
  while (!s_ser_stop) {
    uint32_t now = millis();
    if (!init_done || (now - s_ser_rx_ms > STALE_MS && now - last_init > 30000)) {
      // Anmeldung wie das Original: System, Strom aus, Variablenliste bis "!end", Datenstrom ein
      set_ser_status(tr("Verbinde mit dem LSG …"));
      last_init = now;
      while (s_ser->available()) s_ser->read();
      ser_send("!sys");
      vTaskDelay(pdMS_TO_TICKS(500));
      ser_send("!sof");
      vTaskDelay(pdMS_TO_TICKS(500));
      ser_send("!gvl");
      uint32_t t0 = millis();
      bool end = false;
      while (!s_ser_stop && millis() - t0 < 20000) {
        if (!ser_readline(ln, 200)) continue;
        s_ser_rx_ms = millis();
        if (ln.find("!end") != std::string::npos) {
          end = true;
          break;
        }
        ser_line(ln);
      }
      ser_send("!son");
      init_done = true;
      {
        Lock l;
        logf("Kabel: Variablenliste %s (%u Namen)", end ? "vollstaendig" : "ohne !end", (unsigned)s_ids.size());
      }
      if (!end && !s_ser_rx_ms) set_ser_status(tr("Keine Antwort vom LSG – Verkabelung (RS232-Wandler, RX/TX) prüfen"));
    }
    if (ser_readline(ln, 100)) {
      s_ser_rx_ms = millis();
      ser_line(ln);
    }
    bool up = s_ser_rx_ms && millis() - s_ser_rx_ms < STALE_MS;
    {
      Lock l;
      if (up != s_ser_up) {
        s_ser_up = up;
        s_ver++;
      }
      if (up) {
        if (s_ser_status != tr("Verbunden")) {
          s_ser_status = tr("Verbunden");
          s_ver++;
        }
      } else if (s_ser_rx_ms) {
        s_ser_status = tr("Keine Daten mehr vom LSG");
      }
    }
    // Befehle (Kabel hat Vorrang vor Bluetooth)
    Op op;
    bool have = false;
    int id = 0;
    {
      Lock l;
      if (s_ser_up && !s_ops.empty()) {
        op = s_ops.front();
        s_ops.pop_front();
        have = true;
        id = ser_id_of(op.key);
      }
    }
    if (have) {
      char cmd[32];
      if (op.wr == W_RUN) snprintf(cmd, sizeof(cmd), "!run:%d", op.val ? 30 : 31);
      else if (!id) cmd[0] = 0;
      else if (op.wr == W_TGL) snprintf(cmd, sizeof(cmd), "!tgl:%d", id);
      else snprintf(cmd, sizeof(cmd), "!set:%d=%d", id, op.val);
      if (cmd[0]) {
        ser_send(cmd);
        logf("Kabel -> %s (%s)", cmd, op.key.c_str());
        vTaskDelay(pdMS_TO_TICKS(200));
      } else {
        logf("Kabel: Nummer von %s unbekannt – Befehl verworfen", op.key.c_str());
      }
    }
  }
  s_ser->end();
  delete s_ser;
  s_ser = nullptr;
  {
    Lock l;
    s_ser_up = false;
    s_ser_status = "";
    s_ver++;
  }
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
  if (s_cfg.rx < 0 || s_cfg.tx < 0 || s_cfg.uart < 1 || s_cfg.uart >= hobby_uart_count()) {
    set_ser_status(tr("RX/TX-Pin und UART einstellen"));
    return;
  }
  s_ser_stop = false;
  s_ser_rx_ms = 0;
  xTaskCreatePinnedToCore(ser_task, "hobbyser", 4096, nullptr, 2, &s_ser_task, 1);
}

// ---------------------------------------------------------------- Bluetooth (HobbyConnect-Box)
class ClientCB : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient *, int reason) override {
    s_ble_lost = true;
    logf("Box getrennt (Grund %d)", reason);
  }
};
ClientCB s_ccb;

void set_ble_status(const std::string &t) {
  Lock l;
  if (s_ble_status != t) {
    s_ble_status = t;
    s_ver++;
  }
}

void on_notify(NimBLERemoteCharacteristic *, uint8_t *d, size_t n, bool) {
  std::string raw((const char *)d, n);
  Lock l;
  s_ble_rx_ms = millis();
  if (!raw.empty() && raw.back() == '@') {   // Teilstueck, Rest folgt
    s_frag += raw.substr(0, raw.size() - 1);
    if (s_frag.size() > 240) s_frag.clear();
    return;
  }
  std::string full = s_frag + raw;
  s_frag.clear();
  size_t c = full.find(':');
  if (c == std::string::npos || c == 0) return;
  std::string key = full.substr(0, c);
  while (!key.empty() && key.front() == ' ') key.erase(0, 1);
  for (char ch : key)
    if (!(isalnum((unsigned char)ch) || ch == '_')) return;
  if (key == "BT_START") return;
  set_var(key, full.substr(c + 1));
}

bool ble_write(const std::string &cmd) {
  if (!s_chr) return false;
  bool rsp = s_chr->canWrite();
  if (cmd.size() <= 17) return s_chr->writeValue((const uint8_t *)cmd.data(), cmd.size(), rsp);
  std::string a = cmd.substr(0, 17) + "@", b = cmd.substr(17);   // Box nimmt hoechstens 17 Zeichen am Stueck
  if (!s_chr->writeValue((const uint8_t *)a.data(), a.size(), rsp)) return false;
  vTaskDelay(pdMS_TO_TICKS(100));
  return s_chr->writeValue((const uint8_t *)b.data(), b.size(), rsp);
}

void ble_drop(uint32_t retry_ms) {
  NimBLEClient *c;
  {
    Lock l;
    c = s_client;
    s_client = nullptr;
    s_chr = nullptr;
    if (s_ble_up) s_ver++;
    s_ble_up = false;
    s_hs_step = 0;
    s_frag.clear();
    s_next_try = millis() + retry_ms;
  }
  if (c) {
    if (c->isConnected()) c->disconnect();
    NimBLEDevice::deleteClient(c);
  }
}

}   // namespace

// ---------------------------------------------------------------- oeffentlich
void hobby_begin() {
  s_mtx = xSemaphoreCreateRecursiveMutex();
  for (size_t i = 0; i < N_DEFS; i++) {
    s_val[i] = NAN;
    s_dim_last[i] = 15;
  }
  load_cfg();
  ser_apply();
}

HobbyCfg hobby_cfg() {
  if (!s_mtx) return HobbyCfg();
  Lock l;
  return s_cfg;
}

void hobby_set_cfg(const HobbyCfg &c) {
  if (!s_mtx) return;
  bool ser_changed, ble_changed;
  {
    Lock l;
    ser_changed = c.uart_on != s_cfg.uart_on || c.uart != s_cfg.uart || c.rx != s_cfg.rx || c.tx != s_cfg.tx;
    ble_changed = c.ble_on != s_cfg.ble_on || c.mac != s_cfg.mac;
    s_cfg = c;
    if (!c.ble_on) s_search_until = 0;
    s_fails = 0;
    s_next_try = 0;
    s_ver++;
  }
  save_cfg();
  if (ser_changed) ser_apply();
  if (ble_changed) {
    Lock l;
    s_ble_lost = true;   // Bluetooth-Task trennt die alte Verbindung
  }
}

bool hobby_active() {
  if (!s_mtx) return false;
  Lock l;
  return s_cfg.ble_on || s_cfg.uart_on;
}

std::string hobby_status() {
  if (!s_mtx) return "";
  Lock l;
  if (!s_cfg.ble_on) return tr("Aus");
  if (s_cfg.mac.empty()) return tr("Keine Box gewählt");
  if (s_ble_up) return s_ble_rx_ms ? tr("Verbunden") : tr("Verbunden, warte auf Daten …");
  return s_ble_status.empty() ? tr("Suche Box …") : s_ble_status;
}

std::string hobby_uart_status() {
  if (!s_mtx) return "";
  Lock l;
  if (!s_cfg.uart_on) return tr("Aus");
  return s_ser_status;
}

uint32_t hobby_version() {
  if (!s_mtx) return 0;
  Lock l;
  return s_ver;
}

std::vector<SatPoint> hobby_points() {
  std::vector<SatPoint> r;
  if (!s_mtx) return r;
  Lock l;
  if (!s_cfg.ble_on && !s_cfg.uart_on) return r;
  bool up = s_ser_up || s_ble_up;
  uint32_t last = s_ser_rx_ms > s_ble_rx_ms ? s_ser_rx_ms : s_ble_rx_ms;
  int32_t age = last ? (int32_t)((millis() - last) / 1000) : -1;
  bool water_done = false;
  for (size_t i = 0; i < N_DEFS; i++) {
    if (!s_has[i] || group_hidden(DEFS[i].key)) continue;
    const Def &d = DEFS[i];
    if (d.conv == C_WATER) {   // FRESH_WATER_LEVEL (Kabel) und WATER_LEVEL (Box): nur einmal anzeigen
      if (water_done) continue;
      water_done = true;
    }
    SatPoint p;
    p.kind = d.kind;
    p.idx = (uint8_t)(i + 1);
    p.id = sat_point_id(p.kind, p.idx);
    p.name = tr(d.name);
    p.cls = d.cls;
    p.unit = d.unit;
    p.ok = up;
    p.on = s_on[i];
    if (d.kind != SP_SWITCH && d.kind != SP_INPUT) p.value = s_val[i];
    if (d.wr == W_DIM) p.value = s_val[i];
    p.age_s = age;
    if (!up) p.status = tr("Hobby getrennt");
    r.push_back(p);
  }
  return r;
}

bool hobby_switch(uint8_t idx, bool on) {
  if (!s_mtx || idx < 1 || idx > N_DEFS) return false;
  size_t i = idx - 1;
  const Def &d = DEFS[i];
  Lock l;
  if (d.wr == W_NO || !(s_ser_up || s_ble_up)) return false;
  if (d.wr == W_RUN && !s_ser_up) return false;   // Hauptschalter nur ueber das Kabel
  Op op{d.wr, d.key, on ? 1 : 0};
  if (d.wr == W_TGL) {
    if (s_has[i] && s_on[i] == on) return true;   // schon so (Box kann nur umschalten)
  } else if (d.wr == W_DIM) {
    op.val = on ? (s_dim_last[i] > 0 ? s_dim_last[i] : 15) : 0;
  }
  // gleiche Variable schon in der Warteschlange: ersetzen (schnelles Tippen)
  for (auto it = s_ops.begin(); it != s_ops.end(); ++it)
    if (it->key == op.key) {
      s_ops.erase(it);
      if (d.wr == W_TGL) {   // zweimal umschalten = nichts tun
        s_on[i] = on;
        s_ver++;
        return true;
      }
      break;
    }
  if (s_ops.size() >= 16) return false;
  s_ops.push_back(op);
  s_on[i] = on;   // sofort anzeigen; die Rueckmeldung der Box korrigiert notfalls
  s_ver++;
  return true;
}

bool hobby_battery(VictronInfo &v) {
  if (!s_mtx) return false;
  Lock l;
  if (!s_cfg.ble_on && !s_cfg.uart_on) return false;
  int iv = def_of("IBS0_UBAT"), ii = def_of("IBS0_IBAT"), is = def_of("IBS0_SOC2"), it = def_of("IBS0_REMAINING_TIME"),
      itp = def_of("IBS0_TEMPERATURE");
  if (iv < 0 || !s_has[iv] || group_hidden("IBS0_")) return false;
  v = VictronInfo();
  v.key = HOBBY_BAT_KEY;
  v.mac = HOBBY_SAT_ID;
  v.name = tr("Bordbatterie");
  v.kind = VIC_SHUNT;
  v.record = 0x02;
  v.has = true;
  v.bat_v = s_val[iv];
  v.bat_i = ii >= 0 && s_has[ii] ? s_val[ii] : NAN;
  v.power_w = isnan(v.bat_v) || isnan(v.bat_i) ? NAN : v.bat_v * v.bat_i;
  v.soc = is >= 0 && s_has[is] ? s_val[is] : NAN;
  v.consumed_ah = NAN;
  float h = it >= 0 && s_has[it] ? s_val[it] : NAN;
  v.remaining_min = isnan(h) ? -1 : (int)lroundf(h * 60);
  if (itp >= 0 && s_has[itp] && !isnan(s_val[itp])) {
    v.aux_mode = 2;
    v.aux = s_val[itp];
  }
  bool up = s_ser_up || s_ble_up;
  uint32_t last = s_ser_rx_ms > s_ble_rx_ms ? s_ser_rx_ms : s_ble_rx_ms;
  v.age_s = up && last ? (int32_t)((millis() - last) / 1000) : 9999;
  if (up) v.age_s = 0;   // Box/LSG melden nur Aenderungen: verbunden = aktuell
  v.status = up ? std::string("OK") : std::string(tr("Hobby getrennt"));
  return true;
}

void hobby_search(uint32_t ms) {
  if (!s_mtx) return;
  Lock l;
  s_search_until = (millis() + ms) | 1;
}

std::vector<HobbyCand> hobby_candidates() {
  if (!s_mtx) return {};
  Lock l;
  return s_cands;
}

int hobby_uart_count() { return SOC_UART_HP_NUM; }

void hobby_print_debug() {
  if (!s_mtx) {
    log_printf("Hobby: nicht gestartet\n");
    return;
  }
  Lock l;
  log_printf("Hobby: Bluetooth %s (Box %s) – %s\n", s_cfg.ble_on ? "an" : "aus", s_cfg.mac.empty() ? "-" : s_cfg.mac.c_str(),
             hobby_status().c_str());
  log_printf("Hobby: Kabel %s (UART%d RX %d TX %d) – %s, %u Nummern aus !gvl\n", s_cfg.uart_on ? "an" : "aus", s_cfg.uart,
             s_cfg.rx, s_cfg.tx, s_ser_status.c_str(), (unsigned)s_ids.size());
  for (auto &kv : s_raw) log_printf("  %-24s %s\n", kv.first.c_str(), kv.second.c_str());
  for (auto &kv : s_ids)
    if (def_of(kv.second) < 0) log_printf("  $%d = %s (nicht ausgewertet)\n", kv.first, kv.second.c_str());
}

// ---- Bluetooth-Verteiler (truma.cpp, Bluetooth-Task)
bool hobby_ble_needed() {
  if (!s_mtx) return false;
  Lock l;
  return s_cfg.ble_on && (!s_cfg.mac.empty() || s_search_until);
}

bool hobby_on_adv(const NimBLEAdvertisedDevice *dev) {
  if (!s_mtx) return false;
  std::string name = dev->haveName() ? dev->getName() : "";
  bool hit = name.rfind("HobbyConnect", 0) == 0;
  if (!hit && dev->haveServiceUUID()) hit = dev->isAdvertisingService(NimBLEUUID(SVC_UUID));
  if (!hit) return false;
  std::string mac = dev->getAddress().toString();
  for (auto &c : mac) c = (char)tolower((unsigned char)c);
  Lock l;
  if (mac == s_cfg.mac) s_cfg.mac_type = dev->getAddress().getType();
  if (s_search_until) {
    for (auto &c : s_cands)
      if (c.mac == mac) {
        c.rssi = dev->getRSSI();
        return true;
      }
    if (s_cands.size() < 8) {
      HobbyCand c;
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

void hobby_scan_wanted(bool &, bool &active) {
  if (!s_mtx) return;
  Lock l;
  if (s_search_until && (int32_t)(millis() - s_search_until) >= 0) {
    s_search_until = 0;
    s_ver++;
  }
  if (s_cfg.ble_on && s_search_until) active = true;
}

bool hobby_want_connect() {
  if (!s_mtx) return false;
  Lock l;
  return s_cfg.ble_on && !s_cfg.mac.empty() && !s_client && (int32_t)(millis() - s_next_try) >= 0;
}

void hobby_connect_now() {
  std::string mac;
  uint8_t type;
  {
    Lock l;
    mac = s_cfg.mac;
    type = s_cfg.mac_type;
    s_ble_lost = false;
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
  bool ok = c->connect(NimBLEAddress(mac, type), true);
  NimBLERemoteCharacteristic *chr = nullptr;
  if (ok) {
    NimBLERemoteService *svc = c->getService(NimBLEUUID(SVC_UUID));
    if (svc)
      for (auto *ch : svc->getCharacteristics(true))
        if (ch->canNotify() && (ch->canWrite() || ch->canWriteNoResponse())) {
          chr = ch;
          break;
        }
    if (!chr) logf("Box %s: Dienst/Kanal fehlt", mac.c_str());
    else if (!chr->subscribe(true, on_notify, true)) {
      logf("Box %s: Abo fehlgeschlagen", mac.c_str());
      chr = nullptr;
    }
  }
  if (!chr) {
    uint8_t f;
    {
      Lock l;
      f = s_fails < 8 ? ++s_fails : s_fails;
    }
    ble_drop(f * 15000UL);
    set_ble_status(ok ? tr("Box antwortet nicht wie erwartet") : tr("Box nicht erreichbar – in Reichweite? App getrennt?"));
    if (!ok) logf("Box %s: Verbindung fehlgeschlagen", mac.c_str());
    return;
  }
  Lock l;
  s_chr = chr;
  s_ble_up = true;
  s_fails = 0;
  s_conn_ms = millis();
  s_hs_step = 1;
  s_hs_at = millis() + 500;
  s_ble_status = "";
  s_ver++;
  logf("Box %s verbunden", mac.c_str());
}

void hobby_ble_loop() {
  if (!s_mtx) return;
  bool drop = false, stop = false;
  {
    Lock l;
    if (!s_client) return;
    if (s_ble_lost) {
      drop = true;
      stop = !s_cfg.ble_on || s_cfg.mac.empty();
    } else if (s_ble_up && millis() - s_conn_ms > SESSION_MS) {
      drop = true;   // Sitzung erneuern (wie die Original-App)
    }
  }
  if (drop) {
    ble_drop(stop ? 0 : 2000);
    if (stop) set_ble_status("");
    return;
  }
  uint32_t now = millis();
  std::string cmd;
  {
    Lock l;
    if (!s_ble_up || now - s_last_tx < 200) return;
    if (s_hs_step == 1 && (int32_t)(now - s_hs_at) >= 0) {
      std::string own = NimBLEDevice::getAddress().toString();
      for (auto &ch : own) ch = (char)tolower((unsigned char)ch);
      cmd = "net-BT_ID-" + own;   // Anmeldung mit der eigenen Bluetooth-Adresse
      s_hs_step = 2;
    } else if (s_hs_step == 2) {
      cmd = "net-BT_VARS";        // alle Variablen schicken lassen
      s_hs_step = 3;
    } else if (s_hs_step == 3 && !use_serial() && !s_ops.empty()) {
      Op op = s_ops.front();
      s_ops.pop_front();
      if (op.wr == W_TGL) cmd = "cmd-tgl:" + op.key;
      else if (op.wr == W_DIM) cmd = "net-" + op.key + "-" + std::to_string(op.val);
    }
    if (cmd.empty()) return;
    s_last_tx = now;
  }
  bool ok = ble_write(cmd);
  logf("Box -> %s%s", cmd.c_str(), ok ? "" : " (fehlgeschlagen)");
}

#endif
