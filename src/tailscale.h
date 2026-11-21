// Tailscale (Beta, nur P4): bringt das Panel per microlink (src/third_party/microlink, MIT) in ein Tailnet.
// Editor und Weboberflaeche sind dann von unterwegs unter der 100.x-Adresse erreichbar.
//
// Ab Werk AUS (NVS "tailscale": on, key, name). An: eigener Task "ts" (startet microlink, sobald WLAN da ist;
// Stoppen kann bis 30 s dauern – deshalb nicht im UI-Task). Anmeldung per Link (ohne Auth-Key: der Status liefert die
// Login-Adresse) oder mit Auth-Key (nur im NVS, nie in der Firmware). Die Schluessel des Geraets speichert microlink
// selbst (NVS "microlink", "ml_peers"); "Abmelden" loescht sie.
#pragma once
#include <string>

#ifndef WITH_TAILSCALE
#define WITH_TAILSCALE 0
#endif

struct TsStatus {
  bool avail = false;      // in der Firmware
  bool on = false;         // eingeschaltet
  bool has_key = false;    // Auth-Key gespeichert
  bool running = false;    // microlink laeuft
  int code = -1;           // microlink_state_t, -1 = nicht gestartet
  std::string state;       // Klartext
  std::string ip;          // 100.x.y.z, wenn verbunden
  std::string url;         // Anmelde-Link (nur solange angemeldet werden muss)
  std::string name;        // Geraetename im Tailnet
  int peers = 0;
};

#if WITH_TAILSCALE
void ts_begin();                                            // Einstellungen laden, Task nur wenn eingeschaltet
TsStatus ts_status();
// on/name/key setzen und anwenden (nullptr = unveraendert; key "" = loeschen -> Anmeldung per Link)
void ts_set(const bool *on, const std::string *name, const std::string *key);
void ts_logout();                                           // Geraeteschluessel loeschen, danach neu anmelden
std::string ts_status_line();                               // fuer den Debug-Log ("" = aus)
#else
inline void ts_begin() {}
inline TsStatus ts_status() { return TsStatus(); }
inline void ts_set(const bool *, const std::string *, const std::string *) {}
inline void ts_logout() {}
inline std::string ts_status_line() { return std::string(); }
#endif
