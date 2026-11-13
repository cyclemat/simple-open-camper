// ESP32-P4-Panels: eine Firmware fuer alle Boards.
// Jedes Board (und jede Board-Revision) ist ein Eintrag in P4_BOARDS (board_p4_boards.cpp) mit Pins und
// Display-Timings. Welches Board aktiv ist, steht in den Einstellungen (NVS "panel", Schluessel "p4_board") und
// laesst sich ohne neues Kompilieren umstellen; P4_DEFAULT_BOARD (p4/platformio.ini, [p4] board) gilt nur beim ersten
// Start. Neues Board = neuer Tabelleneintrag (normaler Build, kein Framework-Neubau – alle P4-Boards teilen sich
// p4/ mit demselben custom_sdkconfig). Ausnahme: Chip-Revision 3 (esp32-p4_r3) braucht eine eigene Umgebung.
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifndef BOARD_P4
#define BOARD_P4 0
#endif
#ifndef P4_DEFAULT_BOARD
#define P4_DEFAULT_BOARD "crowpanel7-v12"
#endif

enum P4LcdDriver : uint8_t { P4_LCD_EK79007 = 1 };
enum P4TouchDriver : uint8_t { P4_TOUCH_GT911 = 1 };

struct P4BoardDef {
  const char *id;      // Schluessel in den Einstellungen, z. B. "crowpanel7-v12"
  const char *name;    // Anzeige, z. B. "ELECROW CrowPanel Advanced 7\" (V1.2)"
  // Display (MIPI-DSI)
  P4LcdDriver lcd;
  uint16_t h_res, v_res;
  uint8_t dsi_lanes;
  uint16_t dsi_mbps;   // je Lane
  float dpi_mhz;        // Pixeltakt; so waehlen, dass das Panel 60 Hz hat (darunter flackern mittlere Grautoene)
  uint16_t hpw, hbp, hfp, vpw, vbp, vfp;
  int8_t lcd_rst;
  int8_t ldo_mipi_chan;   // MIPI-PHY-Versorgung (-1 = keine)
  uint16_t ldo_mipi_mv;
  int8_t ldo_aux_chan;    // weitere LDO (-1 = keine)
  uint16_t ldo_aux_mv;
  // Hintergrundlicht
  int8_t bl_pwm;          // PWM-Pin (-1 = keiner)
  int8_t bl_power;        // Versorgung schalten (-1 = keine)
  bool bl_power_active_low;
  // Touch (I2C)
  P4TouchDriver touch;
  int8_t i2c_sda, i2c_scl, touch_int, touch_rst;
  // ESP32-C6 fuer WLAN/Bluetooth (esp-hosted ueber SDIO)
  int8_t c6_clk, c6_cmd, c6_d0, c6_d1, c6_d2, c6_d3, c6_rst;
  // SD-Karte (SDMMC, 1 Bit)
  int8_t sd_cmd, sd_clk, sd_d0;
};

extern const P4BoardDef P4_BOARDS[];
extern const size_t P4_BOARD_COUNT;

const P4BoardDef *p4_board_find(const char *id);   // nullptr = unbekannt
const P4BoardDef &p4_board();                       // aktives Board (Einstellung, sonst P4_DEFAULT_BOARD)
bool p4_board_select(const char *id);               // speichern; gilt nach dem Neustart
