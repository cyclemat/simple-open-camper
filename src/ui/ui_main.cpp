// Hauptbildschirm: Statusleiste, Seiten, Kacheln, Einstiegshilfen
#include "ui.h"
#include "../i18n/i18n.h"
#include "ui_internal.h"
#include "theme.h"
#include "color.h"
#include "../app.h"
#include "../app_config.h"
#include "../layout.h"
#include "../net.h"
#include "../settings.h"
#include "../z2m.h"
#include "../truma.h"
#include "../victron.h"
#include "../panelsat.h"
#include "../power.h"

#include <Arduino.h>
#include <time.h>
#include <math.h>
#include <algorithm>
#include <vector>

struct TileUi {
  std::string key;
  std::string icon_id;
  lv_obj_t *obj = nullptr, *icon = nullptr, *name = nullptr, *sub = nullptr, *track = nullptr, *fill = nullptr;
  int w = 0, h = 0;
  bool small = false, big = false;
  bool gas = false, vic = false, heat = false, sio = false;   // sio: Anschluss eines Satelliten
  lv_obj_t *pct = nullptr;  // grosser Wert: Gasflaschen und Victron (Pegel nutzt "fill")
  uint32_t sig = 0;
};

static lv_obj_t *s_scr, *s_bg, *s_bar, *s_tabs, *s_clock, *s_ic_wifi, *s_ic_mqtt, *s_content;
static lv_obj_t *s_ic_batt, *s_batt;   // Akkuanzeige in der Leiste
static lv_obj_t *s_wifi_banner, *s_wifi_banner_txt;  // Hinweis: Heim-WLAN nicht erreichbar
static std::vector<TileUi> s_tiles;
static Layout s_lay;
static int s_page = 0;
static uint32_t s_seen_sat = 0;
static uint32_t s_seen_layout = 0, s_seen_devs = 0, s_seen_state = 0, s_seen_truma = 0, s_seen_vic = 0, s_seen_mop = 0, s_seen_heat = 0;
static uint32_t s_last_gesture = 0;
static int s_onboard_mode = -1;

lv_obj_t *ui_main_screen() { return s_scr; }

// ---------------------------------------------------------------- Kacheln
static const char *auto_icon(const ControlView *v) {
  if (!v) return "bulb";
  return v->kind == CTL_SWITCH ? "plug" : "bulb";
}

static uint32_t state_sig(bool found, const ControlView &v) {
  uint32_t h = found ? 1 : 0;
  auto mix = [&h](int x) { h = h * 31 + (uint32_t)x; };
  mix(v.on); mix(v.online); mix(v.pending); mix(v.known);
  mix(v.bri_pct()); mix(v.ct / 8); mix((int)(v.hue / 4)); mix((int)(v.sat / 4));
  mix((int)(v.cx * 200)); mix((int)(v.cy * 200)); mix(v.cmode.size() ? v.cmode[0] : 0);
  return h;
}

