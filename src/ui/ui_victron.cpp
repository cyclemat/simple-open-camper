// Victron (Instant Readout): Texte fuer Kacheln und Listen, Detailansicht
#include "ui_internal.h"
#include "../i18n/i18n.h"
#include "theme.h"
#include "../layout.h"
#include "../victron.h"
#include "../cerbo.h"
#include "../bms.h"
#include "../panelsat.h"
#include <Arduino.h>
#include <math.h>

static const int32_t STALE_S = 120;  // so lange ohne Paket gilt ein Geraet als nicht erreichbar

bool ui_vic_get(const std::string &key, VictronInfo &out) {
  if (victron_get(key, out)) return true;   // Panel-Bluetooth und Cerbo GX
  return panelsat_victron_get(key, out);
}

// Nicht am Panel eingerichtet (Cerbo GX, Satellit): kein Schluessel/Entfernen, Name nur fuer das Panel
static bool remote(const std::string &key, const VictronInfo &v) { return v.gx || !panelsat_sat_of(key).empty(); }

std::string ui_num(float v, int decimals, const char *unit) {
  if (isnan(v)) return "–";
  char b[32];
  snprintf(b, sizeof(b), "%.*f\xE2\x80\xAF%s", decimals, (double)v, unit);
  for (char *p = b; *p; p++)
    if (*p == '.') *p = ',';
  // "-" durch echtes Minuszeichen ersetzen
  if (b[0] == '-') return std::string("\xE2\x88\x92") + (b + 1);
  return b;
}

static std::string remaining_text(int min) {
  if (min < 0) return "–";
  char b[32];
  if (min >= 14400) return "über 10 Tage";
  if (min >= 1440) snprintf(b, sizeof(b), "%d d %d h", min / 1440, min / 60 % 24);
  else snprintf(b, sizeof(b), "%d h %02d min", min / 60, min % 60);
  return b;
}

const char *ui_vic_icon(VicKind k) {
  switch (k) {
    case VIC_SOLAR: return ICON_SUN;
    case VIC_SHUNT: return ICON_BATTERY;
    case VIC_DCDC: return ICON_LIGHTNING;
    case VIC_GRID: return ICON_PLUG;
    case VIC_CHARGER: return ICON_LIGHTNING;
    case VIC_DCSYS: return ICON_BULB;
    case VIC_INVERTER: return ICON_POWER;
    case VIC_PSTATION: return ICON_BATTERY;
    default: return ICON_BLUETOOTH;
  }
}

std::string ui_vic_label(const Layout &lay, const VictronInfo &v) {
  auto it = lay.names.find(v.key);
  if (it != lay.names.end() && !it->second.empty()) return it->second;
  return v.name.empty() ? "Victron" : v.name;
}

static bool charging_state(int s) { return s == 3 || s == 4 || s == 5 || s == 6 || s == 7 || s == 246 || s == 247; }

static std::string watt(float w) { return isnan(w) ? "–" : std::to_string((int)lroundf(w)) + "\xE2\x80\xAFW"; }

