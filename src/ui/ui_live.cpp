// Live-Ansicht (Browser/App unter /live): Schaltbefehle aus dem Webserver-Task sicher in den UI-Task bringen.
// Die Ansicht selbst ist der Editor im Bedien-Modus (web/editor.src.html, LIVE); Werte kommen ueber /api/devices.
#include "ui.h"
#include "ui_internal.h"
#include <Arduino.h>
#include <algorithm>
#include <deque>
#include <ArduinoJson.h>

namespace {
struct Cmd {
  std::string key;
  char a;
  int v;
};
std::deque<Cmd> s_q;
SemaphoreHandle_t s_mx = nullptr;
portMUX_TYPE s_init_mux = portMUX_INITIALIZER_UNLOCKED;
// Stand fuer GET /api/live (unter s_mx)
std::string s_snap;
uint32_t s_snap_ms = 0, s_snap_ver = 0;
volatile uint32_t s_want_ms = 0;   // letzte Abfrage
std::string s_detail;              // offenes Detail-Fenster (unter s_mx)
uint32_t s_detail_ms = 0;

void ensure_mx() {
  if (!s_mx) {
    portENTER_CRITICAL(&s_init_mux);
    if (!s_mx) s_mx = xSemaphoreCreateMutex();
    portEXIT_CRITICAL(&s_init_mux);
  }
}

// Stand aller Kacheln bauen (UI-Task)
void build_snapshot() {
  JsonDocument d;
  d["v"] = ++s_snap_ver;
  JsonObject t = d["t"].to<JsonObject>();
  for (const std::string &k : ui_tile_keys()) {
    if (t[k].is<JsonObject>()) continue;   // gleiche Kachel auf mehreren Seiten
    TileModel m;
    if (!ui_tile_model(k, m)) continue;
    JsonObject o = t[k].to<JsonObject>();
    o["n"] = m.name;
    o["v"] = m.value;
    if (!m.sub.empty()) o["s"] = m.sub;
    o["a"] = m.accent;
    if (m.fill >= 0) o["f"] = m.fill;
    if (m.lit) {
      o["l"] = 1;
      o["lc"] = m.lit_color;
    }
    if (m.dim) o["d"] = 1;
    if (m.act) o["x"] = m.act;
    if (m.bri >= 0) o["b"] = m.bri;
    if (m.vset) {
      o["vs"] = m.vset;
      o["vm"] = m.vmode;
    }
    if (m.vlim >= 0) o["vl"] = m.vlim;
  }
  xSemaphoreTake(s_mx, portMAX_DELAY);
  std::string dk = millis() - s_detail_ms < 10000 ? s_detail : std::string();
  xSemaphoreGive(s_mx);
  if (!dk.empty()) {
    std::string nm;
    auto rows = ui_tile_rows(dk, &nm);
    d["dk"] = dk;
    if (!nm.empty()) d["dn"] = nm;
    JsonArray r = d["r"].to<JsonArray>();
    for (auto &row : rows) {
      JsonArray x = r.add<JsonArray>();
      x.add(row.first);
      x.add(row.second);
    }
  }
  std::string js;
  serializeJson(d, js);
  xSemaphoreTake(s_mx, portMAX_DELAY);
  s_snap.swap(js);
  xSemaphoreGive(s_mx);
}
}  // namespace

std::string ui_live_snapshot(const std::string &detail) {
  ensure_mx();
  s_want_ms = millis() | 1;
  xSemaphoreTake(s_mx, portMAX_DELAY);
  if (!detail.empty()) {
    bool fresh = detail != s_detail;
    s_detail = detail;
    s_detail_ms = millis();
    if (fresh) s_snap_ms = 0;   // neues Fenster: gleich neu bauen
  }
  std::string r = s_snap;
  xSemaphoreGive(s_mx);
  return r;
}

bool ui_live_queue(const std::string &key, char a, int value) {
  if (key.empty() || !strchr("tbhcml", a)) return false;
  ensure_mx();
  xSemaphoreTake(s_mx, portMAX_DELAY);
  bool ok = s_q.size() < 16;
  if (ok) s_q.push_back({key, a, value});
  xSemaphoreGive(s_mx);
  return ok;
}

// im UI-Task (ui_loop): wartende Befehle ausfuehren – nur fuer Kacheln, die wirklich auf dem Panel liegen
void ui_live_tick() {
  if (!s_mx) return;
  // Stand fuer die Live-Ansicht: hoechstens alle 0,7 s, nur solange jemand abfragt; danach Speicher frei
  uint32_t now = millis();
  if (s_want_ms) {
    if (now - s_want_ms < 15000) {
      if (now - s_snap_ms >= 700) {
        s_snap_ms = now;
        build_snapshot();
      }
    } else {
      s_want_ms = 0;
      xSemaphoreTake(s_mx, portMAX_DELAY);
      std::string().swap(s_snap);
      xSemaphoreGive(s_mx);
    }
  }
  std::deque<Cmd> q;
  xSemaphoreTake(s_mx, portMAX_DELAY);
  q.swap(s_q);
  xSemaphoreGive(s_mx);
  if (q.empty()) return;
  std::vector<std::string> keys = ui_tile_keys();
  for (auto &c : q) {
    if (std::find(keys.begin(), keys.end(), c.key) == keys.end()) continue;
    std::string err;
    bool ok = c.a == 't' ? ui_tile_action(c.key, &err) : ui_tile_set(c.key, c.a, c.v);
    if (!ok) log_w("Live: %s %c fehlgeschlagen %s", c.key.c_str(), c.a, err.c_str());
  }
}
