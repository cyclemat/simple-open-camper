// Hardware-Schicht: RGB-Panel, Touch (GT911), IO-Erweiterung (CH32V003), LVGL-Anbindung
#pragma once
#include <stdint.h>
#include <time.h>
#include <lvgl.h>

bool board_init();                         // komplette Initialisierung inkl. lv_init()
void board_set_backlight(uint8_t pct);     // 0 = aus, 1..100 %
uint8_t board_backlight();                 // zuletzt gesetzte Nutzerhelligkeit
void board_dim(uint8_t pct);               // Ruhezustand (0 = Display aus)
void board_wake();                         // Ruhezustand beenden
bool board_is_dimmed();
float board_battery_voltage();             // Volt, < 0 wenn nicht lesbar
// Uhrenchip (PCF85063 an I2C 0x51), falls bestueckt. Zeit in UTC.
bool board_rtc_present();
bool board_rtc_read(time_t &utc);          // false = kein Chip oder Zeit ungueltig (Oszillator war aus)
void board_rtc_write(time_t utc);
bool board_touch_ok();
void board_sd_select(bool on);             // CS der SD-Karte (am IO-Expander)
const char *board_touch_info();
// Drehknopf mit Taster (nur Boards mit UI_KNOB, z. B. CrowPanel 1.46")
int board_knob_take();                     // Rasten seit dem letzten Aufruf (+ im Uhrzeigersinn, - dagegen)
int board_knob_button();                   // 0 nichts, 1 kurz, 2 lang gedrueckt (>= 0,8 s) – je Druck einmal
// LED-Ring (nur Boards mit UI_LEDRING): n Farben 0xRRGGBB, schon mit Helligkeit verrechnet; alle 0 = Ring stromlos
void board_ring_show(const uint32_t *rgb, int n);
void *board_framebuffer();
// Eigene Auswertung der Beruehrungen (bis zu zwei Finger, n = 0 losgelassen).
// Rueckgabe true = verbraucht: weder aufwecken noch an LVGL weitergeben. nullptr = aus.
typedef bool (*BoardTouchHook)(uint8_t n, const int16_t *x, const int16_t *y);
void board_set_touch_hook(BoardTouchHook h);                 // Bildspeicher (RGB565, im PSRAM) – nur fuer Diagnose
