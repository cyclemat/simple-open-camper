// Bildschirmschoner: abgedunkeltes Dashboard, schwarzes Bild, dunkle Uhr, huepfendes Logo oder Ball-Duell.
// Schwarz, Uhr und Logo liegen als Ebene ueber allem (lv_layer_sys). Das Dashboard darunter wird
// weiter aktualisiert, damit beim Aufwecken sofort der aktuelle Stand zu sehen ist.
#include "ui_internal.h"
#include "../i18n/i18n.h"
#include "theme.h"
#include "../app_config.h"
#include "../board.h"
#include "../net.h"
#include "../settings.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>
#include "esp_heap_caps.h"

static lv_obj_t *s_ov = nullptr, *s_item = nullptr, *s_time = nullptr, *s_date = nullptr, *s_icon = nullptr;
static lv_obj_t *s_corner = nullptr;
static lv_timer_t *s_timer = nullptr;
static bool s_active = false;
static int s_mode = SAVER_DIM;
static float s_x = 0, s_y = 0, s_vx = 2.6f, s_vy = 2.0f;
static int s_col = 0, s_last_min = -1;
static uint32_t s_corners = 0, s_flash_until = 0, s_corner_hide = 0, s_last_clock = 0;
static bool s_use_logo = false;

static lv_img_dsc_t s_logo;
static uint8_t *s_logo_buf = nullptr;
static bool s_logo_tried = false;

static const uint32_t PALETTE[] = {0xF3A43B, 0x3C8DFF, 0xFF4FA3, 0x5BD38A, 0xB46BFF, 0xFFE14D, 0x34D6E0, 0xFF6A5C};
static const int PALETTE_N = sizeof(PALETTE) / sizeof(PALETTE[0]);
static const char *WD[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
static const char *MO[] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli",
                           "August", "September", "Oktober", "November", "Dezember"};

bool ui_saver_active() { return s_active; }

// Eigenes Logo: /saver_logo.bin = uint16 Breite, uint16 Hoehe (LE), danach RGB565 + Alpha je Pixel
static const lv_img_dsc_t *load_logo() {
  if (s_logo_buf) return &s_logo;
  if (s_logo_tried) return nullptr;
  s_logo_tried = true;
  File f = LittleFS.open("/saver_logo.bin", "r");
  if (!f) return nullptr;
  uint8_t hdr[4];
  if (f.read(hdr, 4) != 4) {
    f.close();
    return nullptr;
  }
  uint16_t w = hdr[0] | (hdr[1] << 8), h = hdr[2] | (hdr[3] << 8);
  size_t n = (size_t)w * h * 3;
  if (!w || !h || w > 400 || h > 300 || f.size() != n + 4) {
    f.close();
    return nullptr;
  }
  s_logo_buf = (uint8_t *)heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!s_logo_buf) {
    f.close();
    return nullptr;
  }
  f.read(s_logo_buf, n);
  f.close();
  memset(&s_logo, 0, sizeof(s_logo));
  s_logo.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
  s_logo.header.w = w;
  s_logo.header.h = h;
  s_logo.data_size = n;
  s_logo.data = s_logo_buf;
  return &s_logo;
}

void ui_saver_logo_changed() {
  if (s_active) ui_saver_stop();  // das alte Bild darf nicht mehr angezeigt werden
  if (s_logo_buf) heap_caps_free(s_logo_buf);
  s_logo_buf = nullptr;
  s_logo_tried = false;
}

static bool local_time(struct tm &tm) { return net_time_valid() && getLocalTime(&tm, 0); }

static void set_clock_texts() {
  struct tm tm;
  char b[48];
  if (local_time(tm)) {
    strftime(b, sizeof(b), "%H:%M", &tm);
    lv_label_set_text(s_time, b);
    if (s_date) {
      snprintf(b, sizeof(b), "%s, %d. %s", WD[tm.tm_wday], tm.tm_mday, MO[tm.tm_mon]);
      lv_label_set_text(s_date, b);
    }
    s_last_min = tm.tm_min;
  } else {
    lv_label_set_text(s_time, "--:--");
    if (s_date) lv_label_set_text(s_date, "");
  }
}

static void apply_color(lv_color_t c) {
  if (!s_item) return;
  if (s_use_logo) {
    if (g_settings.saver_recolor) {
      lv_obj_set_style_img_recolor(s_item, c, 0);
      lv_obj_set_style_img_recolor_opa(s_item, LV_OPA_COVER, 0);
    }
  } else {
    lv_obj_set_style_border_color(s_item, c, 0);
    lv_obj_set_style_text_color(s_icon, c, 0);
    lv_obj_set_style_text_color(s_time, c, 0);
  }
}

