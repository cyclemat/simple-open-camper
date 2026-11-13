// Gasflaschen und Tanks (Truma LevelControl, Mopeka): Detailansicht, Hilfsfunktionen fuer Kacheln und Einrichtung
#include "../cerbo.h"
#include "ui_internal.h"
#include "../i18n/i18n.h"
#include "theme.h"
#include "../layout.h"
#include "../truma.h"
#include "../mopeka.h"
#include "../panelsat.h"
#include "../z2m.h"
#include <Arduino.h>
#include <math.h>

lv_color_t ui_gas_color(int pct) {
  if (pct >= 40) return C_GREEN;
  if (pct >= 15) return C_AMBER;
  return C_RED;
}

std::string ui_kg_text(float kg) {
  char b[16];
  snprintf(b, sizeof(b), "%.1f\xE2\x80\xAFkg", kg);
  for (char *p = b; *p; p++)
    if (*p == '.') *p = ',';
  return b;
}

std::string ui_age_text(int32_t s) {
  char b[32];
  if (s < 0) return "Zeitpunkt unbekannt";
  if (s < 60) return "gerade eben";
  if (s < 3600) snprintf(b, sizeof(b), tr("vor %ld Min."), (long)(s / 60));
  else if (s < 86400) snprintf(b, sizeof(b), tr("vor %ld Std."), (long)(s / 3600));
  else snprintf(b, sizeof(b), tr("vor %ld %s"), (long)(s / 86400), s / 86400 == 1 ? tr("Tag") : tr("Tagen"));
  return b;
}

// ---------------------------------------------------------------- gemeinsame Sicht
static GasView view_of(const TrumaInfo &t) {
  GasView g;
  g.key = t.key;
  g.mac = t.mac;
  g.name = t.name;
  g.source = "Truma LevelControl";
  g.has = t.has_reading;
  g.busy = t.busy;
  g.percent = t.percent;
  g.amount = ui_kg_text(t.kg);
  g.battery = t.battery;
  g.temperature = t.temperature;
  g.quality = t.quality;
  g.age_s = t.age_s;
  g.status = t.status;
  return g;
}

static GasView view_of(const MopekaInfo &m) {
  GasView g;
  g.key = m.key;
  g.mac = m.mac;
  g.name = m.name;
  g.source = tr("Mopeka ") + m.model_text;
  g.mopeka = true;
  g.water = m.medium != MOP_PROPANE;
  g.has = m.has;
  g.percent = m.percent;
  if (m.capacity > 0) {
    if (!strcmp(m.unit, "kg")) g.amount = ui_kg_text(m.amount);
    else g.amount = std::to_string((int)lroundf(m.amount)) + "\xE2\x80\xAFl";
  }
  g.battery = m.battery;
  g.temperature = m.temperature;
  g.quality = m.quality;
  g.level_mm = m.level_mm;
  g.age_s = m.age_s;
  g.status = m.status;
  return g;
}

bool ui_gas_is_key(const std::string &key) { return truma_is_key(key) || mopeka_is_key(key); }

bool ui_gas_get(const std::string &key, GasView &out) {
  // eigene Sensoren des Panels zuerst, dann Satelliten
  if (truma_is_key(key)) {
    TrumaInfo t;
    if (truma_get(key, t)) {
      out = view_of(t);
      return true;
    }
    if (!panelsat_truma_get(key, t)) return false;
    out = view_of(t);
    out.sat = panelsat_via_text(key);
    return true;
  }
  if (mopeka_is_key(key)) {
    MopekaInfo m;
    if (mopeka_get(key, m)) {
      out = view_of(m);
      return true;
    }
    if (!panelsat_mopeka_get(key, m)) return false;
    out = view_of(m);
    out.sat = panelsat_via_text(key);
    return true;
  }
  return false;
}

std::vector<GasView> ui_gas_list() {
  std::vector<GasView> out;
  for (auto &t : truma_list()) out.push_back(view_of(t));
  for (auto &m : mopeka_list()) out.push_back(view_of(m));
  auto known = [&out](const std::string &k) {
    for (auto &g : out)
      if (g.key == k) return true;
    return false;
  };
  for (auto &t : panelsat_truma_list())
    if (!known(t.key)) {
      out.push_back(view_of(t));
      out.back().sat = panelsat_via_text(t.key);
    }
  for (auto &m : panelsat_mopeka_list())
    if (!known(m.key)) {
      out.push_back(view_of(m));
      out.back().sat = panelsat_via_text(m.key);
    }
  return out;
}

