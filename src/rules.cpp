// Regel-Engine des Panels – Beschreibung und Format siehe rules.h
#include "rules.h"

#if WITH_RULES

#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>

#include <deque>
#include <map>
#include <vector>

#include "app.h"
#include "esp_heap_caps.h"
#include "net.h"
#include "zb_direct.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ui/ui_internal.h"
#include "audio.h"

namespace {

const char *PATH = "/rules.json";
const size_t MAX_SIZE = 64 * 1024;

struct PsramAlloc : ArduinoJson::Allocator {
  void *allocate(size_t n) override {
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : malloc(n);
  }
  void deallocate(void *p) override { heap_caps_free(p); }
  void *reallocate(void *p, size_t n) override {
    void *q = heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return q ? q : realloc(p, n);
  }
};
PsramAlloc s_ps;

SemaphoreHandle_t s_mx = nullptr;
JsonDocument s_doc(&s_ps);   // {"rules":[…],"ws":{…}} im PSRAM
struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mx); }
};

// Zustand je Regel (nicht gespeichert)
struct State {
  uint32_t last_ms = 0;   // zuletzt ausgeloest
  size_t pc = 0;          // naechste Aktion (laeuft, wenn running)
  bool running = false;
  uint32_t resume_ms = 0; // bei "warten"
  uint32_t every_ms = 0;  // Zeitgeber "alle n s"
  std::string err;
};
std::vector<State> s_st;
std::map<std::string, std::string> s_tile_sig;   // Kachel -> letzter Zustand (fuer Kachel-Ausloeser)
int s_last_minute = -1;
bool s_started = false;
uint32_t s_boot_ms = 0;

// Zigbee-Meldungen aus dem Empfangs-Task: hier sammeln, in rules_loop auswerten
struct ZbEv {
  std::string ieee, json;
};
std::deque<ZbEv> s_zb_q;
// Eigene kleine Sperre nur fuer die Warteschlange: der Zigbee-Empfang darf nie auf die Regel-Engine warten (die
// beim Schalten ueber MQTT haengen kann) – sonst kommen Zigbee-Meldungen verspaetet an
SemaphoreHandle_t s_qmx = nullptr;

// Letzte Ereignisse fuer den seriellen Befehl "rules" (Fehlersuche)
std::deque<std::string> s_trace;
void trace(const std::string &t) {
  char b[16];
  snprintf(b, sizeof(b), "%7.1f s ", millis() / 1000.0);
  s_trace.push_back(b + t);
  while (s_trace.size() > 16) s_trace.pop_front();
}

// Dimmen per Taster: zuletzt gesetzte Helligkeit merken (die Lampe meldet sie erst etwas spaeter zurueck)
struct DimLast {
  int pct;
  uint32_t ms;
};
std::map<std::string, DimLast> s_dim_last;

void set_enabled_locked(int i, bool on);

// Kacheln fuer die Auswahl im Editor: der Webserver-Task darf ui_tile_model nicht aufrufen -> hier zwischenspeichern
std::vector<std::pair<std::string, std::string>> s_tiles;   // Schluessel, Name
uint32_t s_tiles_ms = 0;

JsonArrayConst rules_c() { return s_doc["rules"].as<JsonArrayConst>(); }

void load() {
  Lock l;
  s_doc.clear();
  File f = LittleFS.open(PATH, "r");
  if (f) {
    // Blockly-Arbeitsbereich: jede Aktion einer Kette ist eine Ebene tiefer ("next":{"block":…}) – Vorgabe 10 reicht nicht
    if (deserializeJson(s_doc, f, DeserializationOption::NestingLimit(RULES_NESTING))) {
      log_w("Regeln: %s ist beschaedigt", PATH);
      s_doc.clear();
    }
    f.close();
  }
  if (!s_doc["rules"].is<JsonArray>()) s_doc["rules"].to<JsonArray>();
  s_st.assign(rules_c().size(), State());
  s_tile_sig.clear();
}

bool save() {
  File f = LittleFS.open("/rules.tmp", "w");
  if (!f) return false;
  serializeJson(s_doc, f);
  f.close();
  LittleFS.remove(PATH);
  return LittleFS.rename("/rules.tmp", PATH);
}