VicTileText ui_vic_tile_text(const VictronInfo &v, bool small, bool big) {
  VicTileText t;
  t.accent = C_TEXT3;
  t.stale = v.has && (v.age_s < 0 || v.age_s > STALE_S);
  if (!v.has) {
    t.value = "…";
    t.sub = v.status;
    return t;
  }
  switch (v.kind) {
    case VIC_SOLAR: {
      t.value = isnan(v.pv_w) ? "–" : std::to_string((int)lroundf(v.pv_w)) + "\xE2\x80\xAFW";
      t.accent = (!isnan(v.pv_w) && v.pv_w > 0) ? C_AMBER : C_TEXT2;
      t.sub = victron_state_text(v.state);
      if (!small) t.sub += "  ·  " + ui_num(v.bat_v, 1, "V");
      if (big) t.sub += tr("  ·  heute ") + ui_num(v.yield_kwh, 2, "kWh");
      break;
    }
    case VIC_SHUNT: {
      t.value = isnan(v.soc) ? "–" : std::to_string((int)lroundf(v.soc)) + "\xE2\x80\xAF%";
      if (!isnan(v.soc)) {
        t.fill = (int)lroundf(v.soc);
        t.accent = ui_gas_color(t.fill);
      }
      t.sub = ui_num(v.bat_v, 2, "V");
      if (!small) t.sub += "  ·  " + (v.gx && !isnan(v.power_w) ? watt(v.power_w) : ui_num(v.bat_i, 1, "A"));
      if (big && v.remaining_min >= 0) t.sub += "  ·  " + remaining_text(v.remaining_min);
      break;
    }
    case VIC_DCDC: {
      t.value = ui_num(v.bat_v, 1, "V");
      t.accent = v.state == 0 ? C_TEXT2 : charging_state(v.state) ? C_GREEN : C_BLUE;
      t.sub = victron_state_text(v.state);
      if (!small && !isnan(v.bat_i)) t.sub += "  ·  " + ui_num(v.bat_i, 1, "A");
      if (big && !isnan(v.in_v)) t.sub += tr("  ·  Eingang ") + ui_num(v.in_v, 1, "V");
      break;
    }
    case VIC_GRID: {   // Cerbo GX: Netz/Landstrom
      bool conn = v.ac_connected != 0;
      t.value = conn ? watt(v.ac_w) : "–";
      t.accent = !conn ? C_TEXT2 : (!isnan(v.ac_w) && v.ac_w > 5) ? C_BLUE : C_GREEN;
      t.sub = !conn ? std::string(tr("nicht verbunden"))
                    : (v.ac_source > 0 && v.ac_source != 240) ? std::string(tr(cerbo_source_text(v.ac_source))) : std::string(tr("verbunden"));
      if (conn && !small) t.sub += "  ·  " + ui_num(v.ac_v, 0, "V");
      if (conn && big && !isnan(v.ac_limit)) t.sub += tr("  ·  max. ") + ui_num(v.ac_limit, 1, "A");
      break;
    }
    case VIC_CHARGER: {   // Cerbo GX: Multi/Quattro oder Netzladegeraet
      t.value = !isnan(v.power_w) ? watt(v.power_w) : ui_num(v.bat_i, 1, "A");
      t.accent = v.state <= 0 ? C_TEXT2 : charging_state(v.state) ? C_GREEN : v.state == 2 ? C_RED : C_BLUE;
      t.sub = tr(victron_state_text(v.state));
      if (!small && v.mode > 0) t.sub += std::string("  ·  ") + tr(cerbo_mode_text(v.mode));
      if (big && !isnan(v.bat_i)) t.sub += "  ·  " + ui_num(v.bat_i, 1, "A");
      break;
    }
    case VIC_INVERTER: {   // Cerbo GX: Wechselrichter-Ausgang
      bool on = v.state == 9 || v.state == 10 || (!isnan(v.ac_w) && v.ac_w > 5);
      t.value = watt(v.ac_w);
      t.accent = on ? C_AMBER : C_TEXT2;
      t.sub = tr(victron_state_text(v.state));
      if (!small) t.sub += "  ·  " + ui_num(v.ac_v, 0, "V");
      if (big && !isnan(v.ac_i)) t.sub += "  ·  " + ui_num(v.ac_i, 1, "A");
      break;
    }
    case VIC_PSTATION: {   // Power Station: Ladezustand, darunter rein/raus
      t.value = isnan(v.soc) ? "–" : std::to_string((int)lroundf(v.soc)) + "\xE2\x80\xAF%";
      if (!isnan(v.soc)) {
        t.fill = (int)lroundf(v.soc);
        t.accent = ui_gas_color(t.fill);
      }
      t.sub = small ? tr("raus ") + watt(v.ps_out_w) : tr("rein ") + watt(v.ps_in_w) + tr("  ·  raus ") + watt(v.ps_out_w);
      if (big && v.ac_on >= 0) t.sub += v.ac_on ? tr("  ·  AC ein") : tr("  ·  AC aus");
      break;
    }
    case VIC_DCSYS:   // Cerbo GX: DC-Verbraucher; per Bluetooth: SmartShunt als DC-Energiezaehler
      t.value = watt(v.power_w);
      t.accent = (!isnan(v.power_w) && fabsf(v.power_w) > 2) ? C_AMBER : C_TEXT2;
      t.sub = ui_num(v.bat_i, 1, "A");
      if (!small) t.sub += "  ·  " + ui_num(v.bat_v, 1, "V");
      if (!v.gx && big && *victron_meter_text(v.meter)) t.sub += std::string("  ·  ") + tr(victron_meter_text(v.meter));
      break;
    default:
      t.value = "–";
      t.sub = v.status;
      break;
  }
  if (t.stale) t.sub = "kein Empfang";
  if (v.error > 0) {
    t.sub = tr("Fehler ") + std::to_string(v.error);
    t.accent = C_RED;
  }
  return t;
}

std::string ui_vic_summary(const VictronInfo &v) {
  if (!v.has) return v.status;
  if (v.age_s < 0 || v.age_s > STALE_S) return "kein Empfang";
  switch (v.kind) {
    case VIC_SOLAR: return ui_num(v.pv_w, 0, "W") + ", " + ui_num(v.bat_v, 2, "V") + ", " + victron_state_text(v.state);
    case VIC_SHUNT: return ui_num(v.soc, 0, "%") + ", " + ui_num(v.bat_v, 2, "V") + ", " + ui_num(v.bat_i, 1, "A");
    case VIC_DCDC: return ui_num(v.bat_v, 2, "V") + ", " + victron_state_text(v.state);
    case VIC_GRID: return v.ac_connected == 0 ? std::string(tr("nicht verbunden")) : ui_num(v.ac_w, 0, "W") + ", " + ui_num(v.ac_v, 0, "V");
    case VIC_CHARGER: return ui_num(v.power_w, 0, "W") + ", " + tr(victron_state_text(v.state));
    case VIC_DCSYS: return ui_num(v.power_w, 0, "W") + ", " + ui_num(v.bat_i, 1, "A");
    case VIC_INVERTER: return ui_num(v.ac_w, 0, "W") + ", " + tr(victron_state_text(v.state));
    case VIC_PSTATION:
      return ui_num(v.soc, 0, "%") + tr(", rein ") + ui_num(v.ps_in_w, 0, "W") + tr(", raus ") + ui_num(v.ps_out_w, 0, "W");
    default: return v.status;
  }
}

