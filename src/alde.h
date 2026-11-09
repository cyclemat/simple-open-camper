// Alde Compact 3020 / 3030 / 3030 Plus ueber den LIN-Bus (gelbe RJ12-Buchse "CI-Bus" am Bedienteil bzw. Kessel).
// Das Geraet ist LIN-Master: es fragt alle 500 ms den Infoblock (ID 0x1B) ab und schickt den Steuerblock
// (ID 0x1A) nur, wenn am Panel etwas geaendert wurde – Einstellungen am Alde-Bedienteil bleiben also gueltig.
// Protokoll nach daviddootson/alde_3030_plus und holdestmade/esphome-alde-3020 (beide MIT), eigene Umsetzung.
// Hardware: LIN-Transceiver (TJA1020/TJA1021/MCP2025), LIN = RJ12 Pin 4, Masse = Pin 2.
// Alles einstellbar im Abschnitt [alde] in platformio.ini.
#pragma once
#include "truma_heat.h"

#ifndef ALDE_MODEL
#define ALDE_MODEL 3030          // 3020 = Compact 3020, 3030 = Compact 3030, 3031 = Compact 3030 Plus
#endif
#ifndef ALDE_BAUD
#define ALDE_BAUD 19200          // gelber Bus; laut einer Quelle beim 3020 auch 9600
#endif
#ifndef ALDE_CHECKSUM
#define ALDE_CHECKSUM 0          // 0 = automatisch erkennen, 1 = klassisch (LIN 1.x), 2 = erweitert (LIN 2.x)
#endif
#ifndef ALDE_LIN_UART
#define ALDE_LIN_UART 2
#endif
#ifndef ALDE_LIN_RX
#define ALDE_LIN_RX -1
#endif
#ifndef ALDE_LIN_TX
#define ALDE_LIN_TX -1
#endif
#ifndef ALDE_LIN_EN
#define ALDE_LIN_EN -1           // Treiberfreigabe des Transceivers, -1 = fest verdrahtet
#endif
// Nur 3030: zweiter, interner Bus (rote RJ12) zum Mitlesen von Glykol- und Warmwassertemperatur
#ifndef ALDE_RED_UART
#define ALDE_RED_UART 1
#endif
#ifndef ALDE_RED_RX
#define ALDE_RED_RX -1           // -1 = nicht angeschlossen
#endif

static inline bool alde_enabled() { return WITH_ALDE; }
void alde_begin();
uint32_t alde_version();
bool alde_get(TrumaHeatInfo &out);

void alde_set_heat(bool on, float target_c);
void alde_set_water(TrumaWater w);               // TW_OFF, TW_ECO (= an), TW_BOOST
void alde_set_gas(bool on);
void alde_set_electric(uint8_t kw);              // 0..3
