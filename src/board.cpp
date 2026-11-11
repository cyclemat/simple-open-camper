#include "board.h"
#include "app_config.h"

#include <Arduino.h>
#include <Wire.h>
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "board";

static esp_lcd_panel_handle_t s_panel = nullptr;
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_indev_drv_t s_indev_drv;
static SemaphoreHandle_t s_i2c_mtx = nullptr;

static uint8_t s_exio_out = 0;
static bool s_exio_ok = false;
static uint8_t s_gt_addr = 0;
static uint16_t s_gt_xmax = LCD_H_RES, s_gt_ymax = LCD_V_RES;
static char s_touch_info[48] = "kein Touch erkannt";

static uint8_t s_user_bl = 80;
static bool s_dimmed = false;
static bool s_swallow = false;

// Bis zu zwei Finger (GT911 kann fuenf); fuer LVGL zaehlt nur der erste
static uint8_t s_tn = 0;
static int16_t s_tx[2], s_ty[2];
static BoardTouchHook s_hook = nullptr;
void board_set_touch_hook(BoardTouchHook h) { s_hook = h; }

// ---------------------------------------------------------------- I2C-Helfer
struct I2CLock {
  I2CLock() { xSemaphoreTakeRecursive(s_i2c_mtx, portMAX_DELAY); }
  ~I2CLock() { xSemaphoreGiveRecursive(s_i2c_mtx); }
};

