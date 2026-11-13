// Hardware-Test fuer ESP32-P4-Boards (zuerst ELECROW CrowPanel Advanced 7" V1.2) – eigene kleine Firmware,
// Umgebung crowpanel-p4-test (p4/platformio.ini). Prueft Schritt fuer Schritt, ob Pins und Timings aus der
// Board-Tabelle (src/board_p4_boards.cpp) stimmen, ohne LVGL und ohne den Rest des Panels:
//   1. Speicher (PSRAM), Chip-Revision          -> serielle Ausgabe (115200)
//   2. Display MIPI-DSI (EK79007) + Licht       -> Testbild: Farbbalken, weisser Rand, Ecken markiert
//   3. Touch GT911                              -> jeder Finger malt einen Punkt in eigener Farbe, Ausgabe x/y
//   4. WLAN ueber den ESP32-C6 (SDIO, esp-hosted) -> Netze suchen, Anzahl/Namen seriell
// Serielle Befehle: "scan" (WLAN erneut suchen), "clear" (Testbild neu), "bl <0..100>" (Helligkeit),
//   "fps" (gemessene Bildrate), "takt <MHz> [240|160|apll]" (Pixeltakt + Quelle, gespeichert, Neustart),
//   "takt" (zurueck auf den Wert der Board-Tabelle),
//   "reg <hex> <hex>" (Register des Display-Chips live schreiben, z. B. "reg b3 04"), "rd <hex>" (lesen).
// EK79007 (Datenblatt Rev. 1.7): B0 D7 = VCOM-Puffer an, B1 = HFRC/Dithering, B3 D2 = FRAME (1 = gleichbleibend,
// 0 = Polaritaet je Bild umkehren, Standard), 80..86 = Gamma.
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Preferences.h>
#include "esp_cache.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "../board_p4.h"

