// Detailansicht (langes Druecken auf eine Kachel): Ein/Aus, Helligkeit, Farbtemperatur, Farbe
#include "../app_config.h"
#include "ui_internal.h"
#include "../i18n/i18n.h"
#include "theme.h"
#include "color.h"
#include "../layout.h"
#include "../z2m.h"
#include <Arduino.h>

struct Detail {
  std::string key;
  bool has_color = false;
  int ct_min = 153, ct_max = 500;
  float hue = 30, sat = 80;
  lv_obj_t *overlay = nullptr, *card = nullptr;
  lv_obj_t *hdr_bg = nullptr, *hdr_icon = nullptr, *title = nullptr, *subtitle = nullptr;
  lv_obj_t *power = nullptr, *power_icon = nullptr, *power_lbl = nullptr;
  lv_obj_t *bri = nullptr, *bri_val = nullptr, *ct = nullptr, *ct_val = nullptr;
  lv_obj_t *sat_sl = nullptr, *sat_val = nullptr, *wheel = nullptr, *preview = nullptr;
  uint32_t t_bri = 0, t_ct = 0, t_col = 0, t_touch = 0;
};
static Detail *s_d = nullptr;

bool ui_detail_is_open() { return s_d != nullptr; }

void ui_detail_close() {
  if (s_d && s_d->overlay) lv_obj_del_async(s_d->overlay);
}

static void on_overlay_delete(lv_event_t *e) {
  Detail *d = (Detail *)lv_event_get_user_data(e);
  if (d == s_d) s_d = nullptr;
  delete d;
}

static void on_overlay_click(lv_event_t *e) {
  if (lv_event_get_target(e) == lv_event_get_current_target(e)) ui_detail_close();
}

static int ct_to_slider(const Detail *d, int mired) { return d->ct_min + d->ct_max - mired; }
static int slider_to_ct(const Detail *d, int v) { return d->ct_min + d->ct_max - v; }

static void set_val_text(lv_obj_t *l, const char *fmt, int v) {
  char b[24];
  snprintf(b, sizeof(b), fmt, v);
  lv_label_set_text(l, b);
}

static void send_bri(bool force) {
  if (!s_d) return;
  uint32_t now = millis();
  if (!force && now - s_d->t_bri < 300) return;
  s_d->t_bri = now;
  z2m_set_brightness_pct(s_d->key, lv_slider_get_value(s_d->bri));
}

static void send_ct(bool force) {
  if (!s_d) return;
  uint32_t now = millis();
  if (!force && now - s_d->t_ct < 300) return;
  s_d->t_ct = now;
  z2m_set_ct(s_d->key, slider_to_ct(s_d, lv_slider_get_value(s_d->ct)));
}

static void send_color(bool force) {
  if (!s_d) return;
  uint32_t now = millis();
  if (!force && now - s_d->t_col < 300) return;
  s_d->t_col = now;
  z2m_set_color_hs(s_d->key, s_d->hue, s_d->sat);
}

static void update_preview() {
  if (!s_d || !s_d->preview) return;
  lv_obj_set_style_bg_color(s_d->preview, lv_color_hsv_to_rgb((uint16_t)s_d->hue % 360, (uint8_t)s_d->sat, 100), 0);
  if (s_d->sat_sl)
    lv_obj_set_style_bg_grad_color(s_d->sat_sl, lv_color_hsv_to_rgb((uint16_t)s_d->hue % 360, 100, 100), LV_PART_MAIN);
}

static void bri_cb(lv_event_t *e) {
  if (!s_d) return;
  s_d->t_touch = millis();
  set_val_text(s_d->bri_val, "%d\xE2\x80\xAF%%", lv_slider_get_value(s_d->bri));
  send_bri(lv_event_get_code(e) == LV_EVENT_RELEASED);
}

static void ct_cb(lv_event_t *e) {
  if (!s_d) return;
  s_d->t_touch = millis();
  int k = 1000000 / slider_to_ct(s_d, lv_slider_get_value(s_d->ct));
  set_val_text(s_d->ct_val, "%d\xE2\x80\xAFK", k / 50 * 50);
  send_ct(lv_event_get_code(e) == LV_EVENT_RELEASED);
}

static void sat_cb(lv_event_t *e) {
  if (!s_d) return;
  s_d->t_touch = millis();
  s_d->sat = lv_slider_get_value(s_d->sat_sl);
  set_val_text(s_d->sat_val, "%d\xE2\x80\xAF%%", (int)s_d->sat);
  update_preview();
  send_color(lv_event_get_code(e) == LV_EVENT_RELEASED);
}

