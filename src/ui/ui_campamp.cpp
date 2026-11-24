// CampAmp: Musikfenster im Stil der klassischen MP3-Player der 90er (eigene Gestaltung, nur LVGL-Grundformen,
// keine Bilder). Gruene Anzeige mit Laufzeit und Spektrum, Lauftext, kbps/kHz, Lautstaerke, Spulen, Tasten,
// Zufall/Wiederholen und Titelliste. Oeffnet bei langem Druck auf die Kachel "Musik" (audio.cpp).
#include "ui_internal.h"
#include "../audio.h"
#if WITH_AUDIO
#include "../i18n/i18n.h"
#include "theme.h"
#include <Arduino.h>

namespace {
// Farben (fest, eigener Retro-Look)
const lv_color_t K_FRAME_A = lv_color_hex(0x3A3F55), K_FRAME_B = lv_color_hex(0x1C1F2B), K_EDGE = lv_color_hex(0x6B7190);
const lv_color_t K_LCD = lv_color_hex(0x000000), K_GREEN = lv_color_hex(0x00E83A), K_GREEN_D = lv_color_hex(0x0B4A16);
const lv_color_t K_GOLD = lv_color_hex(0xE4C66A), K_BTN_A = lv_color_hex(0xD6D8DE), K_BTN_B = lv_color_hex(0x8C909C);
const lv_color_t K_INK = lv_color_hex(0x15171E), K_DIM = lv_color_hex(0x4A5060);
const int W = 900, H_MAIN = 372, H_PL = 196;

struct Amp {
  lv_obj_t *ov = nullptr, *card = nullptr;
  lv_obj_t *state = nullptr, *time = nullptr, *title = nullptr, *kbps = nullptr, *khz = nullptr, *mono = nullptr, *stereo = nullptr;
  lv_obj_t *bar[AUDIO_BANDS] = {nullptr}, *peak[AUDIO_BANDS] = {nullptr};
  float pk[AUDIO_BANDS] = {0};
  lv_obj_t *seek = nullptr, *vol = nullptr, *shuf = nullptr, *rep = nullptr, *plbtn = nullptr, *pl = nullptr;
  lv_timer_t *tm = nullptr;
  std::string last_title;
  int last_idx = -2, last_count = -1;
  bool pl_open = false;
};
Amp *s_a = nullptr;
const int SPEC_H = 74;
}  // namespace

static lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, lv_color_t bg, int r = 0) {
  lv_obj_t *o = lv_obj_create(p);
  ui_plain(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, r, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(o, bg, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}
static lv_obj_t *txt(lv_obj_t *p, const char *t, const lv_font_t *f, lv_color_t c) {
  lv_obj_t *l = lv_label_create(p);
  lv_label_set_text(l, t);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, c, 0);
  return l;
}
static lv_obj_t *lcd(lv_obj_t *p, int x, int y, int w, int h) {   // schwarzes Anzeigefeld, eingelassen
  lv_obj_t *o = box(p, x, y, w, h, K_LCD, 3);
  lv_obj_set_style_border_width(o, 2, 0);
  lv_obj_set_style_border_color(o, lv_color_hex(0x0A0B10), 0);
  lv_obj_set_style_outline_width(o, 1, 0);
  lv_obj_set_style_outline_color(o, K_EDGE, 0);
  return o;
}

static void close_cb(lv_event_t *) {
  if (s_a && s_a->ov) lv_obj_del_async(s_a->ov);
}
static void ov_cb(lv_event_t *e) {
  if (lv_event_get_target(e) == lv_event_get_current_target(e)) close_cb(e);
}
static void del_cb(lv_event_t *) {
  if (!s_a) return;
  if (s_a->tm) lv_timer_del(s_a->tm);
  audio_spectrum_on(false);
  delete s_a;
  s_a = nullptr;
}

static void btn_cb(lv_event_t *e) {
  switch ((int)(intptr_t)lv_event_get_user_data(e)) {
    case 0: audio_prev(); break;
    case 1: {
      AudioState st = audio_state();
      if (!st.playing) st.paused ? audio_toggle() : audio_play(-1);
      break;
    }
    case 2:
      if (audio_state().playing) audio_toggle();
      break;
    case 3: audio_stop(); break;
    case 4: audio_next(); break;
  }
}

// Taste wie frueher: hell gewoelbt, Symbol dunkel
static lv_obj_t *key(lv_obj_t *p, int x, int y, int w, int h, const char *sym, lv_event_cb_t cb, intptr_t ud) {
  lv_obj_t *b = box(p, x, y, w, h, K_BTN_A, 4);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_grad_color(b, K_BTN_B, 0);
  lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(0x2A2D38), 0);
  lv_obj_set_style_bg_color(b, K_BTN_B, LV_STATE_PRESSED);
  lv_obj_set_style_bg_grad_color(b, K_BTN_A, LV_STATE_PRESSED);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)ud);
  lv_obj_t *l = txt(b, sym, &lv_font_montserrat_28, K_INK);
  lv_obj_center(l);
  return b;
}

