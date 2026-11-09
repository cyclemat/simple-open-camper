// Bildschirmschoner: abgedunkeltes Dashboard, schwarzes Bild, dunkle Uhr oder huepfendes Logo.
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
    else build_bounce();
    board_dim(LV_MAX(lvl, 6));  // bei 0 waere von Uhr/Logo nichts zu sehen
  }
}

void ui_saver_stop() {
  if (!s_active) return;
  s_active = false;
  if (s_timer) lv_timer_del(s_timer);
  s_timer = nullptr;
  if (s_ov) lv_obj_del(s_ov);
  s_ov = s_item = s_time = s_date = s_icon = s_corner = nullptr;
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
