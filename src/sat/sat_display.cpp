// Anzeige fuer Satelliten mit Display – siehe sat_display.h
// Optik wie das Hauptpanel: LVGL, gleiche Farben (ui/theme.h), Schriften Inter 16/20 und Symbolschrift.
#include "sat_display.h"

#if SAT_DISPLAY
#include <Arduino.h>
#include <WiFi.h>
#include <math.h>
#include <algorithm>
#include <string>
#include <vector>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "esp_heap_caps.h"
#include <Preferences.h>
#if SAT_VICTRON_ONLY   // eigene Firmware "Victron-Display" (src/vicdisp/): Cerbo GX, Victron-Bluetooth, Truma LevelControl
#include "../vicdisp/vd_main.h"
#else
#include "sat_config.h"
#include "sat_io.h"
#include "../mopeka.h"
#endif
#include "../truma.h"
#include "../victron.h"
#include "../bms.h"
#if WITH_CERBO
#include "../cerbo.h"
#endif
#include "../fonts/fonts.h"
#include "../fonts/icons.h"
#include "../ui/theme.h"
#if SAT_SPLASH   // Startbild 3 s (tools/embed_splash_cyd.py, Bild assets/splash_cyd.jpg 320x240)
#include "../generated/splash_cyd_jpg.h"
#endif

// ---------------------------------------------------------------- Hardware: ESP32-2432S028R
// Display ILI9341 an HSPI (SCK 14, MOSI 13, MISO 12, CS 15, DC 2), Licht GPIO 21.
// Touch XPT2046 an eigenem SPI (CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36). RGB-LED 4/16/17 (Low = an).
#ifndef SAT_TOUCH_XMIN
#define SAT_TOUCH_XMIN 300    // Rohwerte des Touch-Controllers an den Raendern – bei Bedarf nachstellen
#define SAT_TOUCH_XMAX 3900
// Y vertauscht (2026-11-13 am echten Board): quer liegt die Waagerechte auf der Y-Achse des Touch-Controllers,
// links/rechts waren gespiegelt
#define SAT_TOUCH_YMIN 3700
#define SAT_TOUCH_YMAX 200
#endif
#ifndef SAT_DISPLAY_ROTATION
#define SAT_DISPLAY_ROTATION 1   // quer
#endif

namespace {
class CydDisplay : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
  lgfx::Touch_XPT2046 touch_;

 public:
  CydDisplay() {
    {
      auto c = bus_.config();
      c.spi_host = SPI2_HOST;   // HSPI
      c.spi_mode = 0;
      c.freq_write = 40000000;
      c.freq_read = 16000000;
      c.spi_3wire = false;
      c.use_lock = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = 14;
      c.pin_mosi = 13;
      c.pin_miso = 12;
      c.pin_dc = 2;
      bus_.config(c);
      panel_.setBus(&bus_);
    }
    {
      auto c = panel_.config();
      c.pin_cs = 15;
      c.pin_rst = -1;
      c.pin_busy = -1;
      c.panel_width = 240;
      c.panel_height = 320;
      c.offset_rotation = 0;
      c.readable = true;
      c.invert = false;
      c.rgb_order = false;
      c.dlen_16bit = false;
      c.bus_shared = false;
      panel_.config(c);
    }
    {
      auto c = light_.config();
      c.pin_bl = 21;
      c.invert = false;
      c.freq = 12000;
      c.pwm_channel = 7;
      light_.config(c);
      panel_.setLight(&light_);
    }
    {
      auto c = touch_.config();
      c.x_min = SAT_TOUCH_XMIN;
      c.x_max = SAT_TOUCH_XMAX;
      c.y_min = SAT_TOUCH_YMIN;
      c.y_max = SAT_TOUCH_YMAX;
      c.pin_int = 36;
      c.bus_shared = false;
      c.offset_rotation = 0;
      c.spi_host = SPI3_HOST;   // VSPI
      c.freq = 1000000;
      c.pin_sclk = 25;
      c.pin_mosi = 32;
      c.pin_miso = 39;
      c.pin_cs = 33;
      touch_.config(c);
      panel_.setTouch(&touch_);
    }
    setPanel(&panel_);
  }
};

static CydDisplay s_lcd;
static const int W = 320, H = 240;
static const int BUF_LINES = 20;   // Zeichenpuffer 320 x 20 Pixel (12,5 KB intern, DMA-faehig)
static lv_disp_draw_buf_t s_buf;
static lv_disp_drv_t s_disp_drv;
static lv_indev_drv_t s_indev_drv;
static uint32_t s_last_touch = 0;
static bool s_dim = false;
static const uint8_t DIMMED = 12;
// Einstellungen (Weboberflaeche, NVS "satdisp")
static int s_per = 4;            // Kacheln pro Seite fuer Seiten ohne eigene Angabe: 4 (2x2), 2 (gross) oder 1 (ganz)
static std::vector<uint8_t> s_pl; // Kacheln je Seite (Seite 1, 2, …) aus dem Editor; danach gilt s_per
static int s_lay = 0;            // Aufteilung, fuer die die Kacheln gerade angeordnet sind
static uint8_t s_bright = 200;   // Helligkeit 20..255
static uint16_t s_dim_min = 2;   // nach so vielen Minuten ohne Beruehrung dimmen, 0 = nie

// Lichtsensor vorne neben dem Display (LDR an GPIO 34, nur Analogeingang): Helligkeit hell/dunkel umschalten.
// Groesserer Messwert = dunkler. Geglaettet, mit Hysterese (zurueck auf hell unter 75 % der Grenze) und 3 s
// Bestaetigung, damit ein Schatten nicht flackert. Kein eigener Task, gelesen alle 0,5 s aus sat_display_loop().
#ifndef SAT_LDR_PIN
#define SAT_LDR_PIN 34
#endif
static bool s_auto = true;          // Helligkeit automatisch (ab Werk an)
static uint8_t s_night = 40;        // Helligkeit bei Dunkelheit
static uint16_t s_ldr_thr = 300;    // ab diesem Messwert ist es dunkel
static float s_ldr = -1;            // geglaetteter Messwert, -1 = noch keiner
static bool s_night_on = false;     // Sensor meldet dunkel
static uint32_t s_ldr_ms = 0, s_ldr_chg_ms = 0;

static uint8_t cur_bright() { return s_auto && s_night_on ? s_night : s_bright; }
struct TileCfg {
  std::string k, n;   // Schluessel ("p:tank1", "t:<mac>", "m:<mac>", "v:<mac>"), eigener Name
  bool on = true;
  uint8_t pg = 255;   // feste Seite (0 = Seite 1) aus dem Editor, 255 = automatisch (hinten anhaengen)
};
static std::vector<TileCfg> s_cfg;

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px) {
  uint32_t w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
  s_lcd.startWrite();
  s_lcd.setAddrWindow(a->x1, a->y1, w, h);
  s_lcd.writePixels((lgfx::rgb565_t *)px, w * h);
  s_lcd.endWrite();
  lv_disp_flush_ready(drv);
}

static void touch_cb(lv_indev_drv_t *, lv_indev_data_t *data) {
  lgfx::touch_point_t tp;
  bool down = s_lcd.getTouch(&tp) > 0;
  if (down) {
    s_last_touch = millis();
    if (s_dim) {   // erste Beruehrung weckt nur (nicht an LVGL weitergeben)
      s_dim = false;
      s_lcd.setBrightness(cur_bright());
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }
  }
  data->state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  if (down) {
    data->point.x = tp.x;
    data->point.y = tp.y;
  }
}

