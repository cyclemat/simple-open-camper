// Anzeige fuer Satelliten mit Display – siehe sat_display.h
// Optik wie das Hauptpanel: LVGL, gleiche Farben (ui/theme.h), Schriften Inter 16/20 und Symbolschrift.
#include "sat_display.h"

#if SAT_DISPLAY
#include <Arduino.h>
#include <WiFi.h>
#include <math.h>
#include <string>
#include <vector>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "esp_heap_caps.h"
#include <Preferences.h>
#include "sat_config.h"
#include "sat_io.h"
#include "../truma.h"
#include "../mopeka.h"
#include "../victron.h"
#include "../fonts/fonts.h"
#include "../fonts/icons.h"
#include "../ui/theme.h"

// ---------------------------------------------------------------- Hardware: ESP32-2432S028R
// Display ILI9341 an HSPI (SCK 14, MOSI 13, MISO 12, CS 15, DC 2), Licht GPIO 21.
// Touch XPT2046 an eigenem SPI (CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36). RGB-LED 4/16/17 (Low = an).
#ifndef SAT_TOUCH_XMIN
#define SAT_TOUCH_XMIN 300    // Rohwerte des Touch-Controllers an den Raendern – bei Bedarf nachstellen
#define SAT_TOUCH_XMAX 3900
#define SAT_TOUCH_YMIN 200
#define SAT_TOUCH_YMAX 3700
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
static int s_per = 4;            // Kacheln pro Seite: 4 (2x2) oder 2 (gross)
static uint8_t s_bright = 200;   // Helligkeit 20..255
static uint16_t s_dim_min = 2;   // nach so vielen Minuten ohne Beruehrung dimmen, 0 = nie
struct TileCfg {
  std::string k, n;   // Schluessel ("p:tank1", "t:<mac>", "m:<mac>", "v:<mac>"), eigener Name
  bool on = true;
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
      s_lcd.setBrightness(s_bright);
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
};

static const int MAX_TILES = 4, TOP = 30, GAP = 8, DOTS_H = 16;