std::string ui_gas_label(const Layout &lay, const GasView &g) {
  auto it = lay.names.find(g.key);
  if (it != lay.names.end() && !it->second.empty()) return it->second;
  return g.name.empty() ? (g.water ? tr("Tank") : tr("Gasflasche")) : g.name;
}

// Reihenfolge: zuerst alles ausser Zigbee (Gas, Victron, Cerbo GX, Heizung, Satelliten) – sonst fuellen viele
// Zigbee-Geraete die Seiten (MAX_PAGES) und die uebrigen kommen gar nicht mehr dran. Zigbee/Shelly/WLED danach.
std::vector<ControlView> ui_arrange_items() {
  std::vector<ControlView> all;
  for (auto &g : ui_gas_list()) {
    ControlView v;
    v.key = g.key;
    v.name = g.name;
    all.push_back(v);
  }
  for (auto &x : victron_list()) {
    ControlView v;
    v.key = x.key;
    v.name = x.name;
    all.push_back(v);
  }
  for (auto &h : ui_heat_list()) {
    ControlView v;
    v.key = h.key;
    v.name = "Heizung";
    all.push_back(v);
  }
  for (auto &x : panelsat_victron_list()) {
    bool known = false;
    for (auto &a : all) known |= a.key == x.key;
    if (known) continue;
    ControlView v;
    v.key = x.key;
    v.name = x.name;
    all.push_back(v);
  }
  for (auto &p : panelsat_point_list()) {
    ControlView v;
    v.key = p.key;
    v.name = p.name;
    all.push_back(v);
  }
  for (auto &x : cerbo_list()) {   // Victron Cerbo GX (MQTT)
    ControlView v;
    v.key = x.key;
    v.name = x.name;
    all.push_back(v);
  }
  for (auto &z : z2m_all()) all.push_back(std::move(z));
  return all;
}

// ---------------------------------------------------------------- Detailansicht
struct GasDlg {
  std::string key, mac;
  bool mopeka = false;
  lv_obj_t *ov = nullptr, *hdr_bg = nullptr, *hdr_icon = nullptr, *title = nullptr, *pct = nullptr, *bar = nullptr;
  lv_obj_t *rows[6] = {nullptr};
  lv_obj_t *status = nullptr;
};
static GasDlg *s_g = nullptr;

bool ui_gas_is_open() { return s_g != nullptr; }
void ui_gas_close() {
  if (s_g && s_g->ov) lv_obj_del_async(s_g->ov);
}

static void g_delete_cb(lv_event_t *e) {
  GasDlg *d = (GasDlg *)lv_event_get_user_data(e);
  if (d == s_g) s_g = nullptr;
  delete d;
}
static void g_ov_click(lv_event_t *e) {
  if (lv_event_get_target(e) == lv_event_get_current_target(e)) ui_gas_close();
}
static void g_close_cb(lv_event_t *) { ui_gas_close(); }
static void g_measure_cb(lv_event_t *) {
  if (!s_g) return;
  GasView g;
  if (ui_gas_get(s_g->key, g) && !g.sat.empty()) {
    ui_toast(panelsat_measure(s_g->key) ? "Messung beim Satelliten angefordert."
                                        : "Messen geht nur, wenn der Satellit per MQTT verbunden ist.");
    return;
  }
  truma_measure(s_g->mac);
  ui_toast("Messung angefordert. Der Sensor meldet sich nur alle paar Minuten – das kann etwas dauern.");
}
static void g_rename_cb(lv_event_t *) {
  if (!s_g) return;
  GasView g;
  if (!ui_gas_get(s_g->key, g)) return;
  std::string mac = g.mac;
  bool mop = g.mopeka;
  if (!g.sat.empty()) {   // Satellit: nur der Name auf dem Panel (am Satelliten in dessen Weboberflaeche)
    std::string key = s_g->key, def = g.name;
    InputOptions o;
    o.title = "Umbenennen";
    o.hint = "Der Name gilt nur für dieses Panel. Leer lassen für den Namen vom Satelliten.";
    o.initial = ui_gas_label(layout_get(), g);
    o.max_len = 48;
    o.on_ok = [key, def](const std::string &t, bool) {
      layout_set_name(key, (t.empty() || t == def) ? "" : t);
      ui_toast("Name gespeichert.");
    };
    ui_gas_close();
    ui_input_open(o);
    return;
  }
  InputOptions o;
  o.title = g.water ? "Tank umbenennen" : "Gasflasche umbenennen";
  o.hint = g.water ? "Zum Beispiel „Frischwasser“." : "Zum Beispiel „Flasche links“ oder „Reserve“.";
  o.initial = g.name;
  o.max_len = 32;
  o.on_ok = [mac, mop](const std::string &t, bool) {
    if (t.empty()) return;
    if (mop) mopeka_rename(mac, t);
    else truma_rename(mac, t);
  };
  ui_gas_close();
  ui_input_open(o);
}
static void g_remove_cb(lv_event_t *) {
  if (!s_g) return;
  std::string mac = s_g->mac;
  bool mop = s_g->mopeka;
  ui_gas_close();
  ui_confirm("Sensor entfernen?", "Das Panel wertet diesen Sensor dann nicht mehr aus. Die Kachel bleibt leer, bis du sie im Editor entfernst.",
             "Entfernen", true, [mac, mop] {
               if (mop) mopeka_remove(mac);
               else truma_remove(mac);
             });
}

