// Truma CP Plus ueber den LIN-Bus: Das Geraet gibt sich als iNet Box aus (LIN-Slave, NAD 0x03).
// Hardware: LIN-Transceiver (z. B. TJA1020/TJA1021-Modul) an einer UART, LIN-Leitung an Pin 3, Masse an Pin 5
// einer RJ12-Buchse am Combi bzw. per Verteiler in der Leitung Combi – CP Plus. Danach am CP Plus einmal
// "PR SET" -> Init ausfuehren, damit es die neue "iNet Box" anmeldet.
// Schalter und Pins: Abschnitt [truma] in platformio.ini (cpplus, lin_uart, lin_rx, lin_tx, lin_en)
#pragma once
#include "truma_heat.h"

// Werte kommen aus dem Abschnitt [truma] in platformio.ini
#ifndef TRUMA_LIN_UART
#define TRUMA_LIN_UART 2
#endif
#ifndef TRUMA_LIN_RX
#define TRUMA_LIN_RX -1
#endif
#ifndef TRUMA_LIN_TX
#define TRUMA_LIN_TX -1
#endif
#ifndef TRUMA_LIN_EN
#define TRUMA_LIN_EN -1           // Treiberfreigabe (CS/SLP) des Transceivers, -1 = fest verdrahtet
#endif

static inline bool cpplus_enabled() { return WITH_TRUMA_CPPLUS; }
void cpplus_begin();
uint32_t cpplus_version();
bool cpplus_get(TrumaHeatInfo &out);

// Befehle – sie gelten, sobald das CP Plus den naechsten Statuspuffer abholt (meist innerhalb weniger Sekunden)
void cpplus_set_heat(bool on, float target_c, uint8_t mode);   // mode: 0 Eco, 1 Hoch, 2 Boost
void cpplus_set_water(TrumaWater w);
void cpplus_set_energy(TrumaEnergy e, uint16_t el_power);
void cpplus_sync_clock();                                      // Uhrzeit des Panels an das CP Plus geben
