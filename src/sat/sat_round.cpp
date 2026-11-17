// Rundes Zusatzdisplay: ELECROW CrowPanel 1.46" (360x360, Drehknopf, LED-Ring) zeigt ausgewaehlte Kacheln des
// Hauptpanels – eine Kachel pro Bildschirm, wie das runde Hauptpanel, nur abgespeckt (keine Datenseiten, kein Farbrad).
// Nur mit -DSAT_ROUND=1 (Umgebung crowpanel-146-remote); ersetzt dort sat_display.cpp (gleiche Schnittstelle).
//
// Bedienung: Drehknopf blaettert; kurz druecken schaltet; lang druecken (Lampe) -> Dimmen, dann regelt der Knopf die
// Helligkeit; lang druecken (MultiPlus/Ladegeraet ueber den GX des Hauptpanels) -> Bedienfenster. Touch: wischen blaettert, 0,4 s halten schaltet, 1,5 s halten dimmt (im Dimm-Fenster am Ring ziehen).
// BOOT-Taste 3 s halten: Einrichtungsmodus (sat_pair, GPIO 0). Hardware: src/board_crowpanel146.cpp.
#include "sat_display.h"
#if SAT_ROUND
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <lvgl.h>
#include <time.h>
#include <string>
#include <vector>
#include "esp_heap_caps.h"
#include "../board.h"
#include "../fonts/fonts.h"
#include "../fonts/icons.h"
#include "rd_client.h"
#include "sat_config.h"