// Mopeka: eigene Flaschenmasse in drei Schritten abfragen
static void ask_custom_tank(const std::string &mac) {
  MopekaInfo m;
  if (!mopeka_get(MOPEKA_KEY_PREFIX + mac, m)) return;
  bool top = m.medium == MOP_AIR;
  InputOptions o;
  o.title = "Füllhöhe bei leer (mm)";
  o.hint = top ? "Sensor misst von oben: Abstand zur Oberfläche, wenn der Tank leer ist – also etwa die Tankhöhe."
               : "Messwert, wenn die Flasche leer ist. Bei Gasflaschen meist 38 mm.";
  o.initial = std::to_string(m.empty_mm);
  o.numeric = true;
  o.max_len = 4;
  o.on_ok = [mac, m, top](const std::string &e, bool) {
    int empty = atoi(e.c_str());
    InputOptions o2;
    o2.title = "Füllhöhe bei voll (mm)";
    o2.hint = top ? "Abstand zur Oberfläche, wenn der Tank voll ist." : tr("Messwert bei voller Flasche (bei Gas: 80 % Füllung).");
    o2.initial = std::to_string(m.full_mm);
    o2.numeric = true;
    o2.max_len = 4;
    o2.on_ok = [mac, m, empty](const std::string &f, bool) {
      int full = atoi(f.c_str());
      InputOptions o3;
      o3.title = m.medium == MOP_PROPANE ? "Gasmenge bei voll (kg)" : "Inhalt bei voll (Liter)";
      o3.hint = "0 = keine Menge anzeigen, nur Prozent.";
      char b[16];
      snprintf(b, sizeof(b), "%g", m.capacity);
      o3.initial = b;
      o3.numeric = true;
      o3.max_len = 6;
      o3.on_ok = [mac, empty, full](const std::string &c, bool) {
        std::string cs = c;
        for (auto &ch : cs)
          if (ch == ',') ch = '.';
        std::string err = mopeka_set_tank(mac, MOPEKA_TANK_CUSTOM, empty, full, atof(cs.c_str()));
        ui_toast(err.empty() ? "Gespeichert." : err.c_str());
      };
      ui_input_open(o3);
    };
    ui_input_open(o2);
  };
  ui_input_open(o);
}

static void g_tank_cb(lv_event_t *e) {
  if (!s_g) return;
  uint32_t sel = lv_dropdown_get_selected(lv_event_get_target(e));
  std::string mac = s_g->mac;
  if (sel < mopeka_tank_count()) {
    mopeka_set_tank(mac, sel, 0, 0, 0);
  } else {
    ui_gas_close();
    ask_custom_tank(mac);
  }
}

