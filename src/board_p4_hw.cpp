// Hardware-Schicht fuer ESP32-P4-Panels (board.h) – Display MIPI-DSI, Touch GT911, Hintergrundlicht (LEDC),
// WLAN/Bluetooth ueber den ESP32-C6 (esp-hosted, SDIO). Pins und Timings aus der Board-Tabelle (board_p4.h).
// Am ELECROW CrowPanel Advanced 7" V1.2 erprobt mit der Testfirmware src/p4test/ (2026-11-13).
// Das Waveshare-Panel (ESP32-S3) nutzt board.cpp.
#include "board.h"
#include "board_p4.h"

#if BOARD_P4
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include "app_config.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const P4BoardDef *B = nullptr;
static esp_lcd_panel_handle_t s_panel = nullptr;
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_indev_drv_t s_indev_drv;
static SemaphoreHandle_t s_i2c_mtx = nullptr;

static uint8_t s_gt_addr = 0;
static uint16_t s_gt_xmax = LCD_H_RES, s_gt_ymax = LCD_V_RES;
static char s_touch_info[48] = "kein Touch erkannt";

static uint8_t s_user_bl = 80;
static bool s_dimmed = false;
static bool s_swallow = false;
static bool s_bl_init = false;

static uint8_t s_tn = 0;
static int16_t s_tx[2], s_ty[2];
static BoardTouchHook s_hook = nullptr;
void board_set_touch_hook(BoardTouchHook h) { s_hook = h; }

namespace {
struct I2CLock {
  I2CLock() { xSemaphoreTakeRecursive(s_i2c_mtx, portMAX_DELAY); }
  ~I2CLock() { xSemaphoreGiveRecursive(s_i2c_mtx); }
};
}  // namespace