// ---------------------------------------------------------------- Werte
struct Val {
  bool ok = false, num = false;
  double n = 0;
  std::string s;
};
Val from_json_text(const std::string &js) {   // gespeicherter Zigbee-Wert (JSON) -> Val
  Val v;
  JsonDocument d;
  if (deserializeJson(d, js)) return v;
  v.ok = true;
  if (d.is<bool>()) {
    v.num = true;
    v.n = d.as<bool>();
    v.s = d.as<bool>() ? "true" : "false";
  } else if (d.is<double>()) {
    v.num = true;
    v.n = d.as<double>();
    v.s = js;
  } else {
    v.s = d.as<std::string>();
  }
  return v;
}
Val tile_val(const std::string &key, const std::string &part) {
  Val v;
  TileModel m;
  if (!ui_tile_model(key, m)) return v;
  v.ok = true;
  if (part == "lit") {
    v.num = true;
    v.n = m.lit;
    v.s = m.lit ? "true" : "false";
  } else if (part == "fill") {
    v.num = m.fill >= 0;
    v.n = m.fill;
    v.s = std::to_string(m.fill);
  } else {   // angezeigter Wert: fuehrende Zahl, sonst Text
    v.s = m.value;
    std::string t = m.value;
    for (auto &c : t)
      if (c == ',') c = '.';
    char *e;
    double x = strtod(t.c_str(), &e);
    if (e != t.c_str()) {
      v.num = true;
      v.n = x;
    }
  }
  return v;
}
Val value(JsonVariantConst w) {
  Val v;
  if (w["n"].is<double>()) {
    v.ok = v.num = true;
    v.n = w["n"].as<double>();
    v.s = std::to_string(v.n);
  } else if (w["b"].is<bool>()) {
    v.ok = v.num = true;
    v.n = w["b"].as<bool>();
    v.s = w["b"].as<bool>() ? "true" : "false";
  } else if (w["s"].is<const char *>()) {
    v.ok = true;
    v.s = w["s"].as<const char *>();
    char *e;
    double x = strtod(v.s.c_str(), &e);
    if (e != v.s.c_str() && !*e) {
      v.num = true;
      v.n = x;
    }
  } else if (w["zb"].is<const char *>()) {
    std::string p = w["p"] | "";
    for (auto &kv : zbd_values(w["zb"].as<const char *>()))
      if (kv.first == p) return from_json_text(kv.second);
  } else if (w["tile"].is<const char *>()) {
    return tile_val(w["tile"].as<const char *>(), w["p"] | "lit");
  }
  return v;
}
// "ON"/"true" = 1, "OFF"/"false" = 0 – damit Ein/Aus-Werte mit Wahr/Falsch vergleichbar sind
bool as_bool_num(const Val &v, double &out) {
  if (v.num) {
    out = v.n;
    return true;
  }
  std::string s = v.s;
  for (auto &c : s) c = tolower((unsigned char)c);
  if (s == "on" || s == "true" || s == "open") { out = 1; return true; }
  if (s == "off" || s == "false" || s == "closed") { out = 0; return true; }
  return false;
}
bool compare(const Val &a, const std::string &c, const Val &b) {
  if (!a.ok || !b.ok) return false;
  double x, y;
  if (as_bool_num(a, x) && as_bool_num(b, y)) {
    if (c == "=") return fabs(x - y) < 1e-9;
    if (c == "!=") return fabs(x - y) >= 1e-9;
    if (c == "<") return x < y;
    if (c == ">") return x > y;
    if (c == "<=") return x <= y;
    if (c == ">=") return x >= y;
    return false;
  }
  int r = strcasecmp(a.s.c_str(), b.s.c_str());
  if (c == "=") return r == 0;
  if (c == "!=") return r != 0;
  if (c == "<") return r < 0;
  if (c == ">") return r > 0;
  if (c == "<=") return r <= 0;
  if (c == ">=") return r >= 0;
  return false;
}

int minutes_of(const char *hhmm) {
  int h = 0, m = 0;
  if (!hhmm || sscanf(hhmm, "%d:%d", &h, &m) < 1) return -1;
  return h * 60 + m;
}
int now_minutes() {
  if (!net_time_valid()) return -1;
  time_t t = time(nullptr);
  struct tm lt;
  localtime_r(&t, &lt);
  return lt.tm_hour * 60 + lt.tm_min;
}