namespace {
const int W = 360, H = 360, RING = 8;
const uint32_t BG = 0x0F1317, TILE = 0x1A2027, TEXT = 0xE8ECEF, TEXT2 = 0x9AA5B1, TEXT3 = 0x5A6470, AMBER = 0xF3A43B,
               INK = 0x1A1205, BLUE = 0x3C8DFF;
const uint16_t HOLD_MS = 400, DIM_HOLD_MS = 1500, MOVE_PX = 35;

struct Cfg {
  std::string k, n;
  bool on = true;
};
std::vector<Cfg> s_cfg;
std::vector<RdTile> s_list;   // angezeigte Kacheln (Reihenfolge/Namen aus s_cfg, ausgeblendete fehlen)
int s_idx = 0;
uint8_t s_bright = 180;       // 20..255 wie beim CYD (Weboberflaeche), umgerechnet in Prozent
uint16_t s_dim_min = 2;
uint8_t s_saver = 1;          // nach der Dimm-Zeit: 0 abdunkeln, 1 Uhr
uint8_t s_ring_bri = 30;      // LED-Ring in %

lv_obj_t *s_scr, *s_bgc, *s_arc, *s_icon, *s_name, *s_value, *s_sub, *s_pos;
lv_obj_t *s_dp = nullptr, *s_dp_arc = nullptr, *s_dp_pct = nullptr;   // Dimm-Fenster
lv_obj_t *s_clk = nullptr, *s_clk_t = nullptr, *s_clk_d = nullptr;     // Uhr (Bildschirmschoner)
std::string s_dp_key;
int s_dp_bri = 0;
uint32_t s_dp_sent = 0, s_dp_ms = 0;
uint32_t s_press_ms = 0;
lv_point_t s_press_pt;
bool s_long = false, s_dimmed = false;
uint32_t s_seen = 0, s_list_ms = 0;
uint32_t s_ring_sig = 0xFFFFFFFF;

uint8_t pct_of(uint8_t b) { return (uint8_t)std::max(3, (int)b * 100 / 255); }

// ---------------------------------------------------------------- Einstellungen (NVS "satdisp", wie beim CYD)
void load_cfg() {
  Preferences p;
  p.begin("satdisp", true);
  String js = p.getString("cfg", "");
  p.end();
  JsonDocument d;
  if (js.isEmpty() || deserializeJson(d, js)) return;
  s_bright = constrain((int)(d["b"] | 180), 20, 255);
  s_dim_min = d["d"] | 2;
  s_saver = d["sv"] | 1;
  s_ring_bri = constrain((int)(d["rb"] | 30), 0, 100);
  s_cfg.clear();
  for (JsonObject t : d["t"].as<JsonArray>()) s_cfg.push_back({t["k"] | "", t["n"] | "", (bool)(t["o"] | 1)});
}

void save_cfg() {
  JsonDocument d;
  d["b"] = s_bright;
  d["d"] = s_dim_min;
  d["sv"] = s_saver;
  d["rb"] = s_ring_bri;
  JsonArray a = d["t"].to<JsonArray>();
  for (auto &c : s_cfg) {
    JsonObject t = a.add<JsonObject>();
    t["k"] = c.k;
    if (!c.n.empty()) t["n"] = c.n;
    t["o"] = c.on ? 1 : 0;
  }
  String js;
  serializeJson(d, js);
  Preferences p;
  p.begin("satdisp", false);
  p.putString("cfg", js);
  p.end();
}

// Kacheln vom Hauptpanel in eingestellter Reihenfolge (neue hinten), ausgeblendete weg, eigene Namen
void build_list() {
  std::vector<RdTile> raw = rd_tiles();
  std::vector<bool> used(raw.size(), false);
  std::string cur = s_idx < (int)s_list.size() ? s_list[s_idx].key : std::string();
  s_list.clear();
  for (auto &c : s_cfg)
    for (size_t i = 0; i < raw.size(); i++)
      if (!used[i] && "r:" + raw[i].key == c.k) {
        used[i] = true;
        if (c.on) {
          if (!c.n.empty()) raw[i].name = c.n;
          s_list.push_back(raw[i]);
        }
        break;
      }
  for (size_t i = 0; i < raw.size(); i++)
    if (!used[i]) s_list.push_back(raw[i]);
  for (size_t i = 0; i < s_list.size(); i++)   // auf derselben Kachel bleiben
    if (s_list[i].key == cur) s_idx = i;
  if (s_idx >= (int)s_list.size()) s_idx = 0;
}

// ---------------------------------------------------------------- LED-Ring
uint32_t scale(uint32_t rgb, int pct) {
  auto c = [&](int sh) { return (uint32_t)(((rgb >> sh) & 0xFF) * pct / 100) << sh; };
  return c(16) | c(8) | c(0);
}
void ring_update() {
  uint32_t px[RING] = {0};
  int bri = s_dimmed ? 0 : s_ring_bri;
  if (s_dp) {   // Dimmen: Helligkeit als Leuchtpunkte
    int n = (s_dp_bri * RING + 50) / 100;
    for (int i = 0; i < n; i++) px[i] = scale(AMBER, bri);
  } else if (s_idx < (int)s_list.size()) {
    const RdTile &t = s_list[s_idx];
    if (t.lit) {
      for (auto &p : px) p = scale(t.lit_color ? t.lit_color : AMBER, bri);
    } else if (t.fill >= 0) {
      int n = (t.fill * RING + 50) / 100;
      for (int i = 0; i < n && i < RING; i++) px[i] = scale(t.accent, bri);
    }
  }
  uint32_t sig = 7;
  for (auto p : px) sig = sig * 31 + p;
  if (sig == s_ring_sig) return;
  s_ring_sig = sig;
  board_ring_show(px, RING);
}

// ---------------------------------------------------------------- Anzeige
lv_obj_t *label(lv_obj_t *p, const lv_font_t *f, uint32_t col, int y, int w) {
  lv_obj_t *l = lv_label_create(p);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(l, w);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
  return l;
}

void set_text(lv_obj_t *l, const std::string &s) {
  if (strcmp(lv_label_get_text(l), s.c_str())) lv_label_set_text(l, s.c_str());
}

void render() {
  if (s_list.empty()) {
    lv_obj_set_style_bg_color(s_bgc, lv_color_hex(TILE), 0);
    lv_obj_add_flag(s_arc, LV_OBJ_FLAG_HIDDEN);
    set_text(s_icon, ICON_INFO);
    lv_obj_set_style_text_color(s_icon, lv_color_hex(TEXT3), 0);
    set_text(s_name, rd_panel_seen() ? "Keine Kacheln" : "Suche Hauptpanel");
    set_text(s_value, "");
    std::string ip = WiFi.isConnected() ? std::string(WiFi.localIP().toString().c_str())
                                        : (WiFi.getMode() & WIFI_AP) ? std::string("WLAN ") + WiFi.softAPSSID().c_str() : "";
    set_text(s_sub, rd_panel_seen() ? "im Editor des Hauptpanels wählen" : ip);
    set_text(s_pos, "");
    ring_update();
    return;
  }
  const RdTile &t = s_list[s_idx];
  bool lit = t.act && t.lit;
  uint32_t lc = t.lit_color ? t.lit_color : AMBER;
  lv_color_t bg = lit ? lv_color_hex(lc) : lv_color_hex(TILE);
  bool dark_text = lit && lv_color_brightness(bg) > 150;
  uint32_t fg = lit ? (dark_text ? INK : 0xFFFFFF) : TEXT, fg2 = lit ? fg : TEXT2;
  lv_obj_set_style_bg_color(s_bgc, bg, 0);
  set_text(s_icon, t.icon);
  lv_obj_set_style_text_color(s_icon, lit ? lv_color_hex(fg) : (t.dim ? lv_color_hex(TEXT3) : lv_color_hex(t.accent)), 0);
  set_text(s_name, t.name);
  lv_obj_set_style_text_color(s_name, lv_color_hex(fg2), 0);
  set_text(s_value, t.value);
  lv_obj_set_style_text_color(s_value, lv_color_hex(t.dim ? TEXT3 : fg), 0);
  set_text(s_sub, t.sub);
  lv_obj_set_style_text_color(s_sub, lv_color_hex(fg2), 0);
  char b[16];
  snprintf(b, sizeof(b), "%d / %d", s_idx + 1, (int)s_list.size());
  set_text(s_pos, s_list.size() > 1 ? b : "");
  if (t.fill >= 0 && !lit) {
    lv_obj_clear_flag(s_arc, LV_OBJ_FLAG_HIDDEN);
    lv_arc_set_value(s_arc, t.fill);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(t.accent), LV_PART_INDICATOR);
  } else {
    lv_obj_add_flag(s_arc, LV_OBJ_FLAG_HIDDEN);
  }
  ring_update();
}