struct Tile {
  lv_obj_t *box, *icon, *name, *value;
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

static void collect_raw(std::vector<Item> &s_items) {
  s_items.clear();
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
  for (auto &t : truma_list()) {
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
  for (auto &v : victron_list()) {
    Item it;
    it.key = "v:" + v.mac;
    it.name = v.name.empty() ? "Victron" : v.name;
    it.accent = v.has ? C_BLUE : C_TEXT3;
    switch (v.kind) {
      case VIC_SOLAR: it.icon = ICON_SUN; break;
      case VIC_SHUNT: it.icon = ICON_BATTERY; break;
      case VIC_CHARGER: it.icon = ICON_PLUG; break;
      default: it.icon = ICON_LIGHTNING; break;
    }
    if (!v.has) it.value = v.status.empty() ? "–" : v.status;
    else if (v.kind == VIC_SOLAR) {
      it.value = num(v.pv_w, 0, "W") + "  ·  " + num(v.bat_v, 1, "V");
      it.accent = (!isnan(v.pv_w) && v.pv_w > 5) ? C_AMBER : C_TEXT2;
    } else if (v.kind == VIC_SHUNT) {
      it.value = num(v.soc, 0, "%") + "  ·  " + num(v.bat_v, 1, "V");
      it.accent = (!isnan(v.soc) && v.soc < 20) ? C_RED : C_GREEN;
    } else if (v.kind == VIC_CHARGER) {
      it.value = num(v.power_w, 0, "W") + "  ·  " + num(v.bat_v, 1, "V");
    } else {
      it.value = num(v.bat_v, 2, "V");
    }
    s_items.push_back(it);
  }
}

// Anzeige-Liste: Reihenfolge, Auswahl und Namen aus den Einstellungen; neue Geraete hinten (sichtbar)
static void collect() {
  std::vector<Item> raw;
  collect_raw(raw);
  s_items.clear();
  std::vector<bool> used(raw.size(), false);
  for (auto &c : s_cfg)
    for (size_t i = 0; i < raw.size(); i++)
      if (!used[i] && raw[i].key == c.k) {
        used[i] = true;
        if (c.on) {
          Item it = raw[i];
          if (!c.n.empty()) it.name = c.n;
          s_items.push_back(it);
        }
        break;
      }
  for (size_t i = 0; i < raw.size(); i++)
    if (!used[i]) s_items.push_back(raw[i]);
}

static int pages() { return s_items.empty() ? 1 : (int)((s_items.size() + s_per - 1) / s_per); }

static void tile_cb(lv_event_t *e) {
  int slot = (int)(intptr_t)lv_event_get_user_data(e);
  size_t k = s_page * s_per + slot;
  if (k < s_items.size() && s_items[k].sw >= 0) {
    sat_io_switch((uint8_t)s_items[k].sw, 2);   // umschalten
    s_next_ms = 0;                               // gleich auffrischen
  }
}

static void gesture_cb(lv_event_t *) {
  lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
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
  }
  s_dots = lv_label_create(scr);
  lv_obj_set_style_text_font(s_dots, &font_ui_16, 0);
  lv_obj_set_style_text_color(s_dots, C_TEXT3, 0);
  lv_obj_align(s_dots, LV_ALIGN_BOTTOM_MID, 0, -1);
}

// Groesse und Lage der Kacheln: 4 pro Seite (2x2) oder 2 grosse (volle Breite)
static void relayout() {
  int cols = s_per == 2 ? 1 : 2, rows = 2;
  int tw = (W - GAP * (cols + 1)) / cols, th = (H - TOP - DOTS_H - GAP * rows) / rows;
  for (int i = 0; i < MAX_TILES; i++) {
    Tile &t = s_tiles[i];
    int col = i % cols, row = (i / cols) % rows;
    lv_obj_set_size(t.box, tw, th);
    lv_obj_set_pos(t.box, GAP + col * (tw + GAP), TOP + row * (th + GAP));
    lv_obj_set_width(t.name, tw - 50);
    lv_obj_set_width(t.value, tw - 20);
    lv_obj_align(t.value, LV_ALIGN_BOTTOM_LEFT, 10, -10);
  }
}

static void set_text(lv_obj_t *l, const std::string &s) {
  // nur bei Aenderung neu zeichnen; Klammer um den Namen umgeht die Uebersetzung (Geraetenamen)
  if (strcmp(lv_label_get_text(l), s.c_str())) (lv_label_set_text)(l, s.c_str());
}

static void refresh() {
  collect();
  if (s_page >= pages()) s_page = pages() - 1;
  set_text(s_title, std::string(g_sat.name.length() ? g_sat.name.c_str() : g_sat.id.c_str()));
  std::string net = WiFi.isConnected() ? std::string(WiFi.localIP().toString().c_str())
                    : (WiFi.getMode() & WIFI_AP) ? std::string("Einrichtung 192.168.4.1")
                                                 : std::string("kein WLAN");
  set_text(s_net, net);
  for (int i = 0; i < MAX_TILES; i++) {
    Tile &t = s_tiles[i];
    size_t k = s_page * s_per + i;
    if (i >= s_per) {   // bei 2 grossen Kacheln die uebrigen verstecken
      lv_obj_add_flag(t.box, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    if (k >= s_items.size()) {
      if (s_items.empty() && i == 0) {
        lv_obj_clear_flag(t.box, LV_OBJ_FLAG_HIDDEN);
        set_text(t.icon, ICON_INFO);
        set_text(t.name, "Noch nichts eingerichtet");
        set_text(t.value, "Web: Einstellungen");
        lv_obj_set_style_text_color(t.icon, C_TEXT3, 0);
        lv_obj_set_style_text_color(t.name, C_TEXT2, 0);
        lv_obj_set_style_text_color(t.value, C_TEXT2, 0);
      } else {
        lv_obj_add_flag(t.box, LV_OBJ_FLAG_HIDDEN);
      }
      continue;
    }
    const Item &it = s_items[k];
    lv_obj_clear_flag(t.box, LV_OBJ_FLAG_HIDDEN);
    set_text(t.icon, it.icon);
    set_text(t.name, it.name);
    set_text(t.value, it.value);
    bool lit = it.sw >= 0 && it.on;   // eingeschalteter Schalter: Kachel in Bernstein wie die Lichter am Panel
    lv_obj_set_style_bg_color(t.box, lit ? C_AMBER : C_TILE, 0);
    lv_obj_set_style_border_color(t.box, lit ? C_AMBER_D : C_TILE_LINE, 0);
    lv_obj_set_style_text_color(t.icon, lit ? C_INK : it.accent, 0);
    lv_obj_set_style_text_color(t.name, lit ? C_INK : C_TEXT2, 0);
    lv_obj_set_style_text_color(t.value, lit ? C_INK : C_TEXT, 0);
  }
  std::string dots;
  if (pages() > 1)
    for (int p = 0; p < pages(); p++) dots += p == s_page ? " • " : " · ";   // Zeichen aus der Panel-Schrift
  set_text(s_dots, dots);
}
static void load_cfg() {
  Preferences p;
  p.begin("satdisp", true);
  String js = p.getString("cfg", "");
  p.end();
  if (js.isEmpty()) return;
  JsonDocument d;
  if (deserializeJson(d, js)) return;
  s_per = (d["p"] | 4) == 2 ? 2 : 4;
  s_bright = constrain((int)(d["b"] | 200), 20, 255);
  s_dim_min = d["d"] | 2;
  s_cfg.clear();
  for (JsonObject t : d["t"].as<JsonArray>()) s_cfg.push_back({t["k"] | "", t["n"] | "", (bool)(t["o"] | 1)});
}

static void save_cfg() {
  JsonDocument d;
  d["p"] = s_per;
  d["b"] = s_bright;
  d["d"] = s_dim_min;
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
}  // namespace

bool sat_display_present() { return true; }

void sat_display_json(JsonObject o) {
  o["per"] = s_per;
  o["bright"] = s_bright;
  o["dim"] = s_dim_min;
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

bool sat_display_apply(JsonObjectConst c) {
  if (c["per"].is<int>()) s_per = c["per"].as<int>() == 2 ? 2 : 4;
  if (c["bright"].is<int>()) s_bright = constrain(c["bright"].as<int>(), 20, 255);
  if (c["dim"].is<int>()) s_dim_min = constrain(c["dim"].as<int>(), 0, 600);
  if (c["tiles"].is<JsonArrayConst>()) {
    s_cfg.clear();
    for (JsonObjectConst t : c["tiles"].as<JsonArrayConst>()) {
      const char *k = t["k"] | "";
      if (!*k) continue;
      s_cfg.push_back({k, t["n"] | "", t["on"] | true});
    }
  }
  save_cfg();
  relayout();
  s_page = 0;
  s_dim = false;
  s_lcd.setBrightness(s_bright);
  s_last_touch = millis();
  s_next_ms = 0;   // gleich neu zeichnen
  return true;
}

void sat_display_begin() {
  for (int p : {4, 16, 17}) {   // RGB-LED aus (Low = an)
    pinMode(p, OUTPUT);
    digitalWrite(p, HIGH);
  }
  s_lcd.init();
  s_lcd.setRotation(SAT_DISPLAY_ROTATION);
  load_cfg();
  s_lcd.setBrightness(s_bright);

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
  lv_indev_drv_register(&s_indev_drv);

  s_last_touch = millis();
  build();
  relayout();
  refresh();
}

void sat_display_loop() {
  lv_timer_handler();
  uint32_t now = millis();
  if (!s_dim && s_dim_min && now - s_last_touch > s_dim_min * 60000UL) {
    s_dim = true;
    s_lcd.setBrightness(DIMMED);
  }
  if ((int32_t)(now - s_next_ms) < 0) return;
  s_next_ms = now + 1000;   // Werte einmal pro Sekunde auffrischen
  refresh();
}

#else
void sat_display_begin() {}
void sat_display_loop() {}
bool sat_display_present() { return false; }
void sat_display_json(JsonObject) {}
bool sat_display_apply(JsonObjectConst) { return false; }
#endif