// ---------------------------------------------------------------- Detailansicht
static const int MAX_ROWS = 8;
struct VicDlg {
  std::string key, mac;
  VicKind kind = VIC_UNKNOWN;
  lv_obj_t *ov = nullptr, *hdr_bg = nullptr, *hdr_icon = nullptr, *title = nullptr, *big = nullptr, *bar = nullptr;
  lv_obj_t *rows[MAX_ROWS] = {nullptr};
  int nrows = 0;
  lv_obj_t *status = nullptr;
  // Bedienen ueber den Cerbo GX (MultiPlus/Quattro bzw. Ladegeraet): nur wenn der GX es als einstellbar meldet
  lv_obj_t *lim_lbl = nullptr;
  float lim_val = NAN;
  uint32_t edit_ms = 0;              // letzte eigene Eingabe: so lange keine Werte vom GX uebernehmen
  lv_timer_t *lim_timer = nullptr;   // Begrenzung erst kurz nach dem letzten Tippen senden
  lv_obj_t *mode_btn[4] = {nullptr};
  int mode_val[4] = {0};
  int nmode = 0;
  // Power Station: AC-/DC-Ausgang schalten (bms_set_output)
  lv_obj_t *ps_btn[2][2] = {{nullptr, nullptr}, {nullptr, nullptr}};
};
static VicDlg *s_v = nullptr;

bool ui_vic_is_open() { return s_v != nullptr; }
void ui_vic_close() {
  if (s_v && s_v->ov) lv_obj_del_async(s_v->ov);
}

static void v_delete_cb(lv_event_t *e) {
  VicDlg *d = (VicDlg *)lv_event_get_user_data(e);
  if (d == s_v) s_v = nullptr;
  if (d->lim_timer) {   // noch nicht gesendete Begrenzung jetzt senden
    lv_timer_del(d->lim_timer);
    if (!isnan(d->lim_val)) cerbo_set_limit(d->key, d->lim_val);
  }
  delete d;
}

// ---------------------------------------------------------------- Bedienen ueber den Cerbo GX
static void ctl_show_limit() {
  if (s_v && s_v->lim_lbl) lv_label_set_text(s_v->lim_lbl, isnan(s_v->lim_val) ? "–" : ui_num(s_v->lim_val, 1, "A").c_str());
}

static void ctl_show_mode(int mode) {
  if (!s_v) return;
  for (int i = 0; i < s_v->nmode; i++) {
    bool on = s_v->mode_val[i] == mode;
    lv_obj_set_style_bg_color(s_v->mode_btn[i], on ? C_AMBER : C_FIELD, 0);
    lv_obj_t *l = lv_obj_get_child(s_v->mode_btn[i], 0);
    if (l) lv_obj_set_style_text_color(l, on ? C_INK : C_TEXT, 0);
  }
}

static void ctl_send_cb(lv_timer_t *t) {
  VicDlg *d = (VicDlg *)t->user_data;
  d->lim_timer = nullptr;
  lv_timer_del(t);
  if (!isnan(d->lim_val) && !cerbo_set_limit(d->key, d->lim_val)) ui_toast("Der GX nimmt die Begrenzung gerade nicht an.");
  d->edit_ms = lv_tick_get();
}

static void ctl_step_cb(lv_event_t *e) {
  if (!s_v || isnan(s_v->lim_val)) return;
  int dir = (int)(intptr_t)lv_event_get_user_data(e);
  float v = roundf(s_v->lim_val) + dir;   // ganze Ampere
  s_v->lim_val = v < 0 ? 0 : v > 100 ? 100 : v;
  s_v->edit_ms = lv_tick_get();
  ctl_show_limit();
  if (s_v->lim_timer) lv_timer_reset(s_v->lim_timer);
  else s_v->lim_timer = lv_timer_create(ctl_send_cb, 800, s_v);
}

static void ctl_mode_cb(lv_event_t *e) {
  if (!s_v) return;
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i < 0 || i >= s_v->nmode) return;
  s_v->edit_ms = lv_tick_get();
  if (cerbo_set_mode(s_v->key, s_v->mode_val[i])) ctl_show_mode(s_v->mode_val[i]);
  else ui_toast("Der GX nimmt den Schalter gerade nicht an.");
}

