// Zentrale Hardware- und Projektkonstanten
#pragma once

#include "soc_version.h"
#define FW_VERSION SOC_VERSION   // gemeinsame Versionsnummer (soc_version.h)
#define FW_NAME    "Simple Open Camper"

// ---------- Display und Bedienung je Board ----------
// Merkmale, die jedes Board einzeln setzt (weitere Geraete, z. B. andere runde Displays, kombinieren sie neu):
//   UI_ROUND      runde Oberflaeche (ui_main.cpp): eine Kachel je Bildschirm, links/rechts = Geraet, hoch/runter =
//                 Datenseiten, Info-Seite; Editor zeigt einen Kreis. Groesse kommt aus LCD_H_RES/LCD_V_RES.
//   UI_KNOB       Drehknopf mit Taster (board_knob_take/board_knob_button): drehen = Geraet, druecken = schalten
//   UI_LEDRING    Anzahl LEDs im Ring (WS2812, board_ring_show): Drehen anzeigen, Warnung, mit dem Display dimmen
//   UI_WEB_SETUP  kein Einstellungsmenue am Display, eingerichtet wird nur im Browser: ohne Camp-WLAN gleich das
//                 Panel-WLAN (4.3.2.1, Captive Portal, Kanal 6), nach 3 min ohne Camp-WLAN wieder zuschalten
#if BOARD_CROWPANEL146   // ELECROW CrowPanel 1.46" Rotary (DHR55146D): rund 360x360, Drehknopf
#define LCD_H_RES 360
#define LCD_V_RES 360
#define UI_ROUND 1
#define UI_KNOB 1
#define UI_WEB_SETUP 1
#define UI_LEDRING 8
#else                    // Waveshare ESP32-S3-Touch-LCD-7B, CrowPanel Advanced 7" (P4)
#define LCD_H_RES 1024
#define LCD_V_RES 600
#endif
#ifndef UI_ROUND
#define UI_ROUND 0
#endif
#ifndef UI_KNOB
#define UI_KNOB 0
#endif
#ifndef UI_WEB_SETUP
#define UI_WEB_SETUP 0
#endif
#ifndef UI_LEDRING
#define UI_LEDRING 0
#endif
// 30 MHz ist das Maximum fuer dieses Panel; bei Flackern/Bilddrift auf 21 oder 18 MHz senken.
// Die Werte mit #ifndef lassen sich per -D ueberschreiben (Display-Tests: display-testing/platformio.ini)
// Bildwiederholrate = Pixeltakt / (1386 x 661 Bildpunkte inkl. Sync-Raender). 2026-11-17: 30 MHz (33 Hz) -> 25 Hz,
// weil der Bildschirm sonst zu viel aus dem PSRAM liest (Bildfehler bei vollen Seiten).
#ifndef LCD_PCLK_HZ
#define LCD_PCLK_HZ (25 * (LCD_H_RES + LCD_HSYNC_PULSE + LCD_HSYNC_BACK + LCD_HSYNC_FRONT) * \
                     (LCD_V_RES + LCD_VSYNC_PULSE + LCD_VSYNC_BACK + LCD_VSYNC_FRONT))   // 25 Hz = 22,9 MHz
