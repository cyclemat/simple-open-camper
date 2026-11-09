// Truma-Heizung (CP Plus per LIN, iNet X per Bluetooth): Kachel-Texte und Bedienung
#include "ui_internal.h"
#include "../i18n/i18n.h"
#include "theme.h"
#include "../layout.h"
#include "../truma_cpplus.h"
#include "../truma_inetx.h"
#include "../alde.h"
#include "../settings.h"
#include <Arduino.h>

// iNet X zaehlt nur, wenn es eingebaut und in den Einstellungen eingeschaltet ist
static bool inetx_on() { return inetx_enabled() && ble_proto_on(BLE_P_INETX); }

bool ui_heat_get(const std::string &key, TrumaHeatInfo &out) {
  if (key == TRUMA_HEAT_KEY_CPPLUS && cpplus_enabled()) return cpplus_get(out);
  if (key == TRUMA_HEAT_KEY_INETX && inetx_on()) return inetx_get(out);
  if (key == TRUMA_HEAT_KEY_ALDE && alde_enabled()) return alde_get(out);
  return false;
}

std::vector<TrumaHeatInfo> ui_heat_list() {
  std::vector<TrumaHeatInfo> out;
  TrumaHeatInfo h;
  if (cpplus_enabled() && cpplus_get(h)) out.push_back(h);
  if (inetx_on() && inetx_get(h)) out.push_back(h);
  if (alde_enabled() && alde_get(h)) out.push_back(h);
  return out;
}

uint32_t ui_heat_version() { return (cpplus_version() * 31 + inetx_version()) * 31 + alde_version(); }

std::string ui_temp_text(float c) {
  if (isnan(c)) return "–";
  char b[16];
  snprintf(b, sizeof(b), "%.1f\xE2\x80\xAF°C", c);
  for (char *p = b; *p; p++)
    if (*p == '.') *p = ',';
  return b;
}

const char *ui_water_text(TrumaWater w) {
  switch (w) {
    case TW_ECO: return tr("Eco");
    case TW_HOT: return tr("Heiß");
    case TW_BOOST: return tr("Boost");
    default: return tr("aus");
  }
}

std::string ui_heat_label(const Layout &lay, const TrumaHeatInfo &h) {
  auto it = lay.names.find(h.key);
  if (it != lay.names.end() && !it->second.empty()) return it->second;
  return tr("Heizung");
}

// Zeile unter dem Namen auf der Kachel
std::string ui_heat_sub(const TrumaHeatInfo &h, bool small) {
  if (!h.connected && !h.has) return h.status;
  std::string s = h.heat_on ? tr("Ziel ") + ui_temp_text(h.room_target) : "Heizung aus";
  if (!small) s += std::string(tr("  ·  Boiler ")) + (!h.water_hot && h.water == TW_ECO ? tr("an") : ui_water_text(h.water));
  if (h.error) s = h.status;
  return s;
}

// Steuerbefehle an das passende Bedienteil
static void heat_set(const std::string &key, bool on, float t, uint8_t mode) {
  if (key == TRUMA_HEAT_KEY_CPPLUS) cpplus_set_heat(on, t, mode);
  else if (key == TRUMA_HEAT_KEY_ALDE) alde_set_heat(on, t);
  else inetx_set_heat(on, t, mode);
}
static void water_set(const std::string &key, TrumaWater w) {
  if (key == TRUMA_HEAT_KEY_CPPLUS) cpplus_set_water(w);
  else if (key == TRUMA_HEAT_KEY_ALDE) alde_set_water(w);
  else inetx_set_water(w);
}
static void energy_set(const std::string &key, TrumaEnergy e, uint16_t p) {
  if (key == TRUMA_HEAT_KEY_CPPLUS) cpplus_set_energy(e, p);
  else if (key == TRUMA_HEAT_KEY_ALDE) {
    alde_set_gas(e == TE_GAS || e == TE_MIX);
    alde_set_electric(e == TE_GAS || e == TE_NONE ? 0 : p / 1000);
  } else inetx_set_energy(e, p);
}

// ---------------------------------------------------------------- Detailansicht
struct HeatDlg {
  std::string key;
  lv_obj_t *ov = nullptr, *title = nullptr, *cur = nullptr, *ctrl = nullptr, *status = nullptr;
  float target = NAN;           // am Panel eingestellt, noch nicht gesendet
  lv_timer_t *send_timer = nullptr;
  uint32_t sig = 0;
};
static HeatDlg *s_h = nullptr;

