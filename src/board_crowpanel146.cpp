// Hardware-Schicht (board.h) fuer das ELECROW CrowPanel 1.46" Rotary Display – siehe board_crowpanel146.h
// Display und Touch ueber LovyanGFX (Panel_ST77961, Touch_CST816S – der CST816T spricht dasselbe Protokoll).
// LVGL zeichnet in zwei kleine interne Puffer, LovyanGFX schiebt sie per DMA ueber SPI.
#include "board.h"
#include "board_crowpanel146.h"

#if BOARD_CROWPANEL146
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "app_config.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "driver/gpio.h"

// ---------------------------------------------------------------- Pins
static const int PIN_RAIL_1 = 1, PIN_RAIL_2 = 2;   // Versorgung Display/Peripherie (HIGH = an)
static const int PIN_POWER_LED = 40;               // Betriebs-LED (LOW wie im Elecrow-Beispiel)
static const int PIN_BL = 46;
static const int PIN_KNOB_A = 45, PIN_KNOB_B = 42, PIN_KNOB_KEY = 41;   // Taster aktiv LOW

namespace {
// ST77961 mit den Startbefehlen dieses Displays. LovyanGFX 1.3 bringt einen allgemeinen ST77961-Ablauf mit, mit dem
// das CrowPanel nur Streifen zeigt; die Werte unten stammen aus der LovyanGFX-Kopie (1.2.7) in Elecrow-RD/CrowPanel-
// 1.46inch-HMI-ESP32-Rotary-Display (libraries/LovyanGFX/.../Panel_ST77961.hpp; LovyanGFX: FreeBSD-Lizenz).
struct Panel_ST77961_Crow146 : public lgfx::Panel_LCD {
  Panel_ST77961_Crow146() {
    _cfg.panel_width = _cfg.memory_width = 360;
    _cfg.panel_height = _cfg.memory_height = 360;
    _cfg.dummy_read_pixel = 8;
  }