static void tile_paint(TileUi &t, bool force) {
  ControlView v;
  bool found = z2m_get(t.key, v);
  uint32_t sig = state_sig(found, v);
  if (!force && sig == t.sig) return;
  t.sig = sig;

  std::string label;
  if (found) label = layout_label(s_lay, v);
  else {
    auto it = s_lay.names.find(t.key);
    label = it != s_lay.names.end() ? it->second : "Unbekanntes Gerät";
  }
  (lv_label_set_text)(t.name, label.c_str());   // Geraetename: nicht uebersetzen
  lv_label_set_text(t.icon, icon_utf8(t.icon_id.empty() ? auto_icon(found ? &v : nullptr) : t.icon_id.c_str()));

  lv_obj_t *o = t.obj;
  bool on = found && v.online && v.known && v.on;
  lv_color_t fg = C_TEXT, fg2 = C_TEXT2, ic = lv_color_hex(0x9AA5B1);
  if (on) {
    lv_color_t c = light_display_color(v);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_grad_color(o, lv_color_darken(c, 52), 0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(o, lv_color_lighten(c, 60), 0);
    lv_obj_set_style_border_opa(o, LV_OPA_50, 0);
    bool light_bg = lv_color_brightness(c) > 150;
    fg = light_bg ? C_INK : lv_color_white();
    fg2 = light_bg ? lv_color_hex(0x4A3214) : lv_color_hex(0xDCE6F2);
    ic = fg;
  } else {
    lv_obj_set_style_bg_color(o, C_TILE, 0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_border_color(o, C_TILE_LINE, 0);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, 0);
  }
  lv_obj_set_style_text_color(t.name, fg, 0);
  lv_obj_set_style_text_color(t.sub, fg2, 0);
  lv_obj_set_style_text_color(t.icon, ic, 0);
  lv_obj_set_style_opa(o, (!found || !v.online) ? LV_OPA_50 : LV_OPA_COVER, 0);

  // Ausstehender Befehl: feiner Rahmen
  lv_obj_set_style_outline_width(o, (found && v.pending) ? 2 : 0, 0);

  char sub[48];
  if (!found) snprintf(sub, sizeof(sub), "%s", z2m_count() ? "nicht in Zigbee2MQTT" : "…");
  else if (!v.online) snprintf(sub, sizeof(sub), "nicht erreichbar");
  else if (!v.known) snprintf(sub, sizeof(sub), "…");
  else if (!v.on) snprintf(sub, sizeof(sub), "Aus");
  else if (v.has_bri && v.bri >= 0) {
    if (t.big && v.has_ct && v.ct > 0 && v.cmode == "color_temp")
      snprintf(sub, sizeof(sub), "%d\xE2\x80\xAF%%  ·  %d\xE2\x80\xAFK", v.bri_pct(), (int)(1000000 / v.ct / 50 * 50));
    else
      snprintf(sub, sizeof(sub), "%d\xE2\x80\xAF%%", v.bri_pct());
  } else snprintf(sub, sizeof(sub), "An");
  lv_label_set_text(t.sub, sub);

  if (t.track) {
    bool show = on && v.has_bri && v.bri >= 0;
    if (show) {
      lv_obj_clear_flag(t.track, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(t.track, fg, 0);
      lv_obj_set_style_bg_color(t.fill, fg, 0);
      int tw = t.w - 28;
      lv_obj_set_width(t.fill, LV_MAX(4, tw * v.bri_pct() / 100));
    } else {
      lv_obj_add_flag(t.track, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

// ---------------------------------------------------------------- Gasflaschen (Truma LevelControl, Mopeka)
static void gas_paint(TileUi &t, bool force) {
  GasView g;
  bool found = ui_gas_get(t.key, g);
  std::string label = found ? ui_gas_label(s_lay, g) : (s_lay.names.count(t.key) ? s_lay.names[t.key] : tr("Gasflasche"));
  uint32_t sig = found ? 7 : 3;
  auto mix = [&sig](int x) { sig = sig * 31 + (uint32_t)x; };
  mix(g.has); mix(g.percent); mix(g.age_s < 0 ? -1 : g.age_s / 60); mix(g.busy); mix(g.water);
  for (char c : g.amount) mix(c);
  for (char c : g.status) mix(c);
  for (char c : label) mix(c);
  if (!force && sig == t.sig) return;
  t.sig = sig;
  (lv_label_set_text)(t.name, label.c_str());   // Geraetename: nicht uebersetzen
  lv_obj_set_style_opa(t.obj, found ? LV_OPA_COVER : LV_OPA_50, 0);
  char b[64];
  if (!found) {
    lv_label_set_text(t.pct, "–");
    lv_label_set_text(t.sub, "Sensor nicht eingerichtet");
    lv_obj_set_height(t.fill, 0);
    lv_obj_set_style_text_color(t.icon, C_TEXT3, 0);
    return;
  }
  lv_label_set_text(t.icon, g.water ? ICON_BATH : ICON_GAS);
  if (!g.has) {
    lv_label_set_text(t.pct, "…");
    lv_label_set_text(t.sub, g.status.c_str());
    lv_obj_set_height(t.fill, 0);
    lv_obj_set_style_text_color(t.icon, lv_color_hex(0x9AA5B1), 0);
    return;
  }
  lv_color_t col = ui_gas_color(g.percent);
  snprintf(b, sizeof(b), "%d\xE2\x80\xAF%%", g.percent);
  lv_label_set_text(t.pct, b);
  lv_obj_set_style_text_color(t.pct, g.percent < 15 ? C_RED : C_TEXT, 0);
  lv_obj_set_style_text_color(t.icon, col, 0);
  lv_obj_set_style_bg_color(t.fill, col, 0);
  lv_obj_set_height(t.fill, t.h * g.percent / 100);
  std::string sub = g.amount;
  // Mopeka sendet laufend – dort nur bei Problemen den Zustand statt "gerade eben" zeigen
  std::string when = g.busy ? std::string(tr("misst …")) : g.mopeka ? (g.status == "OK" ? std::string() : std::string(tr(g.status))) : ui_age_text(g.age_s);
  if (!t.small && !when.empty()) sub += (sub.empty() ? "" : "  ·  ") + when;
  lv_label_set_text(t.sub, sub.c_str());
}

// ---------------------------------------------------------------- Victron (Instant Readout)
static void vic_paint(TileUi &t, bool force) {
  VictronInfo v;
  bool found = ui_vic_get(t.key, v);
  std::string label = found ? ui_vic_label(s_lay, v) : (s_lay.names.count(t.key) ? s_lay.names[t.key] : tr("Victron"));
  VicTileText tt;
  if (found) tt = ui_vic_tile_text(v, t.small, t.big);
  else {
    tt.value = "–";
    tt.sub = "nicht eingerichtet";
    tt.accent = C_TEXT3;
  }
  uint32_t sig = found ? 11 : 5;
  auto mix = [&sig](int x) { sig = sig * 31 + (uint32_t)x; };
  for (char c : tt.value) mix(c);
  for (char c : tt.sub) mix(c);
  for (char c : label) mix(c);
  mix(tt.fill); mix(tt.stale); mix(v.kind); mix(lv_color_to32(tt.accent));
  if (!force && sig == t.sig) return;
  t.sig = sig;
  (lv_label_set_text)(t.name, label.c_str());   // Geraetename: nicht uebersetzen
  lv_label_set_text(t.icon, ui_vic_icon(found ? v.kind : VIC_UNKNOWN));
  lv_label_set_text(t.pct, tt.value.c_str());
  lv_label_set_text(t.sub, tt.sub.c_str());
  lv_obj_set_style_text_color(t.icon, tt.accent, 0);
  lv_obj_set_style_text_color(t.pct, (tt.fill >= 0 && tt.fill < 15) ? C_RED : C_TEXT, 0);
  lv_obj_set_style_opa(t.obj, (!found || tt.stale) ? LV_OPA_50 : LV_OPA_COVER, 0);
  if (tt.fill >= 0) {
    lv_obj_set_style_bg_color(t.fill, tt.accent, 0);
    lv_obj_set_height(t.fill, t.h * tt.fill / 100);
  } else {
    lv_obj_set_height(t.fill, 0);
  }
}

// ---------------------------------------------------------------- Truma-Heizung
static void heat_paint(TileUi &t, bool force) {
  TrumaHeatInfo h;
  bool found = ui_heat_get(t.key, h);
  std::string label = found ? ui_heat_label(s_lay, h) : (s_lay.names.count(t.key) ? s_lay.names[t.key] : tr("Heizung"));
  std::string value = found && h.has ? ui_temp_text(h.room_actual) : "–";
  std::string sub = found ? ui_heat_sub(h, t.small) : "in der Firmware ausgeschaltet";
  uint32_t sig = found ? 13 : 7;
  auto mix = [&sig](int x) { sig = sig * 31 + (uint32_t)x; };
  for (char c : value) mix(c);
  for (char c : sub) mix(c);
  for (char c : label) mix(c);
  mix(h.running); mix(h.connected); mix(h.heat_on); mix(h.error);
  if (!force && sig == t.sig) return;
  t.sig = sig;
  (lv_label_set_text)(t.name, label.c_str());   // Geraetename: nicht uebersetzen
  lv_label_set_text(t.pct, value.c_str());
  lv_label_set_text(t.sub, sub.c_str());
  lv_color_t accent = !found ? C_TEXT3 : h.error ? C_RED : h.running ? C_AMBER : h.heat_on ? C_TEXT : C_TEXT2;
  lv_obj_set_style_text_color(t.icon, accent, 0);
  lv_obj_set_style_opa(t.obj, found && h.connected ? LV_OPA_COVER : LV_OPA_50, 0);
  lv_obj_set_height(t.fill, 0);
}

static void tiles_update_one(const std::string &key);

// ---------------------------------------------------------------- Satelliten-Anschluesse (Tank, Temperatur, Schalter …)
struct SioText {
  std::string value, sub;
  const char *icon = ICON_THERMO;
  lv_color_t accent = C_TEXT2;
  int fill = -1;
  bool dim = false;
};

static SioText sio_text(const SatPoint &p, bool small) {
  SioText r;
  bool val = p.ok && !isnan(p.value);
  switch (p.kind) {
    case SP_TANK: {
      r.icon = ICON_BATH;
      if (val) {
        int pct = (int)lroundf(p.value);
        r.value = std::to_string(pct) + "\xE2\x80\xAF%";
        r.fill = pct;
        r.accent = pct < 15 ? C_RED : C_BLUE;
        int wet = 0;
        for (int i = 0; i < 8; i++) wet += (p.probes >> i) & 1;
        char b[48];
        snprintf(b, sizeof(b), tr("Sonden %d/%d"), wet, p.probes_n);
        r.sub = p.odd ? std::string(tr("Sonden unplausibel")) : std::string(b);
      }
      break;
    }
    case SP_TEMP:
      r.icon = ICON_THERMO;
      if (val) {
        r.value = ui_temp_text(p.value);
        r.accent = C_AMBER;
      }
      break;
    case SP_ANALOG: {
      r.icon = p.cls == SC_TEMP ? ICON_THERMO : (p.cls == SC_LEVEL || p.cls == SC_HUMIDITY) ? ICON_BATH
             : (p.cls == SC_VOLTAGE || p.cls == SC_CURRENT) ? ICON_LIGHTNING : ICON_INFO;
      if (val) {
        float a = fabsf(p.value);
        r.value = ui_num(p.value, a >= 100 ? 0 : a >= 10 ? 1 : 2, p.unit.c_str());
        r.accent = C_BLUE;
        if (p.cls == SC_LEVEL && p.unit == "%") r.fill = std::max(0, std::min(100, (int)lroundf(p.value)));
      }
      break;
    }
    case SP_TILT:
      r.icon = ICON_HOUSE;
      if (val) {
        r.value = ui_num(p.value, 1, "\xC2\xB0");
        r.accent = fabsf(p.value) >= 3 ? C_AMBER : C_GREEN;
      }
      break;
    case SP_INPUT:
      if (p.cls == SC_ONOFF) {   // Zustand statt Kontakt, z. B. Landstrom (Hymer)
        r.icon = ICON_LIGHTNING;
        r.value = p.on ? tr("An") : tr("Aus");
        r.accent = p.on ? C_GREEN : C_TEXT2;
        break;
      }
      r.icon = ICON_DOOR;
      r.value = p.on ? tr("zu") : tr("offen");
      r.accent = p.on ? C_GREEN : C_TEXT2;
      break;
    case SP_SWITCH:
      r.icon = ICON_PLUG;
      r.value = p.on ? tr("An") : tr("Aus");
      r.accent = p.on ? C_AMBER : C_TEXT2;
      if (!small) r.sub = tr("Tippen zum Schalten");
      break;
    default:
      break;
  }
  if (r.value.empty()) {
    r.value = "–";
    r.dim = true;
  }
  if (!p.status.empty()) r.sub = tr(p.status.c_str());
  if (p.status == "Satellit offline") r.dim = true;
  return r;
}

static void sio_paint(TileUi &t, bool force) {
  SatPoint p;
  bool found = panelsat_point_get(t.key, p);
  auto nm = s_lay.names.find(t.key);
  std::string label = nm != s_lay.names.end() && !nm->second.empty() ? nm->second : found ? p.name : std::string(tr("Sensor"));
  SioText tt;
  if (found) tt = sio_text(p, t.small);
  else {
    tt.value = "–";
    tt.sub = tr("Satellit meldet sich nicht");
    tt.dim = true;
  }
  uint32_t sig = found ? 17 : 9;
  auto mix = [&sig](int x) { sig = sig * 31 + (uint32_t)x; };
  for (char c : tt.value) mix(c);
  for (char c : tt.sub) mix(c);
  for (char c : label) mix(c);
  mix(tt.fill); mix(tt.dim); mix(lv_color_to32(tt.accent)); mix(tt.icon[2]);
  if (!force && sig == t.sig) return;
  t.sig = sig;
  (lv_label_set_text)(t.name, label.c_str());   // Geraetename: nicht uebersetzen
  (lv_label_set_text)(t.icon, tt.icon);
  (lv_label_set_text)(t.pct, tt.value.c_str());
  (lv_label_set_text)(t.sub, tt.sub.c_str());
  lv_obj_set_style_text_color(t.icon, tt.accent, 0);
  lv_obj_set_style_text_color(t.pct, (tt.fill >= 0 && tt.fill < 15) ? C_RED : C_TEXT, 0);
  lv_obj_set_style_opa(t.obj, tt.dim ? LV_OPA_50 : LV_OPA_COVER, 0);
  if (tt.fill >= 0) {
    lv_obj_set_style_bg_color(t.fill, tt.accent, 0);
    lv_obj_set_height(t.fill, t.h * tt.fill / 100);
  } else {
    lv_obj_set_height(t.fill, 0);
  }
}

static void sio_click(const std::string &key, bool long_press) {
  SatPoint p;
  if (!panelsat_point_get(key, p)) {
    ui_toast("Der Satellit meldet diesen Anschluss gerade nicht.");
    return;
  }
  if (p.kind == SP_SWITCH && !long_press) {
    std::string err;
    if (!panelsat_switch(key, !p.on, &err)) ui_toast(err.c_str());
    tiles_update_one(key);
    return;
  }
  // Kurzinfo: Wert, Satellit und Weg
  std::string info = p.name + ": " + sio_text(p, false).value;
  if (p.kind == SP_TANK && p.probes_n) {
    info += "  ·  ";
    for (int i = 0; i < p.probes_n; i++) info += (p.probes >> i) & 1 ? "\xE2\x97\x8F" : "\xE2\x97\x8B";
  }
  std::string via = panelsat_via_text(key);
  if (!via.empty()) info += "\n" + via;
  ui_toast(info.c_str());
}

static void tile_event(lv_event_t *e);

// Gasflaschen, Victron und Heizung teilen sich den Aufbau: Icon, grosser Wert oben rechts, Name, Zeile, Pegel
static void create_gas_tile(const Tile &t, bool vic = false, bool heat = false, bool sio = false) {
  TileUi tu;
  tu.key = t.key;
  tu.gas = !vic && !heat && !sio;
  tu.sio = sio;
  tu.vic = vic;
  tu.heat = heat;
  tu.w = t.w;
  tu.h = t.h;
  tu.small = t.h < 96;
  tu.big = t.h >= 170 && t.w >= 220;
  lv_obj_t *o = lv_obj_create(s_content);
  ui_plain(o);
  tu.obj = o;
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(o, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_set_pos(o, t.x, t.y);
  lv_obj_set_size(o, t.w, t.h);
  lv_obj_set_style_radius(o, 18, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(o, C_TILE, 0);
  lv_obj_set_style_border_width(o, 1, 0);
  lv_obj_set_style_border_color(o, C_TILE_LINE, 0);
  lv_obj_set_style_clip_corner(o, true, 0);
  lv_obj_set_style_outline_color(o, lv_color_white(), 0);
  lv_obj_set_style_outline_pad(o, 2, 0);
  lv_obj_set_style_outline_width(o, 3, LV_STATE_PRESSED);
  lv_obj_set_style_outline_opa(o, LV_OPA_40, LV_STATE_PRESSED);
  // Pegel: farbige Flaeche von unten, so hoch wie der Fuellstand
  tu.fill = lv_obj_create(o);
  ui_plain(tu.fill);
  lv_obj_set_size(tu.fill, t.w, 0);
  lv_obj_align(tu.fill, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(tu.fill, LV_OPA_30, 0);

  const lv_font_t *ifont = tu.small ? F_ICON_S : (tu.big ? F_ICON_L : F_ICON_M);
  tu.icon = ui_icon(o, heat ? ICON_FLAME : vic ? ICON_BATTERY : ICON_GAS, ifont, C_TEXT2);
  tu.pct = ui_label(o, "", tu.big ? F_BIG : F_TITLE, C_TEXT);
  tu.name = ui_label(o, "", tu.big ? F_TITLE : F_BODY, C_TEXT);
  tu.sub = ui_label(o, "", F_SMALL, C_TEXT2);
  lv_label_set_long_mode(tu.name, LV_LABEL_LONG_DOT);
  if (tu.small) {
    lv_obj_align(tu.icon, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_set_width(tu.name, t.w - 64 - 70);
    lv_obj_align(tu.name, LV_ALIGN_TOP_LEFT, 50, t.h / 2 - 24);
    lv_obj_align(tu.sub, LV_ALIGN_TOP_LEFT, 50, t.h / 2 + 2);
    lv_obj_align(tu.pct, LV_ALIGN_RIGHT_MID, -14, 0);
  } else {
    int pad = tu.big ? 18 : 14;
    lv_obj_align(tu.icon, LV_ALIGN_TOP_LEFT, pad, pad - 2);
    lv_obj_align(tu.pct, LV_ALIGN_TOP_RIGHT, -pad, pad - 4);
    lv_obj_set_width(tu.name, t.w - 2 * pad);
    lv_obj_align(tu.sub, LV_ALIGN_BOTTOM_LEFT, pad, -16);
    lv_obj_align(tu.name, LV_ALIGN_BOTTOM_LEFT, pad, -38);
  }
  if (vic || heat || sio) {
    // Victron-/Heizungszeilen koennen lang werden ("Wiederholte Absorption · 13,4 V")
    lv_label_set_long_mode(tu.sub, LV_LABEL_LONG_DOT);
    lv_obj_set_width(tu.sub, tu.small ? t.w - 64 - 70 : t.w - 2 * (tu.big ? 18 : 14));
  }
  size_t idx = s_tiles.size();
  lv_obj_add_event_cb(o, tile_event, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)idx);
  lv_obj_add_event_cb(o, tile_event, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)idx);
  s_tiles.push_back(tu);
}

static void tile_event(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  size_t idx = (size_t)(intptr_t)lv_event_get_user_data(e);
  if (idx >= s_tiles.size()) return;
  std::string key = s_tiles[idx].key;
  if (s_tiles[idx].sio) {
    if (millis() - s_last_gesture < 500) return;
    if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_LONG_PRESSED) sio_click(key, code == LV_EVENT_LONG_PRESSED);
    return;
  }
  if (s_tiles[idx].gas || s_tiles[idx].vic || s_tiles[idx].heat) {
    if (millis() - s_last_gesture < 500) return;
    if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_LONG_PRESSED) {
      if (s_tiles[idx].heat) ui_heat_open(key);
      else if (s_tiles[idx].vic) ui_vic_open(key);
      else ui_gas_open(key);
    }
    return;
  }
  if (code == LV_EVENT_SHORT_CLICKED) {
    if (millis() - s_last_gesture < 500) return;
    ControlView v;
    if (!z2m_get(key, v)) {
      ui_toast("Dieses Gerät ist in Zigbee2MQTT nicht (mehr) vorhanden.");
      return;
    }
    if (!z2m_connected()) {
      ui_toast("Keine Verbindung zum MQTT-Broker.");
      return;
    }
    z2m_toggle(key);
  } else if (code == LV_EVENT_LONG_PRESSED) {
    if (millis() - s_last_gesture < 500) return;
    ui_detail_open(key);
  }
}

static void create_tile(const Tile &t) {
  if (panelsat_is_point_key(t.key)) {
    create_gas_tile(t, false, false, true);
    return;
  }
  if (ui_gas_is_key(t.key) || victron_is_key(t.key) || truma_heat_is_key(t.key)) {
    create_gas_tile(t, victron_is_key(t.key), truma_heat_is_key(t.key));
    return;
  }
  TileUi tu;
  tu.key = t.key;
  tu.icon_id = t.icon;
  tu.w = t.w;
  tu.h = t.h;
  tu.small = t.h < 96;
  tu.big = t.h >= 170 && t.w >= 220;

  lv_obj_t *o = lv_obj_create(s_content);
  ui_plain(o);
  tu.obj = o;
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(o, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_set_pos(o, t.x, t.y);
  lv_obj_set_size(o, t.w, t.h);
  lv_obj_set_style_radius(o, 18, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(o, 1, 0);
  lv_obj_set_style_outline_color(o, lv_color_white(), 0);
  lv_obj_set_style_outline_opa(o, LV_OPA_60, 0);
  lv_obj_set_style_outline_pad(o, 2, 0);
  lv_obj_set_style_outline_width(o, 3, LV_STATE_PRESSED);
  lv_obj_set_style_outline_opa(o, LV_OPA_40, LV_STATE_PRESSED);

  const lv_font_t *ifont = tu.small ? F_ICON_S : (tu.big ? F_ICON_L : F_ICON_M);
  tu.icon = ui_icon(o, ICON_BULB, ifont, C_TEXT2);
  tu.name = ui_label(o, "", tu.big ? F_TITLE : F_BODY, C_TEXT);
  tu.sub = ui_label(o, "", F_SMALL, C_TEXT2);
  lv_label_set_long_mode(tu.name, LV_LABEL_LONG_DOT);

  if (tu.small) {
    // Kompakt: Icon links, Name und Zustand rechts daneben
    lv_obj_align(tu.icon, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_set_width(tu.name, t.w - 64);
    lv_obj_align(tu.name, LV_ALIGN_TOP_LEFT, 50, t.h / 2 - 24);
    lv_obj_align(tu.sub, LV_ALIGN_TOP_LEFT, 50, t.h / 2 + 2);
  } else {
    int pad = tu.big ? 18 : 14;
    lv_obj_align(tu.icon, LV_ALIGN_TOP_LEFT, pad, pad - 2);
    lv_obj_set_width(tu.name, t.w - 2 * pad);
    lv_obj_align(tu.sub, LV_ALIGN_BOTTOM_LEFT, pad, -16);
    lv_obj_align(tu.name, LV_ALIGN_BOTTOM_LEFT, pad, -38);
    if (t.h >= 100) {
      tu.track = lv_obj_create(o);
      ui_plain(tu.track);
      lv_obj_set_size(tu.track, t.w - 28, 4);
      lv_obj_set_pos(tu.track, 14, t.h - 9);
      lv_obj_set_style_radius(tu.track, 2, 0);
      lv_obj_set_style_bg_opa(tu.track, LV_OPA_20, 0);
      tu.fill = lv_obj_create(tu.track);
      ui_plain(tu.fill);
      lv_obj_set_size(tu.fill, 4, 4);
      lv_obj_set_style_radius(tu.fill, 2, 0);
      lv_obj_set_style_bg_opa(tu.fill, LV_OPA_80, 0);
      lv_obj_add_flag(tu.track, LV_OBJ_FLAG_HIDDEN);
    }
  }
  size_t idx = s_tiles.size();
  lv_obj_add_event_cb(o, tile_event, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)idx);
  lv_obj_add_event_cb(o, tile_event, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)idx);
  s_tiles.push_back(tu);
}

static void tiles_update_one(const std::string &key) {
  for (auto &t : s_tiles)
    if (t.key == key && t.sio) sio_paint(t, true);
}

static void tiles_update(bool force) {
  for (auto &t : s_tiles) {
    if (t.gas) gas_paint(t, force);
    else if (t.vic) vic_paint(t, force);
    else if (t.heat) heat_paint(t, force);
    else if (t.sio) sio_paint(t, force);
    else tile_paint(t, force);
  }
}

// ---------------------------------------------------------------- Einstiegshilfen
enum OnboardMode { OB_NONE = 0, OB_WIFI, OB_MQTT, OB_ROUTER_WAIT, OB_EMPTY_ALL, OB_EMPTY_PAGE };

static int onboard_mode() {
  if (net_mode() != NET_AP && net_mode() != NET_OFF && g_settings.wifi_ssid.isEmpty()) return OB_WIFI;
  if (!settings_mqtt_configured()) return OB_MQTT;
  size_t total = 0;
  for (auto &p : s_lay.pages) total += p.tiles.size();
  if (total == 0 && settings_mqtt_auto_active() && !z2m_connected()) return OB_ROUTER_WAIT;
  if (total == 0) return OB_EMPTY_ALL;
  if (s_page < (int)s_lay.pages.size() && s_lay.pages[s_page].tiles.empty()) return OB_EMPTY_PAGE;
  return OB_NONE;
}

static void ob_settings_cb(lv_event_t *e) { ui_settings_open((int)(intptr_t)lv_event_get_user_data(e)); }

static void ob_arrange_cb(lv_event_t *) {
  auto all = ui_arrange_items();
  if (all.empty()) {
    ui_toast(z2m_connected() ? z2m_diag_text() : "Noch keine Verbindung zum MQTT-Broker.");
    return;
  }
  layout_auto_arrange(all, true);
  ui_toast("Geräte wurden verteilt. Feinschliff im Browser-Editor.");
}

static void build_onboarding(int mode) {
  lv_obj_t *card = lv_obj_create(s_content);
  ui_plain(card);
  lv_obj_set_size(card, 680, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(card, C_CARD, 0);
  lv_obj_set_style_radius(card, 24, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, C_LINE, 0);
  lv_obj_set_style_pad_all(card, 36, 0);
  lv_obj_set_style_pad_row(card, 14, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_center(card);

  const char *icon = ICON_WIFI, *title = "", *text = "";
  String ip = net_editor_ip();
  char buf[320];
  switch (mode) {
    case OB_WIFI:
      icon = ICON_WIFI;
      title = "Willkommen";
      text = "Verbinde das Panel mit deinem Heim-WLAN – oder lass es im Router-Modus ein eigenes WLAN für "
             "deinen Zigbee-Hub öffnen.";
      break;
    case OB_MQTT:
      icon = ICON_LINK;
      title = "MQTT-Broker eintragen";
      text = "Das Panel spricht direkt mit Zigbee2MQTT. Dafür braucht es die Adresse deines MQTT-Brokers.";
      break;
    case OB_ROUTER_WAIT:
      icon = ICON_WIFI;
      title = "Panel-WLAN ist bereit";
      snprintf(buf, sizeof(buf),
               tr("Verbinde den SMHUB Nano im WLAN-Menü mit „%s“, Passwort %s. Das Panel findet den MQTT-Broker "
                  "darauf von selbst. Stand: %s"),
               g_settings.ap_ssid.c_str(), g_settings.ap_pass.c_str(), z2m_status_text());
      text = buf;
      break;
    case OB_EMPTY_ALL:
      icon = ICON_GRID;
      title = "Noch keine Geräte auf dem Panel";
      snprintf(buf, sizeof(buf), tr("Ordne sie im Browser an: http://%s/ – oder lass alle Lampen und Schalter automatisch verteilen."),
               ip.length() ? ip.c_str() : (g_settings.hostname + ".local").c_str());
      text = buf;
      break;
    default:
      icon = ICON_GRID;
      title = "Diese Seite ist leer";
      snprintf(buf, sizeof(buf), tr("Zieh im Editor unter http://%s/ Geräte auf diese Seite."),
               ip.length() ? ip.c_str() : (g_settings.hostname + ".local").c_str());
      text = buf;
      break;
  }
  ui_icon(card, icon, F_ICON_L, C_AMBER);
  ui_label(card, title, F_TITLE, C_TEXT);
  lv_obj_t *t = ui_label(card, text, F_BODY, C_TEXT2);
  lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(t, lv_pct(100));
  lv_obj_t *row = lv_obj_create(card);
  ui_plain(row);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_top(row, 10, 0);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  if (mode == OB_WIFI) ui_button(row, ICON_WIFI, "Netzwerk einrichten", true, ob_settings_cb, (void *)(intptr_t)SEC_WIFI);
  else if (mode == OB_ROUTER_WAIT) ui_button(row, ICON_INFO, "Status", false, ob_settings_cb, (void *)(intptr_t)SEC_INFO);
  else if (mode == OB_MQTT) ui_button(row, ICON_LINK, "MQTT einrichten", true, ob_settings_cb, (void *)(intptr_t)SEC_MQTT);
  else if (mode == OB_EMPTY_ALL) {
    ui_button(row, ICON_WAND, "Automatisch anordnen", true, ob_arrange_cb, nullptr);
    ui_button(row, ICON_INFO, "Adresse anzeigen", false, ob_settings_cb, (void *)(intptr_t)SEC_INFO);
  }
}

// ---------------------------------------------------------------- Seiten & Statusleiste
static void set_page(int p);

static void tab_cb(lv_event_t *e) { set_page((int)(intptr_t)lv_event_get_user_data(e)); }

static void build_tabs() {
  lv_obj_clean(s_tabs);
  int n = s_lay.pages.size();
  for (int i = 0; i < n; i++) {
    lv_obj_t *b = lv_obj_create(s_tabs);
    ui_plain(b);
    lv_obj_set_size(b, LV_SIZE_CONTENT, UI_TOPBAR_H);
    lv_obj_set_style_pad_hor(b, 14, 0);
    bool act = i == s_page;
    lv_obj_t *l = ui_label(b, s_lay.pages[i].name.c_str(), n > 1 ? F_BODY : F_TITLE, act ? C_TEXT : C_TEXT2);
    lv_obj_center(l);
    if (n > 1) {
      lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(b, tab_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
      if (act) {
        lv_obj_set_style_border_side(b, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(b, 3, 0);
        lv_obj_set_style_border_color(b, C_AMBER, 0);
      }
    }
  }
}

void ui_main_rebuild() {
  s_lay = layout_get();
  if (s_page >= (int)s_lay.pages.size()) s_page = 0;
  build_tabs();
  lv_obj_clean(s_content);
  s_tiles.clear();

  const Page &pg = s_lay.pages[s_page];
  const lv_img_dsc_t *dsc = pg.bg ? bg_get(pg.id) : nullptr;
  if (dsc) {
    lv_img_set_src(s_bg, dsc);
    lv_obj_clear_flag(s_bg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_70, 0);
  } else {
    lv_obj_add_flag(s_bg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, 0);
  }

  s_onboard_mode = onboard_mode();
  if (s_onboard_mode != OB_NONE && s_onboard_mode != OB_EMPTY_PAGE) {
    build_onboarding(s_onboard_mode);
    return;
  }
  if (s_onboard_mode == OB_EMPTY_PAGE) build_onboarding(OB_EMPTY_PAGE);
  for (auto &t : pg.tiles) create_tile(t);
  tiles_update(true);
}

static void set_page(int p) {
  int n = s_lay.pages.size();
  if (n == 0) return;
  p = (p % n + n) % n;
  if (p == s_page) return;
  s_page = p;
  ui_main_rebuild();
}

static void gesture_cb(lv_event_t *) {
  lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (d == LV_DIR_LEFT || d == LV_DIR_RIGHT) {
    s_last_gesture = millis();
    lv_indev_wait_release(lv_indev_get_act());
    if (s_lay.pages.size() > 1) set_page(s_page + (d == LV_DIR_LEFT ? 1 : -1));
  }
}

static void gear_cb(lv_event_t *) { ui_settings_open(SEC_WIFI); }
static void wifi_banner_cb(lv_event_t *) { ui_settings_open(SEC_WIFI); }

// Gespeichertes Heim-WLAN laenger nicht erreichbar: unten einen antippbaren Hinweis zeigen
static void wifi_banner_update() {
  bool show = net_mode() == NET_STA && !g_settings.wifi_ssid.isEmpty() && net_offline_ms() > 30000;
  if (!show) {
    lv_obj_add_flag(s_wifi_banner, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  char b[160];
  snprintf(b, sizeof(b), tr("Kein WLAN: %s  –  antippen, um ein Netz zu wählen"), net_status_text());
  lv_label_set_text(s_wifi_banner_txt, b);
  lv_obj_clear_flag(s_wifi_banner, LV_OBJ_FLAG_HIDDEN);
}
static void status_cb(lv_event_t *) { ui_settings_open(SEC_INFO); }

static void status_update() {
  struct tm tm;
  if (net_time_valid() && getLocalTime(&tm, 0)) {
    char b[8];
    strftime(b, sizeof(b), "%H:%M", &tm);
    lv_label_set_text(s_clock, b);
  } else {
    lv_label_set_text(s_clock, "");
  }
  bool w = net_mode() == NET_AP ? net_ap_clients() > 0 : net_connected();
  lv_label_set_text(s_ic_wifi, w || net_mode() == NET_AP ? ICON_WIFI : ICON_WIFI_OFF);
  lv_obj_set_style_text_color(s_ic_wifi, w ? C_TEXT2 : (net_mode() == NET_AP || net_mode() == NET_OFF ? C_TEXT3 : C_RED), 0);
  bool m = z2m_connected();
  lv_label_set_text(s_ic_mqtt, m ? ICON_LINK : ICON_LINK_OFF);
  lv_obj_set_style_text_color(s_ic_mqtt, !m ? C_RED : (z2m_bridge_online() ? C_TEXT2 : C_AMBER), 0);
  // Akku: Symbol zeigt die geschaetzte Quelle (Akku / laedt / USB voll), daneben der Ladestand
  power_tick();
  PowerInfo p = power_get();
  if (p.has_battery) {
    lv_obj_clear_flag(s_ic_batt, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_batt, LV_OBJ_FLAG_HIDDEN);
    const char *ic = p.src == PWR_CHARGING ? ICON_LIGHTNING : p.src == PWR_USB_FULL ? ICON_PLUG : ICON_BATTERY;
    lv_label_set_text(s_ic_batt, ic);
    lv_color_t col = p.src == PWR_CHARGING ? C_AMBER : p.percent < 15 ? C_RED : p.percent < 30 ? C_AMBER : C_TEXT2;
    lv_obj_set_style_text_color(s_ic_batt, col, 0);
    char b[12];
    snprintf(b, sizeof(b), "%d\xE2\x80\xAF%%", p.percent);
    lv_label_set_text(s_batt, b);
    lv_obj_set_style_text_color(s_batt, p.percent < 15 && p.src == PWR_BATTERY ? C_RED : C_TEXT2, 0);
  } else {
    lv_obj_add_flag(s_ic_batt, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_batt, LV_OBJ_FLAG_HIDDEN);
  }
  wifi_banner_update();
}

void ui_init() {
  theme_init();
  s_scr = lv_scr_act();
  lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(s_scr, C_BG, 0);
  lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);

  s_bg = lv_img_create(s_scr);
  lv_obj_set_pos(s_bg, 0, 0);
  lv_obj_add_flag(s_bg, LV_OBJ_FLAG_HIDDEN);

  s_bar = lv_obj_create(s_scr);
  ui_plain(s_bar);
  lv_obj_set_size(s_bar, LCD_H_RES, UI_TOPBAR_H);
  lv_obj_set_style_bg_color(s_bar, C_BAR, 0);
  lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(s_bar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(s_bar, 1, 0);
  lv_obj_set_style_border_color(s_bar, C_LINE, 0);

  s_tabs = lv_obj_create(s_bar);
  ui_plain(s_tabs);
  lv_obj_set_size(s_tabs, 700, UI_TOPBAR_H);
  lv_obj_set_pos(s_tabs, 8, 0);
  lv_obj_set_flex_flow(s_tabs, LV_FLEX_FLOW_ROW);
  lv_obj_add_flag(s_tabs, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(s_tabs, LV_DIR_HOR);
  lv_obj_set_scrollbar_mode(s_tabs, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *gear = ui_icon_button(s_bar, ICON_GEAR, gear_cb, nullptr, 48);
  lv_obj_align(gear, LV_ALIGN_RIGHT_MID, -8, 0);

  lv_obj_t *status = lv_obj_create(s_bar);
  ui_plain(status);
  lv_obj_set_size(status, LV_SIZE_CONTENT, UI_TOPBAR_H);
  lv_obj_set_flex_flow(status, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(status, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(status, 14, 0);
  lv_obj_set_style_pad_hor(status, 8, 0);
  lv_obj_add_flag(status, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(status, status_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_align(status, LV_ALIGN_RIGHT_MID, -60, 0);
  s_ic_batt = ui_icon(status, ICON_BATTERY, F_ICON_S, C_TEXT2);
  s_batt = ui_label(status, "", F_SMALL, C_TEXT2);
  lv_obj_add_flag(s_ic_batt, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s_batt, LV_OBJ_FLAG_HIDDEN);
  s_ic_mqtt = ui_icon(status, ICON_LINK_OFF, F_ICON_S, C_RED);
  s_ic_wifi = ui_icon(status, ICON_WIFI_OFF, F_ICON_S, C_RED);
  s_clock = ui_label(status, "", F_TITLE, C_TEXT);

  s_content = lv_obj_create(s_scr);
  ui_plain(s_content);
  lv_obj_set_pos(s_content, 0, UI_TOPBAR_H);
  lv_obj_set_size(s_content, LCD_H_RES, UI_CONTENT_H);
  lv_obj_add_flag(s_content, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_content, gesture_cb, LV_EVENT_GESTURE, nullptr);

  s_wifi_banner = lv_obj_create(s_scr);
  ui_plain(s_wifi_banner);
  lv_obj_add_flag(s_wifi_banner, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(s_wifi_banner, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(s_wifi_banner, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(s_wifi_banner, lv_color_hex(0x3A1F1C), 0);
  lv_obj_set_style_bg_color(s_wifi_banner, lv_color_hex(0x52302B), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(s_wifi_banner, 1, 0);
  lv_obj_set_style_border_color(s_wifi_banner, C_RED, 0);
  lv_obj_set_style_radius(s_wifi_banner, 24, 0);
  lv_obj_set_style_pad_hor(s_wifi_banner, 20, 0);
  lv_obj_set_style_pad_ver(s_wifi_banner, 12, 0);
  lv_obj_set_style_pad_column(s_wifi_banner, 12, 0);
  lv_obj_set_flex_flow(s_wifi_banner, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_wifi_banner, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  ui_icon(s_wifi_banner, ICON_WIFI_OFF, F_ICON_S, C_RED);
  s_wifi_banner_txt = ui_label(s_wifi_banner, "", F_BODY, C_TEXT);
  lv_obj_set_style_max_width(s_wifi_banner_txt, 860, 0);
  lv_label_set_long_mode(s_wifi_banner_txt, LV_LABEL_LONG_DOT);
  lv_obj_align(s_wifi_banner, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_add_event_cb(s_wifi_banner, wifi_banner_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(s_wifi_banner, LV_OBJ_FLAG_HIDDEN);

  s_seen_layout = layout_version();
  s_seen_devs = z2m_devices_version();
  s_seen_state = z2m_state_version();
  ui_main_rebuild();
  status_update();
}

void ui_bg_changed(const char *page) {
  lv_obj_add_flag(s_bg, LV_OBJ_FLAG_HIDDEN);
  bg_forget(page ? page : "");
  ui_main_rebuild();
}

void ui_settings_changed() { ui_settings_refresh(); }

void ui_loop() {
  static uint32_t t_fast = 0, t_slow = 0;
  uint32_t now = millis();
  ui_saver_tick();
  if (now - t_fast >= 100) {
    t_fast = now;
    if (layout_version() != s_seen_layout || z2m_devices_version() != s_seen_devs) {
      s_seen_layout = layout_version();
      s_seen_devs = z2m_devices_version();
      ui_main_rebuild();
    }
    if (z2m_state_version() != s_seen_state) {
      s_seen_state = z2m_state_version();
      tiles_update(false);
      ui_detail_update();
    }
    if (truma_version() != s_seen_truma) {
      s_seen_truma = truma_version();
      tiles_update(false);
      ui_gas_update();
      ui_settings_tick_gas();
    }
    if (ui_heat_version() != s_seen_heat) {
      s_seen_heat = ui_heat_version();
      tiles_update(false);
      ui_heat_update();
      ui_settings_tick_heat();
    }
    if (mopeka_version() != s_seen_mop) {
      s_seen_mop = mopeka_version();
      tiles_update(false);
      ui_gas_update();
      ui_settings_tick_mopeka();
    }
    if (victron_version() != s_seen_vic) {
      s_seen_vic = victron_version();
      tiles_update(false);
      ui_vic_update();
      ui_settings_tick_victron();
    }
    if (panelsat_version() != s_seen_sat) {   // Satelliten (MQTT/UDP/Bluetooth), Hymer Connect
      s_seen_sat = panelsat_version();
      ui_settings_tick_hymer();                // Status, Kopplung und Werte von Hymer Connect
      tiles_update(false);
      ui_gas_update();
      ui_vic_update();
      ui_settings_tick_sat();
    }
    char msg[96];
    if (app_pop_toast(msg, sizeof(msg))) ui_toast(msg);
  }
  if (now - t_slow >= 1000) {
    t_slow = now;
    status_update();
    static std::string last_status;
    bool router_wait_changed = s_onboard_mode == OB_ROUTER_WAIT && last_status != z2m_status_text();
    last_status = z2m_status_text();
    if (onboard_mode() != s_onboard_mode || router_wait_changed) ui_main_rebuild();
    if (s_onboard_mode == OB_NONE) tiles_update(false);
    ui_settings_tick();
  }
}
