#include "layout.h"
#include "app_config.h"
#include <Arduino.h>
#include <atomic>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <unistd.h>
#include "esp_jpeg_dec.h"
#include "sdcard.h"
#include <algorithm>
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_mtx;
static Layout s_layout;
static std::atomic<uint32_t> s_ver{1};
static const char *PATH = "/layout.json";

namespace {   // dateilokal: gleichnamige Typen anderer Dateien duerfen sich nicht vermischen
struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};
}  // namespace

static std::string sanitize_id(const char *s) {
  std::string out;
  for (const char *p = s; p && *p && out.size() < 16; p++) {
    char c = *p;
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_') out += c;
    else if (c >= 'A' && c <= 'Z') out += (char)(c + 32);
  }
  return out;
}

// Layout-JSON im PSRAM aufbauen (viele kleine Stuecke, sonst im knappen internen RAM)
namespace {
struct LayoutPsramAlloc : ArduinoJson::Allocator {
  void *allocate(size_t n) override {
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : malloc(n);
  }
  void deallocate(void *p) override { heap_caps_free(p); }
  void *reallocate(void *p, size_t n) override {
    void *q = heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return q ? q : realloc(p, n);
  }
};
}  // namespace
static LayoutPsramAlloc s_ps;

static bool parse(const char *json, size_t len, Layout &out, std::string &msg) {
  JsonDocument doc(&s_ps);
  DeserializationError e = deserializeJson(doc, json, len);
  if (e) {
    msg = std::string("JSON-Fehler: ") + e.c_str();
    return false;
  }
  out = Layout();
  out.rev = doc["rev"] | 0;
  out.grid = constrain((int)(doc["grid"] | 8), 1, 64);
  for (JsonPair kv : doc["names"].as<JsonObject>()) {
    const char *v = kv.value() | "";
    if (*v) out.names[kv.key().c_str()] = std::string(v).substr(0, 48);
  }
  for (JsonObject p : doc["pages"].as<JsonArray>()) {
    if (out.pages.size() >= MAX_PAGES) break;
    Page pg;
    pg.id = sanitize_id(p["id"] | "");
    if (pg.id.empty()) pg.id = "p" + std::to_string(out.pages.size() + 1);
    pg.name = std::string(p["name"] | "Seite").substr(0, 32);
    pg.bg = p["bg"] | false;
    for (JsonObject t : p["tiles"].as<JsonArray>()) {
      if (pg.tiles.size() >= MAX_TILES_PER_PAGE) break;
      Tile tl;
      tl.key = t["key"] | "";
      if (tl.key.empty()) continue;
      tl.w = constrain((int)(t["w"] | 184), 72, LCD_H_RES);
      tl.h = constrain((int)(t["h"] | 116), 56, UI_CONTENT_H);
      tl.x = constrain((int)(t["x"] | 0), 0, LCD_H_RES - tl.w);
      tl.y = constrain((int)(t["y"] | 0), 0, UI_CONTENT_H - tl.h);
      tl.icon = std::string(t["icon"] | "").substr(0, 16);
      pg.tiles.push_back(tl);
    }
    out.pages.push_back(pg);
  }
  if (out.pages.empty()) {
    Page pg;
    pg.id = "p1";
    pg.name = "Start";
    out.pages.push_back(pg);
  }
  return true;
}

static std::string serialize(const Layout &l) {
  JsonDocument doc(&s_ps);
  doc["version"] = 1;
  doc["rev"] = l.rev;
  doc["grid"] = l.grid;
  JsonObject names = doc["names"].to<JsonObject>();
  for (auto &kv : l.names) names[kv.first] = kv.second;
  JsonArray pages = doc["pages"].to<JsonArray>();
  for (auto &p : l.pages) {
    JsonObject po = pages.add<JsonObject>();
    po["id"] = p.id;
    po["name"] = p.name;
    po["bg"] = p.bg;
    JsonArray ta = po["tiles"].to<JsonArray>();
    for (auto &t : p.tiles) {
      JsonObject to = ta.add<JsonObject>();
      to["key"] = t.key;
      to["x"] = t.x;
      to["y"] = t.y;
      to["w"] = t.w;
      to["h"] = t.h;
      if (!t.icon.empty()) to["icon"] = t.icon;
    }
  }
  std::string s;
  serializeJson(doc, s);
  return s;
}