void ui_gas_update() {
  if (!s_g) return;
  GasView g;
  if (!ui_gas_get(s_g->key, g)) {
    ui_gas_close();
    return;
  }
  Layout lay = layout_get();
  (lv_label_set_text)(s_g->title, ui_gas_label(lay, g).c_str());
  char b[64];
  lv_color_t col = g.has ? ui_gas_color(g.percent) : C_TEXT3;
  lv_obj_set_style_bg_color(s_g->hdr_bg, g.has ? col : C_FIELD, 0);
  lv_obj_set_style_text_color(s_g->hdr_icon, g.has ? C_INK : C_TEXT2, 0);
  if (g.has) {
    snprintf(b, sizeof(b), "%d\xE2\x80\xAF%%", g.percent);
    lv_label_set_text(s_g->pct, b);
    lv_bar_set_value(s_g->bar, g.percent, LV_ANIM_ON);
    lv_obj_set_style_bg_color(s_g->bar, col, LV_PART_INDICATOR);
    lv_label_set_text(s_g->rows[0], g.amount.empty() ? "–" : g.amount.c_str());
    snprintf(b, sizeof(b), "%d\xE2\x80\xAF%%", g.battery);
    lv_label_set_text(s_g->rows[1], b);
    snprintf(b, sizeof(b), "%d\xE2\x80\xAF°C", g.temperature);
    lv_label_set_text(s_g->rows[2], b);
    snprintf(b, sizeof(b), "%d von 3", g.quality);
    lv_label_set_text(s_g->rows[3], b);
    lv_label_set_text(s_g->rows[4], ui_age_text(g.age_s).c_str());
    snprintf(b, sizeof(b), "%d\xE2\x80\xAFmm", g.level_mm);
    lv_label_set_text(s_g->rows[5], b);
  } else {
    lv_label_set_text(s_g->pct, "–");
    lv_bar_set_value(s_g->bar, 0, LV_ANIM_OFF);
    for (auto r : s_g->rows) lv_label_set_text(r, "–");
  }
  std::string st = g.busy ? g.status : (g.status == "OK" ? (g.mopeka ? "Empfang in Ordnung" : "Letzte Messung erfolgreich") : g.status);
  lv_label_set_text(s_g->status, st.c_str());
}

