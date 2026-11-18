// Satellit: zusaetzliche Sensoren (bis SAT_SENS_MAX Steckplaetze, Weboberflaeche Karte "Weitere Sensoren", /api/sens).
//   I2C (gemeinsamer Bus mit MPU-6050/OLED): BME280/BMP280, SHT3x, SHT4x, AHT20, SCD40/41 (CO2), INA219, INA226
//   GPIO: DHT22, HX711 (Waegezelle), Durchfluss (YF-S201, Impulse)
//   Tanks (erscheinen als Tank mit %): Ultraschall (JSN-SR04T, HC-SR04), Schwimmer mit Widerstandsgeber (Ohm, an
//   einem Analog-Pin mit Vorwiderstand), Schwimmerschalter voll/leer
// Jeder Typ laesst sich im Build abschalten (satellite/platformio.ini, Abschnitt [sensors]); SAT_SENS=0 = ganz aus.
// Punkte: Werte als "analog<SENS_IDX_BASE+...>" mit eigener Klasse/Einheit, Tanks als "tank<SENS_TANK_BASE+n>".
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <vector>
#include "../sat_proto.h"

#ifndef SAT_SENS
#define SAT_SENS 1
#endif
#define SAT_SENS_MAX 8
#define SENS_IDX_BASE 10    // Werte: analog11 … analog42 (Steckplatz*4 + Wert); analog1–4 = eigene Analogeingaenge
#define SENS_TANK_BASE 4    // Tanks: tank5 … tank12 (tank1–4 = Tanks mit Sonden)

#if SAT_SENS
void sat_sens_begin();
void sat_sens_loop();
uint32_t sat_sens_version();
void sat_sens_points(std::vector<SatPoint> &out);
void sat_sens_json(JsonObject o);                    // Einstellungen, Typen und Live-Werte fuer die Weboberflaeche
String sat_sens_set(JsonObjectConst o);              // "" = gespeichert, sonst Fehlertext
String sat_sens_action(JsonObjectConst o);           // Tara, Kalibrieren, voll/leer uebernehmen, Zaehler zuruecksetzen
// Belegte GPIOs (fuer die Pruefung in sat_io): pin, Bezeichnung, braucht Ausgang, braucht Analog
void sat_sens_for_pins(const std::function<void(int, const String &, bool, bool)> &f);
#else
inline void sat_sens_begin() {}
inline void sat_sens_loop() {}
inline uint32_t sat_sens_version() { return 0; }
inline void sat_sens_points(std::vector<SatPoint> &) {}
inline void sat_sens_json(JsonObject) {}
inline String sat_sens_set(JsonObjectConst) { return "Diese Firmware hat keine Zusatzsensoren"; }
inline String sat_sens_action(JsonObjectConst) { return "Diese Firmware hat keine Zusatzsensoren"; }
inline void sat_sens_for_pins(const std::function<void(int, const String &, bool, bool)> &) {}
#endif
