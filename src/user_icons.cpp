// Eigene Kachel-Symbole – siehe user_icons.h
#include "user_icons.h"
#if WITH_USER_ICONS
#include <Arduino.h>
#include <LittleFS.h>
#include <vector>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "esp_heap_caps.h"
#include "sdcard.h"
#include "fonts/fonts.h"
// lodepng aus LVGL (LV_USE_PNG in lv_conf.h, nur Dekoder); als C eingebunden, weil lodepng.c als C uebersetzt wird
#define LODEPNG_NO_COMPILE_CPP
extern "C" {
#include <src/extra/libs/png/lodepng.h>
}

static const int SIZES[3] = {24, 32, 44};
static const uint32_t CP_BASE = 0xF000;   // Private Use Area, hinter den eingebauten Symbolen (U+E000..)

struct UIcon {
  std::string name, path;
  uint8_t src = 0;            // 0 Flash, 1 SD-Karte
  uint8_t *bmp[3] = {nullptr, nullptr, nullptr};   // A8-Masken 24/32/44 px im PSRAM
  char utf8[4] = {0, 0, 0, 0};
};
static std::vector<UIcon> s_icons;      // nur im UI-Task veraendert (reload), Webserver liest unter s_mx
static SemaphoreHandle_t s_mx = nullptr;

struct Lock {
  Lock() { if (s_mx) xSemaphoreTake(s_mx, portMAX_DELAY); }
  ~Lock() { if (s_mx) xSemaphoreGive(s_mx); }
};

// ---------------------------------------------------------------- Laufzeit-Schrift
static int slot_of(const lv_font_t *f) { return f == &user_icon_font_24 ? 0 : f == &user_icon_font_32 ? 1 : 2; }

static bool glyph_dsc(const lv_font_t *f, lv_font_glyph_dsc_t *d, uint32_t letter, uint32_t) {
  if (letter < CP_BASE || letter >= CP_BASE + s_icons.size()) return false;   // -> fallback (eingebaute Schrift)
  int s = SIZES[slot_of(f)];
  d->adv_w = s;
  d->box_w = s;
  d->box_h = s;
  d->ofs_x = 0;
  d->ofs_y = 0;
  d->bpp = 8;
  d->is_placeholder = 0;
  return true;
}

static const uint8_t *glyph_bmp(const lv_font_t *f, uint32_t letter) {
  size_t i = letter - CP_BASE;
  if (letter < CP_BASE || i >= s_icons.size()) return nullptr;
  return s_icons[i].bmp[slot_of(f)];
}

// Reihenfolge wie lv_font_t: get_glyph_dsc, get_glyph_bitmap, line_height, base_line, subpx, underline_position,
// underline_thickness, dsc, fallback
lv_font_t user_icon_font_24 = {glyph_dsc, glyph_bmp, 24, 0, 0, 0, 0, nullptr, &font_icons_24};
lv_font_t user_icon_font_32 = {glyph_dsc, glyph_bmp, 32, 0, 0, 0, 0, nullptr, &font_icons_32};
lv_font_t user_icon_font_44 = {glyph_dsc, glyph_bmp, 44, 0, 0, 0, 0, nullptr, &font_icons_44};

// ---------------------------------------------------------------- PNG -> Masken
static void free_bmps(UIcon &ic) {
  for (auto &b : ic.bmp) {
    if (b) heap_caps_free(b);
    b = nullptr;
  }
}

// Dekodiert ein PNG (PSRAM, hoechstens 512x512) und rechnet es in die drei Masken um. Deckung = Alphakanal; hat das
// Bild keinen (alles deckend), gilt "dunkel = Symbol" (schwarze Zeichnung auf weissem Grund). Flaechen-Mittelwert je
// Zielpixel, Seitenverhaeltnis bleibt, mittig.
static bool build_masks(const std::string &path, UIcon &ic, std::string *err) {
  size_t len = 0;
  uint8_t *png = fs_read_all(path, USER_ICON_PNG_MAX, len);
  if (!png) {
    if (err) *err = "Datei nicht lesbar oder größer als 300 KB";
    return false;
  }
  unsigned w = 0, h = 0;
  unsigned char *rgba = nullptr;
  unsigned rc = lodepng_decode32(&rgba, &w, &h, png, len);
  heap_caps_free(png);
  if (rc || !rgba) {
    if (err) *err = "Kein gültiges PNG";
    if (rgba) lv_mem_free(rgba);
    return false;
  }
  if (w == 0 || h == 0 || w > 512 || h > 512) {
    lv_mem_free(rgba);
    if (err) *err = "Bild höchstens 512 × 512 Pixel";
    return false;
  }
  bool has_alpha = false;
  for (size_t i = 3; i < (size_t)w * h * 4; i += 4)
    if (rgba[i] != 255) {
      has_alpha = true;
      break;
    }
  for (int k = 0; k < 3; k++) {
    int s = SIZES[k];
    ic.bmp[k] = (uint8_t *)heap_caps_calloc(s * s, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ic.bmp[k]) {
      free_bmps(ic);
      lv_mem_free(rgba);
      if (err) *err = "Kein Speicher";
      return false;
    }
    float sc = std::min((float)s / w, (float)s / h);
    int dw = std::max(1, (int)lroundf(w * sc)), dh = std::max(1, (int)lroundf(h * sc));
    int ox = (s - dw) / 2, oy = (s - dh) / 2;
    for (int ty = 0; ty < dh; ty++) {
      int y0 = ty * h / dh, y1 = std::max(y0 + 1, (int)((ty + 1) * h / dh));
      for (int tx = 0; tx < dw; tx++) {
        int x0 = tx * w / dw, x1 = std::max(x0 + 1, (int)((tx + 1) * w / dw));
        uint32_t sum = 0, n = 0;
        for (int y = y0; y < y1; y++)
          for (int x = x0; x < x1; x++) {
            const unsigned char *p = rgba + ((size_t)y * w + x) * 4;
            sum += has_alpha ? p[3] : 255 - (p[0] * 299 + p[1] * 587 + p[2] * 114) / 1000;
            n++;
          }
        ic.bmp[k][(oy + ty) * s + ox + tx] = (uint8_t)(sum / n);
      }
    }
  }
  lv_mem_free(rgba);
  return true;
}