static lv_obj_t *ctl_btn(lv_obj_t *parent, const char *txt, int w, lv_event_cb_t cb, intptr_t ud) {
  lv_obj_t *b = lv_obj_create(parent);
  ui_plain(b);
  lv_obj_set_size(b, w, 56);
  lv_obj_set_style_radius(b, 16, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, C_FIELD, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, C_LINE, 0);
  lv_obj_set_style_border_color(b, C_AMBER, LV_STATE_PRESSED);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)ud);
  lv_obj_t *l = ui_label(b, txt, F_BODY, C_TEXT);
  lv_obj_center(l);
  return b;
}

static void ctl_build(lv_obj_t *card, const VictronInfo &v) {
  VicDlg *d = s_v;
  if (v.can_limit) {
    lv_obj_t *r = lv_obj_create(card);
    ui_plain(r);
    lv_obj_set_size(r, lv_pct(100), 60);
    lv_obj_t *t = ui_label(r, "Netzstrom-Begrenzung", F_BODY, C_TEXT2);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *plus = ctl_btn(r, "+", 72, ctl_step_cb, 1);
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, 0, 0);
    d->lim_lbl = ui_label(r, "", F_TITLE, C_TEXT);
    lv_obj_set_width(d->lim_lbl, 120);
    lv_obj_set_style_text_align(d->lim_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(d->lim_lbl, LV_ALIGN_RIGHT_MID, -80, 0);
    lv_obj_t *minus = ctl_btn(r, "\xE2\x80\x93", 72, ctl_step_cb, -1);
    lv_obj_align(minus, LV_ALIGN_RIGHT_MID, -208, 0);
    d->lim_val = v.ac_limit;
    ctl_show_limit();
  }
  if (v.can_mode) {
    static const char *MP_TXT[4] = {"Ein", "Aus", "Nur Laden", "Nur Wechselrichter"};
    static const int MP_VAL[4] = {3, 4, 1, 2};
    static const char *CH_TXT[2] = {"Ein", "Aus"};
    static const int CH_VAL[2] = {1, 4};
    bool mp = v.mode_set == 0;
    d->nmode = mp ? 4 : 2;
    lv_obj_t *r = lv_obj_create(card);
    ui_plain(r);
    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(r, 8, 0);
    int w = (640 - 48 - 8 * (d->nmode - 1)) / d->nmode;
    for (int i = 0; i < d->nmode; i++) {
      d->mode_val[i] = mp ? MP_VAL[i] : CH_VAL[i];
      d->mode_btn[i] = ctl_btn(r, mp ? MP_TXT[i] : CH_TXT[i], w, ctl_mode_cb, i);
    }
    ctl_show_mode(v.mode);
  }
}

// ---------------------------------------------------------------- Power Station: Ausgaenge schalten
static void ps_show(int which, int8_t on) {
  if (!s_v) return;
  for (int k = 0; k < 2; k++) {
    lv_obj_t *b = s_v->ps_btn[which][k];
    if (!b) continue;
    bool sel = on >= 0 && (k == 0) == (on == 1);
    lv_obj_set_style_bg_color(b, sel ? C_AMBER : C_FIELD, 0);
    lv_obj_t *l = lv_obj_get_child(b, 0);
    if (l) lv_obj_set_style_text_color(l, sel ? C_INK : C_TEXT, 0);
  }
}

static void ps_cb(lv_event_t *e) {
  if (!s_v) return;
  int x = (int)(intptr_t)lv_event_get_user_data(e);   // Ausgang * 2 + (0 ein, 1 aus)
  int which = x / 2;
  bool on = (x % 2) == 0;
  if (bms_set_output(s_v->mac, which == 0, on)) {
    ps_show(which, on ? 1 : 0);
    ui_toast("Wird geschaltet – die Power Station meldet den neuen Zustand gleich.");
  } else {
    ui_toast("Dieses Gerät lässt sich nicht schalten.");
  }
}

static void ps_build(lv_obj_t *card, const VictronInfo &v) {
  const char *name[2] = {"AC-Ausgang", "DC-Ausgang"};
  bool can[2] = {v.can_ac, v.can_dc};
  for (int w = 0; w < 2; w++) {
    if (!can[w]) continue;
    lv_obj_t *r = lv_obj_create(card);
    ui_plain(r);
    lv_obj_set_size(r, lv_pct(100), 60);
    lv_obj_t *t = ui_label(r, name[w], F_BODY, C_TEXT2);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 0, 0);
    s_v->ps_btn[w][1] = ctl_btn(r, "Aus", 120, ps_cb, w * 2 + 1);
    lv_obj_align(s_v->ps_btn[w][1], LV_ALIGN_RIGHT_MID, 0, 0);
    s_v->ps_btn[w][0] = ctl_btn(r, "Ein", 120, ps_cb, w * 2);
    lv_obj_align(s_v->ps_btn[w][0], LV_ALIGN_RIGHT_MID, -128, 0);
  }
  ps_show(0, v.ac_on);
  ps_show(1, v.dc_on);
}