 protected:
  const uint8_t *getInitCommands(uint8_t listno) const override {
    static constexpr uint8_t list0[] = {
        0xDE, 1, 0x00,
        0xDF, 2, 0x98, 0x55,
        0xB2, 1, 0x1F,
        0xB7, 4, 0x00, 0x1D, 0x00, 0x45,
        0xBB, 6, 0x1B, 0x64, 0xC4, 0x1E, 0x3E, 0xF5,
        0xBC, 4, 0x03, 0x20, 0xF3, 0xC0,
        0xC0, 2, 0x22, 0xA1,
        0xC3, 11, 0x00, 0x02, 0x2A, 0x0B, 0x08, 0x48, 0x08, 0x04, 0x62, 0x30, 0x30,
        0xC4, 11, 0x40, 0x00, 0xAD, 0x68, 0x43, 0x07, 0x04, 0x16, 0x43, 0x07, 0x04,
        0xC8, 32,
        0x3F, 0x36, 0x30, 0x2D, 0x2F, 0x31, 0x2B, 0x2A, 0x27, 0x25, 0x1F, 0x14, 0x10, 0x09, 0x04, 0x00,
        0x3F, 0x36, 0x30, 0x2D, 0x2F, 0x31, 0x2B, 0x2A, 0x27, 0x25, 0x1F, 0x14, 0x10, 0x09, 0x04, 0x00,
        0xD3, 2, 0x28, 0x13,
        0xD9, 6, 0x00, 0x00, 0xFF, 0x00, 0xF0, 0x00,
        0xDE, 1, 0x01,
        0xB7, 8, 0x13, 0xE7, 0x64, 0x39, 0x06, 0x36, 0x19, 0x1C,
        0xBE, 1, 0x00,
        0xC1, 3, 0x00, 0x4A, 0x80,
        0xC2, 4, 0x00, 0x16, 0xDA, 0xE7,
        0xC7, 8, 0x00, 0x00, 0x00, 0x38, 0x08, 0x08, 0x00, 0x01,
        0xC8, 6, 0x00, 0x00, 0x00, 0x00, 0x15, 0x3B,
        0xC9, 5, 0x00, 0x16, 0x06, 0x04, 0x0A,
        0xCA, 5, 0x08, 0x35, 0x16, 0x1F, 0x1F,
        0xCB, 5, 0x01, 0x16, 0x07, 0x05, 0x0B,
        0xCC, 5, 0x09, 0x35, 0x16, 0x1F, 0x1F,
        0xCD, 5, 0x01, 0x16, 0x09, 0x0B, 0x05,
        0xCE, 5, 0x07, 0x15, 0x1F, 0x16, 0x1F,
        0xCF, 5, 0x00, 0x16, 0x08, 0x0A, 0x04,
        0xD0, 5, 0x06, 0x15, 0x1F, 0x16, 0x1F,
        0xD1, 2, 0x02, 0x30,
        0xD2, 5, 0x02, 0x03, 0x52, 0xDF, 0xDD,
        0xD3, 3, 0x3B, 0x04, 0x48,
        0xD5, 7, 0x10, 0x10, 0x07, 0x07, 0x0F, 0x94, 0x26,
        0xD6, 3, 0x00, 0x00, 0x40,
        0xD7, 3, 0x00, 0x00, 0x20,
        0xDE, 1, 0x02,
        0xB6, 1, 0x1C,
        0xDE, 1, 0x00,
        0x4D, 1, 0x00,
        0x4E, 1, 0x00,
        0x4F, 1, 0x00,
        0x4C, 1 + CMD_INIT_DELAY, 0x01, 10,
        0x4C, 1, 0x00,
        0x35, 0,
        0x3A, 1, 0x05,
        0x2A, 4, 0x00, 0x00, 0x01, 0x67,
        0x2B, 4, 0x00, 0x00, 0x01, 0x67,
        CMD_SLPOUT, CMD_INIT_DELAY, 120,
        CMD_DISPON, 0,
        0xFF, 0xFF,   // Ende
    };
    return listno == 0 ? list0 : nullptr;
  }
  void setColorDepth_impl(lgfx::color_depth_t depth) override {
    _write_depth = ((int)depth & lgfx::color_depth_t::bit_mask) > 16 ? lgfx::rgb888_3Byte : lgfx::rgb565_2Byte;
    _read_depth = _write_depth;
  }
};

class Crow146 : public lgfx::LGFX_Device {
  Panel_ST77961_Crow146 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Touch_CST816S touch_;   // Hintergrundlicht steuern wir selbst (LEDC wie im Elecrow-Beispiel)

 public:
  Crow146() {
    {
      auto c = bus_.config();
      c.spi_host = SPI2_HOST;
      c.freq_write = 40000000;   // 80 MHz wie bei Elecrow zeigte Streifen auf dunklem Grund – testweise 40
      c.freq_read = 20000000;
      c.spi_3wire = true;
      c.use_lock = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = 10;
      c.pin_mosi = 11;
      c.pin_miso = -1;
      c.pin_dc = 3;
      bus_.config(c);
      panel_.setBus(&bus_);
    }
    {
      auto c = panel_.config();
      c.pin_cs = 9;
      c.pin_rst = 14;
      c.memory_width = 360;
      c.memory_height = 360;
      c.panel_width = 360;
      c.panel_height = 360;
      c.invert = false;
      c.rgb_order = true;
      c.readable = false;
      panel_.config(c);
    }
    {
      auto c = touch_.config();
      c.i2c_port = 0;
      c.pin_sda = 6;
      c.pin_scl = 7;
      c.pin_int = 5;
      c.pin_rst = 13;
      c.i2c_addr = 0x15;
      c.freq = 400000;
      c.x_min = 0;
      c.x_max = 359;
      c.y_min = 0;
      c.y_max = 359;
      c.bus_shared = false;
      c.offset_rotation = 0;
      touch_.config(c);
      panel_.setTouch(&touch_);
    }
    setPanel(&panel_);
  }
};
}  // namespace