// kleiner Schalter mit Leuchtpunkt (Zufall, Wiederholen, Liste)
static lv_obj_t *toggle(lv_obj_t *p, int x, int y, int w, const char *label, lv_event_cb_t cb) {
  lv_obj_t *b = box(p, x, y, w, 40, lv_color_hex(0x2B2F3E), 4);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, K_EDGE, 0);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *led = box(b, 8, 15, 10, 10, K_GREEN_D, 5);
  (void)led;
  lv_obj_t *l = txt(b, label, F_SMALL, K_GOLD);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 26, 0);
  return b;
}
static void toggle_set(lv_obj_t *b, bool on) {
  if (!b) return;
  lv_obj_t *led = lv_obj_get_child(b, 0);
  lv_obj_set_style_bg_color(led, on ? K_GREEN : K_GREEN_D, 0);
  lv_obj_set_style_shadow_width(led, on ? 8 : 0, 0);
  lv_obj_set_style_shadow_color(led, K_GREEN, 0);
}

static void shuf_cb(lv_event_t *) { audio_shuffle(!audio_state().shuffle); }
static void rep_cb(lv_event_t *) { audio_repeat_one(!audio_state().repeat1); }

static void pl_item_cb(lv_event_t *e) { audio_play((int)(intptr_t)lv_event_get_user_data(e)); }