bool eval(JsonVariantConst e) {
  if (e.isNull()) return true;
  std::string op = e["op"] | "";
  if (op == "and") {
    for (JsonVariantConst x : e["a"].as<JsonArrayConst>())
      if (!eval(x)) return false;
    return true;
  }
  if (op == "or") {
    for (JsonVariantConst x : e["a"].as<JsonArrayConst>())
      if (eval(x)) return true;
    return false;
  }
  if (op == "not") return !eval(e["a"][0]);
  if (op == "cmp") return compare(value(e["l"]), e["c"] | "=", value(e["r"]));
  if (op == "time") {   // Zeitfenster, auch ueber Mitternacht (20:00 – 06:00)
    int now = now_minutes(), a = minutes_of(e["from"] | ""), b = minutes_of(e["to"] | "");
    if (now < 0 || a < 0 || b < 0) return false;
    return a <= b ? (now >= a && now < b) : (now >= a || now < b);
  }
  return false;
}

// ---------------------------------------------------------------- Ausfuehren
std::string json_value(JsonVariantConst v) {
  std::string s;
  serializeJson(v, s);
  return s;
}
// Aktionen ab s.pc ausfuehren, bis zum Ende oder bis "warten"
void run_steps(int i) {
  JsonVariantConst r = rules_c()[i];
  State &s = s_st[i];
  JsonArrayConst acts = r["act"].as<JsonArrayConst>();
  while (s.running && s.pc < acts.size()) {
    JsonVariantConst a = acts[s.pc++];
    std::string k = a["k"] | "";
    if (k == "wait") {
      s.resume_ms = millis() + (uint32_t)((a["s"] | 0.0) * 1000);
      return;
    }
    if (k == "zb") {
      std::string body = "{" + json_value(a["p"]) + ":" + json_value(a["v"]) + "}";
      if (!zbd_set(a["dev"] | "", body)) s.err = "Zigbee-Modul nicht verbunden";
    } else if (k == "tile") {
      std::string key = a["key"] | "", what = a["do"] | "toggle";
      TileModel m;
      if (!ui_tile_model(key, m)) {
        s.err = "Gerät fehlt: " + key;
        continue;
      }
      std::string err;
      // Ein/Aus als fester Befehl: Melder schicken oft mehrere Meldungen kurz hintereinander, der Zustand der Lampe
      // kommt erst danach zurueck – mit Umschalten ging das Licht sonst an und gleich wieder aus.
      bool ok = what == "toggle" ? ui_tile_action(key, &err) : ui_tile_switch(key, what == "on", &err);
      if (!ok) s.err = err.empty() ? "Gerät lässt sich nicht schalten" : err;
    } else if (k == "bri") {
      ui_tile_set(a["key"] | "", 'b', a["v"] | 100);
    } else if (k == "dimstep") {   // je Ausloesung um "step" % dunkler, unter "min" % wieder 100 %
      std::string key = a["key"] | "";
      int step = a["step"] | 20, mn = a["min"] | 10;
      if (step < 1) step = 1;
      TileModel m;
      if (!ui_tile_model(key, m)) {
        s.err = "Gerät fehlt: " + key;
        continue;
      }
      int cur = m.lit ? m.bri : -1;
      auto it = s_dim_last.find(key);
      if (it != s_dim_last.end() && millis() - it->second.ms < 4000) cur = it->second.pct;
      int nb = cur <= 0 ? 100 : cur - step;
      if (nb < mn || nb < 1) nb = 100;
      if (!ui_tile_set(key, 'b', nb)) s.err = "Gerät ist nicht dimmbar";
      s_dim_last[key] = {nb, millis()};
    } else if (k == "rule") {   // andere Regel (per Name) ein-, aus- oder umschalten
      std::string name = a["name"] | "", what = a["do"] | "toggle";
      int idx = -1;
      for (size_t j = 0; j < rules_c().size(); j++)
        if (name == (const char *)(rules_c()[j]["name"] | "")) {
          idx = (int)j;
          break;
        }
      if (idx < 0) {
        s.err = "Regel fehlt: " + name;
        continue;
      }
      bool on = what == "on" ? true : what == "off" ? false : !(rules_c()[idx]["on"] | true);
      set_enabled_locked(idx, on);
      app_toast(on ? "Regel „%s“ an" : "Regel „%s“ aus", name.c_str());
    } else if (k == "msg") {
      app_toast("%s", (const char *)(a["text"] | ""));
#if WITH_AUDIO
    } else if (k == "sound") {   // Hinweiston bzw. Ansage (unterbricht Musik kurz)
      std::string f = a["file"] | "";
      if (!f.empty()) audio_sound(f);
      else audio_tone((AudioTone)constrain((int)(a["tone"] | 0), 0, 2));
    } else if (k == "music") {
      std::string d = a["do"] | "";
      if (!a["vol"].isNull()) audio_set_volume(a["vol"] | 40);
      if (d == "play") {
        AudioState st = audio_state();
        if (!st.playing) st.paused ? audio_toggle() : audio_play(-1);
      } else if (d == "pause") {
        if (audio_state().playing) audio_toggle();
      } else if (d == "toggle") audio_toggle();
      else if (d == "stop") audio_stop();
      else if (d == "next") audio_next();
      else if (d == "prev") audio_prev();
#endif
    }
  }
  s.running = false;
  if (!s.err.empty()) trace("  Fehler: " + s.err);
}
void fire(int i) {
  JsonVariantConst r = rules_c()[i];
  std::string nm = r["name"] | "";
  if (!(r["on"] | true)) {
    trace("  Regel \"" + nm + "\": ausgeschaltet");
    return;
  }
  if (!eval(r["cond"])) {
    trace("  Regel \"" + nm + "\": Bedingung nicht erfuellt");
    return;
  }
  trace("  Regel \"" + nm + "\" ausgeloest");
  State &s = s_st[i];
  s.last_ms = millis();
  s.err.clear();
  s.pc = 0;
  s.running = true;   // laeuft die Regel schon (wartet), beginnt sie von vorn
  s.resume_ms = 0;
  run_steps(i);
}

