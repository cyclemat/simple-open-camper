// Akku der 7B (CS8501-Lader): Spannung ueber den IO-Baustein, daraus Ladestand und geschaetzte Stromquelle.
// Die Platine meldet nicht, ob USB-Strom anliegt (nur LEDs) – das Panel schliesst es aus dem Spannungsverlauf.
#pragma once
#include <stdint.h>

enum PowerSrc : uint8_t {
  PWR_UNKNOWN = 0,     // noch zu wenig Messwerte
  PWR_NO_BATTERY,      // kein Akku angeschlossen bzw. nicht lesbar
  PWR_CHARGING,        // Spannung steigt: USB-Strom, Akku laedt
  PWR_USB_FULL,        // Spannung hoch und stabil: USB-Strom, Akku voll
  PWR_BATTERY,         // Spannung faellt: Betrieb aus dem Akku
};

struct PowerInfo {
  bool has_battery = false;
  float volt = 0;      // geglaettet
  int percent = 0;     // grob aus der Entladekurve (beim Laden zu hoch)
  PowerSrc src = PWR_UNKNOWN;
};

void power_tick();                  // 1x pro Sekunde aus der Oberflaeche
PowerInfo power_get();
const char *power_src_text(PowerSrc s);