static bool exio_write(uint8_t reg, uint8_t val) {
  I2CLock l;
  Wire.beginTransmission(EXIO_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static void exio_pin(uint8_t pin, bool level) {
  if (level) s_exio_out |= (1 << pin);
  else s_exio_out &= ~(1 << pin);
  exio_write(EXIO_REG_OUT, s_exio_out);
}

static bool gt_read(uint16_t reg, uint8_t *buf, size_t len) {
  I2CLock l;
  Wire.beginTransmission(s_gt_addr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission() != 0) return false;
  size_t got = Wire.requestFrom((int)s_gt_addr, (int)len);
  if (got != len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

static bool gt_write8(uint16_t reg, uint8_t val) {
  I2CLock l;
  Wire.beginTransmission(s_gt_addr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool i2c_probe(uint8_t addr) {
  I2CLock l;
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// ---------------------------------------------------------------- IO-Erweiterung
static void exio_init() {
  // Alle Pins als Ausgang (ohne diesen Schritt reagiert der CH32V003 nicht)
  s_exio_ok = exio_write(EXIO_REG_DIR, 0xFF);
  s_exio_out = (1 << EXIO_TP_RST) | (1 << EXIO_LCD_RST) | (1 << EXIO_SD_CS) | (1 << EXIO_LCD_VDD);
  // DISP (Backlight) zunaechst aus, USB_SEL low = native USB aktiv
  exio_write(EXIO_REG_OUT, s_exio_out);
  log_i("CH32V003 %s", s_exio_ok ? "gefunden" : "NICHT gefunden");
}

static void backlight_apply(uint8_t pct) {
  if (!s_exio_ok) return;
  if (pct == 0) {
    exio_pin(EXIO_DISP, false);
    return;
  }
  // Register ist invertiert: kleiner Wert = hell
  int reg = (int)((100 - pct) * 255 / 100);
  if (reg < BL_PWM_MIN) reg = BL_PWM_MIN;
  if (reg > BL_PWM_MAX) reg = BL_PWM_MAX;
  exio_write(EXIO_REG_PWM, (uint8_t)reg);
  if (!(s_exio_out & (1 << EXIO_DISP))) exio_pin(EXIO_DISP, true);
}

void board_set_backlight(uint8_t pct) {
  if (pct > 100) pct = 100;
  if (pct < 3) pct = 3;  // nie versehentlich ganz dunkel
  s_user_bl = pct;
  if (!s_dimmed) backlight_apply(pct);
}
uint8_t board_backlight() { return s_user_bl; }

void board_dim(uint8_t pct) {
  s_dimmed = true;
  backlight_apply(pct);
}
void board_wake() {
  if (!s_dimmed) return;
  s_dimmed = false;
  backlight_apply(s_user_bl);
  lv_disp_trig_activity(nullptr);
}
bool board_is_dimmed() { return s_dimmed; }

float board_battery_voltage() {
  if (!s_exio_ok) return -1;
  I2CLock l;
  Wire.beginTransmission(EXIO_ADDR);
  Wire.write(EXIO_REG_ADC);
  if (Wire.endTransmission() != 0) return -1;
  if (Wire.requestFrom(EXIO_ADDR, 2) != 2) return -1;
  uint16_t lo = Wire.read(), hi = Wire.read();
  uint16_t adc = (hi << 8) | lo;
  return adc * 3.0f * 3.3f / 1023.0f;
}

// ---------------------------------------------------------------- Uhrenchip PCF85063
// Register ab 0x04: Sekunden (Bit 7 = Oszillator war gestoppt), Minuten, Stunden, Tag, Wochentag, Monat, Jahr – BCD
static const uint8_t RTC_ADDR = 0x51;
static bool s_rtc_ok = false;

static uint8_t bcd(uint8_t v) { return (v / 10) << 4 | (v % 10); }
static uint8_t unbcd(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

static void rtc_probe() {
  I2CLock l;
  // Einmal beim Start: alle I2C-Teilnehmer ins Log (zeigt, was auf der Platine bestueckt ist)
  char found[96] = "";
  for (uint8_t a = 0x08; a < 0x78; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0 && strlen(found) < sizeof(found) - 6) snprintf(found + strlen(found), 6, " 0x%02X", a);
  }
  log_i("I2C-Geräte:%s", found[0] ? found : " keine");
  Wire.beginTransmission(RTC_ADDR);
  s_rtc_ok = Wire.endTransmission() == 0;
  log_i("Uhrenchip (0x51): %s", s_rtc_ok ? "gefunden" : "nicht bestückt");
}

bool board_rtc_present() { return s_rtc_ok; }

bool board_rtc_read(time_t &utc) {
  if (!s_rtc_ok) return false;
  uint8_t r[7];
  {
    I2CLock l;
    Wire.beginTransmission(RTC_ADDR);
    Wire.write(0x04);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(RTC_ADDR, (uint8_t)7) != 7) return false;
    for (auto &b : r) b = Wire.read();
  }
  if (r[0] & 0x80) return false;   // Oszillator war aus – Zeit ungueltig
  struct tm tm = {};
  tm.tm_sec = unbcd(r[0] & 0x7F);
  tm.tm_min = unbcd(r[1] & 0x7F);
  tm.tm_hour = unbcd(r[2] & 0x3F);
  tm.tm_mday = unbcd(r[3] & 0x3F);
  tm.tm_mon = unbcd(r[5] & 0x1F) - 1;
  tm.tm_year = unbcd(r[6]) + 100;
  // mktime rechnet in Ortszeit – der Chip haelt UTC, also ohne Zeitzone umrechnen
  time_t t = 0;
  int days_before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  int y = tm.tm_year + 1900;
  if (tm.tm_mon < 0 || tm.tm_mon > 11 || tm.tm_mday < 1) return false;
  long days = (y - 1970) * 365L + ((y - 1969) / 4) - ((y - 1901) / 100) + ((y - 1601) / 400) + days_before[tm.tm_mon] +
              tm.tm_mday - 1;
  if (tm.tm_mon > 1 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) days++;
  t = days * 86400L + tm.tm_hour * 3600L + tm.tm_min * 60L + tm.tm_sec;
  utc = t;
  return t > 1700000000;
}

void board_rtc_write(time_t utc) {
  if (!s_rtc_ok) return;
  struct tm tm;
  gmtime_r(&utc, &tm);
  I2CLock l;
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x04);
  Wire.write(bcd(tm.tm_sec));        // Bit 7 = 0: Oszillator laeuft
  Wire.write(bcd(tm.tm_min));
  Wire.write(bcd(tm.tm_hour));
  Wire.write(bcd(tm.tm_mday));
  Wire.write(tm.tm_wday);
  Wire.write(bcd(tm.tm_mon + 1));
  Wire.write(bcd(tm.tm_year % 100));
  Wire.endTransmission();
}

// ---------------------------------------------------------------- Touch GT911
static void touch_init() {
  // Reset-Sequenz: INT waehrend des Resets low -> Adresse 0x5D
  pinMode(PIN_TOUCH_INT, OUTPUT);
  digitalWrite(PIN_TOUCH_INT, LOW);
  exio_pin(EXIO_TP_RST, false);
  delay(20);
  exio_pin(EXIO_TP_RST, true);
  delay(10);
  pinMode(PIN_TOUCH_INT, INPUT);
  delay(60);

  if (i2c_probe(GT911_ADDR_A)) s_gt_addr = GT911_ADDR_A;
  else if (i2c_probe(GT911_ADDR_B)) s_gt_addr = GT911_ADDR_B;
  if (!s_gt_addr) {
    log_e("GT911 nicht gefunden");
    return;
  }
  uint8_t id[4] = {0};
  uint8_t res[4] = {0};
  gt_read(0x8140, id, 4);
  if (gt_read(0x8048, res, 4)) {
    uint16_t xm = res[0] | (res[1] << 8), ym = res[2] | (res[3] << 8);
    if (xm > 100 && ym > 100) {
      s_gt_xmax = xm;
      s_gt_ymax = ym;
    }
  }
  snprintf(s_touch_info, sizeof(s_touch_info), "GT%c%c%c%c @0x%02X, %ux%u", id[0] ? id[0] : '?', id[1], id[2],
           id[3] ? id[3] : ' ', s_gt_addr, s_gt_xmax, s_gt_ymax);
  log_i("Touch: %s", s_touch_info);
}

void board_sd_select(bool on) {
  if (s_exio_ok) exio_pin(EXIO_SD_CS, !on);
}

bool board_touch_ok() { return s_gt_addr != 0; }
const char *board_touch_info() { return s_touch_info; }

static bool touch_read(int16_t *x, int16_t *y) {
  if (!s_gt_addr) return false;
  uint8_t st = 0;
  if (!gt_read(0x814E, &st, 1)) return false;
  static bool last_pressed = false;
  static int16_t lx = 0, ly = 0;
  if (!(st & 0x80)) {  // keine neuen Daten -> letzten Zustand halten
    if (last_pressed) {
      *x = lx;
      *y = ly;
    }
    return last_pressed;
  }
  uint8_t n = st & 0x0F;
  bool pressed = false;
  s_tn = 0;
  if (n > 0 && n <= 5) {
    uint8_t k = n > 2 ? 2 : n;
    uint8_t p[16];
    if (gt_read(0x814F, p, k * 8)) {
      for (uint8_t i = 0; i < k; i++) {
        const uint8_t *q = p + i * 8;   // je Punkt: ID, X (2), Y (2), Groesse (2), reserviert
        int32_t rx = q[1] | (q[2] << 8);
        int32_t ry = q[3] | (q[4] << 8);
#if TOUCH_SWAP_XY
        int32_t t = rx; rx = ry; ry = t;
#endif
        rx = rx * LCD_H_RES / (TOUCH_SWAP_XY ? s_gt_ymax : s_gt_xmax);
        ry = ry * LCD_V_RES / (TOUCH_SWAP_XY ? s_gt_xmax : s_gt_ymax);
#if TOUCH_INVERT_X
        rx = LCD_H_RES - 1 - rx;
#endif
#if TOUCH_INVERT_Y
        ry = LCD_V_RES - 1 - ry;
#endif
        s_tx[i] = constrain(rx, 0, LCD_H_RES - 1);
        s_ty[i] = constrain(ry, 0, LCD_V_RES - 1);
      }
      s_tn = k;
      lx = s_tx[0];
      ly = s_ty[0];
      pressed = true;
    }
  }
  gt_write8(0x814E, 0);
  last_pressed = pressed;
  *x = lx;
  *y = ly;
  return pressed;
}

static void indev_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  int16_t x, y;
  bool pressed = touch_read(&x, &y);
  // Bildschirmschoner (Spiel) wertet die Finger selbst aus: dann weder aufwecken noch an LVGL geben
  if (s_hook && s_hook(pressed ? s_tn : 0, s_tx, s_ty)) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  if (pressed && s_dimmed) {
    // Erste Beruehrung weckt nur auf und loest nichts aus
    board_wake();
    s_swallow = true;
  }
  if (s_swallow) {
    if (!pressed) s_swallow = false;
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  data->point.x = x;
  data->point.y = y;
  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// ---------------------------------------------------------------- RGB-Panel
static bool lcd_init() {
  esp_lcd_rgb_panel_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  cfg.timings.pclk_hz = LCD_PCLK_HZ;
  cfg.timings.h_res = LCD_H_RES;
  cfg.timings.v_res = LCD_V_RES;
  cfg.timings.hsync_pulse_width = LCD_HSYNC_PULSE;
  cfg.timings.hsync_back_porch = LCD_HSYNC_BACK;
  cfg.timings.hsync_front_porch = LCD_HSYNC_FRONT;
  cfg.timings.vsync_pulse_width = LCD_VSYNC_PULSE;
  cfg.timings.vsync_back_porch = LCD_VSYNC_BACK;
  cfg.timings.vsync_front_porch = LCD_VSYNC_FRONT;
  cfg.timings.flags.pclk_active_neg = LCD_PCLK_ACTIVE_NEG;
  cfg.data_width = 16;
  cfg.num_fbs = 1;
  cfg.bounce_buffer_size_px = LCD_BOUNCE_BUFFER_PX;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  cfg.dma_burst_size = 64;
#else
  cfg.psram_trans_align = 64;
#endif
  cfg.hsync_gpio_num = PIN_LCD_HSYNC;
  cfg.vsync_gpio_num = PIN_LCD_VSYNC;
  cfg.de_gpio_num = PIN_LCD_DE;
  cfg.pclk_gpio_num = PIN_LCD_PCLK;
  cfg.disp_gpio_num = -1;
  const int pins[16] = LCD_DATA_PINS;
  for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = pins[i];
  cfg.flags.fb_in_psram = 1;

  esp_err_t err = esp_lcd_new_rgb_panel(&cfg, &s_panel);
  if (err != ESP_OK) {
    log_e("esp_lcd_new_rgb_panel: %s", esp_err_to_name(err));
    return false;
  }
  esp_lcd_panel_reset(s_panel);
  esp_lcd_panel_init(s_panel);
  // Framebuffer schwarz fuellen, bevor das Backlight angeht
  void *fb = nullptr;
  if (esp_lcd_rgb_panel_get_frame_buffer(s_panel, 1, &fb) == ESP_OK && fb) memset(fb, 0, BG_BYTES);
  return true;
}

void *board_framebuffer() {
  void *fb = nullptr;
  if (s_panel) esp_lcd_rgb_panel_get_frame_buffer(s_panel, 1, &fb);
  return fb;
}

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px) {
  esp_lcd_panel_draw_bitmap(s_panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, px);
  lv_disp_flush_ready(drv);
}

bool board_init() {
  s_i2c_mtx = xSemaphoreCreateRecursiveMutex();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
  exio_init();
  rtc_probe();
  // LCD-Reset und Versorgung
  exio_pin(EXIO_LCD_RST, false);
  delay(10);
  exio_pin(EXIO_LCD_RST, true);
  delay(20);

  if (!lcd_init()) return false;
  touch_init();

  lv_init();
  size_t px = (size_t)LCD_H_RES * LVGL_BUF_LINES;
  size_t bytes = px * sizeof(lv_color_t);
  lv_color_t *buf = nullptr;
  if (heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) > bytes + 96 * 1024)
    buf = (lv_color_t *)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!buf) {
    buf = (lv_color_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    log_w("LVGL-Puffer im PSRAM");
  }
  if (!buf) return false;
  lv_disp_draw_buf_init(&s_draw_buf, buf, nullptr, px);
  lv_disp_drv_init(&s_disp_drv);
  s_disp_drv.hor_res = LCD_H_RES;
  s_disp_drv.ver_res = LCD_V_RES;
  s_disp_drv.flush_cb = flush_cb;
  s_disp_drv.draw_buf = &s_draw_buf;
  lv_disp_drv_register(&s_disp_drv);

  lv_indev_drv_init(&s_indev_drv);
  s_indev_drv.type = LV_INDEV_TYPE_POINTER;
  s_indev_drv.read_cb = indev_read_cb;
  s_indev_drv.long_press_time = 450;
  s_indev_drv.gesture_limit = 60;
  s_indev_drv.gesture_min_velocity = 4;
  lv_indev_drv_register(&s_indev_drv);
  ESP_LOGI(TAG, "Display bereit");
  return true;
}
