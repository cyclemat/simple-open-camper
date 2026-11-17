// Satellit mit kleinem OLED (z. B. ABrobot ESP32-C3 mit 0,42"-OLED 72x40, SSD1306 an I2C) – nur mit -DSAT_OLED=1.
// Zeigt den Wert eines angeschlossenen Sensors gross an (Name klein darueber) oder, mit MPU-6050, eine Wasserwaage
// in zwei Richtungen (Blase im Kreis, Neigung laengs und quer). Ohne Sensorwert: Name und IP des Satelliten.
// Einstellungen in NVS "satoled": on, src ("" = automatisch, sonst Punkt-ID wie "temp1"), rot (180 Grad), con (Kontrast).
#pragma once
#include <ArduinoJson.h>

#ifndef SAT_OLED
#define SAT_OLED 0
#endif
#ifndef SAT_OLED_SDA
#define SAT_OLED_SDA 5      // ABrobot ESP32-C3 0,42" OLED
#endif
#ifndef SAT_OLED_SCL
#define SAT_OLED_SCL 6
#endif

#if SAT_OLED
void sat_oled_begin();
void sat_oled_loop();
bool sat_oled_present();
void sat_oled_json(JsonObject o);                  // Einstellungen + waehlbare Sensoren fuer die Weboberflaeche
bool sat_oled_apply(JsonObjectConst o);
#else
inline void sat_oled_begin() {}
inline void sat_oled_loop() {}
inline bool sat_oled_present() { return false; }
inline void sat_oled_json(JsonObject) {}
inline bool sat_oled_apply(JsonObjectConst) { return false; }
#endif