static bool write_file(const std::string &s) {
  File f = LittleFS.open("/layout.tmp", "w");
  if (!f) return false;
  size_t w = f.write((const uint8_t *)s.data(), s.size());
  f.close();
  if (w != s.size()) return false;
  LittleFS.remove(PATH);
  return LittleFS.rename("/layout.tmp", PATH);
}

void layout_init() {
  s_mtx = xSemaphoreCreateRecursiveMutex();
  std::string msg;
  File f = LittleFS.open(PATH, "r");
  if (f) {
    size_t n = f.size();
    char *buf = (char *)heap_caps_malloc(n + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf) {
      f.readBytes(buf, n);
      buf[n] = 0;
      Layout l;
      if (parse(buf, n, l, msg)) {
        Lock lk;
        s_layout = l;
      } else {
        log_e("layout.json: %s", msg.c_str());
      }
      heap_caps_free(buf);
    }
    f.close();
  }
  Lock lk;
  if (s_layout.pages.empty()) {
    Page pg;
    pg.id = "p1";
    pg.name = "Start";
    s_layout.pages.push_back(pg);
  }
  // Hintergrund-Flag mit Dateisystem abgleichen
  for (auto &p : s_layout.pages) p.bg = bg_exists(p.id);
}

Layout layout_get() {
  Lock lk;
  return s_layout;
}
uint32_t layout_version() { return s_ver; }
std::string layout_json() {
  Lock lk;
  return serialize(s_layout);
}

int layout_store_json(const char *json, size_t len, bool force, std::string &msg) {
  Layout l;
  if (!parse(json, len, l, msg)) return 400;
  Lock lk;
  if (!force && l.rev != s_layout.rev) {
    msg = "Das Layout wurde inzwischen am Panel geändert.";
    return 409;
  }
  l.rev = s_layout.rev + 1;
  for (auto &p : l.pages) p.bg = bg_exists(p.id);
  // Hintergruende geloeschter Seiten entfernen
  for (auto &old : s_layout.pages) {
    bool keep = false;
    for (auto &p : l.pages) keep |= p.id == old.id;
    if (!keep) {
      bg_remove(old.id);
    }
  }
  if (!write_file(serialize(l))) {
    msg = "Speichern im Flash fehlgeschlagen";
    return 500;
  }
  s_layout = l;
  s_ver++;
  return 200;
}

void layout_refresh_bg() {
  Lock lk;
  for (auto &p : s_layout.pages) p.bg = bg_exists(p.id);
  s_ver++;
}

void layout_set_name(const std::string &key, const std::string &label) {
  Lock lk;
  if (label.empty()) s_layout.names.erase(key);
  else s_layout.names[key] = label.substr(0, 48);
  s_layout.rev++;
  write_file(serialize(s_layout));
  s_ver++;
}

std::string layout_label(const Layout &l, const ControlView &v) {
  auto it = l.names.find(v.key);
  if (it != l.names.end() && !it->second.empty()) return it->second;
  std::string n = v.name;
  if (!v.ep.empty()) n += " " + v.ep;
  return n;
}