namespace {

const P4BoardDef *B = nullptr;
esp_lcd_panel_handle_t s_panel = nullptr;
uint16_t *s_fb = nullptr;   // Bildspeicher im PSRAM (RGB565), gehoert dem DPI-Treiber
int W = 0, H = 0;
uint8_t s_touch_addr = 0;
esp_lcd_panel_io_handle_t s_io = nullptr;   // Befehlskanal zum Display-Chip (DBI ueber DSI)
volatile uint32_t s_vsync = 0;   // Zaehler fuer die Bildrate (VSYNC-Rueckruf)
float s_dpi_mhz = 0;             // eingestellter Pixeltakt
uint8_t s_dpi_src = 0;           // 0 = PLL 240 MHz, 1 = PLL 160 MHz, 2 = APLL

bool IRAM_ATTR on_vsync(esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t *, void *) {
  s_vsync++;
  return false;
}

void load_clock() {   // Versuchswerte aus NVS "p4test", sonst Board-Tabelle
  Preferences p;
  p.begin("p4test", true);
  s_dpi_mhz = p.getFloat("mhz", B->dpi_mhz);
  s_dpi_src = p.getUChar("src", 0);
  p.end();
}

float measure_fps(uint32_t ms = 2000) {
  uint32_t v0 = s_vsync, t0 = millis();
  delay(ms);
  return (s_vsync - v0) * 1000.0f / (millis() - t0);
}

// ---------------------------------------------------------------- Zeichnen direkt in den Bildspeicher
inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

void flush_rows(int y0, int y1) {   // Cache -> PSRAM, damit der DPI-DMA die neuen Pixel sieht
  if (y0 < 0) y0 = 0;
  if (y1 > H) y1 = H;
  if (y1 <= y0) return;
  esp_cache_msync(s_fb + (size_t)y0 * W, (size_t)(y1 - y0) * W * 2,
                  ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
}

void fill_rect(int x, int y, int w, int h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > W) w = W - x;
  if (y + h > H) h = H - y;
  if (w <= 0 || h <= 0) return;
  for (int j = 0; j < h; j++) {
    uint16_t *p = s_fb + (size_t)(y + j) * W + x;
    for (int i = 0; i < w; i++) p[i] = c;
  }
}

void test_pattern() {
  static const uint16_t BARS[8] = {rgb(255, 255, 255), rgb(255, 255, 0), rgb(0, 255, 255), rgb(0, 255, 0),
                                   rgb(255, 0, 255),   rgb(255, 0, 0),   rgb(0, 0, 255),   rgb(0, 0, 0)};
  int bw = W / 8;
  for (int i = 0; i < 8; i++) fill_rect(i * bw, 0, i == 7 ? W - 7 * bw : bw, H * 2 / 3, BARS[i]);
  for (int x = 0; x < W; x++) {   // Grauverlauf unten (zeigt Farbtiefe/Streifen)
    uint8_t v = x * 255 / (W - 1);
    fill_rect(x, H * 2 / 3, 1, H / 3, rgb(v, v, v));
  }
  uint16_t white = rgb(255, 255, 255), red = rgb(255, 0, 0);
  fill_rect(0, 0, W, 2, white);       // Rand: zeigt, ob das Bild verschoben/abgeschnitten ist
  fill_rect(0, H - 2, W, 2, white);
  fill_rect(0, 0, 2, H, white);
  fill_rect(W - 2, 0, 2, H, white);
  fill_rect(0, 0, 40, 40, red);       // Ecke oben links rot (Ausrichtung)
  flush_rows(0, H);
}

// ---------------------------------------------------------------- Display (MIPI-DSI, EK79007)
bool ldo_on(int chan, int mv) {
  if (chan < 0) return true;
  static esp_ldo_channel_handle_t h[2] = {nullptr, nullptr};
  static int n = 0;
  esp_ldo_channel_config_t c = {};
  c.chan_id = chan;
  c.voltage_mv = mv;
  esp_err_t e = esp_ldo_acquire_channel(&c, &h[n < 2 ? n++ : 1]);
  Serial.printf("LDO%d %d mV: %s\n", chan, mv, esp_err_to_name(e));
  return e == ESP_OK;
}

bool tx(esp_lcd_panel_io_handle_t io, uint8_t cmd, std::initializer_list<uint8_t> d, int delay_ms = 0) {
  uint8_t buf[8];
  size_t n = 0;
  for (uint8_t v : d) buf[n++] = v;
  esp_err_t e = esp_lcd_panel_io_tx_param(io, cmd, n ? buf : nullptr, n);
  if (delay_ms) delay(delay_ms);
  if (e != ESP_OK) Serial.printf("  Befehl 0x%02X: %s\n", cmd, esp_err_to_name(e));
  return e == ESP_OK;
}

bool display_begin() {
  ldo_on(B->ldo_mipi_chan, B->ldo_mipi_mv);
  ldo_on(B->ldo_aux_chan, B->ldo_aux_mv);

  esp_lcd_dsi_bus_config_t bc = {};
  bc.bus_id = 0;
  bc.num_data_lanes = B->dsi_lanes;
  // Chip-Revision < 3 (z. B. v1.3): PHY-Takt nur aus PLL_F20M ("LEGACY"); XTAL (IDF-5.5-Standard) erst ab v3.0
  esp_chip_info_t ci;
  esp_chip_info(&ci);
  bc.phy_clk_src = ci.revision < 300 ? MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT_LEGACY : MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT;
  bc.lane_bit_rate_mbps = B->dsi_mbps;
  esp_lcd_dsi_bus_handle_t bus = nullptr;
  esp_err_t e = esp_lcd_new_dsi_bus(&bc, &bus);
  Serial.printf("DSI-Bus (%d Lanes, %d Mbit/s): %s\n", B->dsi_lanes, B->dsi_mbps, esp_err_to_name(e));
  if (e != ESP_OK) return false;

  esp_lcd_dbi_io_config_t ic = {};
  ic.virtual_channel = 0;
  ic.lcd_cmd_bits = 8;
  ic.lcd_param_bits = 8;
  esp_lcd_panel_io_handle_t io = nullptr;
  e = esp_lcd_new_panel_io_dbi(bus, &ic, &io);
  s_io = io;
  Serial.printf("DBI (Befehle): %s\n", esp_err_to_name(e));
  if (e != ESP_OK) return false;

  if (B->lcd_rst >= 0) {   // Reset aktiv Low
    pinMode(B->lcd_rst, OUTPUT);
    digitalWrite(B->lcd_rst, LOW);
    delay(10);
    digitalWrite(B->lcd_rst, HIGH);
    delay(20);
  }

  esp_lcd_dpi_panel_config_t pc = {};
  pc.virtual_channel = 0;
  static const mipi_dsi_dpi_clock_source_t SRC[3] = {MIPI_DSI_DPI_CLK_SRC_PLL_F240M, MIPI_DSI_DPI_CLK_SRC_PLL_F160M,
                                                     MIPI_DSI_DPI_CLK_SRC_APLL};
  load_clock();
  pc.dpi_clk_src = SRC[s_dpi_src < 3 ? s_dpi_src : 0];
  pc.dpi_clock_freq_mhz = s_dpi_mhz;
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
  pc.flags.use_dma2d = 1;
  e = esp_lcd_new_panel_dpi(bus, &pc, &s_panel);
  static const char *SRCN[3] = {"PLL 240", "PLL 160", "APLL"};
  Serial.printf("DPI-Panel %dx%d, %.1f MHz aus %s: %s\n", B->h_res, B->v_res, s_dpi_mhz, SRCN[s_dpi_src % 3],
                esp_err_to_name(e));
  if (e != ESP_OK) return false;
  esp_lcd_dpi_panel_event_callbacks_t cbs = {};
  cbs.on_vsync = on_vsync;
  esp_lcd_dpi_panel_register_event_callbacks(s_panel, &cbs, nullptr);

  // EK79007: 2 Lanes, Herstellerwerte (wie esp_lcd_ek79007 von Espressif), Sleep Out
  if (B->lcd == P4_LCD_EK79007) {
    tx(io, 0xB2, {uint8_t(B->dsi_lanes == 4 ? 0x00 : 0x10)});
    tx(io, 0x80, {0x8B});
    tx(io, 0x81, {0x78});
    tx(io, 0x82, {0x84});
    tx(io, 0x83, {0x88});
    tx(io, 0x84, {0xA8});
    tx(io, 0x85, {0xE3});
    tx(io, 0x86, {0x88});
    tx(io, 0x11, {}, 120);
  }
  e = esp_lcd_panel_init(s_panel);
  Serial.printf("Panel-Init (Video an): %s\n", esp_err_to_name(e));
  if (e != ESP_OK) return false;

  void *fb = nullptr;
  esp_lcd_dpi_panel_get_frame_buffer(s_panel, 1, &fb);
  s_fb = (uint16_t *)fb;
  W = B->h_res;
  H = B->v_res;
  Serial.printf("Bildspeicher %p (%s)\n", fb, esp_ptr_external_ram(fb) ? "PSRAM" : "intern");
  return s_fb != nullptr;
}

void backlight(int pct) {
  static bool init = false;
  if (B->bl_power >= 0) {
    pinMode(B->bl_power, OUTPUT);
    digitalWrite(B->bl_power, B->bl_power_active_low ? LOW : HIGH);
  }
  if (B->bl_pwm < 0) return;
  if (!init) {
    ledcAttach(B->bl_pwm, 30000, 10);
    init = true;
  }
  ledcWrite(B->bl_pwm, constrain(pct, 0, 100) * 1023 / 100);
  Serial.printf("Licht %d %%\n", pct);
}

// ---------------------------------------------------------------- Touch GT911
bool gt_read(uint16_t reg, uint8_t *d, size_t n) {
  Wire.beginTransmission(s_touch_addr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(s_touch_addr, (uint8_t)n) != n) return false;
  for (size_t i = 0; i < n; i++) d[i] = Wire.read();
  return true;
}

void gt_write8(uint16_t reg, uint8_t v) {
  Wire.beginTransmission(s_touch_addr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.write(v);
  Wire.endTransmission();
}

bool touch_begin() {
  if (B->touch_rst >= 0 && B->touch_int >= 0) {   // Reset mit INT=Low -> Adresse 0x5D
    pinMode(B->touch_int, OUTPUT);
    pinMode(B->touch_rst, OUTPUT);
    digitalWrite(B->touch_int, LOW);
    digitalWrite(B->touch_rst, LOW);
    delay(10);
    digitalWrite(B->touch_rst, HIGH);
    delay(10);
    pinMode(B->touch_int, INPUT);
    delay(50);
  }
  Wire.begin(B->i2c_sda, B->i2c_scl, 400000);
  for (uint8_t a : {0x5D, 0x14}) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      s_touch_addr = a;
      break;
    }
  }
  if (!s_touch_addr) {
    Serial.println("Touch GT911: nicht gefunden (I2C 0x5D/0x14)");
    Serial.print("  I2C-Geraete:");
    for (uint8_t a = 1; a < 127; a++) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) Serial.printf(" 0x%02X", a);
    }
    Serial.println();
    return false;
  }
  uint8_t id[4] = {0}, res[4] = {0};
  gt_read(0x8140, id, 4);
  gt_read(0x8048, res, 4);
  Serial.printf("Touch GT911 an 0x%02X, Kennung %c%c%c%c, Aufloesung %dx%d\n", s_touch_addr, id[0], id[1], id[2],
                id[3], res[0] | res[1] << 8, res[2] | res[3] << 8);
  return true;
}