void go(int d) {
  if (s_list.empty()) return;
  int n = s_list.size();
  s_idx = ((s_idx + d) % n + n) % n;
  render();
}

void toggle() {
  if (s_idx >= (int)s_list.size() || !s_list[s_idx].act) return;
  if (rd_action(s_list[s_idx].key)) {   // springt gleich um (rd_client), Bestaetigung folgt
    build_list();
    render();
  }
}

// ---------------------------------------------------------------- Dimm-Fenster
void dp_send(bool fin) {
  uint32_t now = millis();
  if (fin || now - s_dp_sent > 350) {
    s_dp_sent = now;
    rd_set(s_dp_key, 'b', s_dp_bri);
  }
}
void dp_show_pct() {
  char b[16];
  snprintf(b, sizeof(b), s_dp_bri ? "%d %%" : "Aus", s_dp_bri);
  lv_label_set_text(s_dp_pct, b);
  ring_update();
}
void dp_close() {
  if (!s_dp) return;
  dp_send(true);
  lv_obj_del(s_dp);
  s_dp = s_dp_arc = s_dp_pct = nullptr;
  ring_update();
}
void dp_arc_cb(lv_event_t *e) {
  s_dp_bri = lv_arc_get_value(s_dp_arc);
  s_dp_ms = millis();
  dp_show_pct();
  dp_send(lv_event_get_code(e) == LV_EVENT_RELEASED);
}
void dp_open() {
  if (s_dp || s_idx >= (int)s_list.size()) return;
  const RdTile &t = s_list[s_idx];
  if (t.bri < 0) return;
  s_dp_key = t.key;
  s_dp_bri = t.bri;
  s_dp_ms = millis();
  s_dp = lv_obj_create(s_scr);
  lv_obj_remove_style_all(s_dp);
  lv_obj_set_size(s_dp, W, H);
  lv_obj_set_style_radius(s_dp, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(s_dp, lv_color_hex(BG), 0);
  lv_obj_set_style_bg_opa(s_dp, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s_dp, LV_OBJ_FLAG_SCROLLABLE);
  s_dp_arc = lv_arc_create(s_dp);
  lv_obj_set_size(s_dp_arc, W - 24, H - 24);
  lv_obj_center(s_dp_arc);
  lv_arc_set_rotation(s_dp_arc, 135);
  lv_arc_set_bg_angles(s_dp_arc, 0, 270);
  lv_arc_set_range(s_dp_arc, 0, 100);
  lv_arc_set_value(s_dp_arc, s_dp_bri);
  lv_obj_set_style_arc_width(s_dp_arc, 22, LV_PART_MAIN);
  lv_obj_set_style_arc_width(s_dp_arc, 22, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(s_dp_arc, lv_color_hex(0x2A323B), LV_PART_MAIN);
  lv_obj_set_style_arc_color(s_dp_arc, lv_color_hex(AMBER), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(s_dp_arc, lv_color_white(), LV_PART_KNOB);
  lv_obj_set_style_pad_all(s_dp_arc, 6, LV_PART_KNOB);
  lv_obj_add_event_cb(s_dp_arc, dp_arc_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(s_dp_arc, dp_arc_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_t *nm = label(s_dp, &font_ui_20, TEXT2, 110, 220);
  lv_label_set_text(nm, t.name.c_str());
  s_dp_pct = label(s_dp, &font_ui_40, TEXT, 150, 220);
  lv_obj_t *h = label(s_dp, &font_ui_16, TEXT3, 220, 220);
  lv_label_set_text(h, "Knopf drehen · drücken = fertig");
  lv_obj_t *ok = lv_btn_create(s_dp);
  lv_obj_set_size(ok, 90, 40);
  lv_obj_align(ok, LV_ALIGN_TOP_MID, 0, 260);
  lv_obj_set_style_bg_color(ok, lv_color_hex(TILE), 0);
  lv_obj_add_event_cb(ok, [](lv_event_t *) { dp_close(); }, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ol = lv_label_create(ok);
  lv_label_set_text(ol, "Fertig");
  lv_obj_set_style_text_font(ol, &font_ui_16, 0);
  lv_obj_center(ol);
  dp_show_pct();
}

// ---------------------------------------------------------------- Bedienfenster MultiPlus/Ladegeraet
// Ueber den GX des Hauptpanels (dort cerbo_set_*): Netzstrom-Begrenzung per Drehknopf (ganze Ampere, gesendet 0,8 s
// nach dem letzten Dreh), Schalter per Antippen. Knopf druecken oder "Fertig" schliesst; nach 20 s von selbst.
lv_obj_t *s_cp = nullptr, *s_cp_lim = nullptr, *s_cp_btn[4];
int s_cp_val[4], s_cp_n = 0, s_cp_lim10 = -1;
std::string s_cp_key;
uint32_t s_cp_send = 0, s_cp_edit = 0;

bool has_ctl(const RdTile &t) { return t.vset || t.vlim >= 0; }

void cp_show_lim() {
  if (!s_cp_lim) return;
  char b[16];
  snprintf(b, sizeof(b), "%d,%d A", s_cp_lim10 / 10, s_cp_lim10 % 10);
  lv_label_set_text(s_cp_lim, b);
}
void cp_show_mode(int mode) {
  for (int i = 0; i < s_cp_n; i++) {
    bool sel = s_cp_val[i] == mode;
    lv_obj_set_style_bg_color(s_cp_btn[i], lv_color_hex(sel ? BLUE : TILE), 0);
  }
}
void cp_flush() {
  if (!s_cp_send) return;
  s_cp_send = 0;
  rd_set(s_cp_key, 'l', s_cp_lim10);
}
void cp_close() {
  if (!s_cp) return;
  cp_flush();
  lv_obj_del_async(s_cp);   // kann aus einem Ereignis des Fensters kommen
  s_cp = s_cp_lim = nullptr;
  s_cp_n = 0;
}
void cp_mode_cb(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i < 0 || i >= s_cp_n) return;
  s_cp_edit = millis();
  if (rd_set(s_cp_key, 'm', s_cp_val[i])) cp_show_mode(s_cp_val[i]);
}
void cp_knob(int k) {
  if (!s_cp_lim || s_cp_lim10 < 0) return;
  int a = (s_cp_lim10 + 5) / 10 + k;   // ganze Ampere
  s_cp_lim10 = constrain(a, 0, 100) * 10;
  cp_show_lim();
  s_cp_edit = millis();
  s_cp_send = (millis() + 800) | 1;
}
lv_obj_t *cp_btn(const char *txt, int x, int y, int w, int h, lv_event_cb_t cb, intptr_t ud) {
  lv_obj_t *b = lv_obj_create(s_cp);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, w, h);
  lv_obj_align(b, LV_ALIGN_TOP_MID, x, y);
  lv_obj_set_style_radius(b, 14, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(TILE), 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(0x2A323B), 0);
  lv_obj_set_style_border_color(b, lv_color_hex(AMBER), LV_STATE_PRESSED);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)ud);
  lv_obj_t *l = lv_label_create(b);
  lv_obj_set_style_text_font(l, &font_ui_16, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(TEXT), 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, w - 8);
  lv_label_set_text(l, txt);
  lv_obj_center(l);
  return b;
}
void cp_open() {
  if (s_cp || s_dp || s_idx >= (int)s_list.size()) return;
  const RdTile &t = s_list[s_idx];
  if (!has_ctl(t)) return;
  s_cp_key = t.key;
  s_cp_lim10 = t.vlim;
  s_cp_send = s_cp_edit = 0;
  s_cp = lv_obj_create(s_scr);
  lv_obj_remove_style_all(s_cp);
  lv_obj_set_size(s_cp, W, H);
  lv_obj_set_style_radius(s_cp, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(s_cp, lv_color_hex(BG), 0);
  lv_obj_set_style_bg_opa(s_cp, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s_cp, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_cp, LV_OBJ_FLAG_CLICKABLE);   // Kachel darunter nicht bedienen
  lv_obj_t *nm = label(s_cp, &font_ui_20, TEXT2, 34, 220);
  lv_label_set_text(nm, t.name.c_str());
  int y = 72;
  if (t.vlim >= 0) {
    lv_label_set_text(label(s_cp, &font_ui_16, TEXT3, y, 260), "Netzstrom-Begrenzung");
    s_cp_lim = label(s_cp, &font_ui_40, TEXT, y + 20, 220);
    cp_show_lim();
    lv_label_set_text(label(s_cp, &font_ui_16, TEXT3, y + 70, 260), "Knopf drehen");
    y += 100;
  } else {
    y += 20;
  }
  if (t.vset) {
    static const char *MP_TXT[4] = {"Ein", "Aus", "Nur Laden", "Nur Wechselrichter"};
    static const int MP_VAL[4] = {3, 4, 1, 2};
    static const char *CH_TXT[2] = {"Ein", "Aus"};
    static const int CH_VAL[2] = {1, 4};
    bool mp = t.vset == 1;
    s_cp_n = mp ? 4 : 2;
    for (int i = 0; i < s_cp_n; i++) {
      s_cp_val[i] = mp ? MP_VAL[i] : CH_VAL[i];
      s_cp_btn[i] = cp_btn(mp ? MP_TXT[i] : CH_TXT[i], (i % 2) ? 68 : -68, y + (i / 2) * 52, 128, 46, cp_mode_cb, i);
    }
    cp_show_mode(t.vmode);
    y += (s_cp_n / 2) * 52;
  }
  cp_btn("Fertig", 0, std::max(y + 6, 292), 96, 36, [](lv_event_t *) { cp_close(); }, 0);
}
// neue Werte vom Hauptpanel: uebernehmen, ausser direkt nach eigener Eingabe
void cp_sync() {
  if (!s_cp || s_cp_send || (s_cp_edit && millis() - s_cp_edit < 3000)) return;
  for (auto &t : s_list)
    if (t.key == s_cp_key) {
      if (t.vlim >= 0) {
        s_cp_lim10 = t.vlim;
        cp_show_lim();
      }
      cp_show_mode(t.vmode);
    }
}

// ---------------------------------------------------------------- Uhr (Bildschirmschoner)
void clock_text() {
  static const char *DAYS[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  time_t t = time(nullptr);
  struct tm tm;
  localtime_r(&t, &tm);
  char b[24];
  snprintf(b, sizeof(b), "%02d:%02d", tm.tm_hour, tm.tm_min);
  lv_label_set_text(s_clk_t, b);
  snprintf(b, sizeof(b), "%s, %02d.%02d.", DAYS[tm.tm_wday % 7], tm.tm_mday, tm.tm_mon + 1);
  lv_label_set_text(s_clk_d, b);
}
void clock_open() {
  if (s_clk) return;
  s_clk = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(s_clk);
  lv_obj_set_size(s_clk, W, H);
  lv_obj_set_style_bg_color(s_clk, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(s_clk, LV_OPA_COVER, 0);
  s_clk_t = lv_label_create(s_clk);
  lv_obj_set_style_text_font(s_clk_t, &font_clock_104, 0);
  lv_obj_set_style_text_color(s_clk_t, lv_color_hex(0x8D97A2), 0);
  lv_obj_align(s_clk_t, LV_ALIGN_CENTER, 0, -16);
  s_clk_d = lv_label_create(s_clk);
  lv_obj_set_style_text_font(s_clk_d, &font_ui_20, 0);
  lv_obj_set_style_text_color(s_clk_d, lv_color_hex(0x5A6470), 0);
  lv_obj_align(s_clk_d, LV_ALIGN_CENTER, 0, 62);
  clock_text();
}
void clock_close() {
  if (!s_clk) return;
  lv_obj_del(s_clk);
  s_clk = s_clk_t = s_clk_d = nullptr;
}

// ---------------------------------------------------------------- Touch
void scr_cb(lv_event_t *e) {
  lv_event_code_t c = lv_event_get_code(e);
  lv_indev_t *in = lv_indev_get_act();
  if (c == LV_EVENT_PRESSED) {
    s_press_ms = millis();
    if (in) lv_indev_get_point(in, &s_press_pt);
    s_long = false;
  } else if (c == LV_EVENT_PRESSING) {
    if (!s_long && !s_dp && !s_cp && millis() - s_press_ms >= DIM_HOLD_MS && s_idx < (int)s_list.size()) {
      const RdTile &t = s_list[s_idx];
      if (t.bri >= 0) {
        s_long = true;
        dp_open();
      } else if (has_ctl(t)) {   // MultiPlus/Ladegeraet: Bedienfenster
        s_long = true;
        cp_open();
      }
    }
  } else if (c == LV_EVENT_RELEASED) {
    lv_point_t p = s_press_pt;
    if (in) lv_indev_get_point(in, &p);
    uint32_t held = millis() - s_press_ms;
    if (!s_long && !s_dp && !s_cp && held >= HOLD_MS && abs(p.x - s_press_pt.x) <= MOVE_PX && abs(p.y - s_press_pt.y) <= MOVE_PX) toggle();
  } else if (c == LV_EVENT_GESTURE && !s_dp && !s_cp && in) {
    lv_dir_t d = lv_indev_get_gesture_dir(in);
    if (d == LV_DIR_LEFT) go(1);
    else if (d == LV_DIR_RIGHT) go(-1);
  }
}

void build() {
  s_scr = lv_scr_act();
  lv_obj_set_style_bg_color(s_scr, lv_color_black(), 0);
  lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
  for (auto ev : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_GESTURE}) lv_obj_add_event_cb(s_scr, scr_cb, ev, nullptr);
  s_bgc = lv_obj_create(s_scr);
  lv_obj_remove_style_all(s_bgc);
  lv_obj_set_size(s_bgc, W, H);
  lv_obj_set_style_radius(s_bgc, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(s_bgc, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s_bgc, LV_OBJ_FLAG_CLICKABLE);
  s_arc = lv_arc_create(s_scr);   // Pegel (Batterie, Gas, Tank) am Rand
  lv_obj_set_size(s_arc, W - 12, H - 12);
  lv_obj_center(s_arc);
  lv_arc_set_rotation(s_arc, 135);
  lv_arc_set_bg_angles(s_arc, 0, 270);
  lv_arc_set_range(s_arc, 0, 100);
  lv_obj_set_style_arc_width(s_arc, 10, LV_PART_MAIN);
  lv_obj_set_style_arc_width(s_arc, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x2A323B), LV_PART_MAIN);
  lv_obj_remove_style(s_arc, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
  s_icon = label(s_scr, &font_icons_44, TEXT2, 52, 80);
  s_name = label(s_scr, &font_ui_20, TEXT2, 112, 260);
  s_value = label(s_scr, &font_ui_40, TEXT, 146, 280);
  s_sub = label(s_scr, &font_ui_16, TEXT2, 206, 250);
  s_pos = label(s_scr, &font_ui_16, TEXT3, 300, 120);
}
}  // namespace

// ---------------------------------------------------------------- Schnittstelle (wie sat_display.h)
bool sat_display_present() { return true; }
bool sat_display_reset_requested() { return false; }
bool sat_display_night() { return false; }
void sat_display_web_touch() {}
void sat_display_light(JsonObject o) {
  o["ldr"] = -1;
  o["night"] = false;
  o["auto"] = false;
}

void sat_display_begin() {
  load_cfg();
  if (!board_init()) log_e("Display nicht bereit");
  board_set_backlight(pct_of(s_bright));
  build();
  build_list();
  render();
}

void sat_display_loop() {
  uint32_t now = millis();
  lv_timer_handler();
  // Drehknopf: blaettern bzw. dimmen; kurz druecken schaltet, lang druecken dimmt
  int k = board_knob_take();
  int btn = board_knob_button();
  if (k || btn) {
    lv_disp_trig_activity(nullptr);
    if (board_is_dimmed()) {   // erst aufwecken
      board_wake();
      k = btn = 0;
    }
  }
  if (k) {
    if (s_cp) {
      cp_knob(k > 0 ? 1 : -1);
    } else if (s_dp) {
      s_dp_bri = constrain(s_dp_bri + k * 5, 0, 100);
      lv_arc_set_value(s_dp_arc, s_dp_bri);
      s_dp_ms = now;
      dp_show_pct();
      dp_send(false);
    } else {
      go(k > 0 ? 1 : -1);
    }
  }
  if (btn == 1) {
    if (s_cp) cp_close();
    else if (s_dp) dp_close();
    else toggle();
  } else if (btn == 2 && !s_dp && !s_cp) {
    if (s_idx < (int)s_list.size() && s_list[s_idx].bri >= 0) dp_open();
    else cp_open();
  }
  if (s_dp && now - s_dp_ms > 8000 && now - s_press_ms > 8000) dp_close();   // Dimm-Fenster schliesst von selbst
  if (s_cp_send && (int32_t)(now - s_cp_send) >= 0) cp_flush();
  if (s_cp && lv_disp_get_inactive_time(nullptr) > 20000) cp_close();   // Bedienfenster schliesst von selbst
  // Neue Daten vom Hauptpanel (gebuendelt)
  if (rd_version() != s_seen && now - s_list_ms >= 200) {
    s_seen = rd_version();
    s_list_ms = now;
    build_list();
    if (!s_dp) render();
    cp_sync();
  }
  // Dimmen bzw. Uhr nach der eingestellten Zeit ohne Bedienung
  uint32_t idle = lv_disp_get_inactive_time(nullptr);
  if (!s_dimmed && s_dim_min && idle > s_dim_min * 60000UL) {
    s_dimmed = true;
    dp_close();
    cp_close();
    if (s_saver && time(nullptr) > 1600000000) {
      clock_open();
      board_dim(std::max(8, pct_of(s_bright) / 3));
    } else {
      board_dim(4);
    }
    ring_update();
  } else if (s_dimmed && !board_is_dimmed()) {   // Touch/Knopf hat geweckt
    s_dimmed = false;
    clock_close();
    render();
  }
  if (s_clk) {
    static uint32_t last = 0;
    if (now - last >= 1000) {
      last = now;
      clock_text();
    }
  }
}

void sat_display_json(JsonObject o) {
  o["round"] = true;   // Weboberflaeche: eine Kachel pro Seite, kein Lichtsensor
  o["per"] = 1;
  o["pl"].to<JsonArray>();
  o["bright"] = s_bright;
  o["dim"] = s_dim_min;
  o["auto"] = false;
  o["night"] = 40;
  o["ldr_thr"] = 300;
  o["clock"] = 1;
  o["saver"] = s_saver;
  JsonArray a = o["tiles"].to<JsonArray>();
  std::vector<RdTile> raw = rd_tiles();
  std::vector<bool> used(raw.size(), false);
  auto add = [&](const RdTile &r, const Cfg *c) {
    JsonObject t = a.add<JsonObject>();
    t["k"] = "r:" + r.key;
    t["name"] = r.name;
    t["value"] = r.sub.empty() ? r.value : r.value + "  ·  " + r.sub;
    t["n"] = c ? c->n : "";
    t["on"] = c ? c->on : true;
  };
  for (auto &c : s_cfg)
    for (size_t i = 0; i < raw.size(); i++)
      if (!used[i] && "r:" + raw[i].key == c.k) {
        used[i] = true;
        add(raw[i], &c);
        break;
      }
  for (size_t i = 0; i < raw.size(); i++)
    if (!used[i]) add(raw[i], nullptr);
}

bool sat_display_apply(JsonObjectConst c) {
  if (c["bright"].is<int>()) s_bright = constrain(c["bright"].as<int>(), 20, 255);
  if (c["dim"].is<int>()) s_dim_min = constrain(c["dim"].as<int>(), 0, 600);
  if (c["saver"].is<int>()) s_saver = c["saver"].as<int>() ? 1 : 0;
  if (c["tiles"].is<JsonArrayConst>()) {
    s_cfg.clear();
    for (JsonObjectConst t : c["tiles"].as<JsonArrayConst>()) {
      const char *k = t["k"] | "";
      if (*k) s_cfg.push_back({k, t["n"] | "", t["on"] | true});
    }
  }
  save_cfg();
  board_set_backlight(pct_of(s_bright));
  build_list();
  render();
  return true;
}
#endif