static void pl_fill() {   // Titelliste (gruen auf schwarz, laufender Titel weiss)
  if (!s_a || !s_a->pl) return;
  lv_obj_clean(s_a->pl);
  std::vector<std::string> f = audio_files();
  AudioState st = audio_state();
  if (f.empty()) {
    lv_obj_t *l = txt(s_a->pl, tr("Keine MP3-Dateien im Ordner /music der SD-Karte"), F_SMALL, K_GREEN);
    lv_obj_set_style_pad_all(l, 8, 0);
    return;
  }
  for (size_t i = 0; i < f.size(); i++) {
    std::string n = std::to_string(i + 1) + ". " + f[i].substr(0, f[i].size() - 4);
    lv_obj_t *r = lv_label_create(s_a->pl);
    lv_label_set_text(r, n.c_str());
    lv_label_set_long_mode(r, LV_LABEL_LONG_DOT);
    lv_obj_set_width(r, lv_pct(100));
    lv_obj_set_style_text_font(r, F_SMALL, 0);
    lv_obj_set_style_text_color(r, (int)i == st.idx ? lv_color_white() : K_GREEN, 0);
    lv_obj_set_style_bg_color(r, lv_color_hex(0x0A2A6A), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_pad_ver(r, 6, 0);
    lv_obj_set_style_pad_hor(r, 8, 0);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(r, pl_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }
  s_a->last_idx = st.idx;
  s_a->last_count = (int)f.size();
}

static void pl_cb(lv_event_t *) {
  if (!s_a) return;
  s_a->pl_open = !s_a->pl_open;
  lv_obj_set_height(s_a->card, H_MAIN + (s_a->pl_open ? H_PL : 0));
  if (s_a->pl_open) {
    lv_obj_clear_flag(s_a->pl, LV_OBJ_FLAG_HIDDEN);
    pl_fill();
  } else {
    lv_obj_add_flag(s_a->pl, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_center(s_a->card);
  toggle_set(s_a->plbtn, s_a->pl_open);
}

static void seek_cb(lv_event_t *e) { audio_seek(lv_slider_get_value(lv_event_get_target(e))); }
static void vol_cb(lv_event_t *e) { audio_set_volume(lv_slider_get_value(lv_event_get_target(e))); }

static std::string mmss(int s) {
  char b[16];
  snprintf(b, sizeof(b), "%02d:%02d", s / 60, s % 60);
  return b;
}

static void tick(lv_timer_t *) {
  if (!s_a) return;
  AudioState st = audio_state();
  bool run = st.playing || st.paused;
  lv_label_set_text(s_a->state, st.playing ? LV_SYMBOL_PLAY : st.paused ? LV_SYMBOL_PAUSE : LV_SYMBOL_STOP);
  // Pause: Zeit blinkt wie frueher
  bool blink_off = st.paused && (millis() / 500) % 2;
  lv_label_set_text(s_a->time, run && !blink_off ? mmss(st.pos_s).c_str() : run ? "" : "--:--");
  std::string t = !st.status.empty() ? st.status
                : st.idx >= 0 && !st.title.empty() ? std::to_string(st.idx + 1) + ". " + st.title + "  (" + mmss(st.dur_s) + ")  ***  "
                : std::string("CampAmp  ***  ") + tr("Lang drücken auf die Kachel Musik öffnet dieses Fenster") + "  ***  ";
  if (t != s_a->last_title) {
    s_a->last_title = t;
    lv_label_set_text(s_a->title, t.c_str());
  }
  lv_label_set_text(s_a->kbps, st.kbps ? std::to_string(st.kbps).c_str() : "");
  lv_label_set_text(s_a->khz, st.hz ? std::to_string((st.hz + 500) / 1000).c_str() : "");
  lv_obj_set_style_text_color(s_a->mono, run && st.ch == 1 ? K_GREEN : K_DIM, 0);
  lv_obj_set_style_text_color(s_a->stereo, run && st.ch == 2 ? K_GREEN : K_DIM, 0);
  if (!lv_slider_is_dragged(s_a->seek))
    lv_slider_set_value(s_a->seek, st.dur_s > 0 ? std::min(1000, st.pos_s * 1000 / st.dur_s) : 0, LV_ANIM_OFF);
  if (!lv_slider_is_dragged(s_a->vol)) lv_slider_set_value(s_a->vol, audio_cfg().vol, LV_ANIM_OFF);
  toggle_set(s_a->shuf, st.shuffle);
  toggle_set(s_a->rep, st.repeat1);
  // Spektrum mit fallenden Spitzen
  uint8_t b[AUDIO_BANDS];
  audio_spectrum(b);
  for (int i = 0; i < AUDIO_BANDS; i++) {
    int v = st.playing ? b[i] : 0;
    int h = std::max(2, v * SPEC_H / 100);
    lv_obj_set_height(s_a->bar[i], h);
    lv_obj_set_y(s_a->bar[i], SPEC_H - h);
    s_a->pk[i] = std::max((float)h, s_a->pk[i] - 1.5f);
    lv_obj_set_y(s_a->peak[i], SPEC_H - (int)s_a->pk[i] - 3);
  }
  if (s_a->pl_open && (st.idx != s_a->last_idx || st.count != s_a->last_count)) pl_fill();
}

void ui_campamp_open() {
  if (!audio_cfg().on) {
    ui_toast("Ton ist ausgeschaltet – im Editor unter Einstellungen → Ton & Musik einschalten.");
    return;
  }
  if (s_a) lv_obj_del(s_a->ov);
  s_a = new Amp();
  Amp *a = s_a;
  a->ov = lv_obj_create(lv_layer_top());
  ui_plain(a->ov);
  lv_obj_set_size(a->ov, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_style_bg_color(a->ov, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(a->ov, LV_OPA_60, 0);
  lv_obj_add_flag(a->ov, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(a->ov, ov_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(a->ov, del_cb, LV_EVENT_DELETE, nullptr);

  // Gehaeuse
  lv_obj_t *c = a->card = box(a->ov, 0, 0, W, H_MAIN, K_FRAME_A, 6);
  lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_grad_color(c, K_FRAME_B, 0);
  lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(c, 2, 0);
  lv_obj_set_style_border_color(c, K_EDGE, 0);
  lv_obj_center(c);

  // Titelleiste: Streifen links und rechts, Name in der Mitte, Schliessen
  for (int i = 0; i < 4; i++) {
    box(c, 14, 10 + i * 6, 330, 2, i % 2 ? lv_color_hex(0x8C7A44) : K_GOLD);
    box(c, W - 344 - 48, 10 + i * 6, 330, 2, i % 2 ? lv_color_hex(0x8C7A44) : K_GOLD);
  }
  lv_obj_t *name = txt(c, "C A M P A M P", F_BODY, K_GOLD);
  lv_obj_align(name, LV_ALIGN_TOP_MID, -20, 6);
  lv_obj_t *x = key(c, W - 46, 6, 34, 30, LV_SYMBOL_CLOSE, close_cb, 0);
  lv_obj_set_style_text_font(lv_obj_get_child(x, 0), &lv_font_montserrat_16, 0);

  // Anzeige links: Zustand, Zeit, Spektrum
  lv_obj_t *l = lcd(c, 16, 48, 340, 176);
  a->state = txt(l, LV_SYMBOL_STOP, &lv_font_montserrat_28, K_GREEN);
  lv_obj_set_pos(a->state, 12, 14);
  a->time = txt(l, "--:--", F_BIG, K_GREEN);
  lv_obj_set_pos(a->time, 60, 4);
  lv_obj_t *sp = box(l, 12, 176 - 4 - 8 - SPEC_H, 19 * 16, SPEC_H, K_LCD);
  for (int i = 0; i < AUDIO_BANDS; i++) {
    a->bar[i] = box(sp, i * 16, SPEC_H - 2, 12, 2, lv_color_hex(0x10C030));
    lv_obj_set_style_bg_grad_color(a->bar[i], lv_color_hex(0xE8D020), 0);
    lv_obj_set_style_bg_grad_dir(a->bar[i], LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_stop(a->bar[i], 255, 0);
    lv_obj_set_style_bg_grad_stop(a->bar[i], 0, 0);
    a->peak[i] = box(sp, i * 16, SPEC_H - 5, 12, 2, lv_color_hex(0xB8BCC8));
  }

  // rechts: Lauftext, kbps/kHz, mono/stereo, Lautstaerke, Schalter
  lv_obj_t *mq = lcd(c, 372, 48, W - 388, 46);
  a->title = txt(mq, "", F_BODY, K_GREEN);
  lv_label_set_long_mode(a->title, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(a->title, W - 388 - 20);
  lv_obj_align(a->title, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_t *kb = lcd(c, 372, 106, 70, 34);
  a->kbps = txt(kb, "", F_SMALL, K_GREEN);
  lv_obj_center(a->kbps);
  lv_obj_t *kbl = txt(c, "kbps", F_SMALL, K_GOLD);
  lv_obj_set_pos(kbl, 450, 112);
  lv_obj_t *kh = lcd(c, 506, 106, 52, 34);
  a->khz = txt(kh, "", F_SMALL, K_GREEN);
  lv_obj_center(a->khz);
  lv_obj_t *khl = txt(c, "kHz", F_SMALL, K_GOLD);
  lv_obj_set_pos(khl, 566, 112);
  a->mono = txt(c, "mono", F_SMALL, K_DIM);
  lv_obj_set_pos(a->mono, W - 150, 112);
  a->stereo = txt(c, "stereo", F_SMALL, K_DIM);
  lv_obj_set_pos(a->stereo, W - 86, 112);

  lv_obj_t *vl = txt(c, tr("Lautstärke"), F_SMALL, K_GOLD);
  lv_obj_set_pos(vl, 372, 160);
  a->vol = lv_slider_create(c);
  lv_obj_set_pos(a->vol, 480, 162);
  lv_obj_set_size(a->vol, W - 500, 14);
  lv_slider_set_range(a->vol, 0, 100);
  lv_obj_set_style_bg_color(a->vol, lv_color_hex(0x10131A), LV_PART_MAIN);
  lv_obj_set_style_bg_color(a->vol, lv_color_hex(0x1FB040), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_color(a->vol, lv_color_hex(0xE04020), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_dir(a->vol, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(a->vol, K_BTN_A, LV_PART_KNOB);
  lv_obj_set_style_radius(a->vol, 2, LV_PART_KNOB);
  lv_obj_set_style_pad_all(a->vol, 6, LV_PART_KNOB);
  lv_obj_add_event_cb(a->vol, vol_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_set_ext_click_area(a->vol, 18);

  a->shuf = toggle(c, 372, 190, 150, tr("Zufall"), shuf_cb);
  a->rep = toggle(c, 534, 190, 170, tr("Wiederholen"), rep_cb);
  a->plbtn = toggle(c, 716, 190, W - 732, tr("Liste"), pl_cb);

  // Spulen
  a->seek = lv_slider_create(c);
  lv_obj_set_pos(a->seek, 24, 248);
  lv_obj_set_size(a->seek, W - 48, 12);
  lv_slider_set_range(a->seek, 0, 1000);
  lv_obj_set_style_bg_color(a->seek, lv_color_hex(0x10131A), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(a->seek, LV_OPA_TRANSP, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(a->seek, K_BTN_A, LV_PART_KNOB);
  lv_obj_set_style_radius(a->seek, 2, LV_PART_KNOB);
  lv_obj_set_style_pad_hor(a->seek, 16, LV_PART_KNOB);
  lv_obj_set_style_pad_ver(a->seek, 6, LV_PART_KNOB);
  lv_obj_add_event_cb(a->seek, seek_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_set_ext_click_area(a->seek, 18);

  // Tasten: Zurueck, Abspielen, Pause, Stopp, Weiter – rechts das Logo
  static const char *SYM[] = {LV_SYMBOL_PREV, LV_SYMBOL_PLAY, LV_SYMBOL_PAUSE, LV_SYMBOL_STOP, LV_SYMBOL_NEXT};
  for (int i = 0; i < 5; i++) key(c, 20 + i * 92, 284, 84, 66, SYM[i], btn_cb, i);
  lv_obj_t *logo = txt(c, "CampAmp", F_TITLE, K_GOLD);
  lv_obj_align(logo, LV_ALIGN_TOP_RIGHT, -24, 296);
  lv_obj_t *sub = txt(c, "Simple Open Camper", F_SMALL, lv_color_hex(0x9AA0B8));
  lv_obj_align(sub, LV_ALIGN_TOP_RIGHT, -24, 330);

  // Titelliste (aufklappbar)
  a->pl = lcd(c, 16, H_MAIN - 6, W - 32, H_PL - 14);
  lv_obj_add_flag(a->pl, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_flex_flow(a->pl, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scrollbar_mode(a->pl, LV_SCROLLBAR_MODE_AUTO);

  audio_spectrum_on(true);
  a->tm = lv_timer_create(tick, 50, nullptr);
  tick(nullptr);
}
#else
void ui_campamp_open() {}
#endif