void touch_poll() {
  static const uint16_t COL[5] = {rgb(255, 0, 0), rgb(0, 255, 0), rgb(0, 128, 255), rgb(255, 255, 0), rgb(255, 0, 255)};
  static uint32_t last_print = 0;
  if (!s_touch_addr) return;
  uint8_t st;
  if (!gt_read(0x814E, &st, 1) || !(st & 0x80)) return;
  int n = st & 0x0F;
  if (n > 5) n = 5;
  uint8_t d[8 * 5];
  if (n && gt_read(0x814F, d, 8 * n)) {
    for (int i = 0; i < n; i++) {
      uint8_t *p = d + 8 * i;
      int x = p[1] | p[2] << 8, y = p[3] | p[4] << 8;
      fill_rect(x - 4, y - 4, 9, 9, COL[i]);
      flush_rows(y - 4, y + 5);
      if (millis() - last_print > 200) Serial.printf("Finger %d: x=%d y=%d\n", i + 1, x, y);
    }
    if (millis() - last_print > 200) last_print = millis();
  }
  gt_write8(0x814E, 0);
}

// ---------------------------------------------------------------- WLAN ueber den C6
void wifi_scan() {
  Serial.println("WLAN: Netze suchen ...");
  int n = WiFi.scanNetworks();
  if (n < 0) {
    Serial.printf("WLAN: Suche fehlgeschlagen (%d) – C6 erreichbar? Pins/Firmware pruefen\n", n);
    return;
  }
  Serial.printf("WLAN: %d Netze\n", n);
  for (int i = 0; i < n && i < 10; i++) Serial.printf("  %-32s %4d dBm  Kanal %d\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
  WiFi.scanDelete();
}

void wifi_begin() {
  bool ok = WiFi.setPins(B->c6_clk, B->c6_cmd, B->c6_d0, B->c6_d1, B->c6_d2, B->c6_d3, B->c6_rst);
  Serial.printf("WLAN: SDIO-Pins CLK %d CMD %d D0-D3 %d/%d/%d/%d RST %d: %s\n", B->c6_clk, B->c6_cmd, B->c6_d0, B->c6_d1,
                B->c6_d2, B->c6_d3, B->c6_rst, ok ? "ok" : "abgelehnt");
  WiFi.mode(WIFI_STA);
  wifi_scan();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  B = &p4_board();
  esp_chip_info_t ci;
  esp_chip_info(&ci);
  Serial.printf("\n=== P4-Hardwaretest: %s ===\n", B->name);
  Serial.printf("Chip ESP32-P4 Revision v%d.%d, %d Kerne, %lu MHz\n", ci.revision / 100, ci.revision % 100, ci.cores,
                (unsigned long)getCpuFrequencyMhz());
  Serial.printf("PSRAM %u KB (frei %u KB), intern frei %u KB\n", (unsigned)(ESP.getPsramSize() / 1024),
                (unsigned)(ESP.getFreePsram() / 1024), (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));

  if (display_begin()) {
    test_pattern();
    Serial.println("Testbild gezeichnet.");
  } else {
    Serial.println("Display: FEHLER (siehe oben)");
  }
  backlight(80);
  if (s_panel) {
    float fps = measure_fps();
    uint32_t tot = (uint32_t)(B->h_res + B->hpw + B->hbp + B->hfp) * (B->v_res + B->vpw + B->vbp + B->vfp);
    Serial.printf("Bildrate gemessen %.1f Hz -> echter Pixeltakt ca. %.1f MHz\n", fps, fps * tot / 1e6f);
  }
  touch_begin();
  wifi_begin();
  Serial.printf("Nach dem Start: intern frei %u KB, PSRAM frei %u KB\n",
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024), (unsigned)(ESP.getFreePsram() / 1024));
  Serial.println("Befehle: scan | clear | bl <0..100>");
}