bool ui_heat_is_open() { return s_h != nullptr; }
void ui_heat_close() {
  if (s_h && s_h->ov) lv_obj_del_async(s_h->ov);
}

static void h_delete_cb(lv_event_t *e) {
  HeatDlg *d = (HeatDlg *)lv_event_get_user_data(e);
  if (d->send_timer) lv_timer_del(d->send_timer);
  if (d == s_h) s_h = nullptr;
  delete d;
}
static void h_ov_click(lv_event_t *e) {
  if (lv_event_get_target(e) == lv_event_get_current_target(e)) ui_heat_close();
}
static void h_close_cb(lv_event_t *) { ui_heat_close(); }

static bool current(TrumaHeatInfo &h) {
  if (!s_h || !ui_heat_get(s_h->key, h)) return false;
  if (!h.has) {
    ui_toast("Noch kein Status vom Bedienteil – bitte kurz warten.");
    return false;
  }
  return true;
}

static void send_target(lv_timer_t *t) {
  if (!s_h) return;
  s_h->send_timer = nullptr;
  TrumaHeatInfo h;
  if (!ui_heat_get(s_h->key, h)) return;
  heat_set(s_h->key, true, s_h->target, h.heat_mode);
  s_h->target = NAN;
  lv_timer_del(t);
}

static void h_onoff_cb(lv_event_t *) {
  TrumaHeatInfo h;
  if (!current(h)) return;
  float t = isnan(h.room_target) ? 20 : h.room_target;
  heat_set(s_h->key, !h.heat_on, t, h.heat_mode);
}

static void h_step_cb(lv_event_t *e) {
  TrumaHeatInfo h;
  if (!current(h)) return;
  int dir = (int)(intptr_t)lv_event_get_user_data(e);
  float base = !isnan(s_h->target) ? s_h->target : isnan(h.room_target) ? 20 : h.room_target;
  float t = base + dir * 0.5f;
  s_h->target = t < 5 ? 5 : t > 30 ? 30 : t;
  // Erst senden, wenn eine Weile nicht mehr getippt wurde – sonst gehen viele Befehle raus
  if (s_h->send_timer) lv_timer_reset(s_h->send_timer);
  else s_h->send_timer = lv_timer_create(send_target, 1500, nullptr);
  s_h->sig = 0;
  ui_heat_update();
}

static void h_mode_cb(lv_event_t *e) {
  TrumaHeatInfo h;
  if (!current(h)) return;
  float t = isnan(h.room_target) ? 20 : h.room_target;
  heat_set(s_h->key, true, t, (uint8_t)(intptr_t)lv_event_get_user_data(e));
}

static void h_water_cb(lv_event_t *e) {
  TrumaHeatInfo h;
  if (!current(h)) return;
  water_set(s_h->key, (TrumaWater)(intptr_t)lv_event_get_user_data(e));
}

// Energie: 0 Gas, 1 Mix 900, 2 Mix 1800, 3 Elektro 900, 4 Elektro 1800
static void h_energy_cb(lv_event_t *e) {
  TrumaHeatInfo h;
  if (!current(h)) return;
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  static const TrumaEnergy E[5] = {TE_GAS, TE_MIX, TE_MIX, TE_ELECTRIC, TE_ELECTRIC};
  static const uint16_t P[5] = {0, 900, 1800, 900, 1800};
  energy_set(s_h->key, E[i], P[i]);
}

// Alde: Gas und Elektro unabhaengig
static void h_alde_gas_cb(lv_event_t *e) {
  TrumaHeatInfo h;
  if (!current(h)) return;
  alde_set_gas(lv_event_get_user_data(e) != nullptr);
}
static void h_alde_el_cb(lv_event_t *e) {
  TrumaHeatInfo h;
  if (!current(h)) return;
  alde_set_electric((uint8_t)(intptr_t)lv_event_get_user_data(e));
}

static lv_obj_t *ctl_row(lv_obj_t *parent, const char *title) {
  lv_obj_t *r = lv_obj_create(parent);
  ui_plain(r);
  lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, 10, 0);
  lv_obj_set_style_pad_row(r, 8, 0);
  lv_obj_t *t = ui_label(r, title, F_BODY, C_TEXT2);
  lv_obj_set_width(t, 120);
  return r;
}

