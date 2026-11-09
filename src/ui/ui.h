// Oeffentliche Schnittstelle der Panel-Oberflaeche
#pragma once
void ui_init();
void ui_loop();                          // regelmaessig aus dem Arduino-loop aufrufen
void ui_bg_changed(const char *page);    // Hintergrundbild einer Seite wurde ersetzt
void ui_settings_changed();              // Einstellungen wurden extern (Web) geaendert
void ui_saver_start();                   // Bildschirmschoner sofort zeigen (auch Vorschau)
void ui_saver_logo_changed();            // eigenes Logo neu laden
