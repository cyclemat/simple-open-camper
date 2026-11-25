// Eigene Kachel-Symbole (PNG) aus dem Flash (Upload im Editor -> /littlefs/icons) und von der SD-Karte (/sd/icons).
// Jedes Bild wird beim Start einmal dekodiert und als Deckungsmaske in 24/32/44 px im PSRAM gehalten (3,5 KB je Symbol);
// eine Laufzeit-Schrift (user_icon_font_*) liefert die Masken als Glyphen U+F000.. und faellt fuer alle anderen Zeichen
// auf die eingebaute Symbolschrift zurueck – Kacheln, Fenster und Zusatzanzeigen brauchen dadurch keine Sonderbehandlung.
// Kachel-ID im Layout: "my:<name>" (Name = Dateiname ohne .png, a-z 0-9 - _, hoechstens 20 Zeichen).
#pragma once
#include <string.h>
#include <string>
#include "fonts/icons.h"

#ifndef WITH_USER_ICONS
#define WITH_USER_ICONS 0
#endif

#define USER_ICON_PREFIX "my:"
#define USER_ICONS_MAX 32
#define USER_ICON_PNG_MAX (300 * 1024)

#if WITH_USER_ICONS
#include <lvgl.h>
extern lv_font_t user_icon_font_24, user_icon_font_32, user_icon_font_44;

void user_icons_begin();                      // nach LittleFS und sd_begin(), vor ui_init() – im UI-Task
void user_icons_reload();                     // Dateien neu einlesen (nur im UI-Task, danach Oberflaeche neu aufbauen)
const char *user_icons_utf8(const char *name);  // Zeichen der Laufzeit-Schrift, nullptr = unbekannt
std::string user_icons_json();                // {"icons":[{"name":…,"src":"flash"|"sd"}],"sd":…,"max":…} (jeder Task)
bool user_icons_valid_name(const char *name);
bool user_icons_store(const char *name, const uint8_t *png, size_t len, std::string &err);   // in den Flash
bool user_icons_remove(const char *name);     // nur Flash-Symbole
bool user_icons_path(const char *name, std::string &path);   // Datei zum Ausliefern (/api/icon)
#endif

// Zeichen fuer eine Symbol-ID: eingebaute ("bulb" …) oder eigene ("my:<name>"), Rueckfall Gluehbirne
static inline const char *icon_any(const char *id) {
#if WITH_USER_ICONS
  if (id && strncmp(id, USER_ICON_PREFIX, 3) == 0) {
    const char *u = user_icons_utf8(id + 3);
    return u ? u : ICON_BULB;
  }
#endif
  return icon_utf8(id);
}
static inline bool icon_is_user(const char *id) { return id && strncmp(id, USER_ICON_PREFIX, 3) == 0; }