// einmal pro Aktualisierung: Werte vom GX uebernehmen, ausser direkt nach einer eigenen Eingabe
static void ctl_sync(const VictronInfo &v) {
  if (!s_v || (!s_v->lim_lbl && !s_v->nmode)) return;
  if (s_v->lim_timer || (s_v->edit_ms && lv_tick_elaps(s_v->edit_ms) < 3000)) return;
  if (s_v->lim_lbl) {
    s_v->lim_val = v.ac_limit;
    ctl_show_limit();
  }
  if (s_v->nmode) ctl_show_mode(v.mode);
}
static void v_ov_click(lv_event_t *e) {
  if (lv_event_get_target(e) == lv_event_get_current_target(e)) ui_vic_close();
}
static void v_close_cb(lv_event_t *) { ui_vic_close(); }

static void v_rename_cb(lv_event_t *) {
  if (!s_v) return;
  VictronInfo v;
  if (!ui_vic_get(s_v->key, v)) return;
  std::string mac = v.mac;
  if (remote(s_v->key, v)) {   // Cerbo GX/Satellit: nur der Name auf dem Panel
    std::string key = s_v->key, def = v.name;
    InputOptions o;
    o.title = "Umbenennen";
    o.hint = "Der Name gilt nur für dieses Panel. Leer lassen für den Standardnamen.";
    o.initial = ui_vic_label(layout_get(), v);
    o.max_len = 48;
    o.on_ok = [key, def](const std::string &t, bool) {
      layout_set_name(key, (t.empty() || t == def) ? "" : t);
      ui_toast("Name gespeichert.");
    };
    ui_vic_close();
    ui_input_open(o);
    return;
  }
  InputOptions o;
  o.title = "Victron-Gerät umbenennen";
  o.hint = "Zum Beispiel „Solar Dach“ oder „Bordbatterie“.";
  o.initial = v.name;
  o.max_len = 32;
  bool bms = v.bms;
  o.on_ok = [mac, bms](const std::string &t, bool) {
    if (t.empty()) return;
#if WITH_BMS
    if (bms) {
      bms_rename(mac, t);
      return;
    }
#endif
    victron_rename(mac, t);
  };
  ui_vic_close();
  ui_input_open(o);
}

static void v_key_cb(lv_event_t *) {
  if (!s_v) return;
  std::string mac = s_v->mac;
  InputOptions o;
  o.title = "Schlüssel ändern";
  o.hint = "32 Zeichen aus VictronConnect: Einstellungen > Produktinfo > Instant Readout > Details anzeigen.";
  o.max_len = 47;
  o.on_ok = [mac](const std::string &t, bool) {
    std::string err = victron_set_key(mac, t);
    ui_toast(err.empty() ? "Schlüssel gespeichert." : err.c_str());
  };
  ui_vic_close();
  ui_input_open(o);
}

static void v_remove_cb(lv_event_t *) {
  if (!s_v) return;
  std::string mac = s_v->mac;
  bool bms = s_v->key.rfind(BMS_KEY_PREFIX, 0) == 0;
  ui_vic_close();
  ui_confirm("Gerät entfernen?", "Das Panel wertet dieses Gerät dann nicht mehr aus. Die Kachel bleibt leer, bis du sie im Editor entfernst.",
             "Entfernen", true, [mac, bms] {
#if WITH_BMS
               if (bms) {
                 bms_remove(mac);
                 return;
               }
#endif
               (void)bms;
               victron_remove(mac);
             });
}

static int row_titles(const VictronInfo &v, const char **t) {
  VicKind k = v.kind;
  switch (k) {
    case VIC_SOLAR: {
      static const char *a[] = {"Batteriespannung", "Ladestrom", "Ertrag heute", "Ladephase", "Laststrom", "Empfang"};
      memcpy(t, a, sizeof(a));
      return 6;
    }
    case VIC_SHUNT: {
      static const char *a[] = {"Spannung", "Strom", "Leistung", "Verbraucht", "Restlaufzeit", "Zusatzeingang", "Empfang"};
      memcpy(t, a, sizeof(a));
      return 7;
    }
    case VIC_DCDC: {
      static const char *a[] = {"Zustand", "Eingang Spannung", "Eingang Strom", "Ausgang Spannung", "Ausgang Strom", "Empfang"};
      memcpy(t, a, sizeof(a));
      return 6;
    }
    case VIC_GRID: {
      static const char *a[] = {"Eingang", "Leistung", "Spannung", "Strom", "Strombegrenzung", "Verbindung"};
      memcpy(t, a, sizeof(a));
      return 6;
    }
    case VIC_CHARGER: {
      if (!v.gx) {   // Blue Smart / Phoenix Smart Charger per Bluetooth
        static const char *b[] = {"Zustand", "Leistung", "Ausgang Spannung", "Ladestrom", "Netzstrom (230 V)", "Temperatur", "Empfang"};
        memcpy(t, b, sizeof(b));
        return 7;
      }
      static const char *a[] = {"Zustand", "Schalter", "Leistung", "Batteriespannung", "Ladestrom", "Verbindung"};
      memcpy(t, a, sizeof(a));
      return 6;
    }
    case VIC_INVERTER: {
      static const char *a[] = {"Zustand", "Leistung", "Spannung", "Strom", "Batteriespannung", "Verbindung"};
      memcpy(t, a, sizeof(a));
      return 6;
    }
    case VIC_PSTATION: {
      static const char *a[] = {"Eingang gesamt", "Ausgang gesamt", "AC-Ausgang", "DC-Ausgang",
                                "AC-Eingang (Netz)", "DC-Eingang (Solar/Auto)", "Restlaufzeit", "Empfang"};
      memcpy(t, a, sizeof(a));
      return 8;
    }
    case VIC_DCSYS: {
      if (!v.gx) {   // SmartShunt als DC-Energiezaehler per Bluetooth
        static const char *b[] = {"Art", "Leistung", "Strom", "Spannung", "Empfang"};
        memcpy(t, b, sizeof(b));
        return 5;
      }
      static const char *a[] = {"Leistung", "Strom", "Batteriespannung", "Verbindung"};
      memcpy(t, a, sizeof(a));
      return 4;
    }
    default: {
      static const char *a[] = {"Empfang"};
      memcpy(t, a, sizeof(a));
      return 1;
    }
  }
}