// ---------------------------------------------------------------- Inhalt
struct Item {
  std::string key, name, value, icon;
  lv_color_t accent;
  int sw = -1;     // Schalter: Punktnummer (idx) fuer sat_io_switch, sonst -1
  bool on = false;
  std::string ctl;   // Cerbo-Schluessel, wenn ueber den GX einstellbar (Bedienfenster beim Antippen)
};

#if SAT_VICTRON_ONLY
static const int MAX_TILES = 4, TOP = 8, GAP = 8, DOTS_H = 16;   // ohne Kopfzeile: nur Kacheln
#else
static const int MAX_TILES = 4, TOP = 30, GAP = 8, DOTS_H = 16;
#endif

struct Tile {
  lv_obj_t *box, *icon, *name, *value, *sub;   // sub = zweiter Wert (Zeile darunter, kleiner)
};
static Tile s_tiles[MAX_TILES];
static lv_obj_t *s_title, *s_net, *s_dots;
static std::vector<Item> s_items;
static int s_page = 0;
static uint32_t s_next_ms = 0;

static std::string num(float v, int dec, const char *unit) {
  if (isnan(v)) return "–";
  char b[32];
  snprintf(b, sizeof(b), "%.*f %s", dec, v, unit);
  for (char *p = b; *p; p++)
    if (*p == '.') *p = ',';
  return b;
}

#if !SAT_VICTRON_ONLY
static const char *point_icon(const SatPoint &p) {
  switch (p.kind) {
    case SP_TANK: return ICON_BATH;
    case SP_TEMP: return ICON_THERMO;
    case SP_INPUT: return ICON_DOOR;
    case SP_SWITCH: return ICON_POWER;
    case SP_TILT: return ICON_HOUSE;
    default: break;
  }
  switch (p.cls) {
    case SC_TEMP: return ICON_THERMO;
    case SC_VOLTAGE: return ICON_BATTERY;
    case SC_CURRENT: return ICON_LIGHTNING;
    case SC_LEVEL: return ICON_BATH;
    case SC_ONOFF: return ICON_PLUG;
    default: return ICON_INFO;
  }
}
#endif

// Victron-Werte als Kachel – Bluetooth-Geraete ("v:<mac>") und Cerbo GX ("c:cerbo:<id>")
static Item vic_item(const VictronInfo &v, const std::string &key) {
  Item it;
  it.key = key;
  it.name = v.name.empty() ? std::string(victron_kind_text(v.kind)) : v.name;
  it.accent = v.has ? C_BLUE : C_TEXT3;
  if (v.gx && (v.can_limit || v.can_mode)) it.ctl = v.key;
  switch (v.kind) {
    case VIC_SOLAR: it.icon = ICON_SUN; break;
    case VIC_SHUNT: it.icon = ICON_BATTERY; break;
    case VIC_CHARGER: it.icon = ICON_LIGHTNING; break;
    case VIC_GRID: it.icon = ICON_PLUG; break;
    case VIC_INVERTER: it.icon = ICON_POWER; break;
    case VIC_PSTATION: it.icon = ICON_BATTERY; break;
    case VIC_LITHIUM: it.icon = ICON_BATTERY; break;
    case VIC_PROTECT: it.icon = ICON_POWER; break;
    case VIC_DCSYS: it.icon = ICON_BULB; break;
    default: it.icon = ICON_LIGHTNING; break;
  }
  if (!v.has) {
    it.value = v.status.empty() ? "–" : v.status;
    return it;
  }
  switch (v.kind) {
    case VIC_SOLAR:
      it.value = num(v.pv_w, 0, "W") + (isnan(v.yield_kwh) ? std::string("") : "  ·  " + num(v.yield_kwh, 2, "kWh"));
      it.accent = (!isnan(v.pv_w) && v.pv_w > 5) ? C_AMBER : C_TEXT2;
      break;
    case VIC_SHUNT: {
      if (victron_is_sense(v)) {   // Smart Battery Sense: Spannung und Temperatur
        it.value = num(v.bat_v, 2, "V") + (v.aux_mode == 2 && !isnan(v.aux) ? "  ·  " + num(v.aux, 0, "°C") : std::string());
        it.accent = C_BLUE;
        break;
      }
      float w = victron_dc_watt(v);
      it.value = num(v.soc, 0, "%") + "  ·  " + num(v.bat_v, 1, "V") + (isnan(w) ? std::string() : "  ·  " + num(w, 0, "W"));
      it.accent = (!isnan(v.soc) && v.soc < 20) ? C_RED : C_GREEN;
      break;
    }
    case VIC_CHARGER:
      it.value = num(v.power_w, 0, "W") + "  ·  " + victron_state_text(v.state);
      break;
    case VIC_GRID:
      it.value = v.ac_connected == 0 ? std::string("nicht verbunden") : num(v.ac_w, 0, "W") + "  ·  " + num(v.ac_v, 0, "V");
      it.accent = v.ac_connected == 0 ? C_TEXT3 : C_BLUE;
      break;
    case VIC_INVERTER:
      it.value = num(v.ac_w, 0, "W") + "  ·  " + victron_state_text(v.state);
      it.accent = (!isnan(v.ac_w) && v.ac_w > 5) ? C_AMBER : C_TEXT2;
      break;
    case VIC_PSTATION:
      it.value = num(v.soc, 0, "%") + "  ·  raus " + num(v.ps_out_w, 0, "W");
      it.accent = (!isnan(v.soc) && v.soc < 20) ? C_RED : C_GREEN;
      break;
    case VIC_DCDC: {   // Booster: Leistung gross, darunter Strom und Spannung (ohne Strom: Spannung und Zustand)
      float w = victron_dc_watt(v);
      if (!isnan(w))
        it.value = num(w, 0, "W") + "  ·  " + num(v.bat_i, 1, "A") + "  ·  " + num(v.bat_v, 1, "V");
      else
        it.value = isnan(v.bat_i) ? num(v.bat_v, 2, "V") + "  ·  " + victron_state_text(v.state)
                                  : num(v.bat_i, 1, "A") + "  ·  " + num(v.bat_v, 1, "V");
      it.accent = (!isnan(v.bat_i) && v.bat_i > 0.2f) ? C_GREEN : C_TEXT2;
      break;
    }
    case VIC_LITHIUM: {   // Spannung gross, darunter Temperatur und Zellen (niedrigste–hoechste)
      float lo = NAN, hi = NAN;
      for (float c : v.cell_v)
        if (!isnan(c)) {
          lo = isnan(lo) || c < lo ? c : lo;
          hi = isnan(hi) || c > hi ? c : hi;
        }
      it.value = num(v.bat_v, 2, "V");
      if (v.aux_mode == 2 && !isnan(v.aux)) it.value += "  ·  " + num(v.aux, 0, "°C");
      if (!isnan(lo)) {
        std::string l = num(lo, 2, "");
        l.pop_back();   // Leerzeichen vor der (leeren) Einheit
        it.value += "  ·  Zellen " + l + "–" + num(hi, 2, "V");
      }
      it.accent = v.alarm ? C_RED : C_GREEN;
      break;
    }
    case VIC_PROTECT:
      it.value = std::string(v.output == 1 ? "Ausgang an" : v.output == 0 ? "Ausgang aus" : "–") + "  ·  " + num(v.in_v, 2, "V");
      it.accent = v.alarm || v.error > 0 ? C_RED : v.output == 1 ? C_GREEN : C_AMBER;
      break;
    case VIC_DCSYS:
      it.value = num(v.power_w, 0, "W") + "  ·  " + num(v.bat_i, 1, "A");
      it.accent = (!isnan(v.power_w) && fabsf(v.power_w) > 2) ? C_AMBER : C_TEXT2;
      break;
    default:
      it.value = num(v.bat_v, 2, "V");
      break;
  }
  return it;
}