// Zigbee-Ereignis gegen alle Ausloeser pruefen
void on_zb(const ZbEv &ev) {
  trace("Zigbee " + ev.ieee + " " + ev.json);
  JsonDocument d(&s_ps);
  if (deserializeJson(d, ev.json)) return;
  JsonArrayConst rs = rules_c();
  for (size_t i = 0; i < rs.size(); i++)
    for (JsonVariantConst t : rs[i]["trig"].as<JsonArrayConst>()) {
      if (strcmp(t["k"] | "", "zb") || ev.ieee != (const char *)(t["dev"] | "")) continue;
      const char *p = t["p"] | "";
      if (d[p].isNull()) continue;
      if (!t["eq"].isNull()) {   // nur bei diesem Wert ("eq":"1_single", Zahl oder Wert-Objekt)
        Val a = from_json_text(json_value(d[p]));
        Val b = t["eq"].is<JsonObjectConst>() ? value(t["eq"]) : from_json_text(json_value(t["eq"]));
        if (!compare(a, "=", b)) {
          trace(std::string("  Regel \"") + (const char *)(rs[i]["name"] | "") + "\": Wert passt nicht (" + a.s + " / " + b.s + ")");
          continue;
        }
      }
      fire(i);
      break;
    }
}

std::string tile_sig(const std::string &key) {
  TileModel m;
  if (!ui_tile_model(key, m)) return "";
  return std::to_string(m.lit) + "|" + std::to_string(m.fill) + "|" + m.value;
}

}  // namespace

void rules_begin() {
  if (!s_mx) s_mx = xSemaphoreCreateRecursiveMutex();
  if (!s_qmx) s_qmx = xSemaphoreCreateMutex();
  s_boot_ms = millis();
  load();
  log_i("Regeln: %u geladen", (unsigned)rules_c().size());
}

void rules_zb_event(const std::string &ieee, const std::string &json_values) {
  if (!s_qmx) return;
  if (xSemaphoreTake(s_qmx, pdMS_TO_TICKS(20)) != pdTRUE) return;   // lieber eine Meldung verlieren als warten
  if (s_zb_q.size() < 32) s_zb_q.push_back({ieee, json_values});
  xSemaphoreGive(s_qmx);
}