static std::string reception(const VictronInfo &v) {
  if (v.bms) {   // Akku mit BMS: Zustand der Verbindung aus bms.cpp
    if (v.age_s < 0) return v.status.empty() ? std::string("noch nichts empfangen") : v.status;
    char ago[32];
    snprintf(ago, sizeof(ago), tr("vor %d s"), (int)v.age_s);
    return v.age_s < 5 ? std::string(tr("gerade eben")) : v.age_s < 60 ? std::string(ago) : ui_age_text(v.age_s);
  }
  if (v.gx) {
    if (v.age_s == 0) return std::string(tr("Cerbo GX verbunden"));
    return v.status.empty() ? std::string("–") : v.status;
  }
  if (v.age_s < 0) return "noch nichts empfangen";
  char ago[32];
  snprintf(ago, sizeof(ago), tr("vor %d s"), (int)v.age_s);   // Wortstellung je Sprache ("5 s ago")
  std::string s = v.age_s < 5 ? tr("gerade eben") : v.age_s < 60 ? std::string(ago) : ui_age_text(v.age_s);
  const char *q = v.rssi > -65 ? "stark" : v.rssi > -80 ? "mittel" : "schwach";
  return s + tr(", Signal ") + q;
}

// Werte zu row_titles() (gleiche Reihenfolge); ohne Messwert "–", nur "Empfang" bleibt
static void row_values(const VictronInfo &v, std::string *val, int n) {
  switch (v.kind) {
    case VIC_SOLAR:
      val[0] = ui_num(v.bat_v, 2, "V");
      val[1] = ui_num(v.bat_i, 1, "A");
      val[2] = ui_num(v.yield_kwh, 2, "kWh");
      val[3] = victron_state_text(v.state);
      val[4] = ui_num(v.load_i, 1, "A");
      val[5] = reception(v);
      break;
    case VIC_SHUNT: {
      val[0] = ui_num(v.bat_v, 2, "V");
      val[1] = ui_num(v.bat_i, 2, "A");
      val[2] = (v.gx && !isnan(v.power_w)) ? ui_num(v.power_w, 0, "W") : (isnan(v.bat_v) || isnan(v.bat_i)) ? "–" : ui_num(v.bat_v * v.bat_i, 0, "W");
      val[3] = isnan(v.consumed_ah) ? "–" : ui_num(-v.consumed_ah, 1, "Ah");
      val[4] = remaining_text(v.remaining_min);
      if (v.aux_mode == 0) val[5] = tr("Starterbatterie ") + ui_num(v.aux, 2, "V");
      else if (v.aux_mode == 1) val[5] = tr("Mittelpunkt ") + ui_num(v.aux, 2, "V");
      else if (v.aux_mode == 2) val[5] = ui_num(v.aux, 1, "°C");
      else val[5] = "nicht belegt";
      val[6] = reception(v);
      break;
    }
    case VIC_DCDC:
      val[0] = victron_state_text(v.state);
      val[1] = ui_num(v.in_v, 2, "V");
      val[2] = ui_num(v.in_i, 1, "A");
      val[3] = ui_num(v.bat_v, 2, "V");
      val[4] = ui_num(v.bat_i, 1, "A");
      val[5] = reception(v);
      break;
    case VIC_GRID:
      val[0] = v.ac_connected == 0 ? std::string(tr("nicht verbunden")) : std::string(tr(cerbo_source_text(v.ac_source)));
      val[1] = ui_num(v.ac_w, 0, "W");
      val[2] = ui_num(v.ac_v, 0, "V");
      val[3] = ui_num(v.ac_i, 1, "A");
      val[4] = ui_num(v.ac_limit, 1, "A");
      val[5] = reception(v);
      break;
    case VIC_CHARGER:
      if (!v.gx) {
        val[0] = tr(victron_state_text(v.state));
        val[1] = ui_num(v.power_w, 0, "W");
        val[2] = ui_num(v.bat_v, 2, "V");
        val[3] = ui_num(v.bat_i, 1, "A");
        val[4] = ui_num(v.ac_i, 1, "A");
        val[5] = ui_num(v.aux, 0, "°C");
        val[6] = reception(v);
        break;
      }
      val[0] = tr(victron_state_text(v.state));
      val[1] = tr(cerbo_mode_text(v.mode));
      val[2] = ui_num(v.power_w, 0, "W");
      val[3] = ui_num(v.bat_v, 2, "V");
      val[4] = ui_num(v.bat_i, 1, "A");
      val[5] = reception(v);
      break;
    case VIC_INVERTER:
      val[0] = tr(victron_state_text(v.state));
      val[1] = ui_num(v.ac_w, 0, "W");
      val[2] = ui_num(v.ac_v, 0, "V");
      val[3] = ui_num(v.ac_i, 1, "A");
      val[4] = ui_num(v.bat_v, 2, "V");
      val[5] = reception(v);
      break;
    case VIC_PSTATION: {
      auto sw = [](int8_t on) { return on < 0 ? std::string() : std::string(on ? tr("  ·  ein") : tr("  ·  aus")); };
      val[0] = ui_num(v.ps_in_w, 0, "W");
      val[1] = ui_num(v.ps_out_w, 0, "W");
      val[2] = ui_num(v.ps_ac_out_w, 0, "W") + sw(v.ac_on);
      val[3] = ui_num(v.ps_dc_out_w, 0, "W") + sw(v.dc_on);
      val[4] = ui_num(v.ps_ac_in_w, 0, "W");
      val[5] = ui_num(v.ps_dc_in_w, 0, "W");
      val[6] = remaining_text(v.remaining_min);
      val[7] = reception(v);
      break;
    }
    case VIC_DCSYS:
      if (!v.gx) {
        val[0] = *victron_meter_text(v.meter) ? std::string(tr(victron_meter_text(v.meter))) : std::string("–");
        val[1] = ui_num(v.power_w, 0, "W");
        val[2] = ui_num(v.bat_i, 2, "A");
        val[3] = ui_num(v.bat_v, 2, "V");
        val[4] = reception(v);
        break;
      }
      val[0] = ui_num(v.power_w, 0, "W");
      val[1] = ui_num(v.bat_i, 1, "A");
      val[2] = ui_num(v.bat_v, 2, "V");
      val[3] = reception(v);
      break;
    default:
      val[0] = reception(v);
      break;
  }
  if (!v.has)
    for (int i = 0; i < n - 1; i++) val[i] = "–";
}