static Crow146 s_lcd;
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_indev_drv_t s_indev_drv;
static bool s_touch_ok = false;
static char s_touch_info[40] = "kein Touch erkannt";

static uint8_t s_user_bl = 80;
static bool s_dimmed = false, s_swallow = false;
static BoardTouchHook s_hook = nullptr;
void board_set_touch_hook(BoardTouchHook h) { s_hook = h; }

// ---------------------------------------------------------------- Hintergrundlicht
static bool s_bl_init = false;
static void backlight_apply(uint8_t pct) {
  if (!s_bl_init) {
    s_bl_init = ledcAttach(PIN_BL, 5000, 8);
    if (!s_bl_init) log_e("Hintergrundlicht (GPIO %d) laesst sich nicht einrichten", PIN_BL);
  }
  ledcWrite(PIN_BL, (uint32_t)pct * 255 / 100);
}

void board_set_backlight(uint8_t pct) {
  if (pct > 100) pct = 100;
  if (pct < 3) pct = 3;
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

float board_battery_voltage() { return -1; }   // kein Akku
bool board_rtc_present() { return false; }
bool board_rtc_read(time_t &) { return false; }
void board_rtc_write(time_t) {}
bool board_touch_ok() { return s_touch_ok; }
void board_sd_select(bool) {}                  // keine SD-Karte
const char *board_touch_info() { return s_touch_info; }
void *board_framebuffer() { return nullptr; }  // kein Bildspeicher (SPI-Display)

// ---------------------------------------------------------------- Drehknopf (1-ms-Takt per esp_timer)
static volatile int s_knob_steps = 0;     // ganze Rasten
static volatile int s_knob_btn = 0;       // 1 kurz, 2 lang
static int8_t s_q = 0;
static uint8_t s_ab = 0;
static uint16_t s_key_ms = 0;
static bool s_key_long = false;

static void knob_tick(void *) {
  static const int8_t TR[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  uint8_t now = (uint8_t)(gpio_get_level((gpio_num_t)PIN_KNOB_A) << 1 | gpio_get_level((gpio_num_t)PIN_KNOB_B));
  s_q += TR[(s_ab << 2) | now];
  s_ab = now;
  // zwei Wechsel je Raste (am Geraet gemessen 2026-11-15: mit vier brauchte jeder Schritt zwei Rasten)
  if (s_q >= 2) {
    s_q = 0;
    s_knob_steps = s_knob_steps + 1;
  } else if (s_q <= -2) {
    s_q = 0;
    s_knob_steps = s_knob_steps - 1;
  }
  bool down = gpio_get_level((gpio_num_t)PIN_KNOB_KEY) == 0;
  if (down) {
    if (s_key_ms < 60000) s_key_ms++;
    if (s_key_ms == 800 && !s_key_long) {   // lang: schon beim Halten melden
      s_key_long = true;
      s_knob_btn = 2;
    }
  } else {
    if (s_key_ms >= 30 && !s_key_long) s_knob_btn = 1;   // entprellt: mindestens 30 ms
    s_key_ms = 0;
    s_key_long = false;
  }
}

int board_knob_take() {
  int n = s_knob_steps;
  s_knob_steps = 0;
  if (n && s_dimmed) {   // erstes Drehen weckt nur auf
    board_wake();
    return 0;
  }
  return n;
}
int board_knob_button() {
  int b = s_knob_btn;
  s_knob_btn = 0;
  if (b && s_dimmed) {
    board_wake();
    return 0;
  }
  return b;
}

static void knob_init() {
  pinMode(PIN_KNOB_A, INPUT_PULLUP);
  pinMode(PIN_KNOB_B, INPUT_PULLUP);
  pinMode(PIN_KNOB_KEY, INPUT_PULLUP);
  s_ab = (uint8_t)(digitalRead(PIN_KNOB_A) << 1 | digitalRead(PIN_KNOB_B));
  esp_timer_create_args_t a = {};
  a.callback = knob_tick;
  a.name = "knob";
  esp_timer_handle_t t = nullptr;
  if (esp_timer_create(&a, &t) == ESP_OK) esp_timer_start_periodic(t, 1000);
}

// ---------------------------------------------------------------- LVGL
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px) {
  uint32_t w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
  s_lcd.startWrite();
  s_lcd.setAddrWindow(a->x1, a->y1, w, h);
  s_lcd.writePixels((lgfx::rgb565_t *)px, w * h);
  s_lcd.endWrite();
  lv_disp_flush_ready(drv);
}

// CST816T wie im Elecrow-Beispiel (cst816t.cpp, mode_touch) einstellen: ab Werk wertet der Chip Wischbewegungen
// selbst aus und meldet dabei nur 1-2 Punkte, danach "losgelassen" – LVGL erkennt so kein Wischen.
// 0xFA IrqCtl = 0x40 (EnTouch: Punkte melden, solange beruehrt), 0xEC MotionMask = 0 (keine eigenen Gesten),
// 0xFE DisAutoSleep = 0xFF (kein Ruhezustand: sonst antwortet er erst nach der ersten Beruehrung auf I2C).
static void touch_setup() {
  const int port = 0, addr = 0x15;
  delay(100);   // nach dem Reset (LovyanGFX: nur 10 ms) braucht der Chip etwas, bis er auf I2C antwortet
  bool ok = false;
  for (int i = 0; i < 5 && !ok; i++) {
    ok = lgfx::i2c::writeRegister8(port, addr, 0xFA, 0x40, 0, 400000).has_value() &&
         lgfx::i2c::writeRegister8(port, addr, 0xEC, 0x00, 0, 400000).has_value() &&
         lgfx::i2c::writeRegister8(port, addr, 0xFE, 0xFF, 0, 400000).has_value();
    if (!ok) delay(20);
  }
  uint8_t id[4] = {0};
  bool rd = lgfx::i2c::readRegister(port, addr, 0xA7, id, 4, 400000).has_value();
  if (rd) snprintf(s_touch_info, sizeof(s_touch_info), "CST816T (ID %02X, FW %u)", id[0], id[3]);
  log_w("Touch: Einstellung %s, %s", ok ? "ok" : "fehlgeschlagen", s_touch_info);
}

// ---------------------------------------------------------------- LED-Ring (8x WS2812, GRB)
// Daten GPIO 48, Versorgung des Rings GPIO 17 (HIGH = an) – nach Elecrow factory.h/RotaryScreen. RMT mit 10 MHz
// (100 ns): 0 = 0,4 us hoch/0,85 us tief, 1 = 0,8 us hoch/0,45 us tief. RMT wird erst beim ersten Leuchten belegt.
static const int PIN_RING_DATA = 48, PIN_RING_POWER = 17, RING_N = 8;
static bool s_ring_init = false, s_ring_on = false;
static uint32_t s_ring_last[RING_N];

void board_ring_show(const uint32_t *rgb, int n) {
  if (n > RING_N) n = RING_N;
  bool any = false, same = s_ring_init;
  for (int i = 0; i < RING_N; i++) {
    uint32_t c = i < n ? rgb[i] & 0xFFFFFF : 0;
    any |= c != 0;
    if (s_ring_init && c != s_ring_last[i]) same = false;
  }
  if (same || (!any && !s_ring_on)) return;   // nichts geaendert bzw. Ring ist ohnehin aus
  if (!s_ring_init) {
    pinMode(PIN_RING_POWER, OUTPUT);
    if (!rmtInit(PIN_RING_DATA, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000)) {
      log_e("LED-Ring: RMT nicht verfuegbar");
      return;
    }
    s_ring_init = true;
  }
  if (any && !s_ring_on) {
    digitalWrite(PIN_RING_POWER, HIGH);
    delayMicroseconds(200);
  }
  static rmt_data_t sym[RING_N * 24];
  for (int i = 0; i < RING_N; i++) {
    uint32_t c = i < n ? rgb[i] & 0xFFFFFF : 0;
    s_ring_last[i] = c;
    uint32_t grb = ((c >> 8) & 0xFF) << 16 | ((c >> 16) & 0xFF) << 8 | (c & 0xFF);
    for (int b = 0; b < 24; b++) {
      bool one = grb & (1u << (23 - b));
      rmt_data_t &s = sym[i * 24 + b];
      s.level0 = 1;
      s.duration0 = one ? 8 : 4;
      s.level1 = 0;
      s.duration1 = one ? 5 : 9;
    }
  }
  rmtWrite(PIN_RING_DATA, sym, RING_N * 24, 20);
  if (!any) digitalWrite(PIN_RING_POWER, LOW);   // alle aus: Ring stromlos
  s_ring_on = any;
}

// Fehlersuche Touch (nur auf Anfrage ueber den seriellen Befehl "touch")
static uint32_t s_tdbg_until = 0;
void board_touch_debug(uint32_t ms) { s_tdbg_until = millis() + ms; }
bool board_touch_debug_on() { return s_tdbg_until && (int32_t)(s_tdbg_until - millis()) > 0; }

static void indev_read_cb(lv_indev_drv_t *, lv_indev_data_t *data) {
  lgfx::touch_point_t tp;
  bool pressed = s_lcd.getTouch(&tp) > 0;
  int16_t x = tp.x, y = tp.y;
  if (board_touch_debug_on()) {
    static bool was = false;
    static uint32_t t_last = 0;
    if (pressed != was) log_printf("Touch %s %d,%d\n", pressed ? "an" : "los", x, y);
    else if (pressed && millis() - t_last >= 40) {
      log_printf("  %d,%d\n", x, y);
      t_last = millis();
    }
    if (pressed && pressed != was) t_last = millis();
    was = pressed;
  }
  if (s_hook && s_hook(pressed ? 1 : 0, &x, &y)) {
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
  if (pressed) {
    s_touch_ok = true;
    data->point.x = constrain(x, 0, LCD_H_RES - 1);
    data->point.y = constrain(y, 0, LCD_V_RES - 1);
  }
  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

bool board_init() {
  // Versorgung kurz aus und wieder an: nach dem Hochladen startet nur der Chip neu, das Display bliebe sonst im alten
  // Zustand haengen und zeigte nur Streifen (erst Abziehen half)
  pinMode(PIN_RAIL_1, OUTPUT);
  pinMode(PIN_RAIL_2, OUTPUT);
  digitalWrite(PIN_RAIL_1, LOW);
  digitalWrite(PIN_RAIL_2, LOW);
  delay(150);
  digitalWrite(PIN_RAIL_1, HIGH);
  digitalWrite(PIN_RAIL_2, HIGH);
  pinMode(PIN_POWER_LED, OUTPUT);
  digitalWrite(PIN_POWER_LED, LOW);
  delay(120);   // Display nach dem Einschalten anlaufen lassen
  if (!s_lcd.init()) {
    log_e("Display ST77961 nicht gefunden");
    return false;
  }
  s_lcd.fillScreen(TFT_BLACK);
  backlight_apply(s_user_bl);
  log_w("CrowPanel 1.46: Display an, Licht %u %%", s_user_bl);
  s_touch_ok = s_lcd.touch() != nullptr;
  snprintf(s_touch_info, sizeof(s_touch_info), "%s", s_touch_ok ? "CST816T (I2C 0x15)" : "kein Touch erkannt");
  if (s_touch_ok) touch_setup();
  knob_init();

  lv_init();
  size_t px = (size_t)LCD_H_RES * LVGL_BUF_LINES;
  lv_color_t *b1 = (lv_color_t *)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  lv_color_t *b2 = (lv_color_t *)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
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
  s_indev_drv.long_press_time = 600;
  s_indev_drv.gesture_limit = 40;
  s_indev_drv.gesture_min_velocity = 3;
  lv_indev_drv_register(&s_indev_drv);
  log_i("CrowPanel 1.46: Display 360x360 bereit, Touch %s", s_touch_info);
  return true;
}
#endif