void rules_loop() {
  if (!s_mx) return;
  Lock l;
  JsonArrayConst rs = rules_c();
  if (s_st.size() != rs.size()) s_st.assign(rs.size(), State());
  for (;;) {   // Meldungen einzeln aus der Warteschlange holen (deren Sperre nur kurz halten)
    ZbEv ev;
    xSemaphoreTake(s_qmx, portMAX_DELAY);
    bool any = !s_zb_q.empty();
    if (any) {
      ev = std::move(s_zb_q.front());
      s_zb_q.pop_front();
    }
    xSemaphoreGive(s_qmx);
    if (!any) break;
    on_zb(ev);
  }
  uint32_t now = millis();
  // wartende Regeln fortsetzen
  for (size_t i = 0; i < rs.size(); i++)
    if (s_st[i].running && s_st[i].resume_ms && (int32_t)(now - s_st[i].resume_ms) >= 0) {
      s_st[i].resume_ms = 0;
      run_steps(i);
    }
  // Start, Zeitgeber, Uhrzeit, Kacheln: hoechstens einmal pro Sekunde pruefen
  static uint32_t tick = 0;
  if (now - tick < 1000) return;
  tick = now;
  if (!s_tiles_ms || now - s_tiles_ms > 10000) {
    s_tiles_ms = now;
    s_tiles.clear();
    for (auto &k : ui_tile_keys()) {
      TileModel m;
      if (ui_tile_model(k, m)) s_tiles.push_back({k, m.name});
    }
  }
  bool start_now = !s_started && now - s_boot_ms > 10000;   // nach dem Start: Kacheln und Werte sind dann da
  int mins = now_minutes();
  bool new_minute = mins >= 0 && mins != s_last_minute;
  if (new_minute) s_last_minute = mins;
  for (size_t i = 0; i < rs.size(); i++) {
    for (JsonVariantConst t : rs[i]["trig"].as<JsonArrayConst>()) {
      std::string k = t["k"] | "";
      bool hit = false;
      if (k == "start") hit = start_now;
      else if (k == "every") {
        uint32_t ms = (uint32_t)((t["s"] | 60.0) * 1000);
        if (ms < 1000) ms = 1000;
        if (!s_st[i].every_ms) s_st[i].every_ms = now;
        if (now - s_st[i].every_ms >= ms) {
          s_st[i].every_ms = now;
          hit = true;
        }
      } else if (k == "time") {
        hit = new_minute && minutes_of(t["at"] | "") == mins;
      } else if (k == "tile") {
        std::string key = t["key"] | "", sig = tile_sig(key);
        auto it = s_tile_sig.find(key);
        if (it == s_tile_sig.end()) s_tile_sig[key] = sig;   // erster Blick: merken, nicht ausloesen
        else if (it->second != sig) {
          it->second = sig;
          hit = !sig.empty();
        }
      }
      if (hit) {
        fire(i);
        break;
      }
    }
  }
  if (start_now) s_started = true;
}

void rules_json(JsonObject out) {
  Lock l;
  out["enabled"] = true;
  out["rules"] = s_doc["rules"];
  if (!s_doc["ws"].isNull()) out["ws"] = s_doc["ws"];
  JsonArray st = out["state"].to<JsonArray>();
  for (size_t i = 0; i < s_st.size(); i++) {
    JsonObject x = st.add<JsonObject>();
    x["last"] = s_st[i].last_ms ? (int)((millis() - s_st[i].last_ms) / 1000) : -1;
    x["running"] = s_st[i].running;
    if (!s_st[i].err.empty()) x["err"] = s_st[i].err;
  }
  // Auswahl fuer den Editor: Zigbee-Geraete (mit bekannten Werten) und Kacheln des Panels
  JsonArray zb = out["zb"].to<JsonArray>();
  for (auto &d : zbd_devices()) {
    JsonObject x = zb.add<JsonObject>();
    x["ieee"] = d.ieee;
    x["name"] = d.name.empty() ? (d.model.empty() ? d.ieee : d.mf + " " + d.model) : d.name;
    JsonArray p = x["p"].to<JsonArray>();
    for (auto &kv : zbd_values(d.ieee)) p.add(kv.first);
  }
  JsonArray tl = out["tiles"].to<JsonArray>();
  for (auto &t : s_tiles) {
    JsonObject x = tl.add<JsonObject>();
    x["key"] = t.first;
    x["name"] = t.second;
  }
}