// Bezeichnung und Wert aller Zeilen des Detailfensters (rundes Panel: eine Zeile je Datenseite)
std::vector<std::pair<std::string, std::string>> ui_vic_rows(const VictronInfo &v) {
  const char *titles[MAX_ROWS];
  std::string val[MAX_ROWS];
  int n = row_titles(v, titles);
  row_values(v, val, n);
  std::vector<std::pair<std::string, std::string>> out;
  for (int i = 0; i < n; i++) out.push_back({tr(titles[i]), val[i]});
  return out;
}

void ui_vic_update() {
  if (!s_v) return;
  VictronInfo v;
  if (!ui_vic_get(s_v->key, v)) {
    ui_vic_close();
    return;
  }
  if (v.kind != s_v->kind) {  // Geraeteart erst jetzt bekannt: neu aufbauen
    std::string key = s_v->key;
    lv_obj_del(s_v->ov);
    ui_vic_open(key);
    return;
  }
  Layout lay = layout_get();
  (lv_label_set_text)(s_v->title, ui_vic_label(lay, v).c_str());
  VicTileText tt = ui_vic_tile_text(v, false, false);
  lv_obj_set_style_bg_color(s_v->hdr_bg, v.has ? tt.accent : C_FIELD, 0);
  lv_obj_set_style_text_color(s_v->hdr_icon, v.has ? C_INK : C_TEXT2, 0);
  lv_label_set_text(s_v->big, tt.value.c_str());
  if (s_v->bar) {
    lv_bar_set_value(s_v->bar, tt.fill < 0 ? 0 : tt.fill, LV_ANIM_ON);
    lv_obj_set_style_bg_color(s_v->bar, tt.accent, LV_PART_INDICATOR);
  }
  std::string val[MAX_ROWS];
  row_values(v, val, s_v->nrows);
  for (int i = 0; i < s_v->nrows; i++) lv_label_set_text(s_v->rows[i], val[i].c_str());
  std::string st = v.status == "OK" ? "" : v.status;
  if (v.gx) st = (v.status == "OK" || v.age_s == 0) ? "" : v.status;
  if (v.error > 0) st = tr("Gerät meldet Fehler ") + std::to_string(v.error) + tr(" – Details in VictronConnect.");
  lv_label_set_text(s_v->status, st.c_str());
  ctl_sync(v);
  if (v.kind == VIC_PSTATION) {
    ps_show(0, v.ac_on);
    ps_show(1, v.dc_on);
  }
}

