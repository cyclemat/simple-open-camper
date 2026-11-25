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
#include "../victron_ctl.h"
#include "../victron.h"
#include "../panelsat.h"
#include "../power.h"
#include "../board.h"
#include "../board_crowpanel146.h"
#if UI_ROUND
#include <WiFi.h>
#include "../bms.h"
#include "../truma_heat.h"
#endif
#include "../cerbo.h"
#include "../bms.h"   // Victron-Edition: automatische Kacheln auch fuer Akkus
#include "../zb_direct.h"
#include "../shelly.h"
#include "../wled.h"
#include "../shelly_ble.h"
#include "../ha_link.h"
#include "../tailscale.h"
#include "../mqtt_broker.h"   // WITH_BROKER
#include "../audio.h"         // WITH_AUDIO
// Victron-Edition: kein Zigbee-Text im Programm (Funktion dort nicht eingebaut)
#if WITH_ZB_DIRECT
#define ZBD_OFFLINE_TXT tr("Zigbee-Modul nicht verbunden")
#else
#define ZBD_OFFLINE_TXT tr("nicht verfügbar")
#endif
#include <Preferences.h>

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
static lv_obj_t *s_wifi_banner, *s_wifi_banner_txt;  // Hinweis: Camp-WLAN nicht erreichbar
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
  for (char c : v.info) mix(c);   // Messwerte (Steckdose): Leistung/Energie
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
  lv_label_set_text(t.icon, icon_any(t.icon_id.empty() ? auto_icon(found ? &v : nullptr) : t.icon_id.c_str()));

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
  if (!found) snprintf(sub, sizeof(sub), "%s", ui_dev_missing_text(t.key));
  else if (!v.online) snprintf(sub, sizeof(sub), "nicht erreichbar");
  else if (!v.known) snprintf(sub, sizeof(sub), "…");
  else if (!v.on) snprintf(sub, sizeof(sub), v.info.empty() ? "%s" : "%s  ·  %s", tr("Aus"), v.info.c_str());
  else if (!v.info.empty()) snprintf(sub, sizeof(sub), "%s  ·  %s", tr("An"), v.info.c_str());   // Steckdose mit Messung
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
  std::string label = found ? ui_heat_label(s_lay, h) : (s_lay.names.count(t.key) ? s_lay.names[t.key] : tr(TXT_HEAT));
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
        if (!p.probes_n) r.sub = p.status == "OK" ? std::string() : p.status;   // ohne Sonden (z. B. Tank am GX): Inhalt
        else r.sub = p.odd ? std::string(tr("Sonden unplausibel")) : std::string(b);
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
      r.icon = p.cls == SC_TEMP ? ICON_THERMO
             : (p.cls == SC_LEVEL || p.cls == SC_HUMIDITY || p.cls == SC_FLOW || p.cls == SC_VOLUME) ? ICON_BATH
             : (p.cls == SC_VOLTAGE || p.cls == SC_CURRENT || p.cls == SC_POWER) ? ICON_LIGHTNING : ICON_INFO;
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
      r.accent = p.on ? C_AMBER : C_TEXT2;   // kein Hinweis "Tippen zum Schalten": lag ueber dem Namen
      break;
    default:
      break;
  }
  if (r.value.empty()) {
    r.value = "–";
    r.dim = true;
  }
  if (!p.status.empty()) r.sub = tr(p.status.c_str());
  if (p.status == tr(TXT_SAT_OFFLINE)) r.dim = true;
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
    tt.sub = tr(TXT_SAT_SILENT);
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
#if WITH_SATELLITES
    ui_toast("Der Satellit meldet diesen Anschluss gerade nicht.");
#endif
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
#if UI_ROUND
void round_ctl_open(const std::string &key);
#endif

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
#if WITH_AUDIO
    if (key == AUDIO_KEY && code == LV_EVENT_SHORT_CLICKED) {   // Musik: kurz = Abspielen/Pause, lang = Fenster
      audio_toggle();
      return;
    }
#endif
#if UI_ROUND
    // Detailfenster sind fuer 1024x600 gebaut. Rund: Heizung, Kuehlbox und Power Station bekommen eine eigene
    // Bedienseite, alles andere zeigt seine Werte ueber die Datenseiten (hoch/runter wischen).
    if (s_tiles[idx].vic && code == LV_EVENT_SHORT_CLICKED) round_ctl_open(key);
    return;
#endif
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
      ui_toast(ui_dev_gone_text(key));
      return;
    }
    if (!zbd_is_key(key) && !z2m_connected()) {
      ui_toast("Keine Verbindung zum MQTT-Broker.");
      return;
    }
    z2m_toggle(key);
  } else if (code == LV_EVENT_LONG_PRESSED) {
    if (millis() - s_last_gesture < 500) return;
#if !UI_ROUND
    ui_detail_open(key);
#endif
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
  if (!SOC_VICTRON_EDITION && g_settings.mqtt_on && !settings_mqtt_configured()) return OB_MQTT;
  size_t total = 0;
  for (auto &p : s_lay.pages) total += p.tiles.size();
  if (!SOC_VICTRON_EDITION && total == 0 && settings_mqtt_auto_active() && !z2m_connected()) return OB_ROUTER_WAIT;
  if (total == 0) return OB_EMPTY_ALL;
  if (s_page < (int)s_lay.pages.size() && s_lay.pages[s_page].tiles.empty()) return OB_EMPTY_PAGE;
  return OB_NONE;
}

static void ob_settings_cb(lv_event_t *e) { ui_settings_open((int)(intptr_t)lv_event_get_user_data(e)); }