static void build_controls(const TrumaHeatInfo &h) {
  lv_obj_t *c = s_h->ctrl;
  lv_obj_clean(c);
  // Heizung: an/aus, Solltemperatur
  lv_obj_t *r = ctl_row(c, "Heizung");
  ui_button(r, ICON_POWER, h.heat_on ? "An" : "Aus", h.heat_on, h_onoff_cb, nullptr);
  ui_icon_button(r, ICON_LEFT, h_step_cb, (void *)(intptr_t)-1, 52);
  float t = !isnan(s_h->target) ? s_h->target : h.room_target;
  lv_obj_t *tl = ui_label(r, ui_temp_text(t).c_str(), F_TITLE, isnan(s_h->target) ? C_TEXT : C_AMBER);
  lv_obj_set_width(tl, 120);
  lv_obj_set_style_text_align(tl, LV_TEXT_ALIGN_CENTER, 0);
  ui_icon_button(r, ICON_RIGHT, h_step_cb, (void *)(intptr_t)1, 52);
  // Modus
  if (h.mode_count > 1) {
    r = ctl_row(c, "Modus");
    for (int i = 0; i < h.mode_count; i++)
      ui_button(r, nullptr, h.mode_names[i], h.heat_on && h.heat_mode == i, h_mode_cb, (void *)(intptr_t)i);
  }
  // Boiler
  r = ctl_row(c, "Boiler");
  for (int w = TW_OFF; w <= TW_BOOST; w++) {
    if (w == TW_BOOST && !h.water_boost) continue;
    if (w == TW_HOT && !h.water_hot) continue;
    const char *n = w == TW_OFF ? "Aus" : w == TW_ECO ? (h.water_hot ? "Eco 40°" : "An") : w == TW_HOT ? "Heiß 60°" : "Boost";
    ui_button(r, nullptr, n, h.water == w, h_water_cb, (void *)(intptr_t)w);
  }
  // Energie
  if (h.energy_style == HES_ALDE) {
    bool gas = h.energy == TE_GAS || h.energy == TE_MIX;
    r = ctl_row(c, "Gas");
    ui_button(r, nullptr, "An", gas, h_alde_gas_cb, (void *)1);
    ui_button(r, nullptr, "Aus", !gas, h_alde_gas_cb, nullptr);
    r = ctl_row(c, "Elektro");
    static const char *N[4] = {"Aus", "1 kW", "2 kW", "3 kW"};
    for (int i = 0; i < 4; i++) ui_button(r, nullptr, N[i], h.el_power / 1000 == i, h_alde_el_cb, (void *)(intptr_t)i);
    return;
  }
  r = ctl_row(c, "Energie");
  int sel = h.energy == TE_GAS || h.el_power == 0 ? 0 : (h.energy == TE_MIX ? 1 : 3) + (h.el_power >= 1800 ? 1 : 0);
  if (h.energy_writable) {
    static const char *N[5] = {"Gas", "Mix 900 W", "Mix 1800 W", "Elektro 900 W", "Elektro 1800 W"};
    for (int i = 0; i < 5; i++) ui_button(r, nullptr, N[i], sel == i, h_energy_cb, (void *)(intptr_t)i);
  } else {
    // iNet X: Gas oder Elektro folgt der Heizung, waehlbar ist nur die Elektrostufe
    int lvl = h.el_power >= 1800 ? 2 : h.el_power ? 1 : 0;
    static const char *N[3] = {"Elektro aus", "900 W", "1800 W"};
    static const int MAP[3] = {0, 1, 2};
    for (int i = 0; i < 3; i++) ui_button(r, nullptr, N[i], lvl == i, h_energy_cb, (void *)(intptr_t)MAP[i]);
  }
}