#endif
#define LCD_HSYNC_PULSE 162
#define LCD_HSYNC_BACK  152
#define LCD_HSYNC_FRONT 48
#define LCD_VSYNC_PULSE 45
#define LCD_VSYNC_BACK  13
#define LCD_VSYNC_FRONT 3
#define LCD_PCLK_ACTIVE_NEG 1
// Bounce-Buffer (Pixel). 0 = aus. 10 Zeilen ist der Espressif-Standard.
#ifndef LCD_BOUNCE_LINES
// 2026-11-17 mit 15 getestet (+20 KB intern): Bildfehler blieben -> wieder 10. 600 muss durch die Zahl teilbar sein.
#define LCD_BOUNCE_LINES 10
#endif
#define LCD_BOUNCE_BUFFER_PX (LCD_H_RES * LCD_BOUNCE_LINES)
// malloc() unter dieser Groesse bleibt im internen RAM, darueber PSRAM (Framework-Vorgabe 1024, sdkconfig). S3-Panels:
// 16 -> auch die vielen kleinen Texte/Listen (Cerbo-, Zigbee-Werte, std::string) ins PSRAM; internes RAM bleibt fuer
// WLAN, Bluetooth und DMA (die fordern es ausdruecklich an). Waveshare hatte sonst ~8 KB intern frei (2026-11-17).
#include "sdkconfig.h"
#if BOARD_P4
#define APP_EXTMEM_MIN CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL
#else
#define APP_EXTMEM_MIN 16
#endif
// LVGL-Zeichenpuffer in Zeilen (liegt bevorzugt im internen RAM)
#ifndef LVGL_BUF_LINES
#if BOARD_P4 || BOARD_CROWPANEL146
#define LVGL_BUF_LINES 20   // genug internes RAM
#else
// Waveshare (ESP32-S3): 10 Zeilen = 20 KB intern. Mit WLAN, Zigbee, Cerbo und Bluetooth waren bei 20 Zeilen nur noch
// ~8 KB intern und kein DMA-Block frei (2026-11-17) – WLAN-Empfang und Bildtransfer brauchen diesen Speicher.
#define LVGL_BUF_LINES 10
#endif
#endif

#define PIN_LCD_HSYNC 46
#define PIN_LCD_VSYNC 3
#define PIN_LCD_DE    5
#define PIN_LCD_PCLK  7
// Reihenfolge: B0..B4, G0..G5, R0..R4 (RGB565, D0 = niederwertigstes Blau-Bit)
#define LCD_DATA_PINS {14, 38, 18, 17, 10,  39, 0, 45, 48, 47, 21,  1, 2, 42, 41, 40}

// ---------- I2C ----------
#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9
#define I2C_FREQ_HZ 400000

// ---------- Touch GT911 ----------
#define PIN_TOUCH_INT 4
#define GT911_ADDR_A 0x5D
#define GT911_ADDR_B 0x14
#define TOUCH_SWAP_XY  0
#define TOUCH_INVERT_X 0
#define TOUCH_INVERT_Y 0

// ---------- IO-Erweiterung CH32V003 ----------
#define EXIO_ADDR      0x24
#define EXIO_REG_DIR   0x02  // 1 = Ausgang
#define EXIO_REG_OUT   0x03
#define EXIO_REG_IN    0x04
#define EXIO_REG_PWM   0x05  // Hintergrundbeleuchtung, invertiert
#define EXIO_REG_ADC   0x06  // 10 Bit, Akkuspannung ueber 3:1-Teiler
#define EXIO_TP_RST    1
#define EXIO_DISP      2     // Hintergrundbeleuchtung an/aus
#define EXIO_LCD_RST   3
#define EXIO_SD_CS     4
#define EXIO_USB_SEL   5     // low = USB, high = CAN
#define EXIO_LCD_VDD   6
// ---------- microSD (SPI; CS ueber EXIO_SD_CS) ----------
#define PIN_SD_MOSI 11
#define PIN_SD_SCK  12
#define PIN_SD_MISO 13
#define SD_SPI_HOST SPI2_HOST
#define SD_FREQ_KHZ 20000

#define BL_PWM_MIN 1         // laut Waveshare/ESPHome sichere Grenzen fuer die 7B
#define BL_PWM_MAX 247

// ---------- Oberflaeche ----------
#define UI_TOPBAR_H 56
#define UI_CONTENT_H (LCD_V_RES - UI_TOPBAR_H)
#define MAX_PAGES 8
#define MAX_TILES_PER_PAGE 40
#define BG_BYTES (LCD_H_RES * LCD_V_RES * 2)       // dekodiertes Bild im PSRAM (RGB565)
#define BG_JPG_MAX (2 * 1024 * 1024)                // groesste JPEG-Datei fuer einen Hintergrund

// ---------- Netzwerk ----------
#define DEFAULT_HOSTNAME "simply-open-camper"
#define DEFAULT_MQTT_BASE "zigbee2mqtt"
#define DEFAULT_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
