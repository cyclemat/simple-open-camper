// Victron-Display: Bluetooth ueber das gemeinsame Modul truma.cpp (wie bei den Satelliten). Es liest Truma
// LevelControl (Gasflaschen) und leitet Victron-Werbepakete an victron.cpp weiter. Bluetooth startet erst, wenn
// ein Geraet eingerichtet ist oder gesucht wird. Hier nur die Schalter: Truma LevelControl, Victron und Akkus (BMS,
// z. B. Super B – bms.cpp) an, alles andere (Mopeka, iNet X, Hymer, Satelliten-Funk) aus.
#include <stddef.h>
#include <stdint.h>
#include <string>
#include "../settings.h"

bool ble_proto_on(uint8_t p) { return p == BLE_P_TRUMA || p == BLE_P_VICTRON || p == BLE_P_BMS; }

// Satelliten-Pakete gibt es hier nicht
bool panelsat_on_adv(const std::string &, const uint8_t *, size_t, int) { return false; }