// Liefert das Geraet wirklich etwas? (Der GX meldet manche Werte als null, Bluetooth-Geraete ohne Paket/Schluessel)
static bool vic_has_value(const VictronInfo &v) {
  if (!v.has) return false;
  if (!v.gx) return true;
  switch (v.kind) {
    case VIC_SOLAR: return !isnan(v.pv_w);
    case VIC_SHUNT: return !isnan(v.soc) || !isnan(v.bat_v);
    case VIC_CHARGER: return !isnan(v.power_w) || !isnan(v.bat_i) || v.state >= 0;
    case VIC_GRID: return !isnan(v.ac_w) || v.ac_connected >= 0;
    case VIC_INVERTER: return !isnan(v.ac_w) || v.state >= 0;
    case VIC_DCSYS: return !isnan(v.power_w);
    default: return !isnan(v.bat_v);
  }
}

static void collect_raw(std::vector<Item> &s_items) {
  s_items.clear();
#if WITH_CERBO
  for (auto &v : cerbo_list()) {
#if SAT_VICTRON_ONLY
    if (!vic_has_value(v)) continue;   // keine leeren Kacheln
#endif
    s_items.push_back(vic_item(v, "c:" + v.key));
  }
#endif
#if !SAT_VICTRON_ONLY
  // Eigene Anschluesse (Tanks, Temperatur, Analog, Eingaenge, Schalter) und Hymer-Punkte
  for (auto &p : sat_io_points()) {
    Item it;
    it.key = "p:" + p.id;
    it.name = p.name;
    it.icon = point_icon(p);
    it.accent = p.ok ? C_BLUE : C_TEXT3;
    if (p.kind == SP_SWITCH) {
      it.sw = p.idx;
      it.on = p.on;
      it.value = p.on ? "An" : "Aus";
      it.accent = C_AMBER;
    } else if (p.kind == SP_INPUT) {
      it.value = p.cls == SC_BINARY ? (p.on ? "offen" : "zu") : (p.on ? "An" : "Aus");
      it.accent = p.on ? C_AMBER : C_TEXT3;
    } else if (!p.ok) {
      it.value = p.status.empty() ? "–" : p.status;
    } else {
      int dec = p.kind == SP_TANK || p.cls == SC_LEVEL ? 0 : 1;
      it.value = num(p.value, dec, p.unit.c_str());
      if (p.kind == SP_TANK) it.accent = p.value < 15 ? C_RED : C_BLUE;
    }
    s_items.push_back(it);
  }
#endif
#if WITH_BMS
  // Akkus mit eigenem BMS (Super B …): erst mit Werten eine Kachel
  for (auto &b : bms_list()) {
    if (!b.has) continue;
    if (b.dev == BMS_DEV_SOLAR || b.dev == BMS_DEV_DCDC || b.dev == BMS_DEV_INVERTER || b.dev == BMS_DEV_PSTATION) {
      VictronInfo v;
      victron_from_bms(b, v);
      s_items.push_back(vic_item(v, "b:" + b.mac));
      continue;
    }
    Item it;
    it.key = "b:" + b.mac;
    it.name = b.name.empty() ? "Akku" : b.name;
    it.icon = ICON_BATTERY;
    std::string second = num(b.volt, 1, "V");
    if (isfinite(b.volt) && isfinite(b.amp)) second += "  ·  " + num(b.volt * b.amp, 0, "W");
    it.value = std::to_string(b.soc) + " %  ·  " + second;
    bool stale = !b.connected || b.age_s > 120;
    it.accent = stale ? C_TEXT3 : (!b.ok || b.soc <= 10) ? C_RED : b.soc <= 50 ? C_AMBER : C_GREEN;
    s_items.push_back(it);
  }
#endif
  for (auto &t : truma_list()) {
#if SAT_VICTRON_ONLY
    if (!t.has_reading) continue;   // keine leeren Kacheln
#endif
    Item it;
    it.key = "t:" + t.mac;
    it.name = t.name.empty() ? "Gasflasche" : t.name;
    it.icon = ICON_GAS;
    if (t.has_reading) {
      it.value = std::to_string(t.percent) + " %  ·  " + num(t.kg, 1, "kg");
      it.accent = t.percent < 15 ? C_RED : C_GREEN;
    } else {
      it.value = t.status.empty() ? "–" : t.status;
      it.accent = C_TEXT3;
    }
    s_items.push_back(it);
  }
#if !SAT_VICTRON_ONLY
  for (auto &m : mopeka_list()) {
    Item it;
    it.key = "m:" + m.mac;
    it.name = m.name.empty() ? "Mopeka" : m.name;
    it.icon = m.medium == MOP_PROPANE ? ICON_GAS : ICON_BATH;
    if (m.has) {
      it.value = std::to_string(m.percent) + " %";
      it.accent = m.percent < 15 ? C_RED : C_GREEN;
    } else {
      it.value = m.status.empty() ? "–" : m.status;
      it.accent = C_TEXT3;
    }
    s_items.push_back(it);
  }
#endif
  for (auto &v : victron_list()) {
#if SAT_VICTRON_ONLY
    if (!vic_has_value(v)) continue;   // eingerichtet, aber (noch) nichts empfangen: keine Kachel
#endif
    s_items.push_back(vic_item(v, "v:" + v.mac));
  }
}

static int page_cnt(int p);
// Anzeige-Liste: Seiten, Reihenfolge, Auswahl und Namen aus den Einstellungen. Kacheln mit fester Seite (Editor)
// stehen auf ihrer Seite, freie Plaetze bleiben leer (Item ohne Schluessel). Neue Geraete und Kacheln ohne feste
// Seite kommen dahinter (sichtbar).
static void collect() {
  std::vector<Item> raw;
  collect_raw(raw);
  s_items.clear();
  std::vector<bool> used(raw.size(), false);
  std::vector<std::vector<Item>> fixed;
  std::vector<Item> autos;
  for (auto &c : s_cfg)
    for (size_t i = 0; i < raw.size(); i++)
      if (!used[i] && raw[i].key == c.k) {
        used[i] = true;
        if (c.on) {
          Item it = raw[i];
          if (!c.n.empty()) it.name = c.n;
          if (c.pg < 16) {
            if (fixed.size() <= c.pg) fixed.resize(c.pg + 1);
            fixed[c.pg].push_back(it);
          } else {
            autos.push_back(it);
          }
        }
        break;
      }
  for (size_t i = 0; i < raw.size(); i++)
    if (!used[i]) autos.push_back(raw[i]);
  for (size_t p = 0; p < fixed.size(); p++) {
    size_t n = page_cnt(p);
    for (size_t j = 0; j < n; j++) s_items.push_back(j < fixed[p].size() ? fixed[p][j] : Item());   // Item() = Luecke
    for (size_t j = n; j < fixed[p].size(); j++) autos.push_back(fixed[p][j]);   // passt nicht mehr auf die Seite
  }
  for (auto &a : autos) s_items.push_back(a);
  while (!s_items.empty() && s_items.back().key.empty()) s_items.pop_back();
}

static int norm_per(int v) { return v == 1 ? 1 : v == 2 ? 2 : 4; }
static int page_cnt(int p) { return p < (int)s_pl.size() ? s_pl[p] : s_per; }
static size_t page_start(int p) {
  size_t st = 0;
  for (int i = 0; i < p; i++) st += page_cnt(i);
  return st;
}
static int pages() {
  if (s_items.empty()) return 1;
  int p = 0;
  for (size_t st = 0; st < s_items.size(); p++) st += page_cnt(p);
  return p;
}