static void wheel_cb(lv_event_t *e) {
  if (!s_d) return;
  s_d->t_touch = millis();
  lv_color_hsv_t hsv = lv_colorwheel_get_hsv(s_d->wheel);
  s_d->hue = hsv.h;
  update_preview();
  send_color(lv_event_get_code(e) == LV_EVENT_RELEASED);
}

static void power_cb(lv_event_t *) {
  if (!s_d) return;
  s_d->t_touch = 0;
  z2m_toggle(s_d->key);
}

static void close_cb(lv_event_t *) { ui_detail_close(); }

static void rename_cb(lv_event_t *) {
  if (!s_d) return;
  std::string key = s_d->key;
  ui_detail_close();
  ui_rename_device(key);
}

static lv_obj_t *section(lv_obj_t *parent, const char *icon, const char *title, lv_obj_t **val_out) {
  lv_obj_t *box = lv_obj_create(parent);
  ui_plain(box);
  lv_obj_set_size(box, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(box, 14, 0);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_t *row = lv_obj_create(box);
  ui_plain(row);
  lv_obj_set_size(row, lv_pct(100), 30);
  lv_obj_t *ic = ui_icon(row, icon, F_ICON_S, C_TEXT2);
  lv_obj_align(ic, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_t *t = ui_label(row, title, F_BODY, C_TEXT);
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 34, 0);
  *val_out = ui_label(row, "", F_BODY, C_TEXT2);
  lv_obj_align(*val_out, LV_ALIGN_RIGHT_MID, 0, 0);
  return box;
}

static lv_obj_t *make_slider(lv_obj_t *box, int min, int max, lv_event_cb_t cb, lv_color_t col) {
  lv_obj_t *s = lv_slider_create(box);
  lv_obj_set_width(s, lv_pct(100));
  ui_style_slider(s, col);
  lv_obj_set_height(s, 18);
  lv_slider_set_range(s, min, max);
  lv_obj_add_event_cb(s, cb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(s, cb, LV_EVENT_RELEASED, nullptr);
  return s;
}

void ui_detail_update() {
  if (!s_d) return;
  ControlView v;
  if (!z2m_get(s_d->key, v)) {
    ui_detail_close();
    return;
  }
  Layout lay = layout_get();
  lv_label_set_text(s_d->title, layout_label(lay, v).c_str());
  bool on = v.known && v.on;
  lv_color_t c = light_display_color(v);
  lv_obj_set_style_bg_color(s_d->hdr_bg, on ? c : C_FIELD, 0);
  lv_obj_set_style_text_color(s_d->hdr_icon, on ? (lv_color_brightness(c) > 150 ? C_INK : lv_color_white()) : C_TEXT2, 0);
  lv_obj_set_style_bg_color(s_d->power, on ? C_AMBER : C_FIELD, 0);
  lv_obj_set_style_text_color(s_d->power_icon, on ? C_INK : C_TEXT, 0);
  lv_obj_set_style_text_color(s_d->power_lbl, on ? C_INK : C_TEXT, 0);
  lv_label_set_text(s_d->power_lbl, !v.online ? "Nicht erreichbar" : (v.pending ? "…" : (on ? "An" : "Aus")));

  bool busy = millis() - s_d->t_touch < 1500;
  if (busy) return;
  if (s_d->bri && v.bri >= 0 && !lv_obj_has_state(s_d->bri, LV_STATE_PRESSED)) {
    int p = LV_MAX(1, v.bri_pct());
    lv_slider_set_value(s_d->bri, p, LV_ANIM_OFF);
    set_val_text(s_d->bri_val, "%d\xE2\x80\xAF%%", p);
  }
  if (s_d->ct && v.ct > 0 && !lv_obj_has_state(s_d->ct, LV_STATE_PRESSED)) {
    lv_slider_set_value(s_d->ct, ct_to_slider(s_d, constrain(v.ct, s_d->ct_min, s_d->ct_max)), LV_ANIM_OFF);
    set_val_text(s_d->ct_val, "%d\xE2\x80\xAFK", 1000000 / v.ct / 50 * 50);
  }
  float h, s;
  if (s_d->wheel && light_hs(v, h, s)) {
    s_d->hue = h;
    s_d->sat = s;
    lv_color_hsv_t hsv;
    hsv.h = (uint16_t)h % 360;
    hsv.s = 100;
    hsv.v = 100;
    lv_colorwheel_set_hsv(s_d->wheel, hsv);
    lv_slider_set_value(s_d->sat_sl, (int)s, LV_ANIM_OFF);
    set_val_text(s_d->sat_val, "%d\xE2\x80\xAF%%", (int)s);
    update_preview();
  }
}

void ui_detail_open(const std::string &key) {
  ControlView v;
  if (!z2m_get(key, v)) {
    ui_toast(TXT_DEV_GONE);
    return;
  }
  if (s_d) lv_obj_del(s_d->overlay);
  Detail *d = new Detail();
  s_d = d;
  d->key = key;
  d->has_color = v.kind == CTL_LIGHT && v.color != COLOR_NONE;
  d->ct_min = v.ct_min;
  d->ct_max = v.ct_max > v.ct_min ? v.ct_max : v.ct_min + 1;

  d->overlay = lv_obj_create(lv_layer_top());
  ui_plain(d->overlay);
  lv_obj_set_size(d->overlay, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_style_bg_color(d->overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(d->overlay, LV_OPA_60, 0);
  lv_obj_add_flag(d->overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(d->overlay, on_overlay_click, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(d->overlay, on_overlay_delete, LV_EVENT_DELETE, d);

  lv_obj_t *card = d->card = lv_obj_create(d->overlay);
  ui_plain(card);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(card, d->has_color ? 840 : 600, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(card, C_CARD, 0);
  lv_obj_set_style_radius(card, 26, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, C_LINE, 0);
  lv_obj_set_style_pad_all(card, 28, 0);
  lv_obj_set_style_pad_row(card, 22, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_center(card);

  // Kopf
  lv_obj_t *hdr = lv_obj_create(card);
  ui_plain(hdr);
  lv_obj_set_size(hdr, lv_pct(100), 64);
  d->hdr_bg = lv_obj_create(hdr);
  ui_plain(d->hdr_bg);
  lv_obj_set_size(d->hdr_bg, 64, 64);
  lv_obj_set_style_radius(d->hdr_bg, 20, 0);
  lv_obj_set_style_bg_opa(d->hdr_bg, LV_OPA_COVER, 0);
  Layout lay = layout_get();
  std::string icon_id;
  for (auto &p : lay.pages)
    for (auto &t : p.tiles)
      if (t.key == key && !t.icon.empty()) icon_id = t.icon;
  if (icon_id.empty()) icon_id = v.kind == CTL_SWITCH ? "plug" : "bulb";
  d->hdr_icon = ui_icon(d->hdr_bg, icon_utf8(icon_id.c_str()), F_ICON_M, C_TEXT2);
  lv_obj_center(d->hdr_icon);
  d->title = ui_label(hdr, "", F_TITLE, C_TEXT);
  lv_label_set_long_mode(d->title, LV_LABEL_LONG_DOT);
  lv_obj_set_width(d->title, (d->has_color ? 840 : 600) - 56 - 64 - 20 - 130);
  lv_obj_align(d->title, LV_ALIGN_TOP_LEFT, 84, 2);
  std::string sub = v.vendor.empty() ? v.model : v.vendor + " " + v.model;
  if (!v.ep.empty()) sub += tr("  ·  Kanal ") + v.ep;
  d->subtitle = ui_label(hdr, sub.c_str(), F_SMALL, C_TEXT2);
  lv_obj_align(d->subtitle, LV_ALIGN_TOP_LEFT, 84, 38);
  lv_obj_t *xb = ui_icon_button(hdr, ICON_CLOSE, close_cb, nullptr, 56);
  lv_obj_align(xb, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_t *pb = ui_icon_button(hdr, ICON_PENCIL, rename_cb, nullptr, 56);
  lv_obj_align(pb, LV_ALIGN_RIGHT_MID, -64, 0);

  // Inhalt: links Regler, rechts Farbrad
  lv_obj_t *body = lv_obj_create(card);
  ui_plain(body);
  lv_obj_set_size(body, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(body, 36, 0);
  lv_obj_t *left = lv_obj_create(body);
  ui_plain(left);
  lv_obj_set_height(left, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(left, 1);
  lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(left, 26, 0);

  d->power = lv_obj_create(left);
  ui_plain(d->power);
  lv_obj_add_flag(d->power, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(d->power, 220, 64);
  lv_obj_set_style_radius(d->power, 32, 0);
  lv_obj_set_style_bg_opa(d->power, LV_OPA_COVER, 0);
  lv_obj_set_style_outline_width(d->power, 3, LV_STATE_PRESSED);
  lv_obj_set_style_outline_color(d->power, lv_color_white(), LV_STATE_PRESSED);
  lv_obj_set_style_outline_opa(d->power, LV_OPA_40, LV_STATE_PRESSED);
  lv_obj_set_style_pad_hor(d->power, 22, 0);
  lv_obj_set_style_pad_column(d->power, 12, 0);
  lv_obj_set_flex_flow(d->power, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(d->power, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  d->power_icon = ui_icon(d->power, ICON_POWER, F_ICON_S, C_TEXT);
  d->power_lbl = ui_label(d->power, "", F_BODY, C_TEXT);
  lv_obj_add_event_cb(d->power, power_cb, LV_EVENT_CLICKED, nullptr);

  if (v.has_bri) {
    lv_obj_t *box = section(left, ICON_SUN, "Helligkeit", &d->bri_val);
    d->bri = make_slider(box, 1, 100, bri_cb, C_AMBER);
    lv_slider_set_value(d->bri, LV_MAX(1, v.bri_pct()), LV_ANIM_OFF);
  }
  if (v.has_ct) {
    lv_obj_t *box = section(left, ICON_THERMO, "Farbtemperatur", &d->ct_val);
    d->ct = make_slider(box, d->ct_min, d->ct_max, ct_cb, C_TEXT);
    lv_obj_set_style_bg_opa(d->ct, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(d->ct, mired_color(d->ct_max), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(d->ct, mired_color(d->ct_min), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(d->ct, LV_GRAD_DIR_HOR, LV_PART_MAIN);
    lv_slider_set_value(d->ct, ct_to_slider(d, v.ct > 0 ? constrain(v.ct, d->ct_min, d->ct_max) : (d->ct_min + d->ct_max) / 2),
                        LV_ANIM_OFF);
  }
  if (d->has_color) {
    lv_obj_t *box = section(left, ICON_PALETTE, "Sättigung", &d->sat_val);
    d->sat_sl = make_slider(box, 0, 100, sat_cb, C_TEXT);
    lv_obj_set_style_bg_opa(d->sat_sl, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(d->sat_sl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(d->sat_sl, LV_GRAD_DIR_HOR, LV_PART_MAIN);

    lv_obj_t *right = lv_obj_create(body);
    ui_plain(right);
    lv_obj_set_size(right, 270, 270);
    d->wheel = lv_colorwheel_create(right, true);
    lv_obj_set_size(d->wheel, 270, 270);
    lv_obj_center(d->wheel);
    lv_colorwheel_set_mode(d->wheel, LV_COLORWHEEL_MODE_HUE);
    lv_colorwheel_set_mode_fixed(d->wheel, true);
    lv_obj_set_style_arc_width(d->wheel, 34, LV_PART_MAIN);
    lv_obj_set_style_pad_all(d->wheel, 8, LV_PART_KNOB);
    lv_obj_set_style_border_width(d->wheel, 3, LV_PART_KNOB);
    lv_obj_set_style_border_color(d->wheel, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_ext_click_area(d->wheel, 10);
    lv_obj_add_event_cb(d->wheel, wheel_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(d->wheel, wheel_cb, LV_EVENT_RELEASED, nullptr);
    d->preview = lv_obj_create(right);
    ui_plain(d->preview);
    lv_obj_set_size(d->preview, 150, 150);
    lv_obj_set_style_radius(d->preview, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(d->preview, LV_OPA_COVER, 0);
    lv_obj_center(d->preview);
    lv_color_hsv_t hsv;
    hsv.h = 30; hsv.s = 100; hsv.v = 100;
    lv_colorwheel_set_hsv(d->wheel, hsv);
    lv_slider_set_value(d->sat_sl, (int)d->sat, LV_ANIM_OFF);
    update_preview();
  }
  if (!v.has_bri && !v.has_ct && !d->has_color) {
    lv_obj_t *hint = ui_label(left, v.kind == CTL_SWITCH ? "Schaltbares Gerät ohne weitere Einstellungen."
                                                          : "Diese Lampe lässt sich nur ein- und ausschalten.",
                              F_SMALL, C_TEXT2);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, lv_pct(100));
  }
  d->t_touch = 0;
  ui_detail_update();
}