bool user_icons_valid_name(const char *name) {
  if (!name) return false;
  size_t n = strlen(name);
  if (n < 1 || n > 20) return false;
  for (size_t i = 0; i < n; i++) {
    char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  }
  return true;
}

static bool has_icon(const std::vector<UIcon> &list, const std::string &name) {
  for (auto &ic : list)
    if (ic.name == name) return true;
  return false;
}

static void scan_dir(const char *dir, uint8_t src, std::vector<UIcon> &list) {
  for (auto &fn : fs_list(dir, false)) {
    if (list.size() >= USER_ICONS_MAX) break;
    if (fn.size() < 5) continue;
    std::string ext = fn.substr(fn.size() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".png") continue;
    std::string name = fn.substr(0, fn.size() - 4);
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    if (!user_icons_valid_name(name.c_str()) || has_icon(list, name)) continue;
    UIcon ic;
    ic.name = name;
    ic.path = std::string(dir) + "/" + fn;
    ic.src = src;
    std::string err;
    if (!build_masks(ic.path, ic, &err)) {
      log_w("Symbol %s: %s", ic.path.c_str(), err.c_str());
      continue;
    }
    list.push_back(std::move(ic));
  }
}

void user_icons_reload() {
  std::vector<UIcon> list;
  scan_dir(LFS_MOUNT "/icons", 0, list);
  if (sd_ok()) scan_dir(SD_MOUNT "/icons", 1, list);
  for (size_t i = 0; i < list.size(); i++) {
    uint32_t cp = CP_BASE + i;
    list[i].utf8[0] = (char)(0xE0 | (cp >> 12));
    list[i].utf8[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    list[i].utf8[2] = (char)(0x80 | (cp & 0x3F));
    list[i].utf8[3] = 0;
  }
  std::vector<UIcon> old;
  {
    Lock l;
    old.swap(s_icons);
    s_icons.swap(list);
  }
  for (auto &ic : old) free_bmps(ic);
  log_i("Eigene Symbole: %u", (unsigned)s_icons.size());
}

void user_icons_begin() {
  if (!s_mx) s_mx = xSemaphoreCreateMutex();
  user_icons_reload();
}

const char *user_icons_utf8(const char *name) {
  if (!name) return nullptr;
  for (auto &ic : s_icons)
    if (ic.name == name) return ic.utf8;
  return nullptr;
}

std::string user_icons_json() {
  std::string s = "{\"icons\":[";
  {
    Lock l;
    bool first = true;
    for (auto &ic : s_icons) {
      if (!first) s += ',';
      first = false;
      s += "{\"name\":\"" + ic.name + "\",\"src\":\"" + (ic.src ? "sd" : "flash") + "\"}";
    }
  }
  s += "],\"sd\":" + std::string(sd_ok() ? "true" : "false") + ",\"max\":" + std::to_string(USER_ICONS_MAX) + "}";
  return s;
}

bool user_icons_store(const char *name, const uint8_t *png, size_t len, std::string &err) {
  if (!user_icons_valid_name(name)) {
    err = "Name: 1–20 Zeichen aus a–z, 0–9, - und _";
    return false;
  }
  if (len < 8 || memcmp(png, "\x89PNG", 4) != 0) {
    err = "Datei ist kein PNG";
    return false;
  }
  size_t room = LittleFS.totalBytes() > LittleFS.usedBytes() ? LittleFS.totalBytes() - LittleFS.usedBytes() : 0;
  if (room < len + 64 * 1024) {
    err = "Zu wenig Speicher im Panel";
    return false;
  }
  fs_mkdirs(LFS_MOUNT "/icons");
  if (!fs_write(std::string(LFS_MOUNT "/icons/") + name + ".png", png, len)) {
    err = "Speichern im Flash fehlgeschlagen";
    return false;
  }
  return true;
}

bool user_icons_remove(const char *name) {
  if (!user_icons_valid_name(name)) return false;
  return LittleFS.remove((std::string("/icons/") + name + ".png").c_str());
}

bool user_icons_path(const char *name, std::string &path) {
  if (!name) return false;
  Lock l;
  for (auto &ic : s_icons)
    if (ic.name == name) {
      path = ic.path;
      return true;
    }
  return false;
}
#endif