void layout_auto_arrange(const std::vector<ControlView> &all, bool only_missing) {
  const int W = 184, H = 116, GX = 16, GY = 16, MX = 20, MY = 16, COLS = 5, ROWS = 4;
  Lock lk;
  Layout l = s_layout;
  std::vector<std::string> placed;
  if (only_missing)
    for (auto &p : l.pages)
      for (auto &t : p.tiles) placed.push_back(t.key);
  else
    for (auto &p : l.pages) p.tiles.clear();
  size_t pi = 0;
  bool full = false;
  for (auto &v : all) {
    if (full) break;
    if (std::find(placed.begin(), placed.end(), v.key) != placed.end()) continue;
    // freien Platz suchen
    bool done = false;
    while (!done && !full) {
      if (pi >= l.pages.size()) {
        if (l.pages.size() >= MAX_PAGES) {
          full = true;
          break;
        }
        Page pg;
        pg.id = "p" + std::to_string(l.pages.size() + 1);
        pg.name = "Seite " + std::to_string(l.pages.size() + 1);
        l.pages.push_back(pg);
      }
      Page &pg = l.pages[pi];
      for (int r = 0; r < ROWS && !done; r++)
        for (int c = 0; c < COLS && !done; c++) {
          int x = MX + c * (W + GX), y = MY + r * (H + GY);
          bool free = true;
          for (auto &t : pg.tiles)
            if (x < t.x + t.w && x + W > t.x && y < t.y + t.h && y + H > t.y) free = false;
          if (free) {
            Tile t;
            t.key = v.key;
            t.x = x;
            t.y = y;
            t.w = W;
            t.h = H;
            pg.tiles.push_back(t);
            done = true;
          }
        }
      if (!done) pi++;
    }
  }
  l.rev = s_layout.rev + 1;
  write_file(serialize(l));
  s_layout = l;
  s_ver++;
}

// ---------------------------------------------------------------- Hintergruende
// Jede Seite kann ein JPEG (1024x600) haben. Liegt eine SD-Karte im Panel, wird dort gespeichert
// (SD_DIR/bg/<id>.jpg), sonst im Flash (/littlefs/bg_<id>.jpg). Aeltere Firmware legte zusaetzlich
// ein RGB565-Rohbild /bg_<id>.bin ab; das wird noch gelesen, aber nicht mehr geschrieben.
static std::string sd_bg(const std::string &page) { return std::string(SD_DIR "/bg/") + page + ".jpg"; }
static std::string lfs_bg(const std::string &page, const char *ext) { return std::string(LFS_MOUNT "/bg_") + page + "." + ext; }

std::string bg_store_path(const std::string &page) { return sd_ok() ? sd_bg(page) : lfs_bg(page, "jpg"); }

std::string bg_jpg_path(const std::string &page) {
  if (sd_ok() && fs_exists(sd_bg(page))) return sd_bg(page);
  if (fs_exists(lfs_bg(page, "jpg"))) return lfs_bg(page, "jpg");
  return "";
}

bool bg_exists(const std::string &page) {
  return !bg_jpg_path(page).empty() || fs_size(lfs_bg(page, "bin")) == BG_BYTES;
}

void bg_remove(const std::string &page) {
  if (sd_ok()) unlink(sd_bg(page).c_str());
  unlink(lfs_bg(page, "jpg").c_str());
  unlink(lfs_bg(page, "bin").c_str());
}

void bg_migrate() {
  // Rohbilder neben einem JPEG sind ueberfluessig (JPEG ist das gleiche Bild in voller Groesse)
  for (auto &n : fs_list(LFS_MOUNT, false)) {
    if (n.rfind("bg_", 0) != 0 || n.size() < 8 || n.compare(n.size() - 4, 4, ".jpg") != 0) continue;
    std::string id = n.substr(3, n.size() - 7);
    unlink(lfs_bg(id, "bin").c_str());
    // mit SD-Karte: Bild auf die Karte verschieben und Flash freigeben
    if (sd_ok() && (fs_exists(sd_bg(id)) || fs_copy(lfs_bg(id, "jpg"), sd_bg(id)))) {
      unlink(lfs_bg(id, "jpg").c_str());
      log_i("Hintergrund %s auf SD-Karte verschoben", id.c_str());
    }
  }
  layout_refresh_bg();
}

bool bg_jpeg_size(const uint8_t *data, size_t len, int &w, int &h) {
  w = h = 0;
  jpeg_dec_config_t cfg = DEFAULT_JPEG_DEC_CONFIG();
  cfg.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
  jpeg_dec_handle_t dec = nullptr;
  if (jpeg_dec_open(&cfg, &dec) != JPEG_ERR_OK) return false;
  jpeg_dec_io_t io = {};
  io.inbuf = (uint8_t *)data;
  io.inbuf_len = (int)len;
  jpeg_dec_header_info_t info = {};
  bool ok = jpeg_dec_parse_header(dec, &io, &info) == JPEG_ERR_OK;
  jpeg_dec_close(dec);
  if (ok) {
    w = info.width;
    h = info.height;
  }
  return ok;
}

