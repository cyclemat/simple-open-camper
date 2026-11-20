#include "theme.h"
#include "../app_config.h"

void theme_init() {
  lv_disp_t *d = lv_disp_get_default();
  lv_theme_t *th = lv_theme_default_init(d, C_BLUE, C_AMBER, true, F_BODY);
  lv_disp_set_theme(d, th);
  lv_obj_set_style_bg_color(lv_scr_act(), C_BG, 0);
  lv_obj_set_style_text_color(lv_scr_act(), C_TEXT, 0);
}

void ui_plain(lv_obj_t *o) {
  lv_obj_remove_style_all(o);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *ui_label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, text ? text : "");
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  return l;
}

lv_obj_t *ui_icon(lv_obj_t *parent, const char *icon, const lv_font_t *font, lv_color_t color) {
  return ui_label(parent, icon, font, color);
}

lv_obj_t *ui_button(lv_obj_t *parent, const char *icon, const char *text, bool primary, lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = lv_obj_create(parent);
  ui_plain(b);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_height(b, 56);
  lv_obj_set_width(b, LV_SIZE_CONTENT);
  lv_obj_set_style_radius(b, 14, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, primary ? C_AMBER : C_FIELD, 0);
  lv_obj_set_style_bg_color(b, primary ? C_AMBER_D : C_LINE, LV_STATE_PRESSED);
  lv_obj_set_style_pad_hor(b, 22, 0);
  lv_obj_set_style_pad_column(b, 10, 0);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_color_t fg = primary ? C_INK : C_TEXT;
  if (icon) ui_icon(b, icon, F_ICON_S, fg);
  if (text) ui_label(b, text, F_BODY, fg);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}

lv_obj_t *ui_icon_button(lv_obj_t *parent, const char *icon, lv_event_cb_t cb, void *ud, int size) {
  lv_obj_t *b = lv_obj_create(parent);
  ui_plain(b);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(b, size, size);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(b, C_FIELD, LV_STATE_PRESSED);
  lv_obj_set_ext_click_area(b, 8);
  lv_obj_t *i = ui_icon(b, icon, F_ICON_S, C_TEXT2);
  lv_obj_center(i);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}

lv_obj_t *ui_row(lv_obj_t *parent, const char *title, const char *value, lv_event_cb_t cb, void *ud, bool chevron,
                 lv_obj_t **row_out) {
  lv_obj_t *r = lv_obj_create(parent);
  ui_plain(r);
  lv_obj_set_size(r, lv_pct(100), 64);
  lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(r, 1, 0);
  lv_obj_set_style_border_color(r, C_LINE, 0);
  lv_obj_set_style_pad_hor(r, 4, 0);
  if (cb) {
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(r, C_FIELD, LV_STATE_PRESSED);
    lv_obj_add_event_cb(r, cb, LV_EVENT_CLICKED, ud);
  }
  lv_obj_t *t = ui_label(r, title, F_BODY, C_TEXT);
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_t *v = ui_label(r, value, F_BODY, C_TEXT2);
  lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_width(v, 380);
  lv_obj_align(v, LV_ALIGN_RIGHT_MID, chevron ? -40 : -8, 0);
  if (chevron) {
    lv_obj_t *c = ui_icon(r, ICON_RIGHT, F_ICON_S, C_TEXT3);
    lv_obj_align(c, LV_ALIGN_RIGHT_MID, -6, 0);
  }
  if (row_out) *row_out = r;
  return v;
}

void ui_style_slider(lv_obj_t *s, lv_color_t color) {
  lv_obj_set_height(s, 14);
  lv_obj_set_style_bg_color(s, C_FIELD, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(s, 7, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s, color, LV_PART_INDICATOR);
  lv_obj_set_style_radius(s, 7, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(s, lv_color_white(), LV_PART_KNOB);
  lv_obj_set_style_pad_all(s, 11, LV_PART_KNOB);
  lv_obj_set_style_shadow_width(s, 0, LV_PART_KNOB);
  lv_obj_set_style_border_width(s, 0, LV_PART_KNOB);
  lv_obj_set_ext_click_area(s, 22);
}

static lv_obj_t *s_toast = nullptr;
static lv_timer_t *s_toast_timer = nullptr;

static void toast_hide(lv_timer_t *t) {
  if (s_toast) lv_obj_del(s_toast);
  s_toast = nullptr;
  s_toast_timer = nullptr;
  lv_timer_del(t);
}

void ui_toast(const char *text) {
  if (s_toast) lv_obj_del(s_toast);
  if (s_toast_timer) lv_timer_del(s_toast_timer);
  s_toast = lv_obj_create(lv_layer_top());
  ui_plain(s_toast);
  lv_obj_set_size(s_toast, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(s_toast, lv_color_hex(0x2B333B), 0);
  lv_obj_set_style_radius(s_toast, 24, 0);
  lv_obj_set_style_pad_hor(s_toast, 24, 0);
  lv_obj_set_style_pad_ver(s_toast, 14, 0);
  lv_obj_t *l = ui_label(s_toast, text, F_BODY, C_TEXT);
#if UI_ROUND
  // rundes Display: schmal und weiter oben, sonst schneidet der Kreis die Meldung ab
  lv_obj_set_style_pad_hor(s_toast, 16, 0);
  lv_obj_set_style_max_width(l, 236, 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0, -56);
#else
  lv_obj_set_style_max_width(l, 860, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0, -28);
#endif
  s_toast_timer = lv_timer_create(toast_hide, 2800, nullptr);
}