void loop() {
  touch_poll();
  if (Serial.available()) {
    String c = Serial.readStringUntil('\n');
    c.trim();
    if (c == "scan") wifi_scan();
    else if (c == "clear" && s_fb) test_pattern();
    else if (c.startsWith("bl ")) backlight(c.substring(3).toInt());
    else if (c == "fps") Serial.printf("Bildrate %.1f Hz\n", measure_fps());
    else if (c.startsWith("reg ") && s_io) {
      int sp = c.indexOf(' ', 4);
      uint8_t r = strtol(c.substring(4, sp).c_str(), nullptr, 16);
      uint8_t v = sp > 0 ? strtol(c.substring(sp + 1).c_str(), nullptr, 16) : 0;
      esp_err_t e = esp_lcd_panel_io_tx_param(s_io, r, &v, 1);
      Serial.printf("Register %02X <- %02X: %s\n", r, v, esp_err_to_name(e));
    } else if (c.startsWith("rd ") && s_io) {
      uint8_t r = strtol(c.substring(3).c_str(), nullptr, 16), v = 0;
      esp_err_t e = esp_lcd_panel_io_rx_param(s_io, r, &v, 1);
      Serial.printf("Register %02X = %02X (%s)\n", r, v, esp_err_to_name(e));
    }
    else if (c == "takt" || c.startsWith("takt ")) {
      Preferences p;
      p.begin("p4test", false);
      if (c == "takt") {
        p.clear();
        Serial.println("Pixeltakt zurueck auf Board-Tabelle, Neustart ...");
      } else {
        String a = c.substring(5);
        a.trim();
        float mhz = a.toFloat();
        uint8_t src = a.indexOf("apll") >= 0 ? 2 : a.indexOf(" 160") >= 0 ? 1 : 0;
        p.putFloat("mhz", mhz);
        p.putUChar("src", src);
        Serial.printf("Pixeltakt %.1f MHz (Quelle %d) gespeichert, Neustart ...\n", mhz, src);
      }
      p.end();
      delay(200);
      ESP.restart();
    }
  }
  delay(10);
}