static lv_obj_t *tank_dropdown(lv_obj_t *parent, const MopekaInfo &m) {
  lv_obj_t *row = lv_obj_create(parent);
  ui_plain(row);
  lv_obj_set_size(row, lv_pct(100), 56);
  lv_obj_t *t = ui_label(row, m.medium == MOP_PROPANE ? "Flasche" : "Tank", F_BODY, C_TEXT);
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 4, 0);
  std::string opts;
  for (size_t i = 0; i < mopeka_tank_count(); i++) opts += std::string(tr(mopeka_tank(i).name)) + "\n";
  opts += tr("Eigene Werte …");
  lv_obj_t *dd = lv_dropdown_create(row);
  lv_dropdown_set_options(dd, opts.c_str());
  lv_dropdown_set_selected(dd, m.tank < mopeka_tank_count() ? m.tank : mopeka_tank_count());
  lv_obj_set_width(dd, 280);
  lv_obj_set_style_text_font(dd, F_BODY, 0);
  lv_obj_set_style_bg_color(dd, C_FIELD, 0);
  lv_obj_set_style_border_width(dd, 0, 0);
  lv_obj_set_style_radius(dd, 12, 0);
  lv_obj_set_style_pad_ver(dd, 12, 0);
  lv_obj_set_style_text_font(lv_dropdown_get_list(dd), F_BODY, 0);
  lv_obj_set_style_bg_color(lv_dropdown_get_list(dd), C_CARD, 0);
  lv_obj_align(dd, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(dd, g_tank_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  return row;
}

void ui_gas_open(const std::string &key) {
  GasView g;
  if (!ui_gas_get(key, g)) {
    ui_toast("Dieser Sensor ist nicht mehr eingerichtet.");
    return;
  }
  if (s_g) lv_obj_del(s_g->ov);
  GasDlg *d = s_g = new GasDlg();
  d->key = key;
  d->mac = g.mac;
  d->mopeka = g.mopeka;

  d->ov = lv_obj_create(lv_layer_top());
  ui_plain(d->ov);
  lv_obj_set_size(d->ov, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_style_bg_color(d->ov, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(d->ov, LV_OPA_60, 0);
  lv_obj_add_flag(d->ov, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(d->ov, g_ov_click, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(d->ov, g_delete_cb, LV_EVENT_DELETE, d);

  lv_obj_t *card = lv_obj_create(d->ov);
  ui_plain(card);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(card, 600, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(card, C_CARD, 0);
  lv_obj_set_style_radius(card, 26, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, C_LINE, 0);
  lv_obj_set_style_pad_all(card, 28, 0);
  lv_obj_set_style_pad_row(card, d->mopeka ? 8 : 12, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_center(card);

  lv_obj_t *hdr = lv_obj_create(card);
  ui_plain(hdr);
  lv_obj_set_size(hdr, lv_pct(100), 64);
  d->hdr_bg = lv_obj_create(hdr);
  ui_plain(d->hdr_bg);
  lv_obj_set_size(d->hdr_bg, 64, 64);
  lv_obj_set_style_radius(d->hdr_bg, 20, 0);
  lv_obj_set_style_bg_opa(d->hdr_bg, LV_OPA_COVER, 0);
  d->hdr_icon = ui_icon(d->hdr_bg, g.water ? ICON_BATH : ICON_GAS, F_ICON_M, C_TEXT2);
  lv_obj_center(d->hdr_icon);
  d->title = ui_label(hdr, "", F_TITLE, C_TEXT);
  lv_label_set_long_mode(d->title, LV_LABEL_LONG_DOT);
  lv_obj_set_width(d->title, 600 - 56 - 64 - 20 - 130);
  lv_obj_align(d->title, LV_ALIGN_TOP_LEFT, 84, 2);
  std::string sub = g.source + "  ·  " + (g.sat.empty() ? g.mac : g.sat);
  lv_obj_t *s = ui_label(hdr, sub.c_str(), F_SMALL, C_TEXT2);
  lv_obj_align(s, LV_ALIGN_TOP_LEFT, 84, 38);
  lv_obj_t *xb = ui_icon_button(hdr, ICON_CLOSE, g_close_cb, nullptr, 56);
  lv_obj_align(xb, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_t *pb = ui_icon_button(hdr, ICON_PENCIL, g_rename_cb, nullptr, 56);
  lv_obj_align(pb, LV_ALIGN_RIGHT_MID, -64, 0);

  d->pct = ui_label(card, "", F_BIG, C_TEXT);
  lv_obj_set_style_pad_top(d->pct, 8, 0);
  d->bar = lv_bar_create(card);
  lv_obj_set_size(d->bar, lv_pct(100), 18);
  lv_bar_set_range(d->bar, 0, 100);
  lv_obj_set_style_bg_color(d->bar, C_FIELD, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(d->bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(d->bar, 9, LV_PART_MAIN);
  lv_obj_set_style_radius(d->bar, 9, LV_PART_INDICATOR);

  lv_obj_t *grp = lv_obj_create(card);
  ui_plain(grp);
  lv_obj_set_size(grp, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(grp, LV_FLEX_FLOW_COLUMN);
  const char *titles[6] = {g.water ? "Inhalt" : "Gasmenge", "Akku des Sensors", "Temperatur am Sensor", "Messqualität",
                           g.mopeka ? "Letzter Empfang" : "Letzte Messung", "Füllhöhe"};
  for (int i = 0; i < 6; i++) {
    d->rows[i] = ui_row(grp, titles[i], "", nullptr, nullptr, false);
    lv_obj_set_width(d->rows[i], 260);
    // Mopeka hat zwei Zeilen mehr (Fuellhoehe, Flasche) – niedrigere Zeilen, damit die Karte auf den Schirm passt
    if (g.mopeka) lv_obj_set_height(lv_obj_get_parent(d->rows[i]), 46);
    if (i == 5 && !g.mopeka) lv_obj_add_flag(lv_obj_get_parent(d->rows[i]), LV_OBJ_FLAG_HIDDEN);
  }
  if (g.mopeka) {
    MopekaInfo m;
    if (mopeka_get(key, m)) tank_dropdown(card, m);
  }
  d->status = ui_label(card, "", F_SMALL, C_TEXT2);
  lv_label_set_long_mode(d->status, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(d->status, lv_pct(100));

  lv_obj_t *row = lv_obj_create(card);
  ui_plain(row);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_top(row, 8, 0);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  // Mopeka sendet von selbst laufend – "Jetzt messen" gibt es nur fuer Truma
  if (!g.mopeka) ui_button(row, ICON_REFRESH, "Jetzt messen", true, g_measure_cb, nullptr);
  lv_obj_t *rm = ui_button(row, ICON_CLOSE, "Entfernen", false, g_remove_cb, nullptr);
  for (uint32_t i = 0; i < lv_obj_get_child_cnt(rm); i++) lv_obj_set_style_text_color(lv_obj_get_child(rm, i), C_RED, 0);
  ui_gas_update();
}