// JPEG-Datei in ein RGB565-Bild (PSRAM, 16 Byte ausgerichtet) dekodieren
static uint8_t *decode_jpg(const std::string &path) {
  size_t len;
  uint8_t *in = fs_read_all(path, BG_JPG_MAX, len);
  if (!in) return nullptr;
  uint32_t t0 = millis();
  uint8_t *out = nullptr;
  jpeg_dec_config_t cfg = DEFAULT_JPEG_DEC_CONFIG();
  cfg.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
  jpeg_dec_handle_t dec = nullptr;
  if (jpeg_dec_open(&cfg, &dec) == JPEG_ERR_OK) {
    jpeg_dec_io_t io = {};
    io.inbuf = in;
    io.inbuf_len = (int)len;
    jpeg_dec_header_info_t info = {};
    int need = 0;
    if (jpeg_dec_parse_header(dec, &io, &info) == JPEG_ERR_OK && info.width == LCD_H_RES && info.height == LCD_V_RES &&
        jpeg_dec_get_outbuf_len(dec, &need) == JPEG_ERR_OK && need == BG_BYTES) {
      out = (uint8_t *)heap_caps_aligned_alloc(16, BG_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (out) {
        io.outbuf = out;
        if (jpeg_dec_process(dec, &io) != JPEG_ERR_OK) {
          heap_caps_free(out);
          out = nullptr;
        }
      }
    } else {
      log_w("Hintergrund %s: kein JPEG mit %dx%d", path.c_str(), LCD_H_RES, LCD_V_RES);
    }
    jpeg_dec_close(dec);
  }
  heap_caps_free(in);
  if (out) log_i("Hintergrund %s in %u ms dekodiert", path.c_str(), (unsigned)(millis() - t0));
  return out;
}

static uint8_t *load_bin(const std::string &page) {
  std::string path = lfs_bg(page, "bin");
  if (fs_size(path) != BG_BYTES) return nullptr;
  FILE *f = fopen(path.c_str(), "rb");
  if (!f) return nullptr;
  uint8_t *out = (uint8_t *)heap_caps_aligned_alloc(16, BG_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (out && fread(out, 1, BG_BYTES, f) != BG_BYTES) {
    heap_caps_free(out);
    out = nullptr;
  }
  fclose(f);
  return out;
}

struct BgCache {
  std::string page;
  lv_img_dsc_t dsc;
  uint8_t *data = nullptr;
  uint32_t used = 0;
};
static BgCache s_bg[2];

void bg_forget(const std::string &page) {
  for (auto &b : s_bg)
    if (b.data && (page.empty() || b.page == page)) {
      heap_caps_free(b.data);
      b.data = nullptr;
      b.page.clear();
    }
}

const lv_img_dsc_t *bg_get(const std::string &page) {
  for (auto &b : s_bg)
    if (b.data && b.page == page) {
      b.used = millis();
      return &b.dsc;
    }
  BgCache *slot = &s_bg[0];
  if (s_bg[0].data && (!s_bg[1].data || s_bg[1].used < s_bg[0].used)) slot = &s_bg[1];
  if (slot->data) {   // erst freigeben, damit zwei Bilder plus Dekoder sicher ins PSRAM passen
    heap_caps_free(slot->data);
    slot->data = nullptr;
    slot->page.clear();
  }
  std::string jpg = bg_jpg_path(page);
  uint8_t *data = jpg.empty() ? load_bin(page) : decode_jpg(jpg);
  if (!data) return nullptr;
  slot->data = data;
  slot->page = page;
  slot->used = millis();
  memset(&slot->dsc, 0, sizeof(slot->dsc));
  slot->dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  slot->dsc.header.always_zero = 0;
  slot->dsc.header.w = LCD_H_RES;
  slot->dsc.header.h = LCD_V_RES;
  slot->dsc.data_size = BG_BYTES;
  slot->dsc.data = slot->data;
  return &slot->dsc;
}
