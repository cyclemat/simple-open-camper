// Startbild – siehe splash.h
#include "splash.h"
#include "generated/splash_jpg.h"

#if SPLASH_JPG_LEN == 0
bool splash_show() { return false; }
#else
#include <Arduino.h>
#include <lvgl.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp32s3/rom/tjpgd.h"   // JPEG-Decoder im ROM des ESP32-S3: kostet keinen Flash

static const uint32_t SHOW_MS = 3000;

struct Ctx {
  size_t pos = 0;
  uint16_t *dst = nullptr;
  int w = 0, h = 0;
};

static UINT in_fn(JDEC *jd, BYTE *buf, UINT n) {
  Ctx *c = (Ctx *)jd->device;
  if (n > SPLASH_JPG_LEN - c->pos) n = SPLASH_JPG_LEN - c->pos;
  if (buf) memcpy(buf, SPLASH_JPG + c->pos, n);
  c->pos += n;
  return n;
}

static UINT out_fn(JDEC *jd, void *bmp, JRECT *r) {
  Ctx *c = (Ctx *)jd->device;
  const uint8_t *p = (const uint8_t *)bmp;   // RGB888
  for (int y = r->top; y <= r->bottom; y++)
    for (int x = r->left; x <= r->right; x++, p += 3)
      if (x < c->w && y < c->h) c->dst[y * c->w + x] = ((p[0] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | (p[2] >> 3);
  return 1;
}

static lv_img_dsc_t s_dsc;
static lv_obj_t *s_box = nullptr;
static uint16_t *s_px = nullptr;

static void hide_cb(lv_timer_t *t) {
  lv_timer_del(t);
  if (s_box) lv_obj_del(s_box);
  s_box = nullptr;
  lv_img_cache_invalidate_src(&s_dsc);
  heap_caps_free(s_px);   // Bild komplett aus dem Speicher
  s_px = nullptr;
  memset(&s_dsc, 0, sizeof(s_dsc));
  log_i("Startbild entfernt");
}

bool splash_show() {
  void *work = heap_caps_malloc(4096, MALLOC_CAP_8BIT);
  if (!work) return false;
  JDEC jd;
  Ctx c;
  bool ok = false;
  if (jd_prepare(&jd, in_fn, work, 4096, &c) == JDR_OK) {
    int sw = lv_disp_get_hor_res(nullptr), sh = lv_disp_get_ver_res(nullptr);
    uint8_t scale = 0;
    while (scale < 3 && ((int)(jd.width >> scale) > sw || (int)(jd.height >> scale) > sh)) scale++;
    c.w = (jd.width + (1 << scale) - 1) >> scale;
    c.h = (jd.height + (1 << scale) - 1) >> scale;
    s_px = (uint16_t *)heap_caps_calloc((size_t)c.w * c.h, 2, MALLOC_CAP_SPIRAM);
    if (s_px) {
      c.dst = s_px;
      ok = jd_decomp(&jd, out_fn, scale) == JDR_OK;
      if (!ok) {
        heap_caps_free(s_px);
        s_px = nullptr;
      }
    }
  }
  heap_caps_free(work);
  if (!ok) {
    log_w("Startbild: splash.jpg nicht lesbar (nur JPEG 'baseline')");
    return false;
  }
  memset(&s_dsc, 0, sizeof(s_dsc));
  s_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  s_dsc.header.w = c.w;
  s_dsc.header.h = c.h;
  s_dsc.data_size = (uint32_t)c.w * c.h * 2;
  s_dsc.data = (const uint8_t *)s_px;
  // Schwarzer Grund ueber allem, Bild mittig; faengt Beruehrungen ab, solange es zu sehen ist
  s_box = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(s_box);
  lv_obj_set_size(s_box, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(s_box, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(s_box, LV_OPA_COVER, 0);
  lv_obj_add_flag(s_box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *img = lv_img_create(s_box);
  lv_img_set_src(img, &s_dsc);
  lv_obj_center(img);
  lv_timer_create(hide_cb, SHOW_MS, nullptr);
  log_i("Startbild %dx%d", c.w, c.h);
  return true;
}
#endif
