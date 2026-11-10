// Zentrale Hardware- und Projektkonstanten
#pragma once

#define FW_VERSION "1.0.0"
#define FW_NAME    "Simply Open Camper"

// ---------- Display (Waveshare ESP32-S3-Touch-LCD-7B) ----------
#define LCD_H_RES 1024
#define LCD_V_RES 600
// 30 MHz ist das Maximum fuer dieses Panel; bei Flackern/Bilddrift auf 21 oder 18 MHz senken.
// Die Werte mit #ifndef lassen sich per -D ueberschreiben (Display-Tests: display-testing/platformio.ini)
#ifndef LCD_PCLK_HZ
#define LCD_PCLK_HZ (30 * 1000 * 1000)
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
#define LCD_BOUNCE_LINES 10
#endif
#define LCD_BOUNCE_BUFFER_PX (LCD_H_RES * LCD_BOUNCE_LINES)
// LVGL-Zeichenpuffer in Zeilen (liegt bevorzugt im internen RAM)
#ifndef LVGL_BUF_LINES
#define LVGL_BUF_LINES 20   // 40 KB intern; mehr kostet RAM, der WLAN/TCP fehlt
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
