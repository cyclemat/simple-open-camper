// Seiten-Layout (Kachelpositionen, Namen, Hintergruende) – gespeichert in LittleFS /layout.json
#pragma once
#include <stdint.h>
#include <map>
#include <string>
#include <vector>
#include <lvgl.h>
#include "z2m.h"

struct Tile {
  std::string key;     // ControlView::key
  int16_t x = 0, y = 0, w = 184, h = 116;
  std::string icon;    // Icon-ID, leer = automatisch
};
struct Page {
  std::string id, name;
  bool bg = false;
  std::vector<Tile> tiles;
};
struct Layout {
  uint32_t rev = 0;
  uint8_t grid = 8;
  std::vector<Page> pages;
  std::map<std::string, std::string> names;   // eigene Anzeigenamen je key
};

void layout_init();                       // LittleFS muss gemountet sein
Layout layout_get();                      // Kopie
uint32_t layout_version();
std::string layout_json();                // aktueller Stand als JSON
// Speichert Layout aus dem Web-Editor. Rueckgabe: HTTP-Status (200, 400, 409)
int layout_store_json(const char *json, size_t len, bool force, std::string &msg);
void layout_refresh_bg();                 // Hintergrund-Flags neu pruefen (nach Upload)
void layout_set_name(const std::string &key, const std::string &label);
void layout_auto_arrange(const std::vector<ControlView> &all, bool only_missing);
std::string layout_label(const Layout &l, const ControlView &v);   // Anzeigename

// Hintergrundbilder: JPEG 1024x600 je Seite, auf der SD-Karte oder im Flash
std::string bg_store_path(const std::string &page);   // Ziel fuer ein neues Bild
std::string bg_jpg_path(const std::string &page);     // vorhandenes JPEG, leer = keins
bool bg_exists(const std::string &page);
void bg_remove(const std::string &page);
void bg_migrate();                                     // nach sd_begin(): alte Dateien aufraeumen/verschieben
bool bg_jpeg_size(const uint8_t *data, size_t len, int &w, int &h);
// Dekodiertes Bild fuer LVGL (Zwischenspeicher fuer zwei Seiten) – nur aus dem UI-Task benutzen
const lv_img_dsc_t *bg_get(const std::string &page);
void bg_forget(const std::string &page);
