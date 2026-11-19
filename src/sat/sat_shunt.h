// Satellit als SmartShunt (Batteriewaechter): INA228 am I2C-Bus mit externem Shunt (50 A … 2000 A, 50–100 mV).
// Misst Spannung, Strom, Leistung; Coulomb-Zaehlung, Ladezustand (SOC) mit Peukert und Ladewirkungsgrad, automatische
// Synchronisierung auf 100 %, Restlaufzeit, Historie, Alarme. Werte gehen als Punkte ans Panel/MQTT/Home Assistant.
// Nur mit -DSAT_SHUNT=1 (Projekt shunt/, ESP32-C3 Super Mini). Spezifikation: docs/smartshunt.md
// Keine Sicherheitsfunktion: Absicherung, BMS und Schutzgeraete bleiben noetig.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <vector>
#include "../sat_proto.h"
#ifndef SAT_SHUNT
#define SAT_SHUNT 0
#endif
#define SHUNT_IDX_BASE 50   // Punkte analog51 … analog60
#if SAT_SHUNT
void sat_shunt_begin();
void sat_shunt_loop();
uint32_t sat_shunt_version();
void sat_shunt_points(std::vector<SatPoint> &out);
void sat_shunt_json(JsonObject o);            // Einstellungen, Messwerte, Historie fuer die Weboberflaeche
String sat_shunt_set(JsonObjectConst o);      // "" = gespeichert, sonst Fehlertext
String sat_shunt_action(JsonObjectConst o);   // sync, soc, zero, reset_hist
void sat_shunt_for_pins(const std::function<void(int, const String &, bool, bool)> &f);
#else
inline void sat_shunt_begin() {}
inline void sat_shunt_loop() {}
inline uint32_t sat_shunt_version() { return 0; }
inline void sat_shunt_points(std::vector<SatPoint> &) {}
inline void sat_shunt_json(JsonObject) {}
inline String sat_shunt_set(JsonObjectConst) { return "Diese Firmware hat keinen SmartShunt"; }
inline String sat_shunt_action(JsonObjectConst) { return "Diese Firmware hat keinen SmartShunt"; }
inline void sat_shunt_for_pins(const std::function<void(int, const String &, bool, bool)> &) {}
#endif