void ui_vic_open(const std::string &key) {
  VictronInfo v;
  if (!ui_vic_get(key, v)) {
    ui_toast("Dieses Victron-Gerät ist nicht mehr eingerichtet.");
    return;
  }
  if (s_v) lv_obj_del(s_v->ov);
  VicDlg *d = s_v = new VicDlg();
  d->key = key;
  d->mac = v.mac;
  d->kind = v.kind;

  d->ov = lv_obj_create(lv_layer_top());
  ui_plain(d->ov);
  lv_obj_set_size(d->ov, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_style_bg_color(d->ov, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(d->ov, LV_OPA_60, 0);
  lv_obj_add_flag(d->ov, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(d->ov, v_ov_click, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(d->ov, v_delete_cb, LV_EVENT_DELETE, d);

  lv_obj_t *card = lv_obj_create(d->ov);
  ui_plain(card);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(card, 640, LV_SIZE_CONTENT);
  lv_obj_set_style_max_height(card, LV_VER_RES - 24, 0);
  lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(card, C_CARD, 0);
  lv_obj_set_style_radius(card, 26, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, C_LINE, 0);
  lv_obj_set_style_pad_all(card, 24, 0);
  lv_obj_set_style_pad_row(card, 8, 0);
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
  d->hdr_icon = ui_icon(d->hdr_bg, ui_vic_icon(v.kind), F_ICON_M, C_TEXT2);
  lv_obj_center(d->hdr_icon);
  d->title = ui_label(hdr, "", F_TITLE, C_TEXT);
  lv_label_set_long_mode(d->title, LV_LABEL_LONG_DOT);
  lv_obj_set_width(d->title, 230);  // rechts daneben steht der grosse Wert
  // nur der Name (keine graue Unterzeile: die lief unter den grossen Wert rechts)
  lv_obj_align(d->title, LV_ALIGN_LEFT_MID, 84, 0);
  lv_obj_t *xb = ui_icon_button(hdr, ICON_CLOSE, v_close_cb, nullptr, 56);
  lv_obj_align(xb, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_t *pb = ui_icon_button(hdr, ICON_PENCIL, v_rename_cb, nullptr, 56);
  lv_obj_align(pb, LV_ALIGN_RIGHT_MID, -64, 0);
  // grosser Wert oben rechts im Kopf
  d->big = ui_label(hdr, "", F_BIG, C_TEXT);
  lv_obj_align(d->big, LV_ALIGN_RIGHT_MID, -136, 0);

  if (v.kind == VIC_SHUNT || v.kind == VIC_PSTATION) {
    d->bar = lv_bar_create(card);
    lv_obj_set_size(d->bar, lv_pct(100), 14);
    lv_bar_set_range(d->bar, 0, 100);
    lv_obj_set_style_bg_color(d->bar, C_FIELD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(d->bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(d->bar, 7, LV_PART_MAIN);
    lv_obj_set_style_radius(d->bar, 7, LV_PART_INDICATOR);
  }

  lv_obj_t *grp = lv_obj_create(card);
  ui_plain(grp);
  lv_obj_set_size(grp, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(grp, LV_FLEX_FLOW_COLUMN);
  const char *titles[MAX_ROWS];
  d->nrows = row_titles(v, titles);
  for (int i = 0; i < d->nrows; i++) {
    lv_obj_t *r = nullptr;
    d->rows[i] = ui_row(grp, titles[i], "", nullptr, nullptr, false, &r);
    lv_obj_set_height(r, 44);
    lv_obj_set_width(d->rows[i], 330);
  }
  d->status = ui_label(card, "", F_SMALL, C_TEXT2);
  lv_label_set_long_mode(d->status, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(d->status, lv_pct(100));
  if (v.gx && (v.can_limit || v.can_mode)) ctl_build(card, v);   // MultiPlus/Ladegeraet ueber den GX bedienen
  if (v.bms && v.kind == VIC_PSTATION && (v.can_ac || v.can_dc)) ps_build(card, v);   // Power Station: Ausgaenge

  lv_obj_t *row = lv_obj_create(card);
  ui_plain(row);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_top(row, 4, 0);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  if (remote(key, v)) {   // Cerbo GX/Satellit: eingerichtet wird dort
    lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
  } else {
    if (!v.bms) ui_button(row, ICON_LOCK, "Schlüssel ändern", false, v_key_cb, nullptr);   // Akku (BMS): kein Schluessel
    lv_obj_t *rm = ui_button(row, ICON_CLOSE, "Entfernen", false, v_remove_cb, nullptr);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(rm); i++) lv_obj_set_style_text_color(lv_obj_get_child(rm, i), C_RED, 0);
  }
  ui_vic_update();
}