#if WITH_CERBO
// ---------------------------------------------------------------- Bedienfenster MultiPlus/Ladegeraet (ueber den GX)
// Antippen einer Kachel mit Einstellmoeglichkeit (Netz, Ladegeraet, Wechselrichter) oeffnet es:
// Eingangsstrom-Begrenzung (- / +, ganze Ampere) und Schalter. Nur, was der GX als einstellbar meldet.
// Die LVGL-Objekte entstehen erst beim Oeffnen und werden beim Schliessen wieder geloescht.
static lv_obj_t *s_pop = nullptr, *s_pop_lim = nullptr;
static lv_obj_t *s_pop_mbtn[4];
static int s_pop_mval[4], s_pop_nm = 0;
static std::string s_pop_key;
static float s_pop_val = NAN;
static uint32_t s_pop_send_ms = 0, s_pop_edit_ms = 0;

static void pop_flush() {
  if (!s_pop_send_ms) return;
  s_pop_send_ms = 0;
  cerbo_set_limit(s_pop_key, s_pop_val);
}

static void pop_show_limit() {
  if (!s_pop_lim) return;
  if (isnan(s_pop_val)) {
    lv_label_set_text(s_pop_lim, "–");
    return;
  }
  lv_label_set_text(s_pop_lim, num(s_pop_val, 1, "A").c_str());
}

static void pop_show_mode(int mode) {
  for (int i = 0; i < s_pop_nm; i++) lv_obj_set_style_bg_color(s_pop_mbtn[i], s_pop_mval[i] == mode ? C_BLUE : C_FIELD, 0);
}

static void pop_close_cb(lv_event_t *) {
  pop_flush();
  if (s_pop) lv_obj_del_async(s_pop);   // aus dem eigenen Ereignis heraus nur verzoegert loeschen
  s_pop = s_pop_lim = nullptr;
  s_pop_nm = 0;
  s_next_ms = 0;
}

static void pop_step_cb(lv_event_t *e) {
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  if (isnan(s_pop_val)) return;
  float v = roundf(s_pop_val) + d;
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  s_pop_val = v;
  pop_show_limit();
  s_pop_edit_ms = millis();
  s_pop_send_ms = (millis() + 800) | 1;   // erst nach kurzer Pause senden (mehrfach tippen = ein Befehl)
}

static void pop_mode_cb(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i < 0 || i >= s_pop_nm) return;
  if (cerbo_set_mode(s_pop_key, s_pop_mval[i])) pop_show_mode(s_pop_mval[i]);
  s_pop_edit_ms = millis();
}

static lv_obj_t *pop_label(lv_obj_t *parent, const char *txt, const lv_font_t *font, lv_color_t col) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, col, 0);
  lv_label_set_text(l, txt);
  return l;
}

static lv_obj_t *pop_btn(const char *txt, int x, int y, int w, int h, lv_event_cb_t cb, intptr_t ud) {
  lv_obj_t *b = lv_obj_create(s_pop);
  lv_obj_remove_style_all(b);
  lv_obj_set_style_radius(b, 10, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, C_FIELD, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, C_TILE_LINE, 0);
  lv_obj_set_style_border_color(b, C_BLUE, LV_STATE_PRESSED);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)ud);
  lv_obj_center(pop_label(b, txt, &font_ui_16, C_TEXT));
  return b;
}

static void pop_open(const std::string &key) {
  VictronInfo v;
  if (s_pop || !cerbo_get(key, v) || (!v.can_limit && !v.can_mode)) return;
  s_pop_key = key;
  s_pop_val = v.ac_limit;
  s_pop_send_ms = s_pop_edit_ms = 0;
  s_pop = lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(s_pop);
  lv_obj_set_size(s_pop, W, H);
  lv_obj_set_style_bg_opa(s_pop, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(s_pop, C_BG, 0);
  lv_obj_clear_flag(s_pop, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_pop, LV_OBJ_FLAG_CLICKABLE);   // Kacheln darunter nicht antippbar
  lv_obj_set_pos(pop_label(s_pop, v.mode_set == 0 ? "MultiPlus" : v.name.c_str(), &font_ui_20, C_TEXT), 12, 12);
  pop_btn("Fertig", W - 92, 6, 84, 34, pop_close_cb, 0);
  int y = 50;
  if (v.can_limit) {
    lv_obj_set_pos(pop_label(s_pop, "Netzstrom-Begrenzung", &font_ui_16, C_TEXT2), 12, y);
    y += 22;
    pop_btn("–", 12, y, 72, 44, pop_step_cb, -1);
    pop_btn("+", W - 84, y, 72, 44, pop_step_cb, 1);
    s_pop_lim = pop_label(s_pop, "", &font_ui_20, C_TEXT);
    lv_obj_align(s_pop_lim, LV_ALIGN_TOP_MID, 0, y + 10);
    pop_show_limit();
    y += 54;
  }
  if (v.can_mode) {
    lv_obj_set_pos(pop_label(s_pop, "Schalter", &font_ui_16, C_TEXT2), 12, y);
    y += 22;
    static const char *MP_TXT[4] = {"Ein", "Aus", "Nur Laden", "Nur Wechselrichter"};
    static const int MP_VAL[4] = {3, 4, 1, 2};
    static const char *CH_TXT[2] = {"Ein", "Aus"};
    static const int CH_VAL[2] = {1, 4};
    s_pop_nm = v.mode_set == 0 ? 4 : 2;
    int bw = (W - 36) / 2, bh = 40;
    for (int i = 0; i < s_pop_nm; i++) {
      s_pop_mval[i] = v.mode_set == 0 ? MP_VAL[i] : CH_VAL[i];
      s_pop_mbtn[i] = pop_btn(v.mode_set == 0 ? MP_TXT[i] : CH_TXT[i], 12 + (i % 2) * (bw + 12), y + (i / 2) * (bh + 6),
                              bw, bh, pop_mode_cb, i);
    }
    pop_show_mode(v.mode);
  }
}

// einmal pro Sekunde: Werte vom GX uebernehmen (nicht direkt nach eigener Eingabe)
static void pop_sync() {
  if (!s_pop || (s_pop_edit_ms && millis() - s_pop_edit_ms < 3000)) return;
  VictronInfo v;
  if (!cerbo_get(s_pop_key, v)) return;
  if (!s_pop_send_ms) {
    s_pop_val = v.ac_limit;
    pop_show_limit();
  }
  pop_show_mode(v.mode);
}
#endif

// Info-Fenster: Name, WLAN und Adresse der Weboberflaeche gross und lesbar.
// Oeffnen: Platzhalter-Kachel antippen oder von oben nach unten wischen; schliessen: irgendwo antippen.
static lv_obj_t *s_info = nullptr;

static void info_close_cb(lv_event_t *) {
  if (!s_info) return;
  lv_obj_del_async(s_info);   // nichts im Speicher behalten
  s_info = nullptr;
}

static void info_line(int &y, const char *label, const std::string &val, const lv_font_t *font) {
  lv_obj_t *l = lv_label_create(s_info);
  lv_obj_set_style_text_font(l, &font_ui_16, 0);
  lv_obj_set_style_text_color(l, C_TEXT2, 0);
  lv_label_set_text(l, label);
  lv_obj_set_pos(l, 14, y);
  y += lv_font_get_line_height(&font_ui_16) + 2;
  l = lv_label_create(s_info);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, C_TEXT, 0);
  lv_obj_set_width(l, W - 28);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  (lv_label_set_text)(l, val.c_str());   // Klammer: keine Uebersetzung (Namen/Adressen)
  lv_obj_set_pos(l, 14, y);
  y += lv_font_get_line_height(font) + 10;
}

