// Bildschirm-Tastatur (QWERTZ mit Umlauten), Eingabedialog und Rueckfrage
#include "ui_internal.h"
#include "theme.h"
#include <Arduino.h>

// ---------------------------------------------------------------- Deutsche Tastaturbelegung
#define KB_FLAGS (LV_BTNMATRIX_CTRL_NO_REPEAT | LV_BTNMATRIX_CTRL_CLICK_TRIG | LV_BTNMATRIX_CTRL_CHECKED)

static const char *const kb_lower[] = {
    "1#", "q", "w", "e", "r", "t", "z", "u", "i", "o", "p", "ü", LV_SYMBOL_BACKSPACE, "\n",
    "ABC", "a", "s", "d", "f", "g", "h", "j", "k", "l", "ö", "ä", LV_SYMBOL_OK, "\n",
    "ß", "y", "x", "c", "v", "b", "n", "m", ",", ".", "-", "_", "@", "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, "/", ""};
static const lv_btnmatrix_ctrl_t kb_lower_ctrl[] = {
    KB_FLAGS | 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, LV_BTNMATRIX_CTRL_CHECKED | 6,
    KB_FLAGS | 6, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, KB_FLAGS | 6,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    KB_FLAGS | 5, LV_BTNMATRIX_CTRL_CHECKED | 3, 14, LV_BTNMATRIX_CTRL_CHECKED | 3, 3};

static const char *const kb_upper[] = {
    "1#", "Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P", "Ü", LV_SYMBOL_BACKSPACE, "\n",
    "abc", "A", "S", "D", "F", "G", "H", "J", "K", "L", "Ö", "Ä", LV_SYMBOL_OK, "\n",
    "'", "Y", "X", "C", "V", "B", "N", "M", ";", ":", "!", "?", "&", "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, "\\", ""};
static const lv_btnmatrix_ctrl_t kb_upper_ctrl[] = {
    KB_FLAGS | 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, LV_BTNMATRIX_CTRL_CHECKED | 6,
    KB_FLAGS | 6, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, KB_FLAGS | 6,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    KB_FLAGS | 5, LV_BTNMATRIX_CTRL_CHECKED | 3, 14, LV_BTNMATRIX_CTRL_CHECKED | 3, 3};

static const char *const kb_special[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", LV_SYMBOL_BACKSPACE, "\n",
    "abc", "+", "=", "*", "/", "%", "#", "(", ")", "€", "°", LV_SYMBOL_OK, "\n",
    "\"", "'", "<", ">", "[", "]", "{", "}", "|", "$", "~", "^", "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, "§", ""};
static const lv_btnmatrix_ctrl_t kb_special_ctrl[] = {
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, LV_BTNMATRIX_CTRL_CHECKED | 7,
    KB_FLAGS | 6, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, KB_FLAGS | 6,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    KB_FLAGS | 5, LV_BTNMATRIX_CTRL_CHECKED | 3, 14, LV_BTNMATRIX_CTRL_CHECKED | 3, 3};

static const char *const kb_num[] = {
    "1", "2", "3", LV_SYMBOL_BACKSPACE, "\n",
    "4", "5", "6", LV_SYMBOL_OK, "\n",
    "7", "8", "9", LV_SYMBOL_KEYBOARD, "\n",
    LV_SYMBOL_LEFT, "0", LV_SYMBOL_RIGHT, ".", ""};
static const lv_btnmatrix_ctrl_t kb_num_ctrl[] = {
    1, 1, 1, LV_BTNMATRIX_CTRL_CHECKED | 1,
    1, 1, 1, KB_FLAGS | 1,
    1, 1, 1, KB_FLAGS | 1,
    LV_BTNMATRIX_CTRL_CHECKED | 1, 1, LV_BTNMATRIX_CTRL_CHECKED | 1, 1};

static lv_obj_t *make_keyboard(lv_obj_t *parent, bool numeric) {
  lv_obj_t *kb = lv_keyboard_create(parent);
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER, (const char **)kb_lower, kb_lower_ctrl);
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_UPPER, (const char **)kb_upper, kb_upper_ctrl);
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_SPECIAL, (const char **)kb_special, kb_special_ctrl);
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_NUMBER, (const char **)kb_num, kb_num_ctrl);
  lv_keyboard_set_mode(kb, numeric ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_obj_set_size(kb, LV_HOR_RES, numeric ? 300 : 320);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_text_font(kb, F_BODY, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(kb, C_BAR, 0);
  lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(kb, 10, 0);
  lv_obj_set_style_pad_gap(kb, 8, 0);
  lv_obj_set_style_border_width(kb, 0, 0);
  lv_obj_set_style_radius(kb, 0, 0);
  lv_obj_set_style_bg_color(kb, C_FIELD, LV_PART_ITEMS);
  lv_obj_set_style_text_color(kb, C_TEXT, LV_PART_ITEMS);
  lv_obj_set_style_radius(kb, 10, LV_PART_ITEMS);
  lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
  lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(kb, lv_color_hex(0x2C343C), (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(kb, C_AMBER, (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_text_color(kb, C_INK, (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_PRESSED);
  return kb;
}

// ---------------------------------------------------------------- Eingabedialog
struct InputState {
  InputOptions opt;
  lv_obj_t *overlay = nullptr, *ta = nullptr, *check = nullptr, *eye_icon = nullptr;
  bool done = false;
};
static InputState *s_in = nullptr;

bool ui_input_is_open() { return s_in != nullptr; }

static void in_delete_cb(lv_event_t *e) {
  InputState *st = (InputState *)lv_event_get_user_data(e);
  if (st == s_in) s_in = nullptr;
  delete st;
}

static void in_finish(bool ok) {
  if (!s_in || s_in->done) return;
  s_in->done = true;
  std::string text = lv_textarea_get_text(s_in->ta);
  bool checked = s_in->check && lv_obj_has_state(s_in->check, LV_STATE_CHECKED);
  auto cb = s_in->opt.on_ok;
  lv_obj_del_async(s_in->overlay);
  s_in = nullptr;  // Zustand wird im DELETE-Ereignis freigegeben
  if (ok && cb) cb(text, checked);
}

static void in_kb_cb(lv_event_t *e) {
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_READY) in_finish(true);
  else if (c == LV_EVENT_CANCEL) in_finish(false);
}
static void in_ok_cb(lv_event_t *) { in_finish(true); }
static void in_cancel_cb(lv_event_t *) { in_finish(false); }
static void in_eye_cb(lv_event_t *) {
  if (!s_in) return;
  bool pw = !lv_textarea_get_password_mode(s_in->ta);
  lv_textarea_set_password_mode(s_in->ta, pw);
  lv_label_set_text(s_in->eye_icon, pw ? ICON_EYE : ICON_EYE_OFF);
}

void ui_input_open(const InputOptions &opt) {
  if (s_in) lv_obj_del(s_in->overlay);
  InputState *st = new InputState();
  s_in = st;
  st->opt = opt;

  lv_obj_t *ov = st->overlay = lv_obj_create(lv_layer_top());
  ui_plain(ov);
  lv_obj_set_size(ov, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_style_bg_color(ov, C_BG, 0);
  lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
  lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ov, in_delete_cb, LV_EVENT_DELETE, st);

  lv_obj_t *t = ui_label(ov, opt.title.c_str(), F_TITLE, C_TEXT);
  lv_obj_set_pos(t, 40, 26);
  if (!opt.hint.empty()) {
    lv_obj_t *h = ui_label(ov, opt.hint.c_str(), F_SMALL, C_TEXT2);
    lv_obj_set_width(h, 640);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(h, 40, 66);
  }
  lv_obj_t *cancel = ui_button(ov, nullptr, "Abbrechen", false, in_cancel_cb, nullptr);
  lv_obj_align(cancel, LV_ALIGN_TOP_RIGHT, -232, 22);
  lv_obj_t *ok = ui_button(ov, ICON_CHECK, "Übernehmen", true, in_ok_cb, nullptr);
  lv_obj_align(ok, LV_ALIGN_TOP_RIGHT, -40, 22);

  st->ta = lv_textarea_create(ov);
  lv_textarea_set_one_line(st->ta, true);
  lv_textarea_set_max_length(st->ta, opt.max_len);
  lv_textarea_set_text(st->ta, opt.initial.c_str());
  lv_textarea_set_password_mode(st->ta, opt.password);
  if (opt.numeric) lv_textarea_set_accepted_chars(st->ta, "0123456789.");
  lv_obj_set_size(st->ta, opt.password ? 868 : 944, 72);
  lv_obj_set_pos(st->ta, 40, 112);
  lv_obj_set_style_text_font(st->ta, F_TITLE, 0);
  lv_obj_set_style_bg_color(st->ta, C_FIELD, 0);
  lv_obj_set_style_bg_opa(st->ta, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(st->ta, 2, 0);
  lv_obj_set_style_border_color(st->ta, C_AMBER, 0);
  lv_obj_set_style_radius(st->ta, 14, 0);
  lv_obj_set_style_pad_hor(st->ta, 18, 0);
  lv_obj_set_style_pad_ver(st->ta, 16, 0);
  lv_obj_set_style_text_color(st->ta, C_TEXT, 0);
  lv_obj_set_style_bg_color(st->ta, C_AMBER, LV_PART_CURSOR);
  lv_obj_add_state(st->ta, LV_STATE_FOCUSED);

  if (opt.password) {
    lv_obj_t *eye = lv_obj_create(ov);
    ui_plain(eye);
    lv_obj_add_flag(eye, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(eye, 64, 72);
    lv_obj_set_pos(eye, 920, 112);
    lv_obj_set_style_radius(eye, 14, 0);
    lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(eye, C_FIELD, 0);
    st->eye_icon = ui_icon(eye, ICON_EYE, F_ICON_S, C_TEXT2);
    lv_obj_center(st->eye_icon);
    lv_obj_add_event_cb(eye, in_eye_cb, LV_EVENT_CLICKED, nullptr);
  }
  if (!opt.check_label.empty()) {
    st->check = lv_checkbox_create(ov);
    lv_checkbox_set_text(st->check, opt.check_label.c_str());
    lv_obj_set_style_text_font(st->check, F_BODY, 0);
    lv_obj_set_style_text_color(st->check, C_TEXT, 0);
    lv_obj_set_style_pad_column(st->check, 14, 0);
    lv_obj_set_style_border_color(st->check, C_TEXT2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(st->check, C_AMBER, (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(st->check, C_AMBER, (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_ext_click_area(st->check, 12);
    lv_obj_set_pos(st->check, 44, 204);
    if (opt.check_value) lv_obj_add_state(st->check, LV_STATE_CHECKED);
  }
  lv_obj_t *kb = make_keyboard(ov, opt.numeric);
  lv_keyboard_set_textarea(kb, st->ta);
  lv_obj_add_event_cb(kb, in_kb_cb, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(kb, in_kb_cb, LV_EVENT_CANCEL, nullptr);
}

// ---------------------------------------------------------------- Rueckfrage
struct ConfirmState {
  std::function<void()> on_ok;
  lv_obj_t *overlay;
};

static void cf_delete_cb(lv_event_t *e) { delete (ConfirmState *)lv_event_get_user_data(e); }
static void cf_cancel_cb(lv_event_t *e) {
  ConfirmState *c = (ConfirmState *)lv_event_get_user_data(e);
  lv_obj_del_async(c->overlay);
}
static void cf_ok_cb(lv_event_t *e) {
  ConfirmState *c = (ConfirmState *)lv_event_get_user_data(e);
  auto fn = c->on_ok;
  lv_obj_del_async(c->overlay);
  if (fn) fn();
}

void ui_confirm(const char *title, const char *text, const char *ok_label, bool danger, std::function<void()> on_ok) {
  ConfirmState *c = new ConfirmState();
  c->on_ok = on_ok;
  lv_obj_t *ov = c->overlay = lv_obj_create(lv_layer_top());
  ui_plain(ov);
  lv_obj_set_size(ov, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_style_bg_color(ov, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(ov, LV_OPA_60, 0);
  lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ov, cf_delete_cb, LV_EVENT_DELETE, c);

  lv_obj_t *card = lv_obj_create(ov);
  ui_plain(card);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(card, 600, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(card, C_CARD, 0);
  lv_obj_set_style_radius(card, 24, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, C_LINE, 0);
  lv_obj_set_style_pad_all(card, 30, 0);
  lv_obj_set_style_pad_row(card, 14, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_center(card);
  ui_label(card, title, F_TITLE, C_TEXT);
  lv_obj_t *t = ui_label(card, text, F_BODY, C_TEXT2);
  lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(t, lv_pct(100));
  lv_obj_t *row = lv_obj_create(card);
  ui_plain(row);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_top(row, 12, 0);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  ui_button(row, nullptr, "Abbrechen", false, cf_cancel_cb, c);
  lv_obj_t *okb = ui_button(row, nullptr, ok_label, true, cf_ok_cb, c);
  if (danger) {
    lv_obj_set_style_bg_color(okb, C_RED, 0);
    lv_obj_set_style_bg_color(okb, lv_color_darken(C_RED, 40), LV_STATE_PRESSED);
  }
}
