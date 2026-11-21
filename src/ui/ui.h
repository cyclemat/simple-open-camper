// Oeffentliche Schnittstelle der Panel-Oberflaeche
#pragma once
void ui_init();
void ui_loop();                          // regelmaessig aus dem Arduino-loop aufrufen
void ui_bg_changed(const char *page);    // Hintergrundbild einer Seite wurde ersetzt
void ui_settings_changed();              // Einstellungen wurden extern (Web) geaendert
void ui_saver_start();                   // Bildschirmschoner sofort zeigen (auch Vorschau)
void ui_saver_logo_changed();            // eigenes Logo neu laden
#include <string>
// Live-Ansicht (Browser/App unter /live): Schaltbefehl in den UI-Task geben. a: 't' umschalten (wie Antippen),
// 'b' Helligkeit %, 'h' hue*1000+sat, 'c' Mired, 'm' MultiPlus-Schalter, 'l' Begrenzung 0,1 A. Nur Kacheln, die auf
// dem Panel liegen. false = Warteschlange voll bzw. unbekannte Aktion.
bool ui_live_queue(const std::string &key, char a, int value);
// Live-Ansicht: alle Kacheln fertig berechnet wie am Panel (Wert, Unterzeile, Farbe, Pegel, Ein/Aus) als JSON
// {"v":n,"t":{"<key>":{"n","v","s","a","f","l","lc","d","x","b"}}}. Aufruf aus dem Webserver; der UI-Task baut den Stand
// nur, solange jemand abfragt (15 s), und gibt ihn danach frei. Leer = noch kein Stand (gleich nochmal fragen).
// detail = Kachel, deren Fenster gerade offen ist: dann zusaetzlich "dk" und "r":[[Bezeichnung,Wert],…] (alle Werte)
std::string ui_live_snapshot(const std::string &detail = std::string());
