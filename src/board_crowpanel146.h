// ELECROW CrowPanel 1.46" HMI ESP32 Rotary Display (Modell DHR55146D): ESP32-S3R8, 16 MB Flash, 8 MB PSRAM (OPI),
// rundes IPS-Display 360x360 (ST77961, SPI 3-Draht), Touch CST816T, Drehknopf mit Taster.
// Pins nach Elecrow-RD/CrowPanel-1.46inch-HMI-ESP32-Rotary-Display (Common_1_46.h, Lesson03-Rotate).
// Zusaetzlich zu board.h: Fehlersuche Touch. Drehknopf (1-ms-Takt, ohne eigenen Task) ueber board.h (UI_KNOB).
#pragma once
#include <stdint.h>

#ifndef BOARD_CROWPANEL146
#define BOARD_CROWPANEL146 0
#endif

#if BOARD_CROWPANEL146
// Drehknopf: board_knob_take/board_knob_button (board.h, UI_KNOB)
void board_touch_debug(uint32_t ms);   // serieller Befehl "touch": so lange Rohwerte und Gesten ausgeben
bool board_touch_debug_on();
#endif
