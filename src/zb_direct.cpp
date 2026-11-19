// Zigbee direkt: Panel-Seite der Verbindung zum Zigbee-Funkmodul (ESP32-H2). Protokoll: zb_link.h.
#include "zb_direct.h"

#if WITH_ZB_DIRECT

#include <Arduino.h>
#include <Preferences.h>
#include <driver/uart.h>

#include <time.h>

#include <deque>
#include <map>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "i18n/i18n.h"   // tr(), UI_DECIMAL_COMMA
#include "rules.h"
#include "zb_link.h"

#ifndef ZBD_UART
#define ZBD_UART UART_NUM_3   // UART0 = Konsole; 2 ist fuer LIN (Truma/Alde) vorgesehen
#endif

namespace {

struct Val {
  std::string json;   // Wert als JSON (Zahl, Text, true/false)
  uint32_t ms = 0;
};
struct ZDev {
  std::string ieee, mf, model, eps;   // eps: JSON-Liste aus dem Modul
  std::string zv, zm, desc;           // laut Treiberliste: Hersteller, Modell, Beschreibung
  std::string name;                   // eigener Name (im Modul gespeichert)
  int n_exp = 0;                      // Anzahl Werte laut Treiber
  std::vector<std::pair<std::string, std::string>> props;   // umgerechnete Werte (Name -> JSON), aus "state"
  uint16_t nwk = 0xFFFF;
  bool done = false;
  uint32_t seen_ms = 0;
  std::map<std::string, Val> vals;    // "ep/cluster/attr" (hex) -> letzter Wert
};

SemaphoreHandle_t s_mx = nullptr;
bool s_on = true, s_started = false;
std::vector<ZDev> s_devs;
std::deque<std::string> s_log;   // letzte Meldungen des Moduls
uint32_t s_seen_ms = 0;          // letzte Nachricht vom Modul
std::string s_fw, s_coord;
bool s_ready = false;
uint16_t s_pan = 0;
uint8_t s_ch = 0;
uint8_t s_open = 0;
uint32_t s_open_ms = 0;
// Einstellungsseite: nur solange sie offen ist
std::string s_cfg_ieee;
std::vector<ZbdCfgItem> s_cfg;
bool s_cfg_done = false;

struct Lock {
  Lock() { xSemaphoreTake(s_mx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(s_mx); }
};

void add_log(const std::string &m) {
  s_log.push_back(m);
  while (s_log.size() > 30) s_log.pop_front();
}

ZDev *find(const std::string &ieee) {
  for (auto &d : s_devs)
    if (d.ieee == ieee) return &d;
  return nullptr;
}
ZDev &get(const std::string &ieee) {
  ZDev *d = find(ieee);
  if (d) return *d;
  s_devs.emplace_back();
  s_devs.back().ieee = ieee;
  return s_devs.back();
}

void write_line(const std::string &s) {
  uart_write_bytes(ZBD_UART, s.data(), s.size());
  uart_write_bytes(ZBD_UART, "\n", 1);
}
// Uhrzeit fuer den Tuya-Zeitabgleich (nur, wenn das Panel die Zeit kennt)
void send_time() {
  time_t now = time(nullptr);
  if (now < 1700000000) return;
  struct tm lt, gm;
  localtime_r(&now, &lt);
  gmtime_r(&now, &gm);
  gm.tm_isdst = lt.tm_isdst;
  long tz = (long)(now - mktime(&gm));   // Abstand Ortszeit – UTC in Sekunden
  write_line("{\"c\":\"time\",\"utc\":" + std::to_string((long)now) + ",\"tz\":" + std::to_string(tz) + "}");
}
void send_simple(const char *cmd) {
  std::string s = std::string("{\"c\":\"") + cmd + "\"}";
  write_line(s);
}

void on_line(const char *line) {
  if (line[0] != '{') return;   // z. B. Startmeldung des H2-ROM auf derselben Leitung
  JsonDocument j;
  if (deserializeJson(j, line)) return;
  std::string t = j["t"] | "";
  bool ask_devices = false;
  {
    Lock l;
    s_seen_ms = millis();
    if (t == "hello" || t == "info") {
      if (t == "hello") {
        add_log("Modul gestartet");
        ask_devices = true;
      }
      if (j["fw"].is<const char *>()) s_fw = j["fw"].as<const char *>();
      if (t == "info") {
        bool was = s_ready;
        s_ready = j["ready"] | false;
        s_pan = j["pan"] | 0;
        s_ch = j["ch"] | 0;
        s_coord = (const char *)(j["ieee"] | "");
        s_open = j["open"] | 0;
        s_open_ms = millis();
        if (s_ready && !was) ask_devices = true;
      }
    } else if (t == "dev") {
      ZDev &d = get(j["ieee"] | "");
      d.nwk = j["nwk"] | 0xFFFF;
      d.mf = (const char *)(j["mf"] | "");
      d.model = (const char *)(j["model"] | "");
      d.name = (const char *)(j["name"] | "");
      if (j["zm"].is<const char *>()) {
        d.zv = (const char *)(j["zv"] | "");
        d.zm = (const char *)(j["zm"] | "");
      }
      d.done = j["done"] | false;
      d.eps.clear();
      serializeJson(j["eps"], d.eps);
    } else if (t == "joined") {
      ZDev &d = get(j["ieee"] | "");
      d.nwk = j["nwk"] | 0xFFFF;
      d.seen_ms = millis();
      add_log("Geraet angemeldet: " + d.ieee + (d.done ? " (bekannt)" : " – wird abgefragt"));
    } else if (t == "left") {
      std::string ieee = j["ieee"] | "";
      for (size_t i = 0; i < s_devs.size(); i++)
        if (s_devs[i].ieee == ieee) {
          s_devs.erase(s_devs.begin() + i);
          break;
        }
      add_log("Geraet abgemeldet: " + ieee);
    } else if (t == "state") {
      std::string ieee = j["ieee"] | "";
      if (!ieee.empty()) {
        ZDev &d = get(ieee);
        d.seen_ms = millis();
        for (JsonPair kv : j["p"].as<JsonObject>()) {
          std::string v;
          serializeJson(kv.value(), v);
          bool hit = false;
          for (auto &x : d.props)
            if (x.first == kv.key().c_str()) {
              x.second = v;
              hit = true;
            }
          if (!hit && d.props.size() < 48) d.props.push_back({kv.key().c_str(), v});
        }
      }
    } else if (t == "cfg") {
      if (s_cfg_ieee == (const char *)(j["ieee"] | "")) {
        ZbdCfgItem it;
        it.name = (const char *)(j["n"] | "");
        it.unit = (const char *)(j["u"] | "");
        it.type = j["ty"] | 0;
        it.group = j["g"] | 0;
        it.min = j["mi"] | 0.0f;
        it.max = j["ma"] | 0.0f;
        it.step = j["st"] | 0.0f;
        for (const char *v : j["v"].as<JsonArray>()) it.values.push_back(v ? v : "");
        if (!j["cur"].isNull()) serializeJson(j["cur"], it.cur);
        s_cfg.push_back(it);
      }
    } else if (t == "cfg_end") {
      if (s_cfg_ieee == (const char *)(j["ieee"] | "")) s_cfg_done = true;
    } else if (t == "set") {
      for (JsonPair kv : j["ok"].as<JsonObject>()) add_log(std::string("Gesetzt: ") + kv.key().c_str());
      for (JsonPair kv : j["err"].as<JsonObject>())
        add_log(std::string("Nicht gesetzt: ") + kv.key().c_str() + " – " + (const char *)(kv.value() | ""));
    } else if (t == "attr") {
      std::string ieee = j["ieee"] | "";
      if (!ieee.empty()) {
        ZDev &d = get(ieee);
        d.seen_ms = millis();
        char k[20];
        snprintf(k, sizeof(k), "%u/%04x/%04x", (unsigned)(j["ep"] | 0), (unsigned)(j["cl"] | 0), (unsigned)(j["at"] | 0));
        Val &v = d.vals[k];
        v.json.clear();
        serializeJson(j["v"], v.json);
        v.ms = millis();
      }
    } else if (t == "drv") {
      std::string ieee = j["ieee"] | "";
      if (!ieee.empty() && (j["found"] | false)) {
        ZDev &d = get(ieee);
        d.zv = (const char *)(j["vendor"] | "");
        d.zm = (const char *)(j["model"] | "");
        d.desc = (const char *)(j["desc"] | "");
        d.n_exp = j["exp"].size();
        add_log("Treiber: " + d.zv + " " + d.zm + (d.desc.empty() ? "" : " – " + d.desc));
      } else if (ieee.empty()) {   // Antwort auf drv_lookup (Test)
        add_log((j["found"] | false) ? "Treiber gefunden: " + std::string(j["vendor"] | "") + " " +
                                           std::string(j["model"] | "") + " (" + std::to_string(j["exp"].size()) + " Werte)"
                                     : std::string("Kein Treiber gefunden"));
      }
    } else if (t == "open") {
      s_open = j["s"] | 0;
      s_open_ms = millis();
      add_log(s_open ? "Anlernen offen (" + std::to_string(s_open) + " s)" : std::string("Anlernen geschlossen"));
    } else if (t == "log" || t == "err") {
      add_log((t == "err" ? "Fehler: " : "") + std::string(j["m"] | ""));
    }
  }
  if (t == "log" || t == "err") log_i("Zigbee: %s", (const char *)(j["m"] | ""));
  // Regeln: nur neue Meldungen (nicht die Wiederholung nach einem Neustart), ausserhalb der Sperre
  if (t == "state" && !(j["old"] | false) && j["p"].is<JsonObject>()) {
    std::string p;
    serializeJson(j["p"], p);
    rules_zb_event(j["ieee"] | "", p);
  }
  if (ask_devices) {
    send_simple("devices");
    send_simple("values");   // letzte Werte, damit nach einem Neustart sofort alles dasteht
  }
  if (t == "hello" || ask_devices) send_time();
}

void rx_task(void *) {
  static char buf[ZB_LINK_MAX_LINE];
  size_t n = 0;
  uint8_t tmp[128];
  uint32_t last_info = 0;
  for (;;) {
    int got = uart_read_bytes(ZBD_UART, tmp, sizeof(tmp), pdMS_TO_TICKS(200));
    for (int i = 0; i < got; i++) {
      char c = (char)tmp[i];
      if (c == '\n' || c == '\r') {
        if (n) {
          buf[n] = 0;
          n = 0;
          on_line(buf);
        }
      } else if (n < sizeof(buf) - 1) {
        buf[n++] = c;
      }
    }
    // Lebenszeichen holen (Modul antwortet mit "info"); ohne Modul alle 30 s erneut fragen
    if (millis() - last_info > 30000) {
      last_info = millis();
      send_simple("info");
      static uint32_t last_time = 0;
      if (!last_time || millis() - last_time > 3600000UL) {
        last_time = millis();
        send_time();
      }
    }
  }
}

}  // namespace

bool zbd_enabled() { return s_on; }

void zbd_set_enabled(bool on) {
  Preferences p;
  if (p.begin("zbd", false)) {
    p.putBool("on", on);
    p.end();
  }
  s_on = on;
  if (on) zbd_begin();   // Ausschalten wirkt nach dem Neustart (Task/UART bleiben bis dahin)
}

void zbd_begin() {
  if (s_started) return;
  Preferences p;
  if (p.begin("zbd", true)) {
    s_on = p.getBool("on", true);
    p.end();
  }
  if (!s_on) return;
  if (!s_mx) s_mx = xSemaphoreCreateMutex();
  uart_config_t c = {};
  c.baud_rate = ZB_LINK_BAUD;
  c.data_bits = UART_DATA_8_BITS;
  c.parity = UART_PARITY_DISABLE;
  c.stop_bits = UART_STOP_BITS_1;
  c.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  c.source_clk = UART_SCLK_DEFAULT;
  if (uart_driver_install(ZBD_UART, ZB_LINK_MAX_LINE * 2, 0, 0, nullptr, 0) != ESP_OK ||
      uart_param_config(ZBD_UART, &c) != ESP_OK ||
      uart_set_pin(ZBD_UART, ZBD_TX, ZBD_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
    log_w("Zigbee: UART %d (TX %d, RX %d) laesst sich nicht einrichten", (int)ZBD_UART, ZBD_TX, ZBD_RX);
    return;
  }
  s_started = true;
  xTaskCreate(rx_task, "zbd", 4096, nullptr, 3, nullptr);
  send_simple("info");
  log_i("Zigbee direkt: UART %d, TX %d, RX %d", (int)ZBD_UART, ZBD_TX, ZBD_RX);
}

bool zbd_present() { return s_started && s_seen_ms && millis() - s_seen_ms < 70000; }

void zbd_permit(int seconds) {
  if (!s_started) return;
  write_line(std::string("{\"c\":\"permit\",\"s\":") + std::to_string(seconds < 0 ? 0 : seconds) + "}");
}

bool zbd_send_cmd(JsonObjectConst cmd) {
  if (!s_started || !cmd["c"].is<const char *>()) return false;
  std::string s;
  serializeJson(cmd, s);
  if (s.size() >= ZB_LINK_MAX_LINE) return false;
  write_line(s);
  return true;
}

void zbd_status_json(JsonObject o) {
  o["enabled"] = s_on;
  o["started"] = s_started;
  o["present"] = zbd_present();
  if (!s_started) return;
  Lock l;
  o["fw"] = s_fw;
  o["ready"] = s_ready;
  o["pan"] = s_pan;
  o["ch"] = s_ch;
  o["coord"] = s_coord;
  int left = s_open ? (int)s_open - (int)((millis() - s_open_ms) / 1000) : 0;
  o["open"] = left > 0 ? left : 0;
  o["age"] = s_seen_ms ? (int)((millis() - s_seen_ms) / 1000) : -1;
  JsonArray da = o["devices"].to<JsonArray>();
  for (auto &d : s_devs) {
    JsonObject x = da.add<JsonObject>();
    x["ieee"] = d.ieee;
    x["nwk"] = d.nwk;
    x["mf"] = d.mf;
    x["model"] = d.model;
    if (!d.name.empty()) x["name"] = d.name;
    if (!d.zm.empty()) {
      x["zv"] = d.zv;
      x["zm"] = d.zm;
      x["desc"] = d.desc;
      x["n_exp"] = d.n_exp;
    }
    x["done"] = d.done;
    x["age"] = d.seen_ms ? (int)((millis() - d.seen_ms) / 1000) : -1;
    if (!d.eps.empty()) x["eps"] = serialized(d.eps);
    JsonObject pr = x["props"].to<JsonObject>();
    for (auto &kv : d.props) pr[kv.first] = serialized(kv.second);
    JsonObject v = x["vals"].to<JsonObject>();
    for (auto &kv : d.vals) v[kv.first] = serialized(kv.second.json);
  }
  JsonArray lg = o["log"].to<JsonArray>();
  for (auto &m : s_log) lg.add(m);
}

ZbdInfo zbd_info() {
  ZbdInfo i;
  i.started = s_started;
  i.present = zbd_present();
  if (!s_started) return i;
  Lock l;
  i.ready = s_ready;
  i.fw = s_fw;
  i.coord = s_coord;
  i.pan = s_pan;
  i.ch = s_ch;
  int left = s_open ? (int)s_open - (int)((millis() - s_open_ms) / 1000) : 0;
  i.open = left > 0 ? left : 0;
  return i;
}

// Bekannte Standardwerte lesbar machen (Treiber folgen spaeter im Modul)
static std::string summary_of(const ZDev &d) {
  std::string s;
  auto add = [&](const std::string &t) { s += (s.empty() ? "" : " · ") + t; };
  // benannte Werte (wie Zigbee2MQTT): die wichtigsten zuerst, mit Einheit
  static const struct {
    const char *name, *unit;
    int dec;
  } KNOWN[] = {{"state", "", -1},       {"temperature", " °C", 1}, {"local_temperature", " °C", 1},
               {"humidity", " % rF", 0}, {"brightness", "", -2},     {"power", " W", 0},
               {"energy", " kWh", 2},    {"contact", "", -3},        {"occupancy", "", -4},
               {"presence", "", -4},     {"water_leak", "", -5},     {"illuminance", " lx", 0},
               {"position", " %", 0},    {"battery", " %", -6}};
  // Tastendruck: "4_single" -> "Taste 4 kurz", "on_2" -> "on 2"
  for (auto &p : d.props)
    if (p.first == "action") {
      std::string a = p.second.size() >= 2 && p.second.front() == '"' ? p.second.substr(1, p.second.size() - 2) : p.second;
      size_t us = a.find('_');
      if (us != std::string::npos && us > 0 && isdigit((unsigned char)a[0])) {
        std::string k = a.substr(us + 1);
        const char *kind = k == "single" ? tr("kurz") : k == "double" ? tr("doppelt") : k == "hold" ? tr("lang") : k.c_str();
        add(std::string(tr("Taste ")) + a.substr(0, us) + " " + kind);
      } else {
        add(a);
      }
    }
  for (auto &k : KNOWN)
    for (auto &p : d.props) {
      if (p.first != k.name) continue;
      const std::string &v = p.second;
      char b[40];
      if (k.dec == -1) add(v.find("ON") != std::string::npos ? tr("An") : tr("Aus"));
      else if (k.dec == -2) {
        snprintf(b, sizeof(b), "%d %%", (int)(atof(v.c_str()) * 100 / 254 + 0.5));
        add(b);
      } else if (k.dec == -3) add(v == "true" ? tr("zu") : tr("offen"));
      else if (k.dec == -4) add(v == "true" ? tr("Bewegung") : tr("ruhig"));
      else if (k.dec == -5) add(v == "true" ? tr("Wasser!") : tr("trocken"));
      else if (k.dec == -6) {
        snprintf(b, sizeof(b), "%s %d %%", tr("Batterie"), (int)(atof(v.c_str()) + 0.5));
        add(b);
      } else {
        snprintf(b, sizeof(b), "%.*f%s", k.dec, atof(v.c_str()), k.unit);
        add(b);
      }
    }
  if (!s.empty()) {
#if UI_DECIMAL_COMMA
    for (auto &c : s)
      if (c == '.') c = ',';
#endif
    return s;
  }
  auto num = [](const std::string &js, double &out) {
    char *e;
    out = strtod(js.c_str(), &e);
    return e != js.c_str();
  };
  for (auto &kv : d.vals) {
    unsigned ep, cl, at;
    if (sscanf(kv.first.c_str(), "%u/%x/%x", &ep, &cl, &at) != 3) continue;
    const std::string &v = kv.second.json;
    double x;
    char b[32];
    if (cl == 0x0006 && at == 0x0000) add(v == "true" || v == "1" ? tr("An") : tr("Aus"));
    else if (cl == 0x0008 && at == 0x0000 && num(v, x)) {
      snprintf(b, sizeof(b), "%d %%", (int)(x * 100 / 254 + 0.5));
      add(b);
    } else if (cl == 0x0402 && at == 0x0000 && num(v, x)) {
      snprintf(b, sizeof(b), "%.1f °C", x / 100);
      add(b);
    } else if (cl == 0x0405 && at == 0x0000 && num(v, x)) {
      snprintf(b, sizeof(b), "%.0f %% rF", x / 100);
      add(b);
    } else if (cl == 0x0001 && at == 0x0021 && num(v, x)) {
      snprintf(b, sizeof(b), "%s %d %%", tr("Batterie"), (int)(x / 2 + 0.5));
      add(b);
    }
  }
#if UI_DECIMAL_COMMA
  for (auto &c : s)
    if (c == '.') c = ',';
#endif
  if (s.empty() && !d.vals.empty()) s = std::to_string(d.vals.size()) + " " + tr("Werte");
  return s;
}

std::vector<ZbdDevice> zbd_devices() {
  std::vector<ZbdDevice> out;
  if (!s_started) return out;
  Lock l;
  for (auto &d : s_devs) {
    ZbdDevice x;
    x.ieee = d.ieee;
    x.mf = d.zv.empty() ? d.mf : d.zv;            // lieber die Namen aus der Treiberliste
    x.model = d.zm.empty() ? d.model : d.zm;
    x.desc = d.desc;
    x.name = d.name;
    x.done = d.done;
    x.age = d.seen_ms ? (int)((millis() - d.seen_ms) / 1000) : -1;
    x.summary = summary_of(d);
    out.push_back(x);
  }
  return out;
}

std::vector<std::string> zbd_log() {
  if (!s_started) return {};
  Lock l;
  return std::vector<std::string>(s_log.begin(), s_log.end());
}

uint32_t zbd_structure_sig() {
  if (!s_started) return 0;
  Lock l;
  uint32_t h = 2166136261u;
  auto mix = [&](uint32_t v) { h = (h ^ v) * 16777619u; };
  mix(zbd_present());
  mix(s_ready);
  mix(s_open > 0 && millis() - s_open_ms < s_open * 1000UL);
  mix(s_devs.size());
  for (auto &d : s_devs) {
    for (char c : d.ieee) mix((uint8_t)c);
    mix(d.done);
    mix(d.model.size());
  }
  return h;
}

void zbd_remove(const std::string &ieee) {
  if (!s_started) return;
  write_line("{\"c\":\"remove\",\"ieee\":\"" + ieee + "\"}");
}

void zbd_rename(const std::string &ieee, const std::string &name) {
  if (!s_started) return;
  {
    Lock l;
    if (ZDev *d = find(ieee)) d->name = name;   // sofort anzeigen, das Modul bestaetigt mit {"t":"dev"}
  }
  JsonDocument j;
  j["c"] = "rename";
  j["ieee"] = ieee;
  j["name"] = name;
  std::string s;
  serializeJson(j, s);
  write_line(s);
}

void zbd_reset_network() {
  if (!s_started) return;
  {
    Lock l;
    s_devs.clear();
    s_ready = false;
    add_log("Netz wird neu gebildet");
  }
  send_simple("reset_net");
}

void zbd_config_open(const std::string &ieee) {
  if (!s_started) return;
  {
    Lock l;
    s_cfg.clear();
    s_cfg_done = false;
    s_cfg_ieee = ieee;
  }
  write_line("{\"c\":\"config\",\"ieee\":\"" + ieee + "\"}");
}

bool zbd_config_get(std::vector<ZbdCfgItem> &out) {
  if (!s_started) return false;
  Lock l;
  out = s_cfg;
  return s_cfg_done;
}

void zbd_config_close() {
  if (!s_started) return;
  Lock l;
  s_cfg_ieee.clear();
  s_cfg.clear();
  s_cfg.shrink_to_fit();   // Speicher wirklich freigeben
  s_cfg_done = false;
}

bool zbd_set(const std::string &ieee, const std::string &json_values) {
  if (!s_started || json_values.size() + ieee.size() + 40 >= ZB_LINK_MAX_LINE) return false;
  write_line("{\"c\":\"set\",\"ieee\":\"" + ieee + "\",\"p\":" + json_values + "}");
  return true;
}

std::vector<std::pair<std::string, std::string>> zbd_values(const std::string &ieee) {
  if (!s_started) return {};
  Lock l;
  ZDev *d = find(ieee);
  return d ? d->props : std::vector<std::pair<std::string, std::string>>();
}

void zbd_debug(const char *args) {
  while (*args == ' ') args++;
  if (!strncmp(args, "permit", 6)) {
    const char *a = args + 6;
    while (*a == ' ') a++;
    int s = *a ? atoi(a) : 180;   // ohne Zahl 180 s, "0" schliesst
    zbd_permit(s);
    log_printf(s ? "Zigbee: Anlernen %d s angefordert\n" : "Zigbee: Anlernen schliessen\n", s);
    return;
  }
  if (!strncmp(args, "raw ", 4)) {
    write_line(args + 4);
    log_printf("Zigbee: gesendet %s\n", args + 4);
    return;
  }
  if (!strcmp(args, "list")) send_simple("devices");
  JsonDocument d;
  zbd_status_json(d.to<JsonObject>());
  String s;
  serializeJsonPretty(d, s);
  log_printf("%s\n", s.c_str());
}

#else   // ohne WITH_ZB_DIRECT: leere Funktionen

void zbd_begin() {}
bool zbd_enabled() { return false; }
void zbd_set_enabled(bool) {}
bool zbd_present() { return false; }
void zbd_permit(int) {}
bool zbd_send_cmd(JsonObjectConst) { return false; }
void zbd_status_json(JsonObject o) { o["enabled"] = false; }
void zbd_debug(const char *) {}
ZbdInfo zbd_info() { return ZbdInfo(); }
std::vector<ZbdDevice> zbd_devices() { return {}; }
std::vector<std::string> zbd_log() { return {}; }
uint32_t zbd_structure_sig() { return 0; }
void zbd_remove(const std::string &) {}
void zbd_rename(const std::string &, const std::string &) {}
void zbd_reset_network() {}
void zbd_config_open(const std::string &) {}
bool zbd_config_get(std::vector<ZbdCfgItem> &) { return false; }
void zbd_config_close() {}
bool zbd_set(const std::string &, const std::string &) { return false; }
std::vector<std::pair<std::string, std::string>> zbd_values(const std::string &) { return {}; }

#endif