std::string rules_save(const char *body, size_t len) {
  if (len > MAX_SIZE) return "Regeln zu gross (max. 64 KB)";
  JsonDocument d(&s_ps);
  if (deserializeJson(d, body, len, DeserializationOption::NestingLimit(RULES_NESTING))) return "Ungültiges JSON";
  if (!d["rules"].is<JsonArray>()) return "Regeln fehlen";
  Lock l;
  s_doc.clear();
  s_doc["rules"] = d["rules"];
  if (!d["ws"].isNull()) s_doc["ws"] = d["ws"];
  if (!save()) return "Speichern fehlgeschlagen (Dateisystem)";
  s_st.assign(rules_c().size(), State());
  s_tile_sig.clear();
  return "";
}

int rules_count() {
  if (!s_mx) return 0;
  Lock l;
  return (int)rules_c().size();
}
std::string rules_name(int i) {
  Lock l;
  return (const char *)(rules_c()[i]["name"] | "");
}
bool rules_enabled(int i) {
  Lock l;
  return rules_c()[i]["on"] | true;
}
namespace {
void set_enabled_locked(int i, bool on) {
  if (i < 0 || i >= (int)rules_c().size()) return;
  s_doc["rules"][i]["on"] = on;
  if (!on && i < (int)s_st.size()) s_st[i].running = false;   // wartende Aktionen verwerfen
  save();
}
}  // namespace
void rules_set_enabled(int i, bool on) {
  Lock l;
  set_enabled_locked(i, on);
}
std::string rules_status(int i) {
  Lock l;
  if (i < 0 || i >= (int)s_st.size()) return "";
  const State &s = s_st[i];
  if (!s.err.empty()) return "Fehler: " + s.err;
  if (s.running && s.resume_ms) {
    int left = (int)((int32_t)(s.resume_ms - millis()) / 1000);
    char b[32];
    snprintf(b, sizeof(b), "wartet noch %d:%02d", left / 60, left % 60);
    return b;
  }
  if (!s.last_ms) return "noch nicht ausgelöst";
  int ago = (millis() - s.last_ms) / 1000;
  char b[40];
  if (ago < 60) snprintf(b, sizeof(b), "zuletzt vor %d s", ago);
  else if (ago < 3600) snprintf(b, sizeof(b), "zuletzt vor %d min", ago / 60);
  else snprintf(b, sizeof(b), "zuletzt vor %d h", ago / 3600);
  return b;
}
void rules_debug() {
  Lock l;
  log_printf("Regeln: %u\n", (unsigned)rules_c().size());
  for (size_t i = 0; i < rules_c().size(); i++) {
    std::string js;
    serializeJson(rules_c()[i], js);
    log_printf("  [%u] %s\n      %s\n", (unsigned)i, rules_status((int)i).c_str(), js.c_str());
  }
  log_printf("Letzte Ereignisse:\n");
  for (auto &t : s_trace) log_printf("  %s\n", t.c_str());
}
void rules_run(int i) {
  Lock l;
  if (i < 0 || i >= (int)rules_c().size()) return;
  if (s_st.size() != rules_c().size()) s_st.assign(rules_c().size(), State());
  State &s = s_st[i];
  s.last_ms = millis();
  s.err.clear();
  s.pc = 0;
  s.running = true;
  s.resume_ms = 0;
  run_steps(i);
}

#else   // ohne WITH_RULES

void rules_begin() {}
void rules_loop() {}
void rules_zb_event(const std::string &, const std::string &) {}
void rules_json(JsonObject out) { out["enabled"] = false; }
std::string rules_save(const char *, size_t) { return "Regeln sind in dieser Firmware nicht eingebaut"; }
int rules_count() { return 0; }
std::string rules_name(int) { return ""; }
bool rules_enabled(int) { return false; }
void rules_set_enabled(int, bool) {}
std::string rules_status(int) { return ""; }
void rules_run(int) {}
void rules_debug() {}

#endif