static void info_open() {
  if (s_info) return;
  s_info = lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(s_info);
  lv_obj_set_size(s_info, W, H);
  lv_obj_set_style_bg_opa(s_info, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(s_info, C_BG, 0);
  lv_obj_clear_flag(s_info, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_info, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_info, info_close_cb, LV_EVENT_CLICKED, nullptr);
  int y = 10;
#if SAT_VICTRON_ONLY
  info_line(y, "Gerät", vd_title(), &font_ui_20);
#else
  info_line(y, "Gerät", std::string(g_sat.name.length() ? g_sat.name.c_str() : g_sat.id.c_str()), &font_ui_20);
#endif
  if (WiFi.isConnected()) {
    info_line(y, "WLAN", std::string(WiFi.SSID().c_str()), &font_ui_20);
    info_line(y, "Weboberfläche im Browser", std::string("http://") + WiFi.localIP().toString().c_str(), &font_ui_20);
  } else if (WiFi.getMode() & WIFI_AP) {
    info_line(y, "Einrichtungs-WLAN verbinden", std::string(WiFi.softAPSSID().c_str()), &font_ui_20);
    info_line(y, "Seite öffnet sich, sonst im Browser", std::string("http://") + WiFi.softAPIP().toString().c_str(), &font_ui_20);
  } else {
    info_line(y, "WLAN", "nicht verbunden", &font_ui_20);
  }
  lv_obj_t *l = lv_label_create(s_info);
  lv_obj_set_style_text_font(l, &font_ui_16, 0);
  lv_obj_set_style_text_color(l, C_TEXT3, 0);
  lv_label_set_text(l, "Antippen zum Schließen");
  lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -8);
}

#if WITH_BMS
// Akku-Uebersicht: alle Werte eines Akkus, jede Sekunde aufgefrischt (refresh), antippen schliesst.
// Nur waehrend es offen ist im Speicher.
static lv_obj_t *s_bat = nullptr;
static std::string s_bat_key;
static const int BAT_N = 9;   // Zeilen je Geraeteart aus bms_rows() (Akku, Shunt, Laderegler, DC-DC-Lader, Wechselrichter)
static lv_obj_t *s_bat_title = nullptr, *s_bat_lbl[BAT_N] = {nullptr}, *s_bat_val[BAT_N] = {nullptr};

static void bat_close_cb(lv_event_t *) {
  if (!s_bat) return;
  lv_obj_del_async(s_bat);
  s_bat = nullptr;
}

