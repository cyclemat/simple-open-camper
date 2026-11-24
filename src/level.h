// Wasserwaage: Neigung des Fahrzeugs aus einem Satelliten mit MPU-6050 (Punkte tilt1/tilt2) oder einem Ruuvi Tag
// (Beschleunigung aus Datenformat 5). Kachel "level:1" (VIC_LEVEL) mit Neigung laengs/quer und dem, was zu tun ist:
//  - Wohnwagen: welche Seite wie viele cm anheben, Stuetzrad wie viele cm hoch/runter
//  - Reisemobil: Keilhoehe je Rad, Hinweis wenn hoeher als die Auffahrkeile
// Wo der Sensor sitzt, spielt keine Rolle (das Fahrzeug ist starr). Einstellen nur in der Weboberflaeche:
// Einbaulage (Kante nach vorne/rechts/hinten/links, links/rechts vertauscht), Nullpunkt, Fahrzeugmasse und ein
// Assistent, der die Einbaulage selbst bestimmt (Front anheben, optional rechte Seite anheben).
// Vorzeichen: Neigung laengs + = Front tiefer, quer + = rechte Seite hoeher.
#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "victron.h"

#ifndef WITH_LEVEL
#if PANEL_SATELLITE || SOC_VICTRON_EDITION
#define WITH_LEVEL 0
#else
#define WITH_LEVEL 1
#endif
#endif

#define LEVEL_KEY "level:1"

struct LevelCfg {
  bool on = false;
  std::string src;          // "satio:<sat>/tilt" (Satellit) bzw. "bms:<mac>" (Ruuvi), "" = keiner
  uint8_t type = 0;         // 0 Wohnwagen, 1 Reisemobil
  float yaw = 0;            // Einbaulage: Drehung in Grad (0 = Kante/Pfeil nach vorne, 90 rechts, 180 hinten, 270 links)
  bool mirror = false;      // links/rechts vertauscht (Sensor kopfueber)
  float z[2] = {0, 0};      // Nullpunkt Satellit (Grad laengs/quer, wie geliefert)
  float g0[3] = {0, 0, 1};  // Nullpunkt Ruuvi (Richtung der Schwerkraft im Sensor, wenn das Fahrzeug gerade steht)
  int track_cm = 200;       // Spurweite (Rad-Mitte zu Rad-Mitte)
  int hitch_cm = 450;       // Wohnwagen: Achse bis Stuetzrad
  int wb_cm = 380;          // Reisemobil: Radstand
  int ramp_cm = 10;         // Reisemobil: hoechste Stufe der Auffahrkeile
  float tol = 0.3f;         // so wenig Neigung gilt als gerade (Grad)
};

struct LevelSource {
  std::string key, name;    // name: "Satellit Heck" bzw. "Ruuvi Kabine"
};

#if WITH_LEVEL
void level_begin();
LevelCfg level_cfg();
void level_set_cfg(const LevelCfg &c);
uint32_t level_version();
std::vector<LevelSource> level_sources();
// Rohwerte der Quelle nach Nullpunkt in Grad (Sensor-Achsen, noch ohne Einbaulage); false = kein Wert
bool level_raw(float &a, float &b, int32_t *age_s = nullptr);
std::string level_zero();                     // Nullpunkt jetzt setzen; "" = ok, sonst Fehlertext
// Assistent: "start" merkt den Ausgangswert, "front" (Front angehoben) bestimmt die Drehung, "right" (rechte Seite
// angehoben) prueft links/rechts, "cancel" bricht ab. Liefert den Text fuer den naechsten Schritt bzw. den Fehler.
std::string level_wizard(const std::string &step, bool &ok);
int level_wizard_step();                      // 0 aus, 1 wartet auf Front, 2 wartet auf rechte Seite
bool level_get(const std::string &key, VictronInfo &v);
std::vector<VictronInfo> level_list();
#else
inline void level_begin() {}
inline uint32_t level_version() { return 0; }
inline bool level_get(const std::string &, VictronInfo &) { return false; }
inline std::vector<VictronInfo> level_list() { return {}; }
#endif