static void ob_arrange_cb(lv_event_t *) {
  auto all = ui_arrange_items();
  if (all.empty()) {
#if SOC_VICTRON_EDITION
    ui_toast("Noch keine Geräte – zuerst Victron, Cerbo GX, Gas oder Akkus einrichten.");
#else
    ui_toast(z2m_connected() ? z2m_diag_text() : "Noch keine Verbindung zum MQTT-Broker.");
#endif
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
#if SOC_VICTRON_EDITION
      text = "Verbinde das Panel mit deinem WLAN (dort, wo auch der Cerbo GX ist). Victron-Geräte, Gasflaschen und "
             "Akkus per Bluetooth laufen auch ohne WLAN.";
#else
      text = "Verbinde das Panel mit deinem Camp-WLAN – oder lass es im Router-Modus ein eigenes WLAN für "
             "deinen Zigbee-Hub öffnen.";
#endif
      break;
    case OB_MQTT:
      icon = ICON_LINK;
      title = "Kein MQTT-Broker eingerichtet";
#if !SOC_VICTRON_EDITION
#if WITH_BROKER
      text = "MQTT ist eingeschaltet, aber es ist noch kein Broker eingerichtet. Unter Einstellungen → MQTT die Adresse "
             "eintragen, den eingebauten Broker einschalten oder MQTT ausschalten, wenn du es nicht brauchst.";
#else
      text = "MQTT ist eingeschaltet, aber es ist noch kein Broker eingerichtet. Unter Einstellungen → MQTT die Adresse "
             "eintragen oder MQTT ausschalten, wenn du es nicht brauchst.";
#endif
#endif
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
#if SOC_VICTRON_EDITION
      snprintf(buf, sizeof(buf), "%s",
               tr("Richte Victron-Geräte, den Cerbo GX, Gasflaschen oder Akkus ein – sobald sie Werte liefern, erscheinen "
                  "ihre Kacheln von selbst. Anordnen geht im Browser-Editor."));
#else
      snprintf(buf, sizeof(buf), tr("Ordne sie im Browser an: http://%s/ – oder lass alle Lampen und Schalter automatisch verteilen."),
               ip.length() ? ip.c_str() : (g_settings.hostname + ".local").c_str());
#endif
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
#if SOC_VICTRON_EDITION
    ui_button(row, ICON_SUN, "Victron", true, ob_settings_cb, (void *)(intptr_t)SEC_VICTRON);
    ui_button(row, ICON_GAS, "Gas", false, ob_settings_cb, (void *)(intptr_t)SEC_GAS);
    ui_button(row, ICON_BATTERY, "Akkus", false, ob_settings_cb, (void *)(intptr_t)SEC_BMS);
#else
    ui_button(row, ICON_WAND, "Automatisch anordnen", true, ob_arrange_cb, nullptr);
    ui_button(row, ICON_INFO, "Adresse anzeigen", false, ob_settings_cb, (void *)(intptr_t)SEC_INFO);
#endif
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

// Alle Werte eines Geraets als Zeilen (wie das Detailfenster der grossen Panels): Datenseiten des runden Panels
// und Detail-Fenster der Live-Ansicht (/api/live?key=…). Nur im UI-Task.
typedef std::vector<std::pair<std::string, std::string>> RoundRows;
static std::string round_onoff(bool on) { return tr(on ? "an" : "aus"); }

RoundRows ui_tile_rows(const std::string &key, std::string *name) {
  RoundRows r;
  std::string nm;
#if WITH_BMS
  BmsInfo b;
  if (bms_get(key, b)) {
    VictronInfo v;
    if (name && ui_vic_get(key, v)) *name = ui_vic_label(s_lay, v);
    RoundRows r = bms_rows(b);
    for (auto &c : bms_cell_rows(b)) r.push_back(c);   // je Zelle eine Datenseite
    return r;
  }
#endif
  VictronInfo v;
  if (victron_is_key(key) && ui_vic_get(key, v)) {
    if (name) *name = ui_vic_label(s_lay, v);
    return ui_vic_rows(v);
  }
  GasView g;
  if (ui_gas_is_key(key) && ui_gas_get(key, g)) {
    if (name) *name = ui_gas_label(s_lay, g);
    if (!g.has) {
      r.push_back({tr("Zustand"), g.status.empty() ? std::string("–") : g.status});
      return r;
    }
    char b[48];
    snprintf(b, sizeof(b), "%d\xE2\x80\xAF%%", g.percent);
    r.push_back({tr("Füllstand"), b});
    if (!g.amount.empty()) r.push_back({tr("Inhalt"), g.amount});
    if (g.level_mm >= 0) {
      snprintf(b, sizeof(b), "%d\xE2\x80\xAFmm", g.level_mm);
      r.push_back({tr("Füllhöhe"), b});
    }
    snprintf(b, sizeof(b), "%d\xE2\x80\xAF°C", g.temperature);
    r.push_back({tr("Temperatur"), b});
    snprintf(b, sizeof(b), "%d\xE2\x80\xAF%%", g.battery);
    r.push_back({tr("Sensor-Batterie"), b});
    snprintf(b, sizeof(b), tr("%d von 3"), g.quality);
    r.push_back({tr("Messqualität"), b});
    r.push_back({tr("Letzte Messung"), ui_age_text(g.age_s)});
    return r;
  }
  TrumaHeatInfo h;
  if (truma_heat_is_key(key) && ui_heat_get(key, h)) {
    if (name) *name = ui_heat_label(s_lay, h);
    r.push_back({tr("Raum"), ui_num(h.room_actual, 1, "°C")});
    r.push_back({tr("Solltemperatur"), ui_num(h.room_target, 1, "°C")});
    r.push_back({tr(TXT_HEAT), round_onoff(h.heat_on)});
    if (!isnan(h.water_actual)) r.push_back({tr("Boiler"), ui_num(h.water_actual, 0, "°C")});
    if (!isnan(h.outdoor)) r.push_back({tr("Außen"), ui_num(h.outdoor, 1, "°C")});
    if (!isnan(h.battery_v)) r.push_back({tr("Batterie"), ui_num(h.battery_v, 1, "V")});
    if (h.error) r.push_back({tr("Fehler"), std::to_string(h.error)});
    r.push_back({tr("Verbindung"), h.connected ? h.source : (h.status.empty() ? std::string("–") : h.status)});
    return r;
  }
  ControlView c;
  if (z2m_get(key, c)) {
    if (name) *name = layout_label(s_lay, c);
    r.push_back({tr("Zustand"), !c.online ? std::string(tr("nicht erreichbar")) : round_onoff(c.on)});
    if (c.has_bri && c.bri_pct() >= 0) r.push_back({tr("Helligkeit"), std::to_string(c.bri_pct()) + "\xE2\x80\xAF%"});
    if (c.has_ct && c.ct > 0) r.push_back({tr("Farbtemperatur"), std::to_string(1000000 / c.ct) + "\xE2\x80\xAFK"});
    if (!c.model.empty()) r.push_back({tr("Modell"), c.vendor.empty() ? c.model : c.vendor + " " + c.model});
  }
  return r;
}

#if UI_ROUND
// ---------------------------------------------------------------- Runde Oberflaeche (CrowPanel 1.46")
// Eine grosse Kachel pro Bildschirm in der Reihenfolge des Layouts (alle Seiten hintereinander), danach eine
// Info-Seite (WLAN, Adresse der Weboberflaeche). Drehknopf/Wischen blaettert, Antippen/Knopfdruck schaltet.
// Eingerichtet wird nur ueber die Weboberflaeche (Editor).
static int s_ri = 0;                 // aktuelle Kachel
static lv_obj_t *s_rpos = nullptr;   // "3 / 7" unten
static const int R_TILE = LCD_H_RES; // runde Kachel = ganzer Bildschirm, ohne Rand (wirkt wie der Hintergrund)

// Inhalt einer Kachel fuer die Kreisform mittig anordnen (die Panel-Kacheln legen Symbol/Name in die Ecken)
static void round_center(lv_obj_t *l, int y, int w) {
  if (!l) return;
  if (w > 0) lv_obj_set_width(l, w);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(l, LV_ALIGN_CENTER, 0, y);
}
static void round_style(TileUi &t) {
  lv_obj_t *o = t.obj;
  if (!o) return;
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_clip_corner(o, true, 0);   // Fuellstand (gas/vic) wird zum Pegel im Kreis
  lv_obj_set_style_border_width(o, 0, 0);     // kein Rand: die Kachel ist der Hintergrund
  lv_obj_set_style_outline_width(o, 0, 0);
  lv_obj_set_style_outline_width(o, 0, LV_STATE_PRESSED);
  int h = t.h;
  if (t.icon) lv_obj_align(t.icon, LV_ALIGN_TOP_MID, 0, 54);
  // Zeile darunter: bis zu zwei Zeilen (erste Zeile bleibt an derselben Stelle), dann "…"
  int lh = lv_font_get_line_height(F_SMALL);
  if (t.pct) {   // Gas, Victron, Heizung, Satellit: grosser Wert in der Mitte
    round_center(t.pct, -18, 290);
    round_center(t.name, 42, 240);
    round_center(t.sub, 78 + lh / 2, 236);
  } else {       // Licht, Schalter
    round_center(t.name, -4, 250);
    round_center(t.sub, 36 + lh / 2, 236);
    if (t.track) lv_obj_set_pos(t.track, 14, h * 72 / 100);
  }
  if (t.pct) lv_label_set_long_mode(t.pct, LV_LABEL_LONG_DOT);
  if (t.name) lv_label_set_long_mode(t.name, LV_LABEL_LONG_DOT);
  if (t.sub) {
    lv_label_set_long_mode(t.sub, LV_LABEL_LONG_DOT);
    lv_obj_set_height(t.sub, 2 * lh);
  }
}
// Lange Werte/Namen passend machen (je Aktualisierung): grosser Wert bzw. Name eine Schriftgroesse kleiner,
// wenn er nicht in den Kreis passt
static void round_fit_label(lv_obj_t *l, const lv_font_t *big, const lv_font_t *small, int maxw) {
  if (!l) return;
  lv_point_t sz;
  lv_txt_get_size(&sz, lv_label_get_text(l), big, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  const lv_font_t *f = sz.x > maxw ? small : big;
  if (lv_obj_get_style_text_font(l, 0) != f) lv_obj_set_style_text_font(l, f, 0);
}
static void round_fit(TileUi &t) {
  if (t.pct) round_fit_label(t.pct, F_BIG, F_TITLE, 270);
  round_fit_label(t.name, F_TITLE, F_BODY, 236);
}


static void round_label(lv_obj_t *p, const char *t, const lv_font_t *f, lv_color_t c, int y, int w = 260) {
  lv_obj_t *l = ui_label(p, "", f, c);
  (lv_label_set_text)(l, t);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, w);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
}

static void round_info() {
  lv_obj_t *c = s_content;
  round_label(c, FW_NAME, F_BODY, C_TEXT2, 62);
  std::string ssid, url;
  if (net_connected()) {
    ssid = net_ssid().c_str();
    url = std::string("http://") + net_ip().c_str() + "/";
  } else if (net_ap_active()) {
    ssid = std::string(tr("WLAN: ")) + WiFi.softAPSSID().c_str() + "\n" + tr("Passwort: ") + g_settings.ap_pass.c_str();
    url = std::string("http://") + net_ap_ip().c_str() + "/";
  } else {
    ssid = net_status_text();
  }
  round_label(c, ssid.c_str(), F_BODY, C_TEXT, 92, 280);
  if (!url.empty()) {
    round_label(c, "Einrichten im Browser:", F_SMALL, C_TEXT2, 176);
    round_label(c, url.c_str(), F_TITLE, C_AMBER, 200, 300);
  }
#if WITH_TAILSCALE
  {
    TsStatus ts = ts_status();
    if (ts.on) {
      std::string t = ts.ip.empty() ? std::string("Tailscale: ") + tr(ts.state.c_str()) : "Tailscale: " + ts.ip;
      round_label(c, t.c_str(), F_SMALL, ts.ip.empty() ? C_TEXT3 : C_TEXT2, 236, 260);
    }
  }
  round_label(c, z2m_connected() ? "MQTT verbunden" : g_settings.mqtt_on ? "MQTT nicht verbunden" : "MQTT aus", F_SMALL,
              z2m_connected() ? C_TEXT2 : C_TEXT3, 262);
#else
  round_label(c, z2m_connected() ? "MQTT verbunden" : g_settings.mqtt_on ? "MQTT nicht verbunden" : "MQTT aus", F_SMALL,
              z2m_connected() ? C_TEXT2 : C_TEXT3, 250);
#endif
}

// ---- Datenseiten: nach oben/unten wischen zeigt die Werte des aktuellen Geraets einzeln und gross
// (dieselben Zeilen wie im Detailfenster der grossen Panels). Seite 0 = die Kachel selbst.
static int s_rd = 0;                   // aktuelle Datenseite (0 = Kachel)
static int s_rd_n = 0;                 // Anzahl Datenseiten des Geraets beim Aufbau
static bool s_rinfo = false;           // Info-Seite wird gezeigt
static std::string s_rkey;             // Geraet auf dem Bildschirm
static lv_obj_t *s_rd_lab = nullptr, *s_rd_val = nullptr;


// Punkte am rechten Rand: wo man in den Datenseiten steht (oben = Kachel)
static void round_dots(int count, int cur) {
  int sp = count > 12 ? 180 / count : 15;
  int y0 = -(count - 1) * sp / 2;
  for (int i = 0; i < count; i++) {
    lv_obj_t *d = lv_obj_create(s_content);
    ui_plain(d);
    lv_obj_clear_flag(d, LV_OBJ_FLAG_CLICKABLE);
    int sz = i == cur ? 9 : 6;
    lv_obj_set_size(d, sz, sz);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(d, i == cur ? C_AMBER : C_TEXT3, 0);
    lv_obj_align(d, LV_ALIGN_RIGHT_MID, -20 + (sz - 6) / 2, y0 + i * sp);
  }
}

static void round_data_click(lv_event_t *) {
  if (millis() - s_last_gesture < 400) return;   // Wischen ist kein Antippen
  s_rd = 0;                                       // Antippen: zurueck zur Kachel
  ui_main_rebuild();
}

static void round_data(const std::string &name, const RoundRows &rows) {
  lv_obj_t *pg = lv_obj_create(s_content);
  ui_plain(pg);
  lv_obj_set_size(pg, LCD_H_RES, LCD_V_RES);
  lv_obj_add_flag(pg, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(pg, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(pg, round_data_click, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *n = ui_label(pg, "", F_SMALL, C_TEXT2);
  (lv_label_set_text)(n, name.c_str());   // Geraetename: nicht uebersetzen
  lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
  lv_obj_set_width(n, 220);
  lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(n, LV_ALIGN_TOP_MID, 0, 62);
  s_rd_lab = ui_label(pg, "", F_BODY, C_TEXT2);
  lv_label_set_long_mode(s_rd_lab, LV_LABEL_LONG_DOT);
  lv_obj_set_width(s_rd_lab, 260);
  lv_obj_set_style_text_align(s_rd_lab, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(s_rd_lab, LV_ALIGN_CENTER, 0, -46);
  s_rd_val = ui_label(pg, "", F_BIG, C_TEXT);
  lv_label_set_long_mode(s_rd_val, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(s_rd_val, 290);
  lv_obj_set_style_text_align(s_rd_val, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(s_rd_val, LV_ALIGN_TOP_MID, 0, LCD_V_RES / 2 - 18);
  const auto &row = rows[s_rd - 1];
  (lv_label_set_text)(s_rd_lab, row.first.c_str());   // schon uebersetzt
  (lv_label_set_text)(s_rd_val, row.second.c_str());
}


// ---- Bedienseite (Dieselheizung, Kuehlbox, Power Station): Kachel antippen oeffnet sie. Drehknopf bzw. −/+ stellt
// Stufe/Solltemperatur (gesendet 0,8 s nach der letzten Aenderung), unten Ein/Aus bzw. AC/DC. Knopfdruck, Antippen
// der Flaeche oder 30 s ohne Bedienung schliesst sie; Wischen nach links/rechts wechselt wie immer das Geraet.
static std::string s_rc_key;              // Geraet mit offener Bedienseite ("" = keine)
static int s_rc_zone = 0;                 // Kuehlbox mit zwei Zonen: welche Zone (Wert antippen wechselt)
static int s_rc_want = -1000;             // eingestellter, noch nicht gesendeter Wert
static uint32_t s_rc_ms = 0, s_rc_act = 0;
static lv_obj_t *s_rc_val = nullptr, *s_rc_sub = nullptr, *s_rc_cap = nullptr, *s_rc_btn[2] = {nullptr, nullptr};

// Victron-Geraet mit Schalter/Begrenzung (MultiPlus/Ladegeraet ueber den GX, Orion ueber den GX oder per Bluetooth)
static bool rc_vic(const VictronInfo &v) {
  return !v.bms && (v.can_mode || v.can_limit) && v.kind != VIC_PSTATION && v.kind != VIC_HEATER && v.kind != VIC_FRIDGE;
}

static bool round_ctl_kind(const VictronInfo &v) {
  return v.kind == VIC_HEATER || v.kind == VIC_FRIDGE || (v.kind == VIC_PSTATION && (v.can_ac || v.can_dc)) || rc_vic(v);
}

// Einstellbarer Wert: Heizung Stufe (Modus Stufe) bzw. Soll-°C (Modus Temperatur), Kuehlbox Soll der Zone
static int rc_cur(const VictronInfo &v, int &what, int &lo, int &hi) {
  if (rc_vic(v)) {   // Begrenzung in ganzen Ampere (Orion 1–50 A, MultiPlus/Ladegeraet 0–100 A)
    what = 3, lo = v.kind == VIC_DCDC ? 1 : 0, hi = v.kind == VIC_DCDC ? 50 : 100;
    return isnan(v.ac_limit) ? -1000 : (int)lroundf(v.ac_limit);
  }
  if (v.kind == VIC_HEATER) {
    if (v.ht_mode == 2) {
      what = 2, lo = 8, hi = 36;
      return v.ht_set > 0 ? v.ht_set : 20;
    }
    what = 1, lo = 1, hi = 10;
    return v.ht_level > 0 ? v.ht_level : 5;
  }
  what = s_rc_zone + 1, lo = v.fr_min, hi = v.fr_max;
  int s = v.fr_set[s_rc_zone];
  return s > -128 ? s : 4;
}

static void rc_btn_style(lv_obj_t *b, bool on, lv_color_t accent) {
  if (!b) return;
  lv_obj_set_style_bg_color(b, on ? accent : C_FIELD, 0);
  lv_obj_t *l = lv_obj_get_child(b, 0);
  if (l) lv_obj_set_style_text_color(l, on ? C_INK : C_TEXT, 0);
}

static void rc_paint() {
  VictronInfo v;
  if (s_rc_key.empty() || !s_rc_val || !ui_vic_get(s_rc_key, v)) return;
  char b[48];
  if (v.kind == VIC_PSTATION) {
    snprintf(b, sizeof(b), "%s", isnan(v.soc) ? "–" : (std::to_string((int)lroundf(v.soc)) + "\xE2\x80\xAF%").c_str());
    (lv_label_set_text)(s_rc_val, b);
    std::string sub = tr("rein ") + ui_num(v.ps_in_w, 0, "W") + tr("  ·  raus ") + ui_num(v.ps_out_w, 0, "W");
    (lv_label_set_text)(s_rc_sub, sub.c_str());
    rc_btn_style(s_rc_btn[0], v.ac_on == 1, C_AMBER);
    rc_btn_style(s_rc_btn[1], v.dc_on == 1, C_AMBER);
    return;
  }
  if (rc_vic(v)) {
    int what, lo, hi;
    int cur = s_rc_want > -1000 ? s_rc_want : rc_cur(v, what, lo, hi);
    if (v.can_limit) snprintf(b, sizeof(b), "%s", cur > -1000 ? (std::to_string(cur) + "\xE2\x80\xAF" "A").c_str() : "…");
    else snprintf(b, sizeof(b), "%s", v.mode == 4 ? tr("Aus") : tr("Ein"));
    (lv_label_set_text)(s_rc_val, b);
    lv_label_set_text(s_rc_cap, v.can_limit ? (v.kind == VIC_DCDC ? "Ladestrom (max.)" : "Netzstrom-Begrenzung") : "Schalter");
    std::string sub = tr(victron_state_text(v.state));
    float w = victron_dc_watt(v);
    if (!isnan(w)) sub += "  ·  " + ui_num(w, 0, "W");
    (lv_label_set_text)(s_rc_sub, sub.c_str());
    lv_obj_t *l = s_rc_btn[0] ? lv_obj_get_child(s_rc_btn[0], 0) : nullptr;
    if (l) lv_label_set_text(l, v.mode == 4 ? "Einschalten" : "Ausschalten");
    rc_btn_style(s_rc_btn[0], v.mode != 4 && v.mode >= 0, C_AMBER);
    return;
  }
  bool heat = v.kind == VIC_HEATER;
  int what, lo, hi;
  int cur = s_rc_want > -1000 ? s_rc_want : rc_cur(v, what, lo, hi);
  rc_cur(v, what, lo, hi);
  if (heat && v.ht_err) snprintf(b, sizeof(b), "E%d", v.ht_err);
  else if (heat && what == 1) snprintf(b, sizeof(b), "%d", cur);
  else snprintf(b, sizeof(b), "%d\xE2\x80\xAF°C", cur);
  (lv_label_set_text)(s_rc_val, b);
  const char *cap = heat ? (v.ht_err ? "Fehler – nur Ausschalten" : what == 1 ? "Stufe" : "Solltemperatur")
                         : v.fr_zones > 1 ? (s_rc_zone ? "Soll rechts (antippen: links)" : "Soll links (antippen: rechts)") : "Solltemperatur";
  lv_label_set_text(s_rc_cap, cap);
  std::string sub;
  if (heat && !isnan(v.aux)) sub = tr("innen ") + ui_num(v.aux, 0, "°C");
  if (!heat && v.fr_cur[s_rc_zone] > -128) sub = tr("jetzt ") + std::to_string(v.fr_cur[s_rc_zone]) + "\xE2\x80\xAF°C";
  (lv_label_set_text)(s_rc_sub, sub.c_str());
  int on = heat ? v.ht_on : v.fr_on;
  lv_obj_t *l = s_rc_btn[0] ? lv_obj_get_child(s_rc_btn[0], 0) : nullptr;
  if (l) lv_label_set_text(l, on == 1 ? "Ausschalten" : "Einschalten");
  rc_btn_style(s_rc_btn[0], on == 1, heat ? C_AMBER : C_BLUE);
}

static void rc_close() {
  s_rc_key.clear();
  s_rc_want = -1000;
  ui_main_rebuild();
}

// Wert um d aendern (Drehknopf, −/+); gesendet wird gesammelt in round_ctl_tick
static void rc_step(int d) {
  VictronInfo v;
  if (!ui_vic_get(s_rc_key, v) || v.kind == VIC_PSTATION || (rc_vic(v) && !v.can_limit)) return;
  s_rc_act = millis();
  if (v.kind == VIC_HEATER && v.ht_err) {
    ui_toast("Die Heizung meldet einen Fehler – nur Ausschalten ist möglich.");
    return;
  }
  int what, lo, hi;
  int cur = s_rc_want > -1000 ? s_rc_want : rc_cur(v, what, lo, hi);
  rc_cur(v, what, lo, hi);
  if (cur <= -1000) return;   // Begrenzung noch nicht gelesen
  s_rc_want = std::max(lo, std::min(hi, cur + d));
  s_rc_ms = millis();
  rc_paint();
}

static void rc_btn_cb(lv_event_t *e) {
  if (millis() - s_last_gesture < 400) return;
  int x = (int)(intptr_t)lv_event_get_user_data(e);   // 0/1 Schalter (Ein/Aus bzw. AC/DC), 10 minus, 11 plus, 20 Zone
  VictronInfo v;
  if (!ui_vic_get(s_rc_key, v)) return;
  s_rc_act = millis();
  if (x == 10 || x == 11) {
    rc_step(x == 11 ? 1 : -1);
    return;
  }
  if (x == 20) {
    if (v.kind == VIC_FRIDGE && v.fr_zones > 1) {
      s_rc_zone ^= 1;
      s_rc_want = -1000;
      rc_paint();
    }
    return;
  }
  bool ok;
  if (rc_vic(v)) ok = v.can_mode && ui_vic_set_mode(s_rc_key, v.mode == 4 ? (v.mode_set == 0 ? 3 : 1) : 4);
  else if (v.kind == VIC_PSTATION) ok = bms_set_output(v.mac, x == 0, !((x == 0 ? v.ac_on : v.dc_on) == 1));
  else if (v.kind == VIC_HEATER) ok = bms_heater_cmd(v.mac, 0, v.ht_on == 1 ? 0 : 1);
  else ok = bms_fridge_cmd(v.mac, 0, v.fr_on == 1 ? 0 : 1);
  ui_toast(ok ? "Wird geschaltet …" : "Das ging nicht.");
}

static void rc_bg_cb(lv_event_t *) {
  if (millis() - s_last_gesture < 400) return;
  rc_close();
}

static lv_obj_t *rc_button(lv_obj_t *p, const char *txt, int w, int h, int ud, const lv_font_t *f) {
  lv_obj_t *b = lv_obj_create(p);
  ui_plain(b);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, C_FIELD, 0);
  lv_obj_set_style_outline_color(b, lv_color_white(), LV_STATE_PRESSED);
  lv_obj_set_style_outline_width(b, 2, LV_STATE_PRESSED);
  lv_obj_set_style_outline_opa(b, LV_OPA_40, LV_STATE_PRESSED);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(b, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_t *l = ui_label(b, txt, f, C_TEXT);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, rc_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ud);
  return b;
}

static void round_ctl_build(const std::string &name) {
  VictronInfo v;
  if (!ui_vic_get(s_rc_key, v)) return;
  lv_obj_t *pg = lv_obj_create(s_content);
  ui_plain(pg);
  lv_obj_set_size(pg, LCD_H_RES, LCD_V_RES);
  lv_obj_add_flag(pg, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(pg, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(pg, rc_bg_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *n = ui_label(pg, "", F_SMALL, C_TEXT2);
  (lv_label_set_text)(n, name.c_str());   // Geraetename: nicht uebersetzen
  lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
  lv_obj_set_width(n, 220);
  lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(n, LV_ALIGN_TOP_MID, 0, 56);
  s_rc_cap = ui_label(pg, "", F_SMALL, C_TEXT3);
  lv_obj_set_width(s_rc_cap, 240);
  lv_obj_set_style_text_align(s_rc_cap, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(s_rc_cap, LV_ALIGN_CENTER, 0, -62);
  s_rc_val = ui_label(pg, "", F_BIG, C_TEXT);
  lv_obj_set_style_text_align(s_rc_val, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(s_rc_val, LV_ALIGN_CENTER, 0, -20);
  s_rc_sub = ui_label(pg, "", F_SMALL, C_TEXT2);
  lv_obj_set_width(s_rc_sub, 250);
  lv_label_set_long_mode(s_rc_sub, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(s_rc_sub, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(s_rc_sub, LV_ALIGN_CENTER, 0, 22);
  s_rc_btn[0] = s_rc_btn[1] = nullptr;
  if (v.kind == VIC_PSTATION) {
    lv_label_set_text(s_rc_cap, "Ladezustand");
    if (v.can_ac) s_rc_btn[0] = rc_button(pg, "AC", 104, 56, 0, F_BODY);
    if (v.can_dc) s_rc_btn[1] = rc_button(pg, "DC", 104, 56, 1, F_BODY);
    if (s_rc_btn[0]) lv_obj_align(s_rc_btn[0], LV_ALIGN_CENTER, s_rc_btn[1] ? -58 : 0, 82);
    if (s_rc_btn[1]) lv_obj_align(s_rc_btn[1], LV_ALIGN_CENTER, s_rc_btn[0] ? 58 : 0, 82);
  } else if (rc_vic(v)) {
    if (v.can_limit) {
      lv_obj_align(rc_button(pg, "−", 60, 60, 10, F_TITLE), LV_ALIGN_CENTER, -118, -20);
      lv_obj_align(rc_button(pg, "+", 60, 60, 11, F_TITLE), LV_ALIGN_CENTER, 118, -20);
    }
    if (v.can_mode) {
      s_rc_btn[0] = rc_button(pg, "Einschalten", 190, 54, 0, F_BODY);
      lv_obj_align(s_rc_btn[0], LV_ALIGN_CENTER, 0, 82);
    }
  } else {
    lv_obj_align(rc_button(pg, "−", 60, 60, 10, F_TITLE), LV_ALIGN_CENTER, -118, -20);
    lv_obj_align(rc_button(pg, "+", 60, 60, 11, F_TITLE), LV_ALIGN_CENTER, 118, -20);
    s_rc_btn[0] = rc_button(pg, "Einschalten", 190, 54, 0, F_BODY);
    lv_obj_align(s_rc_btn[0], LV_ALIGN_CENTER, 0, 82);
    // Wert antippen: bei zwei Zonen die andere Zone
    lv_obj_add_flag(s_rc_val, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_rc_val, rc_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)20);
  }
  rc_paint();
}

// Alle 100 ms: gesammelten Wert senden, nach 30 s ohne Bedienung schliessen
static void round_ctl_tick() {
  if (s_rc_key.empty()) return;
  if (s_rc_want > -1000 && millis() - s_rc_ms > 800) {
    VictronInfo v;
    int what, lo, hi;
    bool ok = false;
    if (ui_vic_get(s_rc_key, v)) {
      rc_cur(v, what, lo, hi);
      if (rc_vic(v)) ok = ui_vic_set_limit(s_rc_key, (float)s_rc_want);
      else ok = v.kind == VIC_HEATER ? bms_heater_cmd(v.mac, what, s_rc_want) : bms_fridge_cmd(v.mac, what, s_rc_want);
    }
    if (!ok) ui_toast("Das ging nicht.");
    s_rc_want = -1000;
    s_rc_act = millis();
  }
  if (s_rc_want <= -1000 && millis() - s_rc_act > 30000) rc_close();
}

void round_ctl_open(const std::string &key) {
  VictronInfo v;
  if (!ui_vic_get(key, v) || !round_ctl_kind(v)) return;
  if (!v.gx && vctl_supported(v.record) && key.rfind(VICTRON_KEY_PREFIX, 0) == 0) vctl_refresh(v.mac);   // Stand lesen
  s_rc_key = key;
  s_rc_zone = 0;
  s_rc_want = -1000;
  s_rc_act = millis();
  s_rd = 0;
  ui_main_rebuild();
}

// Jede Sekunde: Werte der offenen Datenseite neu; andere Zeilenzahl (z. B. Geraeteart erst jetzt bekannt) = neu aufbauen
static void round_data_refresh() {
  if (s_rinfo || s_rkey.empty()) return;
  if (!s_rc_key.empty()) {
    rc_paint();
    return;
  }
  RoundRows rows = ui_tile_rows(s_rkey);
  if ((int)rows.size() != s_rd_n) {
    if (s_rd > (int)rows.size()) s_rd = 0;
    ui_main_rebuild();
    return;
  }
  if (s_rd > 0 && s_rd_val) {
    (lv_label_set_text)(s_rd_lab, rows[s_rd - 1].first.c_str());
    (lv_label_set_text)(s_rd_val, rows[s_rd - 1].second.c_str());
  }
}

void ui_main_rebuild() {
  s_lay = layout_get();
  lv_obj_clean(s_content);
  s_tiles.clear();
  s_rd_lab = s_rd_val = nullptr;
  s_rc_val = s_rc_sub = s_rc_cap = s_rc_btn[0] = s_rc_btn[1] = nullptr;
  std::vector<Tile> all;
  for (auto &p : s_lay.pages)
    for (auto &t : p.tiles) all.push_back(t);
  int total = (int)all.size() + 1;   // + Info-Seite
  s_ri = (s_ri % total + total) % total;
  s_onboard_mode = OB_NONE;
  s_rinfo = s_ri == (int)all.size();
  s_rd_n = 0;
  s_rkey.clear();
  if (s_rinfo) {
    s_rd = 0;
    round_info();
  } else {
    Tile t = all[s_ri];
    s_rkey = t.key;
    std::string name;
    RoundRows rows = ui_tile_rows(t.key, &name);
    s_rd_n = (int)rows.size();
    if (s_rd > s_rd_n || s_rd < 0) s_rd = 0;
    if (s_rc_key != t.key) s_rc_key.clear();   // Bedienseite gehoert zu einem anderen Geraet
    if (!s_rc_key.empty()) {
      if (name.empty()) name = s_lay.names.count(t.key) ? s_lay.names[t.key] : std::string();
      round_ctl_build(name);
    } else if (s_rd == 0) {
      t.x = 0;
      t.y = 0;
      t.w = R_TILE;
      t.h = R_TILE;
      create_tile(t);
      if (!s_tiles.empty()) round_style(s_tiles.back());
      tiles_update(true);
      for (auto &tu : s_tiles) round_fit(tu);
    } else {
      if (name.empty()) name = s_lay.names.count(t.key) ? s_lay.names[t.key] : std::string();
      round_data(name, rows);
    }
    if (s_rd_n && s_rc_key.empty()) round_dots(s_rd_n + 1, s_rd);
  }
  s_rpos = ui_label(s_content, "", F_SMALL, C_TEXT3);
  char b[24];
  snprintf(b, sizeof(b), "%d / %d", s_ri + 1, total);
  lv_label_set_text(s_rpos, b);
  lv_obj_align(s_rpos, LV_ALIGN_BOTTOM_MID, 0, -18);
}

// Anderes Geraet: beginnt immer auf der Kachel
static void set_page(int p) {
  s_ri = p;
  s_rd = 0;
  ui_main_rebuild();
}

// Links/rechts wischen = anderes Geraet, nach oben/unten = Datenseiten des Geraets
static void gesture_cb(lv_event_t *) {
  lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
#if BOARD_CROWPANEL146   // serieller Befehl "touch"
  if (board_touch_debug_on())
    log_printf("Geste %s, Datenseiten %d, Seite %d\n",
               d == LV_DIR_LEFT ? "links" : d == LV_DIR_RIGHT ? "rechts" : d == LV_DIR_TOP ? "hoch" : d == LV_DIR_BOTTOM ? "runter" : "?",
               s_rd_n, s_rd);
#endif
  if (d == LV_DIR_LEFT || d == LV_DIR_RIGHT) {
    s_last_gesture = millis();
    lv_indev_wait_release(lv_indev_get_act());
    set_page(s_ri + (d == LV_DIR_LEFT ? 1 : -1));
  } else if (d == LV_DIR_TOP || d == LV_DIR_BOTTOM) {
    s_last_gesture = millis();
    lv_indev_wait_release(lv_indev_get_act());
    if (!s_rd_n || !s_rc_key.empty()) return;
    int n = s_rd_n + 1;
    s_rd = (s_rd + (d == LV_DIR_TOP ? 1 : -1) + n) % n;
    ui_main_rebuild();
  }
}

// Ruhezustand des runden Panels (der Bildschirmschoner der grossen Panels ist fuer 1024x600 gebaut): nach
// "Starten nach" abdunkeln (Anzeige "Abdunkeln") bzw. Licht aus (alle anderen). Beruehrung/Drehknopf weckt.
static void round_dim_tick() {
  static uint32_t t = 0;
  if (millis() - t < 500) return;
  t = millis();
  uint16_t after = g_settings.dim_after;
  if (!board_is_dimmed() && after && lv_disp_get_inactive_time(nullptr) > (uint32_t)after * 1000)
    board_dim(g_settings.saver == SAVER_DIM ? g_settings.dim_level : 0);
}

#if UI_LEDRING
// ---- LED-Ring: Lichtpunkt beim Drehen (Stelle in der Geraeteliste), rot pulsieren bei knapper Gasflasche/Batterie
// (nur Geraete mit Kachel auf dem Panel), im Ruhezustand nur noch die Warnung, gedimmt. Kein eigener Task.
static uint32_t s_ring_knob_ms = 0;   // letzte Drehung
static bool s_ring_warn = false;

static void ring_check_warn() {
  s_ring_warn = false;
  if (!g_settings.ring_warn) return;
  int lim = g_settings.ring_warn_pct;
  for (auto &p : s_lay.pages)
    for (auto &t : p.tiles) {
      GasView g;
      if (ui_gas_is_key(t.key) && ui_gas_get(t.key, g) && g.has && g.percent <= lim) s_ring_warn = true;
      VictronInfo v;
      if (victron_is_key(t.key) && ui_vic_get(t.key, v) && v.has) {
        VicTileText tt = ui_vic_tile_text(v, false, false);   // fill = Ladezustand wie auf der Kachel, -1 = keiner
        if (!tt.stale && tt.fill >= 0 && tt.fill <= lim) s_ring_warn = true;
      }
    }
}

static void round_ring_loop() {
  static uint32_t t_frame = 0, t_warn = 0;
  uint32_t now = millis();
  if (now - t_warn > 5000) {
    t_warn = now;
    ring_check_warn();
  }
  if (now - t_frame < 40) return;
  t_frame = now;
  uint32_t c[UI_LEDRING] = {0};
  bool dim = board_is_dimmed();
  float bri = g_settings.ring_bri / 100.0f * (dim ? 0.25f : 1.0f);
  if (s_ring_warn) {
    float ph = (now % 2500) / 2500.0f;
    float lv = 0.12f + 0.88f * (0.5f - 0.5f * cosf(ph * 6.2831853f));
    uint32_t r = (uint32_t)(255 * bri * lv);
    for (int i = 0; i < UI_LEDRING; i++) c[i] = r << 16;
  }
  uint32_t since = now - s_ring_knob_ms;
  if (!dim && g_settings.ring_knob && s_ring_knob_ms && since < 2500) {
    int total = 1;
    for (auto &p : s_lay.pages) total += (int)p.tiles.size();
    int led = s_ri * UI_LEDRING / total % UI_LEDRING;
    float f = since < 2000 ? 1.0f : 1.0f - (since - 2000) / 500.0f;
    uint32_t w = (uint32_t)(255 * bri * f);
    c[led] = w << 16 | (w * 3 / 4) << 8 | (w * 3 / 10);   // warmweiss
  }
  board_ring_show(c, UI_LEDRING);
}
#endif

// Drehknopf (UI_KNOB): Geraet waehlen; Knopfdruck = Antippen der Kachel (Licht/Schalter schalten) bzw. auf einer
// Datenseite zurueck zur Kachel; lang = zur Info-Seite. Runde Displays ohne Knopf: nur Touch.
#if UI_KNOB
static void round_knob() {
  int n = board_knob_take();
  int b = board_knob_button();
  if (!n && !b) return;
  if (board_is_dimmed()) {   // im Ruhezustand weckt der Knopf nur
    board_wake();
    return;
  }
  lv_disp_trig_activity(nullptr);
  if (!s_rc_key.empty()) {   // Bedienseite: Drehen stellt ein, Druecken schliesst
    if (n) rc_step(n);
    if (b) rc_close();
    return;
  }
  if (n) {
    set_page(s_ri + n);
#if UI_LEDRING
    s_ring_knob_ms = millis();
#endif
  }
  if (b == 1 && s_rd > 0) {
    s_rd = 0;
    ui_main_rebuild();
  } else if (b == 1 && !s_tiles.empty() && s_tiles[0].obj) {
    lv_event_send(s_tiles[0].obj, LV_EVENT_SHORT_CLICKED, nullptr);
  } else if (b == 2) {
    std::vector<Tile> all;
    for (auto &p : s_lay.pages) all.insert(all.end(), p.tiles.begin(), p.tiles.end());
    set_page((int)all.size());
  }
}
#endif   // UI_KNOB
#else
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
#endif   // UI_ROUND

static void gear_cb(lv_event_t *) { ui_settings_open(SEC_WIFI); }
static void wifi_banner_cb(lv_event_t *) { ui_settings_open(SEC_WIFI); }

// Gespeichertes Camp-WLAN laenger nicht erreichbar: unten einen antippbaren Hinweis zeigen
static void wifi_banner_update() {
#if UI_ROUND
  return;
#endif
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

// ---------------------------------------------------------------- Kachel-Daten fuer Zusatzdisplays (ui_remote.cpp)
// Gleiche Texte wie die Kacheln hier, nur ohne LVGL-Objekte: Name, Icon, Wert, Zeile, Farbe, an/aus.
static uint32_t rgb_of(lv_color_t c) { return lv_color_to32(c) & 0xFFFFFF; }

std::vector<std::string> ui_tile_keys() {
  std::vector<std::string> r;
  for (auto &p : s_lay.pages)
    for (auto &t : p.tiles)
      if (std::find(r.begin(), r.end(), t.key) == r.end()) r.push_back(t.key);
  return r;
}

bool ui_tile_model(const std::string &key, TileModel &m) {
  m = TileModel();
  auto nm = s_lay.names.find(key);
  std::string own = nm != s_lay.names.end() ? nm->second : std::string();
  if (panelsat_is_point_key(key)) {
    SatPoint p;
    bool found = panelsat_point_get(key, p);
    m.name = !own.empty() ? own : found ? p.name : std::string(tr("Sensor"));
    SioText tt;
    if (found) tt = sio_text(p, false);
    else {
      tt.value = "–";
      tt.sub = tr(TXT_SAT_SILENT);
      tt.dim = true;
    }
    m.icon = tt.icon;
    m.value = tt.value;
    m.sub = tt.sub;
    m.accent = rgb_of(tt.accent);
    m.fill = tt.fill;
    m.dim = tt.dim;
    if (found && p.kind == SP_SWITCH) {
      m.act = 1;
      m.lit = p.on;
      m.sub.clear();
    }
    return true;
  }
  if (ui_gas_is_key(key)) {
    GasView g;
    bool found = ui_gas_get(key, g);
    m.name = found ? ui_gas_label(s_lay, g) : (!own.empty() ? own : std::string(tr("Gasflasche")));
    m.icon = found && g.water ? ICON_BATH : ICON_GAS;
    if (!found || !g.has) {
      m.value = found ? "…" : "–";
      m.sub = found ? g.status : std::string(tr("Sensor nicht eingerichtet"));
      m.dim = !found;
      return true;
    }
    m.value = std::to_string(g.percent) + "\xE2\x80\xAF%";
    m.sub = g.amount;
    m.fill = g.percent;
    m.accent = rgb_of(ui_gas_color(g.percent));
    return true;
  }
  if (victron_is_key(key)) {
    VictronInfo v;
    bool found = ui_vic_get(key, v);
    m.name = found ? ui_vic_label(s_lay, v) : (!own.empty() ? own : std::string(tr("Victron")));
    m.icon = ui_vic_icon(found ? v.kind : VIC_UNKNOWN);
    if (!found) {
      m.value = "–";
      m.sub = tr("nicht eingerichtet");
      m.dim = true;
      return true;
    }
    VicTileText tt = ui_vic_tile_text(v, false, false);
    m.value = tt.value;
    m.sub = tt.sub;
    m.accent = rgb_of(tt.accent);
    m.fill = tt.fill;
    m.dim = tt.stale;
    if (v.can_mode) {   // ueber den GX oder per Bluetooth schaltbar: Bedienfenster am Zusatzdisplay
      m.vset = v.mode_set == 0 ? 1 : 2;
      m.vmode = v.mode;
    }
    if (v.can_limit && !isnan(v.ac_limit)) m.vlim = (int)lroundf(v.ac_limit * 10);
    return true;
  }
  if (truma_heat_is_key(key)) {
    TrumaHeatInfo h;
    bool found = ui_heat_get(key, h);
    m.name = found ? ui_heat_label(s_lay, h) : (!own.empty() ? own : std::string(tr(TXT_HEAT)));
    m.icon = ICON_THERMO;
    m.value = found && h.has ? ui_temp_text(h.room_actual) : "–";
    m.sub = found ? ui_heat_sub(h, false) : std::string();
    m.accent = rgb_of(!found ? C_TEXT3 : h.error ? C_RED : h.running ? C_AMBER : h.heat_on ? C_TEXT : C_TEXT2);
    m.dim = !found || !h.connected;
    return true;
  }
  // Zigbee (Licht, Steckdose)
  ControlView v;
  bool found = z2m_get(key, v);
  m.name = found ? layout_label(s_lay, v) : (!own.empty() ? own : std::string("Unbekanntes Gerät"));
  std::string icon_id;
  for (auto &p : s_lay.pages)
    for (auto &t : p.tiles)
      if (t.key == key && !t.icon.empty()) icon_id = t.icon;
  // Eigene Symbole (my:…) kennen Zusatzdisplays nicht -> dort das automatische Symbol
  m.icon = icon_utf8(icon_id.empty() || icon_is_user(icon_id.c_str()) ? auto_icon(found ? &v : nullptr) : icon_id.c_str());
  if (!found) {
    m.value = "–";
    m.sub = ui_dev_missing_text(key);
    m.dim = true;
    return true;
  }
  m.act = 1;
  m.dim = !v.online;
  m.lit = v.online && v.known && v.on;
  if (v.has_bri) m.bri = v.on ? std::max(0, v.bri_pct()) : 0;
  m.color = v.color != COLOR_NONE;
  if (v.hue >= 0) m.hue = (int)lroundf(v.hue);
  if (v.sat >= 0) m.sat = (int)lroundf(v.sat);
  if (v.has_ct) {
    m.ct = v.ct > 0 ? v.ct : (v.ct_min + v.ct_max) / 2;
    m.ct_min = v.ct_min;
    m.ct_max = v.ct_max;
  }
  if (m.lit) m.lit_color = rgb_of(light_display_color(v));
  if (!v.online) m.value = tr("nicht erreichbar");
  else if (!v.known) m.value = "…";
  else if (!v.on) m.value = tr("Aus");
  else if (v.has_bri && v.bri >= 0) m.value = std::to_string(v.bri_pct()) + "\xE2\x80\xAF%";
  else m.value = tr("An");
  return true;
}

bool ui_tile_set(const std::string &key, char what, int value) {
#if WITH_CERBO
  if (victron_is_key(key)) {   // MultiPlus/Ladegeraet ueber den GX (nur was der GX als einstellbar meldet)
    VictronInfo vi;
    if (!ui_vic_get(key, vi)) return false;
    if (what == 'm' && vi.can_mode) return ui_vic_set_mode(key, value);
    if (what == 'l' && vi.can_limit) return ui_vic_set_limit(key, std::max(0, std::min(1000, value)) / 10.f);
    return false;
  }
#endif
  ControlView v;
  if (!z2m_get(key, v) || (!zbd_is_key(key) && !z2m_connected())) return false;
  switch (what) {
    case 'b':
      if (!v.has_bri) return false;
      if (value <= 0) z2m_set_on(key, false);
      else z2m_set_brightness_pct(key, std::min(100, value));
      return true;
    case 'h':
      if (v.color == COLOR_NONE) return false;
      z2m_set_color_hs(key, (float)((value / 1000) % 360), (float)std::min(100, value % 1000));
      return true;
    case 'c':
      if (!v.has_ct) return false;
      z2m_set_ct(key, std::max(v.ct_min, std::min(v.ct_max, value)));
      return true;
  }
  return false;
}

bool ui_tile_action(const std::string &key, std::string *err) {
  if (panelsat_is_point_key(key)) {
    SatPoint p;
    if (!panelsat_point_get(key, p) || p.kind != SP_SWITCH) {
      if (err) *err = tr("Kein Schalter.");
      return false;
    }
    bool ok = panelsat_switch(key, !p.on, err);
    tiles_update_one(key);
    return ok;
  }
  if (ui_gas_is_key(key) || victron_is_key(key) || truma_heat_is_key(key)) {
    if (err) *err = tr("Nur Anzeige.");
    return false;
  }
  ControlView v;
  if (!z2m_get(key, v)) {
    if (err) *err = tr("Gerät nicht gefunden.");
    return false;
  }
  if (!zbd_is_key(key) && !z2m_connected()) {   // Zigbee direkt (H2-Modul) braucht kein MQTT
    if (err) *err = tr("Keine Verbindung zum MQTT-Broker.");
    return false;
  }
  z2m_toggle(key);
  return true;
}

bool ui_tile_switch(const std::string &key, bool on, std::string *err) {
  if (panelsat_is_point_key(key)) {
    SatPoint p;
    if (!panelsat_point_get(key, p) || p.kind != SP_SWITCH) {
      if (err) *err = tr("Kein Schalter.");
      return false;
    }
    bool ok = panelsat_switch(key, on, err);
    tiles_update_one(key);
    return ok;
  }
  if (ui_gas_is_key(key) || victron_is_key(key) || truma_heat_is_key(key)) {
    if (err) *err = tr("Nur Anzeige.");
    return false;
  }
  ControlView v;
  if (!z2m_get(key, v)) {
    if (err) *err = tr("Gerät nicht gefunden.");
    return false;
  }
  if (!zbd_is_key(key) && !z2m_connected()) {   // Zigbee direkt (H2-Modul) braucht kein MQTT
    if (err) *err = tr("Keine Verbindung zum MQTT-Broker.");
    return false;
  }
  z2m_set_on(key, on);   // Zigbee2MQTT, Shelly, WLED: fester Befehl ON/OFF
  return true;
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
#if UI_ROUND
  lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);   // keine Leiste: der runde Bildschirm zeigt nur Kacheln
  lv_obj_set_style_bg_color(s_scr, C_TILE, 0);   // gleiche Farbe wie eine ausgeschaltete Kachel: alles eine Flaeche
  lv_obj_set_pos(s_content, 0, 0);
  lv_obj_set_size(s_content, LCD_H_RES, LCD_V_RES);
#else
  lv_obj_set_pos(s_content, 0, UI_TOPBAR_H);
  lv_obj_set_size(s_content, LCD_H_RES, UI_CONTENT_H);
#endif
  lv_obj_add_flag(s_content, LV_OBJ_FLAG_CLICKABLE);
#if UI_ROUND
  // LVGL reicht Gesten an das Elternobjekt weiter (GESTURE_BUBBLE ist Standard), sie kommen also beim Bildschirm an
  lv_obj_add_event_cb(s_scr, gesture_cb, LV_EVENT_GESTURE, nullptr);
#else
  lv_obj_add_event_cb(s_content, gesture_cb, LV_EVENT_GESTURE, nullptr);
#endif

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
#if UI_ROUND
  lv_obj_add_flag(s_wifi_banner, LV_OBJ_FLAG_HIDDEN);
#endif

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

void ui_icons_changed() {
#if WITH_USER_ICONS
  user_icons_reload();   // im UI-Task: niemand zeichnet gerade mit den alten Masken
  ui_main_rebuild();
#endif
}

#if SOC_VICTRON_EDITION
// Wie das kleine Victron-Display: Geraete, die Werte liefern und noch keine Kachel haben, bekommen automatisch eine
// (hinten angefuegt). Entfernt der Nutzer eine Kachel, kommt sie nicht wieder (gemerkt bis zum Neustart).
static void edition_auto_tiles() {
  static uint32_t last = 0;
  static std::vector<std::string> seen;   // schon einmal angelegt
  if (millis() - last < 10000) return;
  last = millis();
  std::vector<ControlView> add;
  auto want = [&](const std::string &key, const std::string &name) {
    if (std::find(seen.begin(), seen.end(), key) != seen.end()) return;
    seen.push_back(key);
    for (auto &p : s_lay.pages)
      for (auto &t : p.tiles)
        if (t.key == key) return;
    ControlView v;
    v.key = key;
    v.name = name;
    add.push_back(v);
  };
  for (auto &g : ui_gas_list())
    if (g.has) want(g.key, g.name);
  for (auto &x : victron_list())
    if (x.has) want(x.key, x.name);
#if WITH_CERBO
  for (auto &x : cerbo_list())
    if (x.has) want(x.key, x.name);
#endif
#if WITH_BMS
  for (auto &b : bms_list())
    if (b.has) want(b.key, b.name);
#endif
  if (!add.empty()) layout_auto_arrange(add, true);
}
#endif

#if WITH_ZB_DIRECT
// Zigbee direkt: neu angelernte Lampen/Steckdosen/Schalter bekommen sofort eine Kachel. Jeder Kanal nur einmal
// (NVS "zbtiles") – entfernt der Nutzer die Kachel, bleibt sie weg.
static void zb_auto_tiles() {
  static uint32_t last = 0, ver = 0;
  if (millis() - last < 3000) return;
  last = millis();
  uint32_t v = zbd_devices_version();
  if (v == ver) return;
  ver = v;
  std::vector<ControlView> all, add;
  zbd_all(all);
  if (all.empty()) return;
  Preferences p;
  if (!p.begin("zbtiles", false)) return;
  for (auto &c : all) {
    uint32_t h = 2166136261u;
    for (char ch : c.key) h = (h ^ (uint8_t)ch) * 16777619u;
    char nk[12];
    snprintf(nk, sizeof(nk), "%08x", (unsigned)h);
    if (p.isKey(nk)) continue;
    p.putBool(nk, true);
    bool placed = false;
    for (auto &pg : s_lay.pages)
      for (auto &t : pg.tiles) placed |= t.key == c.key;
    if (!placed) add.push_back(c);
  }
  p.end();
  if (add.empty()) return;
  layout_auto_arrange(add, true);
  ui_toast(add.size() == 1 ? "Neues Zigbee-Gerät – Kachel angelegt." : "Neue Zigbee-Geräte – Kacheln angelegt.");
}
#endif

void ui_loop() {
  ui_live_tick();   // Schaltbefehle der Live-Ansicht (Browser/App)
  ha_link_tick();   // Home Assistant: Katalog, Werte, Befehle
#if WITH_ZB_DIRECT
  zb_auto_tiles();
#endif
#if SOC_VICTRON_EDITION
  edition_auto_tiles();
#endif
  static uint32_t t_fast = 0, t_slow = 0;
  uint32_t now = millis();
#if UI_ROUND
#if UI_KNOB
  round_knob();
#endif
  round_ctl_tick();
  round_dim_tick();
#if UI_LEDRING
  round_ring_loop();
#endif
#else
  ui_saver_tick();   // Bildschirmschoner ist fuer 1024x600 gebaut
#endif
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
      ui_settings_tick_hobby();
      ui_settings_tick_votronic();
      ui_settings_tick_hymer();                // Status, Kopplung und Werte von Hymer Connect
      tiles_update(false);
      ui_gas_update();
      ui_vic_update();
      ui_settings_tick_sat();
    }
    char msg[96];
    if (app_pop_toast(msg, sizeof(msg))) ui_toast(msg);
    ui_remote_tick();   // Zusatzdisplays: Kacheln senden, Befehle annehmen
  }
  if (now - t_slow >= 1000) {
    t_slow = now;
    status_update();
    static std::string last_status;
    bool router_wait_changed = s_onboard_mode == OB_ROUTER_WAIT && last_status != z2m_status_text();
    last_status = z2m_status_text();
#if UI_ROUND
    (void)router_wait_changed;
    tiles_update(false);
    for (auto &tu : s_tiles) round_fit(tu);
    round_data_refresh();
    static uint32_t t_info = 0;   // Info-Seite: Verbindung alle 3 s neu
    if (s_rinfo && now - t_info > 3000) {
      t_info = now;
      ui_main_rebuild();
    }
#else
    if (onboard_mode() != s_onboard_mode || router_wait_changed) ui_main_rebuild();
    if (s_onboard_mode == OB_NONE) tiles_update(false);
#endif
    ui_settings_tick();
  }
}

// ---------------------------------------------------------------- fehlende Geraete
// Nur Geraete, die wirklich aus Zigbee2MQTT kommen, heissen "nicht in Zigbee2MQTT". Eingebautes Zigbee (H2-Modul),
// Shelly (MQTT), Shelly per Bluetooth und WLED bekommen eigene Texte.
static bool from_z2m(const std::string &key) {
  return !zbd_is_key(key) && !shelly_is_key(key) && !shb_is_key(key) && !wled_is_key(key);
}

const char *ui_dev_missing_text(const std::string &key) {
  if (zbd_is_key(key)) return zbd_info().present ? tr("nicht verfügbar") : ZBD_OFFLINE_TXT;
  if (from_z2m(key)) return z2m_count() ? TXT_DEV_MISSING : "…";
  return tr("nicht verfügbar");
}

const char *ui_dev_gone_text(const std::string &key) {
  if (from_z2m(key)) return TXT_DEV_GONE;
  if (zbd_is_key(key) && !zbd_info().present) return ZBD_OFFLINE_TXT;
  return tr("Dieses Gerät ist gerade nicht verfügbar.");
}