static bool gt_read(uint16_t reg, uint8_t *buf, size_t len) {
  I2CLock l;
  Wire.beginTransmission(s_gt_addr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(s_gt_addr, (uint8_t)len) != len) return false;
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

// ---------------------------------------------------------------- Hintergrundlicht (LEDC, 10 Bit)
static void backlight_apply(uint8_t pct) {
  if (B->bl_pwm < 0) return;
  if (!s_bl_init) {
    ledcAttach(B->bl_pwm, 30000, 10);
    s_bl_init = true;
  }
  ledcWrite(B->bl_pwm, (uint32_t)pct * 1023 / 100);
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

float board_battery_voltage() { return -1; }   // Akkuspannung am P4-Board noch nicht angebunden

// ---------------------------------------------------------------- Uhrenchip PCF85063 (falls bestueckt)
static const uint8_t RTC_ADDR = 0x51;
static bool s_rtc_ok = false;

static uint8_t bcd(uint8_t v) { return (v / 10) << 4 | (v % 10); }
static uint8_t unbcd(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

static void rtc_probe() {
  I2CLock l;
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
  if (r[0] & 0x80) return false;
  int mon = unbcd(r[5] & 0x1F) - 1, mday = unbcd(r[3] & 0x3F), y = unbcd(r[6]) + 2000;
  if (mon < 0 || mon > 11 || mday < 1) return false;
  static const int DB[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  long days = (y - 1970) * 365L + ((y - 1969) / 4) - ((y - 1901) / 100) + ((y - 1601) / 400) + DB[mon] + mday - 1;
  if (mon > 1 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) days++;
  utc = days * 86400L + unbcd(r[2] & 0x3F) * 3600L + unbcd(r[1] & 0x7F) * 60L + unbcd(r[0] & 0x7F);
  return utc > 1700000000;
}

void board_rtc_write(time_t utc) {
  if (!s_rtc_ok) return;
  struct tm tm;
  gmtime_r(&utc, &tm);
  I2CLock l;
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x04);
  Wire.write(bcd(tm.tm_sec));
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
  if (B->touch_rst >= 0 && B->touch_int >= 0) {   // Reset mit INT = Low -> Adresse 0x5D
    pinMode(B->touch_int, OUTPUT);
    pinMode(B->touch_rst, OUTPUT);
    digitalWrite(B->touch_int, LOW);
    digitalWrite(B->touch_rst, LOW);
    delay(20);
    digitalWrite(B->touch_rst, HIGH);
    delay(10);
    pinMode(B->touch_int, INPUT);
    delay(60);
  }
  if (i2c_probe(0x5D)) s_gt_addr = 0x5D;
  else if (i2c_probe(0x14)) s_gt_addr = 0x14;
  if (!s_gt_addr) {
    log_e("GT911 nicht gefunden");
    return;
  }
  uint8_t id[4] = {0}, res[4] = {0};
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

void board_sd_select(bool) {}   // SD-Karte haengt am P4 direkt am SDMMC (kein Chip-Select)
bool board_touch_ok() { return s_gt_addr != 0; }
const char *board_touch_info() { return s_touch_info; }

static bool touch_read(int16_t *x, int16_t *y) {
  if (!s_gt_addr) return false;
  uint8_t st = 0;
  if (!gt_read(0x814E, &st, 1)) return false;
  static bool last_pressed = false;
  static int16_t lx = 0, ly = 0;
  if (!(st & 0x80)) {
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
        const uint8_t *q = p + i * 8;
        int32_t rx = (q[1] | (q[2] << 8)) * LCD_H_RES / s_gt_xmax;
        int32_t ry = (q[3] | (q[4] << 8)) * LCD_V_RES / s_gt_ymax;
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

static void indev_read_cb(lv_indev_drv_t *, lv_indev_data_t *data) {
  int16_t x, y;
  bool pressed = touch_read(&x, &y);
  if (s_hook && s_hook(pressed ? s_tn : 0, s_tx, s_ty)) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  if (pressed && s_dimmed) {   // erste Beruehrung weckt nur auf
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

// ---------------------------------------------------------------- Display MIPI-DSI
static bool ldo_on(int chan, int mv) {
  if (chan < 0) return true;
  static esp_ldo_channel_handle_t h[2] = {nullptr, nullptr};
  static int n = 0;
  if (n >= 2) return false;
  esp_ldo_channel_config_t c = {};
  c.chan_id = chan;
  c.voltage_mv = mv;
  esp_err_t e = esp_ldo_acquire_channel(&c, &h[n++]);
  if (e != ESP_OK) log_e("LDO%d %d mV: %s", chan, mv, esp_err_to_name(e));
  return e == ESP_OK;
}

static void dcs(esp_lcd_panel_io_handle_t io, uint8_t cmd, uint8_t v) { esp_lcd_panel_io_tx_param(io, cmd, &v, 1); }

// DMA2D hat den Ausschnitt in den Bildspeicher kopiert -> LVGL darf den Zeichenpuffer wieder benutzen
static bool IRAM_ATTR on_trans_done(esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t *, void *) {
  lv_disp_flush_ready(&s_disp_drv);
  return false;
}

static bool lcd_init() {
  ldo_on(B->ldo_mipi_chan, B->ldo_mipi_mv);
  ldo_on(B->ldo_aux_chan, B->ldo_aux_mv);

  esp_chip_info_t ci;
  esp_chip_info(&ci);
  esp_lcd_dsi_bus_config_t bc = {};
  bc.bus_id = 0;
  bc.num_data_lanes = B->dsi_lanes;
  // Chip-Revision < 3: PHY-Takt nur aus PLL_F20M; XTAL (Standard ab IDF 5.5) bricht mit assert ab
  bc.phy_clk_src = ci.revision < 300 ? MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT_LEGACY : MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT;
  bc.lane_bit_rate_mbps = B->dsi_mbps;
  esp_lcd_dsi_bus_handle_t bus = nullptr;
  esp_err_t e = esp_lcd_new_dsi_bus(&bc, &bus);
  if (e != ESP_OK) {
    log_e("DSI-Bus: %s", esp_err_to_name(e));
    return false;
  }
  esp_lcd_dbi_io_config_t ic = {};
  ic.lcd_cmd_bits = 8;
  ic.lcd_param_bits = 8;
  esp_lcd_panel_io_handle_t io = nullptr;
  if (esp_lcd_new_panel_io_dbi(bus, &ic, &io) != ESP_OK) return false;

  if (B->lcd_rst >= 0) {
    pinMode(B->lcd_rst, OUTPUT);
    digitalWrite(B->lcd_rst, LOW);
    delay(10);
    digitalWrite(B->lcd_rst, HIGH);
    delay(20);
  }

  esp_lcd_dpi_panel_config_t pc = {};
  pc.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
  pc.dpi_clock_freq_mhz = B->dpi_mhz;
  pc.in_color_format = LCD_COLOR_FMT_RGB565;
  pc.num_fbs = 1;
  pc.video_timing.h_size = B->h_res;
  pc.video_timing.v_size = B->v_res;
  pc.video_timing.hsync_pulse_width = B->hpw;
  pc.video_timing.hsync_back_porch = B->hbp;
  pc.video_timing.hsync_front_porch = B->hfp;
  pc.video_timing.vsync_pulse_width = B->vpw;
  pc.video_timing.vsync_back_porch = B->vbp;
  pc.video_timing.vsync_front_porch = B->vfp;
  pc.flags.use_dma2d = 1;   // Zeichenpuffer per DMA2D in den Bildspeicher kopieren (CPU frei)
  e = esp_lcd_new_panel_dpi(bus, &pc, &s_panel);
  if (e != ESP_OK) {
    log_e("DPI-Panel: %s", esp_err_to_name(e));
    return false;
  }
  if (B->lcd == P4_LCD_EK79007) {   // wie esp_lcd_ek79007 von Espressif: Lanes, Gamma, Sleep Out
    dcs(io, 0xB2, B->dsi_lanes == 4 ? 0x00 : 0x10);
    static const uint8_t GAMMA[7] = {0x8B, 0x78, 0x84, 0x88, 0xA8, 0xE3, 0x88};
    for (int i = 0; i < 7; i++) dcs(io, 0x80 + i, GAMMA[i]);
    esp_lcd_panel_io_tx_param(io, 0x11, nullptr, 0);
    delay(120);
  }
  if (esp_lcd_panel_init(s_panel) != ESP_OK) return false;
  void *fb = board_framebuffer();
  if (fb) memset(fb, 0, (size_t)B->h_res * B->v_res * 2);   // schwarz, bevor das Licht angeht
  esp_lcd_dpi_panel_event_callbacks_t cbs = {};
  cbs.on_color_trans_done = on_trans_done;
  esp_lcd_dpi_panel_register_event_callbacks(s_panel, &cbs, nullptr);
  log_i("Display %dx%d MIPI-DSI %d Lanes, %.1f MHz", B->h_res, B->v_res, B->dsi_lanes, B->dpi_mhz);
  return true;
}

void *board_framebuffer() {
  void *fb = nullptr;
  if (s_panel) esp_lcd_dpi_panel_get_frame_buffer(s_panel, 1, &fb);
  return fb;
}

static void flush_cb(lv_disp_drv_t *, const lv_area_t *a, lv_color_t *px) {
  // asynchron: on_trans_done meldet LVGL, wann der Puffer frei ist
  if (esp_lcd_panel_draw_bitmap(s_panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, px) != ESP_OK)
    lv_disp_flush_ready(&s_disp_drv);
}

bool board_init() {
  B = &p4_board();
  log_i("Board: %s", B->name);
  s_i2c_mtx = xSemaphoreCreateRecursiveMutex();
  // WLAN/Bluetooth laufen ueber den C6: SDIO-Pins vor dem ersten WiFi-Aufruf festlegen
  WiFi.setPins(B->c6_clk, B->c6_cmd, B->c6_d0, B->c6_d1, B->c6_d2, B->c6_d3, B->c6_rst);
  Wire.begin(B->i2c_sda, B->i2c_scl, 400000);
  rtc_probe();
  if (!lcd_init()) return false;
  touch_init();

  lv_init();
  // zwei Zeichenpuffer intern (DMA2D kopiert den einen, waehrend LVGL in den anderen zeichnet)
  size_t px = (size_t)LCD_H_RES * LVGL_BUF_LINES;
  size_t bytes = px * sizeof(lv_color_t);
  lv_color_t *b1 = (lv_color_t *)heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  lv_color_t *b2 = (lv_color_t *)heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  if (!b1) return false;
  lv_disp_draw_buf_init(&s_draw_buf, b1, b2, px);
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
  log_i("Display bereit");
  return true;
}
#endif