// ---------------------------------------------------------------- Dunkle Uhr
static void clock_place() {
  lv_obj_update_layout(s_item);
  int w = lv_obj_get_width(s_item), h = lv_obj_get_height(s_item);
  int x = random(16, LV_MAX(17, LCD_H_RES - w - 16));
  int y = random(16, LV_MAX(17, LCD_V_RES - h - 16));
  lv_obj_set_pos(s_item, x, y);
}

static void clock_tick(lv_timer_t *) {
  struct tm tm;
  bool valid = local_time(tm);
  if (!valid || tm.tm_min != s_last_min) {
    set_clock_texts();
    clock_place();  // jede Minute an eine neue Stelle: kein Standbild
  }
}

static void build_clock() {
  s_item = lv_obj_create(s_ov);
  ui_plain(s_item);
  lv_obj_set_size(s_item, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(s_item, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  s_time = ui_label(s_item, "", &font_clock_140, lv_color_hex(0x7C8794));
  s_date = ui_label(s_item, "", F_TITLE, lv_color_hex(0x55606B));
  set_clock_texts();
  clock_place();
  s_timer = lv_timer_create(clock_tick, 1000, nullptr);
}

// ---------------------------------------------------------------- Huepfendes Logo
static void corner_hit() {
  s_corners++;
  s_flash_until = millis() + 700;
  apply_color(lv_color_white());
  if (!s_corner) {
    s_corner = ui_label(s_ov, "", F_BODY, lv_color_hex(0x8D97A2));
    lv_obj_align(s_corner, LV_ALIGN_BOTTOM_RIGHT, -20, -16);
  }
  char b[40];
  snprintf(b, sizeof(b), tr("Ecke getroffen! (%lu)"), (unsigned long)s_corners);
  lv_label_set_text(s_corner, b);
  lv_obj_clear_flag(s_corner, LV_OBJ_FLAG_HIDDEN);
  s_corner_hide = millis() + 4000;
}

static void bounce_tick(lv_timer_t *) {
  int w = lv_obj_get_width(s_item), h = lv_obj_get_height(s_item);
  int maxx = LCD_H_RES - w, maxy = LCD_V_RES - h;
  s_x += s_vx;
  s_y += s_vy;
  bool hx = false, hy = false;
  if (s_x <= 0) { s_x = 0; s_vx = fabsf(s_vx); hx = true; }
  if (s_x >= maxx) { s_x = maxx; s_vx = -fabsf(s_vx); hx = true; }
  if (s_y <= 0) { s_y = 0; s_vy = fabsf(s_vy); hy = true; }
  if (s_y >= maxy) { s_y = maxy; s_vy = -fabsf(s_vy); hy = true; }
  lv_obj_set_pos(s_item, (lv_coord_t)s_x, (lv_coord_t)s_y);
  if (hx || hy) {
    // Knapp daneben zaehlt auch: das andere Ende ist hoechstens 6 px vom Rand entfernt
    bool near_x = s_x <= 6 || s_x >= maxx - 6, near_y = s_y <= 6 || s_y >= maxy - 6;
    if ((hx && near_y) || (hy && near_x)) corner_hit();
    else {
      s_col = (s_col + 1 + random(0, PALETTE_N - 1)) % PALETTE_N;
      if (millis() > s_flash_until) apply_color(lv_color_hex(PALETTE[s_col]));
    }
  }
  uint32_t now = millis();
  if (s_flash_until && now > s_flash_until) {
    s_flash_until = 0;
    apply_color(lv_color_hex(PALETTE[s_col]));
  }
  if (s_corner && s_corner_hide && now > s_corner_hide) {
    s_corner_hide = 0;
    lv_obj_add_flag(s_corner, LV_OBJ_FLAG_HIDDEN);
  }
  if (!s_use_logo && now - s_last_clock > 1000) {
    s_last_clock = now;
    struct tm tm;
    if (local_time(tm) && tm.tm_min != s_last_min) {
      set_clock_texts();
      lv_obj_update_layout(s_item);
    }
  }
}

static void build_bounce() {
  const lv_img_dsc_t *logo = load_logo();
  s_use_logo = logo != nullptr;
  if (s_use_logo) {
    s_item = lv_img_create(s_ov);
    lv_img_set_src(s_item, logo);
  } else {
    // Eigenes Abzeichen: Lampensymbol und Uhrzeit in einem Rahmen
    s_item = lv_obj_create(s_ov);
    ui_plain(s_item);
    lv_obj_set_size(s_item, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(s_item, 26, 0);
    lv_obj_set_style_border_width(s_item, 4, 0);
    lv_obj_set_style_pad_hor(s_item, 26, 0);
    lv_obj_set_style_pad_ver(s_item, 16, 0);
    lv_obj_set_style_pad_column(s_item, 16, 0);
    lv_obj_set_flex_flow(s_item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_icon = ui_icon(s_item, ICON_BULB, F_ICON_L, C_AMBER);
    s_time = ui_label(s_item, "", F_BIG, C_AMBER);
    s_date = nullptr;
    set_clock_texts();
  }
  lv_obj_update_layout(s_item);
  int w = lv_obj_get_width(s_item), h = lv_obj_get_height(s_item);
  s_x = random(0, LV_MAX(1, LCD_H_RES - w));
  s_y = random(0, LV_MAX(1, LCD_V_RES - h));
  s_vx = random(0, 2) ? 2.6f : -2.6f;
  s_vy = random(0, 2) ? 2.0f : -2.0f;
  s_col = random(0, PALETTE_N);
  apply_color(lv_color_hex(PALETTE[s_col]));
  lv_obj_set_pos(s_item, (lv_coord_t)s_x, (lv_coord_t)s_y);
  s_last_clock = millis();
  s_timer = lv_timer_create(bounce_tick, 33, nullptr);
}

// ---------------------------------------------------------------- Ball-Duell
// Zwei Schlaeger spielen automatisch gegeneinander. Nur kleine Flaechen bewegen sich (schont PSRAM-Bandbreite).
// Verstecktes Extra (nirgends dokumentieren): zwei Finger gleichzeitig in den beiden oberen Ecken ~0,5 s
// -> Fenster "Los!" -> zwei Spieler, jeder mit einem Finger auf seiner Haelfte. Wer zuerst 10 hat, gewinnt,
// danach zurueck zum Hauptbildschirm.
enum DuelState : uint8_t { D_AUTO, D_READY, D_PLAY, D_END };
static const int PW = 16, PH = 110, BS = 16, PX = 36, WIN = 10, CORNER = 170;
static DuelState s_ds = D_AUTO;
static lv_obj_t *s_pl = nullptr, *s_pr = nullptr, *s_ball = nullptr, *s_score = nullptr, *s_box = nullptr, *s_msg = nullptr;
static float s_ply = 0, s_pry = 0, s_bx = 0, s_by = 0, s_bvx = 0, s_bvy = 0;
static int s_sl = 0, s_sr = 0, s_errl = 0, s_errr = 0;
static uint32_t s_arm = 0, s_wait = 0, s_end_ms = 0;
static volatile bool s_go = false, s_released = false;
static volatile int16_t s_tl = -1, s_tr = -1;   // Fingerhoehe links/rechts (-1 = kein Finger)

static void duel_score() {
  char b[16];
  snprintf(b, sizeof(b), "%d   %d", s_sl, s_sr);
  lv_label_set_text(s_score, b);
}

static void duel_serve(int dir) {
  s_bx = LCD_H_RES / 2 - BS / 2;
  s_by = random(LCD_V_RES / 4, LCD_V_RES * 3 / 4);
  s_bvx = dir * 7.0f;
  s_bvy = random(0, 2) ? 3.0f : -3.0f;
  s_errl = random(-70, 71);   // so verpasst die Automatik auch mal
  s_errr = random(-70, 71);
}

static void duel_box(const char *big, const char *small) {
  if (!s_box) {
    s_box = lv_obj_create(s_ov);
    ui_plain(s_box);
    lv_obj_set_size(s_box, 420, 170);
    lv_obj_center(s_box);
    lv_obj_set_style_radius(s_box, 22, 0);
    lv_obj_set_style_bg_color(s_box, lv_color_hex(0x11151A), 0);
    lv_obj_set_style_bg_opa(s_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_box, 3, 0);
    lv_obj_set_style_border_color(s_box, C_AMBER, 0);
    lv_obj_set_flex_flow(s_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_msg = ui_label(s_box, "", F_BIG, C_AMBER);
    ui_label(s_box, "", F_BODY, lv_color_hex(0x8D97A2));
  }
  lv_label_set_text(s_msg, big);
  lv_label_set_text(lv_obj_get_child(s_box, 1), small);
  lv_obj_clear_flag(s_box, LV_OBJ_FLAG_HIDDEN);
}

static bool in_corner_l(int16_t x, int16_t y) { return x < CORNER && y < CORNER; }
static bool in_corner_r(int16_t x, int16_t y) { return x > LCD_H_RES - CORNER && y < CORNER; }

// Laeuft beim Lesen des Touch-Controllers (im LVGL-Task). true = Beruehrung gehoert dem Spiel.
static bool duel_touch(uint8_t n, const int16_t *x, const int16_t *y) {
  uint32_t now = millis();
  if (s_ds == D_AUTO) {
    bool both = n >= 2 && ((in_corner_l(x[0], y[0]) && in_corner_r(x[1], y[1])) ||
                           (in_corner_r(x[0], y[0]) && in_corner_l(x[1], y[1])));
    if (both) {
      if (!s_arm) s_arm = now;
      if (now - s_arm > 500) s_go = true;   // Uebergang macht der Timer
      return true;
    }
    s_arm = 0;
    // Ein Finger in einer oberen Ecke: kurz auf den zweiten warten, statt sofort aufzuwecken
    if (n == 1 && (in_corner_l(x[0], y[0]) || in_corner_r(x[0], y[0]))) {
      if (!s_wait) s_wait = now;
      return now - s_wait < 400;
    }
    if (n == 0 && s_wait) {   // nur kurz in die Ecke getippt: normal aufwecken
      s_wait = 0;
      board_wake();
      return true;
    }
    s_wait = 0;
    return s_go;   // sonst: normales Aufwecken
  }
  // Im Spiel gehoeren alle Beruehrungen dem Spiel
  if (n == 0) s_released = true;
  int16_t l = -1, r = -1;
  for (uint8_t i = 0; i < n; i++) {
    if (x[i] < LCD_H_RES / 2) l = y[i];
    else r = y[i];
  }
  s_tl = l;
  s_tr = r;
  return true;
}

static float ai_move(float py, float target, float speed) {
  float d = target - (py + PH / 2);
  if (d > speed) d = speed;
  if (d < -speed) d = -speed;
  return py + d;
}

static void duel_tick(lv_timer_t *) {
  uint32_t now = millis();
  if (s_ds == D_AUTO && s_go) {
    s_go = false;
    s_ds = D_READY;
    s_released = false;
    board_dim(g_settings.brightness);   // volle Helligkeit, bleibt aber "Schoner" (Touch geht ans Spiel)
    duel_box(tr("Los!"), tr("Tippen zum Starten"));
    return;
  }
  if (s_ds == D_READY) {
    if (s_released && (s_tl >= 0 || s_tr >= 0)) {
      s_ds = D_PLAY;
      s_sl = s_sr = 0;
      duel_score();
      lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);
      duel_serve(random(0, 2) ? 1 : -1);
    }
    return;
  }
  if (s_ds == D_END) {
    if (now - s_end_ms > 3000) {
      ui_saver_stop();
      lv_disp_trig_activity(nullptr);   // nicht sofort wieder in den Schoner
    }
    return;
  }
  // Schlaeger: Automatik oder Finger
  if (s_ds == D_AUTO) {
    bool to_l = s_bvx < 0;
    s_ply = ai_move(s_ply, to_l ? s_by + BS / 2 + s_errl : LCD_V_RES / 2, 5.5f);
    s_pry = ai_move(s_pry, !to_l ? s_by + BS / 2 + s_errr : LCD_V_RES / 2, 5.5f);
  } else {
    if (s_tl >= 0) s_ply = s_tl - PH / 2;
    if (s_tr >= 0) s_pry = s_tr - PH / 2;
  }
  s_ply = constrain(s_ply, 0.0f, (float)(LCD_V_RES - PH));
  s_pry = constrain(s_pry, 0.0f, (float)(LCD_V_RES - PH));
  // Ball
  s_bx += s_bvx;
  s_by += s_bvy;
  if (s_by <= 0) { s_by = 0; s_bvy = fabsf(s_bvy); }
  if (s_by >= LCD_V_RES - BS) { s_by = LCD_V_RES - BS; s_bvy = -fabsf(s_bvy); }
  auto hit = [&](float py, bool left) {
    float rel = (s_by + BS / 2 - (py + PH / 2)) / (PH / 2);   // -1 oben .. 1 unten
    float sp = fminf(fabsf(s_bvx) * 1.06f, 16.0f);
    s_bvx = left ? sp : -sp;
    s_bvy = rel * 7.0f;
    if (left) s_errl = random(-70, 71);
    else s_errr = random(-70, 71);
  };
  if (s_bvx < 0 && s_bx <= PX + PW && s_bx >= PX - BS && s_by + BS >= s_ply && s_by <= s_ply + PH) {
    s_bx = PX + PW;
    hit(s_ply, true);
  }
  int rx = LCD_H_RES - PX - PW;
  if (s_bvx > 0 && s_bx + BS >= rx && s_bx <= rx + PW && s_by + BS >= s_pry && s_by <= s_pry + PH) {
    s_bx = rx - BS;
    hit(s_pry, false);
  }
  if (s_bx < -BS || s_bx > LCD_H_RES) {
    bool right_scores = s_bx < 0;
    if (right_scores) s_sr++;
    else s_sl++;
    if (s_ds == D_AUTO && (s_sl >= WIN || s_sr >= WIN)) s_sl = s_sr = 0;   // Automatik spielt endlos
    duel_score();
    if (s_ds == D_PLAY && (s_sl >= WIN || s_sr >= WIN)) {
      s_ds = D_END;
      s_end_ms = now;
      duel_box(s_sl > s_sr ? tr("Links gewinnt!") : tr("Rechts gewinnt!"), "");
      return;
    }
    duel_serve(right_scores ? -1 : 1);
  }
  lv_obj_set_y(s_pl, (lv_coord_t)s_ply);
  lv_obj_set_y(s_pr, (lv_coord_t)s_pry);
  lv_obj_set_pos(s_ball, (lv_coord_t)s_bx, (lv_coord_t)s_by);
}

static lv_obj_t *duel_rect(int x, int y, int w, int h, lv_color_t c) {
  lv_obj_t *o = lv_obj_create(s_ov);
  ui_plain(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  return o;
}

static void build_duel() {
  lv_color_t dim = lv_color_hex(0x3A424B), fg = lv_color_hex(0xE6EAEE);
  for (int y = 8; y < LCD_V_RES; y += 40) duel_rect(LCD_H_RES / 2 - 2, y, 4, 20, dim);   // Mittellinie
  s_ply = s_pry = (LCD_V_RES - PH) / 2;
  s_pl = duel_rect(PX, (int)s_ply, PW, PH, fg);
  s_pr = duel_rect(LCD_H_RES - PX - PW, (int)s_pry, PW, PH, fg);
  s_ball = duel_rect(0, 0, BS, BS, C_AMBER);
  lv_obj_set_style_radius(s_ball, BS / 2, 0);
  s_score = ui_label(s_ov, "", F_BIG, dim);
  lv_obj_align(s_score, LV_ALIGN_TOP_MID, 0, 18);
  s_ds = D_AUTO;
  s_sl = s_sr = 0;
  s_arm = s_wait = 0;
  s_go = s_released = false;
  s_tl = s_tr = -1;
  s_box = s_msg = nullptr;
  duel_score();
  duel_serve(random(0, 2) ? 1 : -1);
  board_set_touch_hook(duel_touch);
  s_timer = lv_timer_create(duel_tick, 33, nullptr);
}

// ---------------------------------------------------------------- Steuerung
void ui_saver_start() {
  if (s_active) return;
  s_mode = g_settings.saver;
  uint8_t lvl = g_settings.dim_level;
  s_active = true;
  if (s_mode == SAVER_DIM) {
    board_dim(lvl);
    return;
  }
  s_ov = lv_obj_create(lv_layer_sys());
  ui_plain(s_ov);
  lv_obj_set_size(s_ov, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_style_bg_color(s_ov, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(s_ov, LV_OPA_COVER, 0);
  s_corner = nullptr;
  s_corner_hide = 0;
  s_flash_until = 0;
  if (s_mode == SAVER_BLACK) {
    board_dim(0);
  } else {
    if (s_mode == SAVER_CLOCK) build_clock();
    else if (s_mode == SAVER_DUEL) build_duel();
    else build_bounce();
    board_dim(LV_MAX(lvl, 6));  // bei 0 waere von Uhr/Logo nichts zu sehen
  }
}

void ui_saver_stop() {
  if (!s_active) return;
  s_active = false;
  board_set_touch_hook(nullptr);
  if (s_timer) lv_timer_del(s_timer);
  s_timer = nullptr;
  if (s_ov) lv_obj_del(s_ov);
  s_ov = s_item = s_time = s_date = s_icon = s_corner = nullptr;
  s_pl = s_pr = s_ball = s_score = s_box = s_msg = nullptr;
  if (board_is_dimmed()) board_wake();
}

void ui_saver_tick() {
  // Beruehrung hat das Panel geweckt -> Schoner entfernen
  if (s_active) {
    if (!board_is_dimmed()) ui_saver_stop();
    return;
  }
  static uint32_t t = 0;
  if (millis() - t < 500) return;
  t = millis();
  uint16_t after = g_settings.dim_after;
  if (after && lv_disp_get_inactive_time(nullptr) > (uint32_t)after * 1000) ui_saver_start();
}