void ui_heat_update() {
  if (!s_h) return;
  TrumaHeatInfo h;
  if (!ui_heat_get(s_h->key, h)) {
    ui_heat_close();
    return;
  }
  Layout lay = layout_get();
  (lv_label_set_text)(s_h->title, ui_heat_label(lay, h).c_str());
  std::string cur = tr("Innen ") + ui_temp_text(h.room_actual);
  if (!isnan(h.outdoor)) cur += tr("   ·   Außen ") + ui_temp_text(h.outdoor);
  if (!isnan(h.water_actual)) cur += tr("   ·   Wasser ") + ui_temp_text(h.water_actual);
  if (!isnan(h.fluid)) cur += tr("   ·   Glykol ") + ui_temp_text(h.fluid);
  if (!isnan(h.battery_v)) {
    char b[24];
    snprintf(b, sizeof(b), tr("   ·   %.1f V"), h.battery_v);
    for (char *p = b; *p; p++)
      if (*p == '.') *p = ',';
    cur += b;
  }
  lv_label_set_text(s_h->cur, cur.c_str());
  std::string st = h.status == "OK" ? (h.running ? "Heizt gerade" : "Bereit") : h.status;
  if (h.busy && h.status == "OK") st = "Befehl wird übertragen …";
  lv_label_set_text(s_h->status, st.c_str());
  // Bedienelemente nur neu bauen, wenn sich etwas Sichtbares geaendert hat
  uint32_t sig = 17;
  auto mix = [&sig](int x) { sig = sig * 31 + (uint32_t)x; };
  mix(h.heat_on); mix(h.heat_mode); mix(h.water); mix(h.energy); mix(h.el_power); mix(h.has); mix(h.energy_style);
  mix(isnan(h.room_target) ? -1 : (int)(h.room_target * 10)); mix(isnan(s_h->target) ? -1 : (int)(s_h->target * 10));
  if (sig != s_h->sig) {
    s_h->sig = sig;
    build_controls(h);
  }
}

void ui_heat_open(const std::string &key) {
  TrumaHeatInfo h;
  if (!ui_heat_get(key, h)) {
    ui_toast("Diese Heizung ist in der Firmware nicht eingeschaltet.");
    return;
  }
  if (s_h) lv_obj_del(s_h->ov);
  HeatDlg *d = s_h = new HeatDlg();
  d->key = key;
  d->ov = lv_obj_create(lv_layer_top());
  ui_plain(d->ov);
  lv_obj_set_size(d->ov, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_style_bg_color(d->ov, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(d->ov, LV_OPA_60, 0);
  lv_obj_add_flag(d->ov, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(d->ov, h_ov_click, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(d->ov, h_delete_cb, LV_EVENT_DELETE, d);

  lv_obj_t *card = lv_obj_create(d->ov);
  ui_plain(card);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(card, 860, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(card, C_CARD, 0);
  lv_obj_set_style_radius(card, 26, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, C_LINE, 0);
  lv_obj_set_style_pad_all(card, 26, 0);
  lv_obj_set_style_pad_row(card, 14, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_center(card);

  lv_obj_t *hdr = lv_obj_create(card);
  ui_plain(hdr);
  lv_obj_set_size(hdr, lv_pct(100), 64);
  lv_obj_t *ib = lv_obj_create(hdr);
  ui_plain(ib);
  lv_obj_set_size(ib, 64, 64);
  lv_obj_set_style_radius(ib, 20, 0);
  lv_obj_set_style_bg_opa(ib, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(ib, h.running ? C_AMBER : C_FIELD, 0);
  lv_obj_t *ic = ui_icon(ib, ICON_FLAME, F_ICON_M, h.running ? C_INK : C_TEXT2);
  lv_obj_center(ic);
  d->title = ui_label(hdr, "", F_TITLE, C_TEXT);
  lv_obj_align(d->title, LV_ALIGN_TOP_LEFT, 84, 2);
  lv_obj_t *s = ui_label(hdr, h.source.c_str(), F_SMALL, C_TEXT2);
  lv_obj_align(s, LV_ALIGN_TOP_LEFT, 84, 38);
  lv_obj_t *xb = ui_icon_button(hdr, ICON_CLOSE, h_close_cb, nullptr, 56);
  lv_obj_align(xb, LV_ALIGN_RIGHT_MID, 0, 0);

  d->cur = ui_label(card, "", F_BODY, C_TEXT);
  d->ctrl = lv_obj_create(card);
  ui_plain(d->ctrl);
  lv_obj_set_size(d->ctrl, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(d->ctrl, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(d->ctrl, 12, 0);
  d->status = ui_label(card, "", F_SMALL, C_TEXT2);
  lv_label_set_long_mode(d->status, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(d->status, lv_pct(100));
  ui_heat_update();
}
