// Victron (Instant Readout): Texte fuer Kacheln und Listen, Detailansicht
#include "ui_internal.h"
#include "../i18n/i18n.h"
#include "theme.h"
#include "../layout.h"
#include "../victron.h"
#include "../cerbo.h"
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
};
static VicDlg *s_v = nullptr;

bool ui_vic_is_open() { return s_v != nullptr; }
void ui_vic_close() {
  if (s_v && s_v->ov) lv_obj_del_async(s_v->ov);
}

static void v_delete_cb(lv_event_t *e) {
  VicDlg *d = (VicDlg *)lv_event_get_user_data(e);
  if (d == s_v) s_v = nullptr;
  delete d;
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
  o.on_ok = [mac](const std::string &t, bool) {
    if (!t.empty()) victron_rename(mac, t);
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
  ui_vic_close();
  ui_confirm("Gerät entfernen?", "Das Panel wertet dieses Victron-Gerät dann nicht mehr aus. Die Kachel bleibt leer, bis du sie im Editor entfernst.",
             "Entfernen", true, [mac] { victron_remove(mac); });
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
        val[5] = ui_num(v.aux, 0, "Â°" "C");
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
    for (int i = 0; i < s_v->nrows - 1; i++) val[i] = "–";
  for (int i = 0; i < s_v->nrows; i++) lv_label_set_text(s_v->rows[i], val[i].c_str());
  std::string st = v.status == "OK" ? "" : v.status;
  if (v.gx) st = (v.status == "OK" || v.age_s == 0) ? "" : v.status;
  if (v.error > 0) st = tr("Gerät meldet Fehler ") + std::to_string(v.error) + tr(" – Details in VictronConnect.");
  lv_label_set_text(s_v->status, st.c_str());
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
  lv_obj_align(d->title, LV_ALIGN_TOP_LEFT, 84, 2);
  std::string via = panelsat_sat_of(key).empty() ? v.mac : panelsat_via_text(key);
  std::string sub = std::string(tr(victron_kind_text(v.kind))) + "  ·  " + via;
  lv_obj_t *s = ui_label(hdr, sub.c_str(), F_SMALL, C_TEXT2);
  lv_obj_align(s, LV_ALIGN_TOP_LEFT, 84, 38);
  lv_obj_t *xb = ui_icon_button(hdr, ICON_CLOSE, v_close_cb, nullptr, 56);
  lv_obj_align(xb, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_t *pb = ui_icon_button(hdr, ICON_PENCIL, v_rename_cb, nullptr, 56);
  lv_obj_align(pb, LV_ALIGN_RIGHT_MID, -64, 0);
  // grosser Wert oben rechts im Kopf
  d->big = ui_label(hdr, "", F_BIG, C_TEXT);
  lv_obj_align(d->big, LV_ALIGN_RIGHT_MID, -136, 0);

  if (v.kind == VIC_SHUNT) {
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

  lv_obj_t *row = lv_obj_create(card);
  ui_plain(row);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_top(row, 4, 0);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  if (remote(key, v)) {   // Cerbo GX/Satellit: eingerichtet wird dort
    lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
  } else {
    ui_button(row, ICON_LOCK, "Schlüssel ändern", false, v_key_cb, nullptr);
    lv_obj_t *rm = ui_button(row, ICON_CLOSE, "Entfernen", false, v_remove_cb, nullptr);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(rm); i++) lv_obj_set_style_text_color(lv_obj_get_child(rm, i), C_RED, 0);
  }
  ui_vic_update();
}