static void bat_sync() {
  if (!s_bat) return;
  BmsInfo b;
  if (!bms_get(s_bat_key, b)) {
    bat_close_cb(nullptr);
    return;
  }
  (lv_label_set_text)(s_bat_title, b.name.c_str());
  auto rows = bms_rows(b);
  for (int i = 0; i < BAT_N; i++) {
    if (i < (int)rows.size()) {
      (lv_label_set_text)(s_bat_lbl[i], rows[i].first.c_str());
      (lv_label_set_text)(s_bat_val[i], rows[i].second.c_str());
      lv_obj_clear_flag(s_bat_lbl[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(s_bat_val[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(s_bat_lbl[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(s_bat_val[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void bat_open(const std::string &key) {
  if (s_bat) return;
  s_bat_key = key;
  s_bat = lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(s_bat);
  lv_obj_set_size(s_bat, W, H);
  lv_obj_set_style_bg_opa(s_bat, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(s_bat, C_BG, 0);
  lv_obj_clear_flag(s_bat, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_bat, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_bat, bat_close_cb, LV_EVENT_CLICKED, nullptr);
  s_bat_title = lv_label_create(s_bat);
  lv_obj_set_style_text_font(s_bat_title, &font_ui_20, 0);
  lv_obj_set_style_text_color(s_bat_title, C_TEXT, 0);
  lv_obj_set_width(s_bat_title, W - 20);
  lv_label_set_long_mode(s_bat_title, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(s_bat_title, 10, 4);
  int lh = lv_font_get_line_height(&font_ui_16) + 4;
  for (int i = 0; i < BAT_N; i++) {
    int y = 32 + i * lh;
    s_bat_lbl[i] = lv_label_create(s_bat);
    lv_obj_set_style_text_font(s_bat_lbl[i], &font_ui_16, 0);
    lv_obj_set_style_text_color(s_bat_lbl[i], C_TEXT2, 0);
    lv_obj_set_pos(s_bat_lbl[i], 10, y);
    s_bat_val[i] = lv_label_create(s_bat);
    lv_obj_set_style_text_font(s_bat_val[i], &font_ui_16, 0);
    lv_obj_set_style_text_color(s_bat_val[i], C_TEXT, 0);
    lv_obj_set_width(s_bat_val[i], W - 136);
    lv_label_set_long_mode(s_bat_val[i], LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_bat_val[i], 126, y);   // rechts von der laengsten Bezeichnung ("Ladezustand")
  }
  bat_sync();
}
#endif

// Kacheln mit Fenster (Akku-Uebersicht, MultiPlus/Ladegeraet bedienen) oeffnen erst nach ~2 s Druecken
// (LONG_PRESS_MS), damit Wischen und kurzes Antippen beim Blaettern nichts ausloesen.
static const uint16_t LONG_PRESS_MS = 2000;
static void tile_long_cb(lv_event_t *e) {
  size_t k = page_start(s_page) + (int)(intptr_t)lv_event_get_user_data(e);
  if (k >= s_items.size()) return;
#if WITH_BMS
  if (s_items[k].key.rfind("b:", 0) == 0) {
    bat_open(BMS_KEY_PREFIX + s_items[k].key.substr(2));
    return;
  }
#endif
#if WITH_CERBO
  if (!s_items[k].ctl.empty()) pop_open(s_items[k].ctl);
#endif
}

static void tile_cb(lv_event_t *e) {
  int slot = (int)(intptr_t)lv_event_get_user_data(e);
  size_t k = page_start(s_page) + slot;
  if (s_items.empty()) {   // Platzhalter-Kachel: Adresse gross zeigen
    info_open();
    return;
  }
  // Akku- und MultiPlus-Kacheln: nur lange druecken (tile_long_cb)
  if (k < s_items.size() && s_items[k].sw >= 0) {
#if !SAT_VICTRON_ONLY
    sat_io_switch((uint8_t)s_items[k].sw, 2);   // umschalten
#endif
    s_next_ms = 0;                               // gleich auffrischen
  }
}

static void gesture_cb(lv_event_t *) {
#if WITH_CERBO
  if (s_pop) return;   // Bedienfenster offen: nicht blaettern
#endif
  if (s_info) return;
#if WITH_BMS
  if (s_bat) return;
#endif
  lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (d == LV_DIR_BOTTOM) {   // von oben nach unten wischen: Geraet/WLAN/Adresse zeigen
    info_open();
    return;
  }
  if (d == LV_DIR_LEFT) s_page = (s_page + 1) % pages();
  else if (d == LV_DIR_RIGHT) s_page = (s_page + pages() - 1) % pages();
  else return;
  s_next_ms = 0;
}

static void build() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, C_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, nullptr);

  s_title = lv_label_create(scr);
  lv_obj_set_style_text_font(s_title, &font_ui_16, 0);
  lv_obj_set_style_text_color(s_title, C_TEXT, 0);
  lv_obj_set_pos(s_title, GAP + 2, 7);
  s_net = lv_label_create(scr);
  lv_obj_set_style_text_font(s_net, &font_ui_16, 0);
  lv_obj_set_style_text_color(s_net, C_TEXT2, 0);
  lv_obj_align(s_net, LV_ALIGN_TOP_RIGHT, -GAP - 2, 7);
#if SAT_VICTRON_ONLY
  lv_obj_add_flag(s_title, LV_OBJ_FLAG_HIDDEN);   // keine Kopfzeile, Adresse steht notfalls in der Platzhalter-Kachel
  lv_obj_add_flag(s_net, LV_OBJ_FLAG_HIDDEN);
#endif

  for (int i = 0; i < MAX_TILES; i++) {
    Tile &t = s_tiles[i];
    t.box = lv_obj_create(scr);
    lv_obj_remove_style_all(t.box);
    lv_obj_set_style_radius(t.box, 14, 0);
    lv_obj_set_style_bg_opa(t.box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(t.box, C_TILE, 0);
    lv_obj_set_style_border_width(t.box, 1, 0);
    lv_obj_set_style_border_color(t.box, C_TILE_LINE, 0);
    lv_obj_set_style_bg_color(t.box, C_FIELD, LV_STATE_PRESSED);
    lv_obj_clear_flag(t.box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(t.box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(t.box, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(t.box, tile_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(t.box, tile_long_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);

    t.icon = lv_label_create(t.box);
    lv_obj_set_style_text_font(t.icon, &font_icons_24, 0);
    lv_obj_set_pos(t.icon, 10, 8);
    t.name = lv_label_create(t.box);
    lv_obj_set_style_text_font(t.name, &font_ui_16, 0);
    lv_label_set_long_mode(t.name, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(t.name, 42, 11);
    t.value = lv_label_create(t.box);
    lv_obj_set_style_text_font(t.value, &font_ui_20, 0);
    lv_label_set_long_mode(t.value, LV_LABEL_LONG_DOT);
    t.sub = lv_label_create(t.box);
    lv_obj_set_style_text_font(t.sub, &font_ui_16, 0);
    lv_label_set_long_mode(t.sub, LV_LABEL_LONG_DOT);
    lv_obj_add_flag(t.sub, LV_OBJ_FLAG_HIDDEN);
  }
  s_dots = lv_label_create(scr);
  lv_obj_set_style_text_font(s_dots, &font_ui_16, 0);
  lv_obj_set_style_text_color(s_dots, C_TEXT3, 0);
  lv_obj_align(s_dots, LV_ALIGN_BOTTOM_MID, 0, -1);
}

// Hauptwert unten; mit zweitem Wert eine Zeile hoeher (zweiter Wert darunter)
static void place_value(Tile &t) {
  bool two = !lv_obj_has_flag(t.sub, LV_OBJ_FLAG_HIDDEN);
  const lv_font_t *sf = lv_obj_get_style_text_font(t.sub, 0);
  lv_obj_align(t.value, LV_ALIGN_BOTTOM_LEFT, 10, two ? -8 - lv_font_get_line_height(sf) : -10);
}

// Groesse und Lage der Kacheln fuer die aktuelle Seite: 4 (2x2), 2 grosse (volle Breite) oder 1 (ganzer Bildschirm).
// Groessere Kacheln bekommen groessere Schrift, wenn die Firmware sie mitbringt (SAT_BIG_FONT, Flash).
static void relayout() {
  int n = page_cnt(s_page);
  s_lay = n;
  int cols = n == 4 ? 2 : 1, rows = n == 1 ? 1 : 2;
  int tw = (W - GAP * (cols + 1)) / cols, th = (H - TOP - DOTS_H - GAP * rows) / rows;
#if SAT_BIG_FONT
  const lv_font_t *fv = n == 1 ? &font_ui_40 : n == 2 ? &font_ui_26 : &font_ui_20;
  const lv_font_t *fs = n == 1 ? &font_ui_20 : &font_ui_16;
#else
  const lv_font_t *fv = &font_ui_20, *fs = &font_ui_16;
#endif
  for (int i = 0; i < MAX_TILES; i++) {
    Tile &t = s_tiles[i];
    int col = i % cols, row = (i / cols) % rows;
    lv_obj_set_size(t.box, tw, th);
    lv_obj_set_pos(t.box, GAP + col * (tw + GAP), TOP + row * (th + GAP));
    lv_obj_set_style_text_font(t.value, fv, 0);
    lv_obj_set_style_text_font(t.sub, fs, 0);
    lv_obj_set_style_text_font(t.name, n == 1 ? fs : &font_ui_16, 0);
    // feste Hoehe = eine Zeile: zu lange Texte enden mit "…" statt umzubrechen
    lv_obj_set_size(t.name, tw - 50, lv_font_get_line_height(lv_obj_get_style_text_font(t.name, 0)));
    lv_obj_set_size(t.value, tw - 20, lv_font_get_line_height(fv));
    lv_obj_set_size(t.sub, tw - 20, lv_font_get_line_height(fs));
    lv_obj_align(t.sub, LV_ALIGN_BOTTOM_LEFT, 10, -8);
    place_value(t);
  }
}

static void set_text(lv_obj_t *l, const std::string &s) {
  // nur bei Aenderung neu zeichnen; Klammer um den Namen umgeht die Uebersetzung (Geraetenamen)
  if (strcmp(lv_label_get_text(l), s.c_str())) (lv_label_set_text)(l, s.c_str());
}

// "Wert1  ·  Wert2  ·  …": erster Wert gross, der Rest in der Zeile darunter (nebeneinander war nur der erste lesbar)
static void set_value(Tile &t, const std::string &s) {
  static const char SEP[] = "  ·  ";
  size_t p = s.find(SEP);
  std::string a = p == std::string::npos ? s : s.substr(0, p);
  std::string b = p == std::string::npos ? std::string() : s.substr(p + sizeof(SEP) - 1);
  set_text(t.value, a);
  bool had = !lv_obj_has_flag(t.sub, LV_OBJ_FLAG_HIDDEN);
  if (b.empty()) lv_obj_add_flag(t.sub, LV_OBJ_FLAG_HIDDEN);
  else { set_text(t.sub, b); lv_obj_clear_flag(t.sub, LV_OBJ_FLAG_HIDDEN); }
  if (had != !b.empty()) place_value(t);
}

static void refresh() {
  collect();
  if (s_page >= pages()) s_page = pages() - 1;
  if (s_lay != page_cnt(s_page)) relayout();   // jede Seite kann 1, 2 oder 4 Kacheln haben
#if SAT_VICTRON_ONLY
  set_text(s_title, vd_title());
#else
  set_text(s_title, std::string(g_sat.name.length() ? g_sat.name.c_str() : g_sat.id.c_str()));
#endif
  std::string net = WiFi.isConnected() ? std::string(WiFi.localIP().toString().c_str())
                    : (WiFi.getMode() & WIFI_AP) ? std::string("Einrichtung ") + WiFi.softAPIP().toString().c_str()
                                                 : std::string("kein WLAN");
  set_text(s_net, net);
  for (int i = 0; i < MAX_TILES; i++) {
    Tile &t = s_tiles[i];
    size_t k = page_start(s_page) + i;
    if (i >= page_cnt(s_page)) {   // Seite mit 1 oder 2 grossen Kacheln: die uebrigen verstecken
      lv_obj_add_flag(t.box, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    if (k >= s_items.size()) {
      if (s_items.empty() && i == 0) {
        lv_obj_clear_flag(t.box, LV_OBJ_FLAG_HIDDEN);
        set_text(t.icon, ICON_INFO);
#if SAT_VICTRON_ONLY
        set_text(t.name, "Einrichten");
        set_value(t, "Antippen  ·  für Adresse");
#else
        set_text(t.name, "Noch nichts da");
        set_value(t, "Antippen  ·  für Adresse");
#endif
        lv_obj_set_style_text_color(t.icon, C_TEXT3, 0);
        lv_obj_set_style_text_color(t.name, C_TEXT2, 0);
        lv_obj_set_style_text_color(t.value, C_TEXT2, 0);
      } else {
        lv_obj_add_flag(t.box, LV_OBJ_FLAG_HIDDEN);
      }
      continue;
    }
    const Item &it = s_items[k];
    if (it.key.empty()) {   // freier Platz auf einer Seite aus dem Editor
      lv_obj_add_flag(t.box, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(t.box, LV_OBJ_FLAG_HIDDEN);
    set_text(t.icon, it.icon);
    set_text(t.name, it.name);
    set_value(t, it.value);
    bool lit = it.sw >= 0 && it.on;   // eingeschalteter Schalter: Kachel in Bernstein wie die Lichter am Panel
    lv_obj_set_style_bg_color(t.box, lit ? C_AMBER : C_TILE, 0);
    lv_obj_set_style_border_color(t.box, lit ? C_AMBER_D : C_TILE_LINE, 0);
    lv_obj_set_style_text_color(t.icon, lit ? C_INK : it.accent, 0);
    lv_obj_set_style_text_color(t.name, lit ? C_INK : C_TEXT2, 0);
    lv_obj_set_style_text_color(t.value, lit ? C_INK : C_TEXT, 0);
    lv_obj_set_style_text_color(t.sub, lit ? C_INK : C_TEXT2, 0);
  }
  std::string dots;
  if (pages() > 1)
    for (int p = 0; p < pages(); p++) dots += p == s_page ? " • " : " · ";   // Zeichen aus der Panel-Schrift
  set_text(s_dots, dots);
#if WITH_CERBO
  pop_sync();
#endif
#if WITH_BMS
  bat_sync();
#endif
}
static void load_cfg() {
  Preferences p;
  p.begin("satdisp", true);
  String js = p.getString("cfg", "");
  p.end();
  if (js.isEmpty()) return;
  JsonDocument d;
  if (deserializeJson(d, js)) return;
  s_per = norm_per(d["p"] | 4);
  s_pl.clear();
  for (int v : d["pl"].as<JsonArray>()) s_pl.push_back(norm_per(v));
  s_bright = constrain((int)(d["b"] | 200), 20, 255);
  s_dim_min = d["d"] | 2;
  s_auto = d["a"] | true;
  s_night = constrain((int)(d["nb"] | 40), 5, 255);
  s_ldr_thr = constrain((int)(d["lt"] | 300), 1, 4095);
  s_cfg.clear();
  for (JsonObject t : d["t"].as<JsonArray>())
    s_cfg.push_back({t["k"] | "", t["n"] | "", (bool)(t["o"] | 1), (uint8_t)(t["g"] | 255)});
}

static void save_cfg() {
  JsonDocument d;
  d["p"] = s_per;
  if (!s_pl.empty()) {
    JsonArray pl = d["pl"].to<JsonArray>();
    for (uint8_t v : s_pl) pl.add(v);
  }
  d["b"] = s_bright;
  d["d"] = s_dim_min;
  d["a"] = s_auto;
  d["nb"] = s_night;
  d["lt"] = s_ldr_thr;
  JsonArray a = d["t"].to<JsonArray>();
  for (auto &c : s_cfg) {
    JsonObject t = a.add<JsonObject>();
    t["k"] = c.k;
    if (!c.n.empty()) t["n"] = c.n;
    t["o"] = c.on ? 1 : 0;
    if (c.pg < 16) t["g"] = c.pg;
  }
  String js;
  serializeJson(d, js);
  Preferences p;
  p.begin("satdisp", false);
  p.putString("cfg", js);
  p.end();
}
}  // namespace

bool sat_display_present() { return true; }

void sat_display_json(JsonObject o) {
  o["per"] = s_per;
  JsonArray pl = o["pl"].to<JsonArray>();
  for (uint8_t v : s_pl) pl.add(v);
  o["bright"] = s_bright;
  o["dim"] = s_dim_min;
  o["auto"] = s_auto;
  o["night"] = s_night;
  o["ldr_thr"] = s_ldr_thr;
  // alle bekannten Kacheln: erst in der eingestellten Reihenfolge, dann neue
  std::vector<Item> raw;
  collect_raw(raw);
  std::vector<bool> used(raw.size(), false);
  JsonArray a = o["tiles"].to<JsonArray>();
  auto add = [&](const Item &it, const TileCfg *c) {
    JsonObject t = a.add<JsonObject>();
    t["k"] = it.key;
    t["name"] = it.name;
    t["value"] = it.value;
    t["n"] = c ? c->n : "";
    t["on"] = c ? c->on : true;
    if (c && c->pg < 16) t["pg"] = c->pg;
  };
  for (auto &c : s_cfg)
    for (size_t i = 0; i < raw.size(); i++)
      if (!used[i] && raw[i].key == c.k) {
        used[i] = true;
        add(raw[i], &c);
        break;
      }
  for (size_t i = 0; i < raw.size(); i++)
    if (!used[i]) add(raw[i], nullptr);
}

// ---------------------------------------------------------------- Touch kalibrieren
// LovyanGFX zeichnet nacheinander Pfeile in die vier Ecken, die man antippt. Ergebnis: 8 Werte (16 Bytes) in NVS
// "satdisp"/"tcal", beim Start per setTouchCalibrate gesetzt. Kein Dauer-RAM (laeuft blockierend im loop()).
// Ausloesen: Weboberflaeche (Display -> "Touch kalibrieren") oder beim Einschalten 2 s auf den Bildschirm druecken.
static volatile bool s_cal_req = false;
static bool s_reset_req = false;
bool sat_display_reset_requested() { return s_reset_req; }

static void touch_cal_load() {
  uint16_t cal[8];
  Preferences p;
  p.begin("satdisp", true);
  size_t n = p.getBytesLength("tcal") == sizeof(cal) ? p.getBytes("tcal", cal, sizeof(cal)) : 0;
  p.end();
  if (n == sizeof(cal)) s_lcd.setTouchCalibrate(cal);
}

static void touch_cal_run() {
  uint16_t cal[8];
  s_lcd.setBrightness(cur_bright());
  s_lcd.fillScreen(TFT_BLACK);
  s_lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  s_lcd.setTextDatum(textdatum_t::middle_center);
  s_lcd.setTextSize(2);
  s_lcd.drawString("Touch kalibrieren", W / 2, H / 2 - 14);
  s_lcd.setTextSize(1);
  s_lcd.drawString("Pfeilspitzen nacheinander antippen", W / 2, H / 2 + 12);
  s_lcd.calibrateTouch(cal, TFT_WHITE, TFT_BLACK, 16);
  Preferences p;
  p.begin("satdisp", false);
  p.putBytes("tcal", cal, sizeof(cal));
  p.end();
  s_lcd.setTouchCalibrate(cal);
  s_lcd.fillScreen(TFT_BLACK);
  s_last_touch = millis();
  s_dim = false;
  lv_obj_invalidate(lv_scr_act());   // Kacheln neu zeichnen
}

bool sat_display_apply(JsonObjectConst c) {
  if (c["calibrate"] | false) {   // nur Kalibrierung anstossen, sonst nichts aendern
    s_cal_req = true;
    return true;
  }
  if (c["per"].is<int>()) s_per = norm_per(c["per"].as<int>());
  if (c["pl"].is<JsonArrayConst>()) {   // Kacheln je Seite (Editor), hoechstens 16 Seiten
    s_pl.clear();
    for (int v : c["pl"].as<JsonArrayConst>())
      if (s_pl.size() < 16) s_pl.push_back(norm_per(v));
  }
  if (c["bright"].is<int>()) s_bright = constrain(c["bright"].as<int>(), 20, 255);
  if (c["dim"].is<int>()) s_dim_min = constrain(c["dim"].as<int>(), 0, 600);
  if (c["auto"].is<bool>()) s_auto = c["auto"];
  if (c["night"].is<int>()) s_night = constrain(c["night"].as<int>(), 5, 255);
  if (c["ldr_thr"].is<int>()) s_ldr_thr = constrain(c["ldr_thr"].as<int>(), 1, 4095);
  if (c["tiles"].is<JsonArrayConst>()) {
    s_cfg.clear();
    for (JsonObjectConst t : c["tiles"].as<JsonArrayConst>()) {
      const char *k = t["k"] | "";
      if (!*k) continue;
      int pg = t["pg"] | 255;
      s_cfg.push_back({k, t["n"] | "", t["on"] | true, (uint8_t)(pg >= 0 && pg < 16 ? pg : 255)});
    }
  }
  save_cfg();
  s_page = 0;
  relayout();
  s_dim = false;
  s_lcd.setBrightness(cur_bright());
  s_last_touch = millis();
  s_next_ms = 0;   // gleich neu zeichnen
  return true;
}

static uint32_t s_splash_until = 0;   // != 0: Startbild steht noch, LVGL zeichnet noch nicht

static void ldr_loop(uint32_t now) {
  if (now - s_ldr_ms < 500) return;
  s_ldr_ms = now;
  int raw = analogRead(SAT_LDR_PIN);
  s_ldr = s_ldr < 0 ? raw : s_ldr * 0.8f + raw * 0.2f;
  bool want = s_night_on ? s_ldr > s_ldr_thr * 0.75f : s_ldr > s_ldr_thr;
  if (want == s_night_on) {
    s_ldr_chg_ms = 0;
    return;
  }
  if (!s_ldr_chg_ms) s_ldr_chg_ms = now | 1;
  if (now - s_ldr_chg_ms < 3000) return;
  s_ldr_chg_ms = 0;
  s_night_on = want;
  if (s_auto && !s_dim) s_lcd.setBrightness(cur_bright());
}

void sat_display_light(JsonObject o) {
  o["ldr"] = s_ldr < 0 ? -1 : (int)lroundf(s_ldr);
  o["night"] = s_night_on;
  o["auto"] = s_auto;
}

bool sat_display_night() { return s_auto && s_night_on; }

void sat_display_begin() {
  for (int p : {4, 16, 17}) {   // RGB-LED aus (Low = an)
    pinMode(p, OUTPUT);
    digitalWrite(p, HIGH);
  }
  s_lcd.init();
  s_lcd.setRotation(SAT_DISPLAY_ROTATION);
  touch_cal_load();
  load_cfg();
  analogRead(SAT_LDR_PIN);   // richtet den Pin als Analogeingang ein – erst danach laesst sich der Bereich setzen
  analogSetPinAttenuation(SAT_LDR_PIN, ADC_0db);   // empfindlichster Bereich (der LDR liefert nur kleine Spannungen)
  s_ldr = analogRead(SAT_LDR_PIN);
  s_night_on = s_ldr > s_ldr_thr;
  s_lcd.setBrightness(cur_bright());
#if SAT_SPLASH
  if (SPLASH_CYD_JPG_LEN) {   // direkt aufs Display, der Decoder braucht nur kurz ~4 KB
    s_lcd.drawJpg(SPLASH_CYD_JPG, SPLASH_CYD_JPG_LEN, 0, 0);
    s_splash_until = (millis() + 6000) | 1;   // 6 s wie bei den Panels
  }
#endif

  {   // beim Einschalten gedrueckt halten: 2 s = Touch kalibrieren, 10 s = Passwort zuruecksetzen
    lgfx::touch_point_t tp;
    uint32_t t0 = millis();
    bool shown = false;
    const uint32_t HOLD_MAX = 10000;
    while (s_lcd.getTouch(&tp) > 0 && millis() - t0 < HOLD_MAX) {
      if (!shown && millis() - t0 >= 2000 && HOLD_MAX > 2000) {
        shown = true;
        s_lcd.fillScreen(TFT_BLACK);
        s_lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        s_lcd.setTextDatum(textdatum_t::middle_center);
        s_lcd.setTextSize(1);
        s_lcd.drawString("Jetzt loslassen: Touch kalibrieren", W / 2, H / 2 - 10);
        s_lcd.drawString("Weiter halten (10 s): Passwort zuruecksetzen", W / 2, H / 2 + 10);
      }
      delay(20);
    }
    uint32_t held = millis() - t0;
    if (held >= HOLD_MAX && HOLD_MAX > 2000) {
      s_reset_req = true;
      s_lcd.fillScreen(TFT_BLACK);
      s_lcd.drawString("Passwort zurueckgesetzt (panelsat)", W / 2, H / 2);
      delay(1500);
    } else if (held >= 2000) {
      s_cal_req = true;
    }
  }

  lv_init();
  lv_color_t *buf = (lv_color_t *)heap_caps_malloc(W * BUF_LINES * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  lv_disp_draw_buf_init(&s_buf, buf, nullptr, W * BUF_LINES);
  lv_disp_drv_init(&s_disp_drv);
  s_disp_drv.hor_res = W;
  s_disp_drv.ver_res = H;
  s_disp_drv.flush_cb = flush_cb;
  s_disp_drv.draw_buf = &s_buf;
  lv_disp_drv_register(&s_disp_drv);
  lv_indev_drv_init(&s_indev_drv);
  s_indev_drv.type = LV_INDEV_TYPE_POINTER;
  s_indev_drv.read_cb = touch_cb;
  s_indev_drv.long_press_time = LONG_PRESS_MS;
  lv_indev_drv_register(&s_indev_drv);

  s_last_touch = millis();
  build();
  relayout();
  refresh();
}

void sat_display_loop() {
  if (s_cal_req) {
    s_cal_req = false;
    touch_cal_run();
    s_splash_until = 0;
  }
  if (s_splash_until) {   // Startbild: 3 s oder bis zur ersten Beruehrung
    lgfx::touch_point_t tp;
    if ((int32_t)(millis() - s_splash_until) < 0 && s_lcd.getTouch(&tp) == 0) return;
    s_splash_until = 0;
    s_last_touch = millis();
    lv_obj_invalidate(lv_scr_act());   // alles neu zeichnen
  }
#if WITH_CERBO
  if (s_pop_send_ms && (int32_t)(millis() - s_pop_send_ms) >= 0) pop_flush();
#endif
  lv_timer_handler();
  uint32_t now = millis();
  ldr_loop(now);
  if (!s_dim && s_dim_min && now - s_last_touch > s_dim_min * 60000UL) {
    s_dim = true;
    s_lcd.setBrightness(std::min<uint8_t>(DIMMED, cur_bright()));
  }
  if ((int32_t)(now - s_next_ms) < 0) return;
  s_next_ms = now + 1000;   // Werte einmal pro Sekunde auffrischen
  refresh();
}

#else
void sat_display_begin() {}
void sat_display_loop() {}
void sat_display_light(JsonObject) {}
bool sat_display_night() { return false; }
bool sat_display_present() { return false; }
void sat_display_json(JsonObject) {}
bool sat_display_apply(JsonObjectConst) { return false; }
bool sat_display_reset_requested() { return false; }
#endif
