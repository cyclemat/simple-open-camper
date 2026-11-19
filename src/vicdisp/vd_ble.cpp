// Victron-Display: Bluetooth ueber das gemeinsame Modul truma.cpp (wie bei den Satelliten). Es liest Truma
// LevelControl und Mopeka (Gasflaschen/Tanks), leitet Victron-Werbepakete an victron.cpp weiter und verbindet Akkus,
// Power Stations und SuperSense-Tankmodule (bms.cpp); mit WITH_TPMS auch Reifendruck (tpms.cpp). Bluetooth startet
// erst, wenn ein Geraet eingerichtet ist oder gesucht wird. Aus: iNet X, Hymer, Satelliten-Funk.
#include "../tpms.h"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include "../settings.h"

bool ble_proto_on(uint8_t p) {
  return p == BLE_P_TRUMA || p == BLE_P_MOPEKA || p == BLE_P_VICTRON || p == BLE_P_BMS || (WITH_TPMS && p == BLE_P_TPMS);
}

// Satelliten-Pakete gibt es hier nicht
bool panelsat_on_adv(const std::string &, const uint8_t *, size_t, int) { return false; }
