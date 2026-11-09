#include "backup.h"
#include "app_config.h"
#include "layout.h"
#include "net.h"
#include "settings.h"
#include "sdcard.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <atomic>
#include <time.h>
#include <unistd.h>
#include "esp_heap_caps.h"
#include "nvs.h"

static const char *BK_DIR = SD_DIR "/backup";
// Alles, was das Panel per Preferences ablegt (settings.cpp, truma.cpp, victron.cpp, mopeka.cpp, shelly.cpp, cerbo.cpp)
static const char *NVS_SPACES[] = {"panel", "truma", "victron", "mopeka", "shelly", "cerbo"};
// Dateien im Flash, die zur Sicherung gehoeren
static const char *LFS_FILES[] = {"layout.json", "saver_logo.bin", "saver_logo.png"};

static std::atomic<bool> s_busy{false};
struct Busy {
  bool ok;
  Busy() { ok = !s_busy.exchange(true); }
  ~Busy() {
    if (ok) s_busy = false;
  }
};

bool backup_valid_name(const std::string &n) {
  if (n.empty() || n.size() > 40 || n[0] == '.') return false;
  for (char c : n)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  return true;
}

// ---------------------------------------------------------------- NVS <-> JSON
static void hex_encode(const uint8_t *d, size_t n, std::string &out) {
  static const char *H = "0123456789abcdef";
  out.reserve(n * 2);
  for (size_t i = 0; i < n; i++) {
    out += H[d[i] >> 4];
    out += H[d[i] & 15];
  }
}

static bool hex_decode(const char *s, std::vector<uint8_t> &out) {
  size_t n = strlen(s);
  if (n % 2) return false;
  auto v = [](char c) -> int { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
  out.resize(n / 2);
  for (size_t i = 0; i < n / 2; i++) {
    int a = v(s[2 * i]), b = v(s[2 * i + 1]);
    if (a < 0 || b < 0) return false;
    out[i] = (uint8_t)(a << 4 | b);
  }
  return true;
}

static void nvs_dump(const char *ns, JsonObject out) {
  nvs_handle_t h;
  if (nvs_open(ns, NVS_READONLY, &h) != ESP_OK) return;   // Namensraum gibt es (noch) nicht
  nvs_iterator_t it = nullptr;
  esp_err_t r = nvs_entry_find(NVS_DEFAULT_PART_NAME, ns, NVS_TYPE_ANY, &it);
  while (r == ESP_OK) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    JsonObject e = out[info.key].to<JsonObject>();
    const char *k = info.key;
    switch (info.type) {
      case NVS_TYPE_U8: { uint8_t v; nvs_get_u8(h, k, &v); e["t"] = "u8"; e["v"] = v; break; }
      case NVS_TYPE_I8: { int8_t v; nvs_get_i8(h, k, &v); e["t"] = "i8"; e["v"] = v; break; }
      case NVS_TYPE_U16: { uint16_t v; nvs_get_u16(h, k, &v); e["t"] = "u16"; e["v"] = v; break; }
      case NVS_TYPE_I16: { int16_t v; nvs_get_i16(h, k, &v); e["t"] = "i16"; e["v"] = v; break; }
      case NVS_TYPE_U32: { uint32_t v; nvs_get_u32(h, k, &v); e["t"] = "u32"; e["v"] = v; break; }
      case NVS_TYPE_I32: { int32_t v; nvs_get_i32(h, k, &v); e["t"] = "i32"; e["v"] = v; break; }
      case NVS_TYPE_U64: { uint64_t v; nvs_get_u64(h, k, &v); e["t"] = "u64"; e["v"] = v; break; }
      case NVS_TYPE_I64: { int64_t v; nvs_get_i64(h, k, &v); e["t"] = "i64"; e["v"] = v; break; }
      case NVS_TYPE_STR: {
        size_t n = 0;
        nvs_get_str(h, k, nullptr, &n);
        std::string s(n, '\0');
        if (n) nvs_get_str(h, k, &s[0], &n);
        if (!s.empty() && s.back() == '\0') s.pop_back();
        e["t"] = "str";
        e["v"] = s;
        break;
      }
      case NVS_TYPE_BLOB: {
        size_t n = 0;
        nvs_get_blob(h, k, nullptr, &n);
        std::vector<uint8_t> b(n);
        if (n) nvs_get_blob(h, k, b.data(), &n);
        std::string hex;
        hex_encode(b.data(), n, hex);
        e["t"] = "blob";
        e["v"] = hex;
        break;
      }
      default: out.remove(info.key); break;
    }
    r = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  nvs_close(h);
}

static bool nvs_load(const char *ns, JsonObject in) {
  nvs_handle_t h;
  if (nvs_open(ns, NVS_READWRITE, &h) != ESP_OK) return false;
  nvs_erase_all(h);
  for (JsonPair kv : in) {
    const char *k = kv.key().c_str();
    JsonObject e = kv.value().as<JsonObject>();
    std::string t = e["t"] | "";
    JsonVariant v = e["v"];
    if (t == "u8") nvs_set_u8(h, k, v.as<uint8_t>());
    else if (t == "i8") nvs_set_i8(h, k, v.as<int8_t>());
    else if (t == "u16") nvs_set_u16(h, k, v.as<uint16_t>());
    else if (t == "i16") nvs_set_i16(h, k, v.as<int16_t>());
    else if (t == "u32") nvs_set_u32(h, k, v.as<uint32_t>());
    else if (t == "i32") nvs_set_i32(h, k, v.as<int32_t>());
    else if (t == "u64") nvs_set_u64(h, k, v.as<uint64_t>());
    else if (t == "i64") nvs_set_i64(h, k, v.as<int64_t>());
    else if (t == "str") nvs_set_str(h, k, v | "");
    else if (t == "blob") {
      std::vector<uint8_t> b;
      if (hex_decode(v | "", b)) nvs_set_blob(h, k, b.data(), b.size());
    }
  }
  bool ok = nvs_commit(h) == ESP_OK;
  nvs_close(h);
  return ok;
}

// ---------------------------------------------------------------- Liste
static std::string title_of(const std::string &n) {
  int y, mo, d, h, mi;
  if (sscanf(n.c_str(), "%4d-%2d-%2d_%2d%2d", &y, &mo, &d, &h, &mi) != 5) return n;
  char b[48];
  snprintf(b, sizeof(b), "%02d.%02d.%04d, %02d:%02d Uhr", d, mo, y, h, mi);
  std::string t = b;
  size_t dash = n.find('-', 16);
  if (dash != std::string::npos) t += " (" + n.substr(dash + 1) + ")";
  return t;
}

std::vector<BackupInfo> backup_list() {
  std::vector<BackupInfo> out;
  if (!sd_ok()) return out;
  std::vector<std::string> names = fs_list(BK_DIR, true);
  std::sort(names.begin(), names.end(), std::greater<std::string>());
  for (auto &n : names) {
    if (!backup_valid_name(n)) continue;
    std::string dir = std::string(BK_DIR) + "/" + n;
    BackupInfo b;
    b.name = n;
    b.title = title_of(n);
    size_t len;
    uint8_t *buf = fs_read_all(dir + "/info.json", 4096, len);
    if (buf) {
      JsonDocument in;
      if (!deserializeJson(in, (const char *)buf, len)) b.fw = in["fw"] | "";
      heap_caps_free(buf);
    }
    b.bg = (int)fs_list(dir + "/bg", false).size();
    b.complete = fs_exists(dir + "/nvs.json") && fs_exists(dir + "/layout.json");
    out.push_back(b);
  }
  return out;
}

std::string backup_list_json() {
  JsonDocument d;
  d["sd"]["ok"] = sd_ok();
  d["sd"]["status"] = sd_status();
  d["sd"]["total"] = (double)sd_total();
  d["sd"]["free"] = (double)sd_free();
  JsonArray arr = d["backups"].to<JsonArray>();
  for (auto &b : backup_list()) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = b.name;
    o["title"] = b.title;
    o["fw"] = b.fw;
    o["bg"] = b.bg;
    o["complete"] = b.complete;
  }
  std::string s;
  serializeJson(d, s);
  return s;
}

// ---------------------------------------------------------------- Erstellen
bool backup_create(std::string &name, std::string &msg) {
  Busy busy;
  if (!busy.ok) return msg = "Es läuft bereits eine Sicherung", false;
  if (!sd_ok()) return msg = "Keine SD-Karte im Panel", false;
  if (sd_free() < 8ull * 1024 * 1024) return msg = "Zu wenig Platz auf der SD-Karte", false;

  char base[32];
  if (net_time_valid()) {
    time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(base, sizeof(base), "%Y-%m-%d_%H%M", &tm);
  } else {
    snprintf(base, sizeof(base), "ohne-uhrzeit");
  }
  name = base;
  for (int i = 2; fs_exists(std::string(BK_DIR) + "/" + name); i++) name = std::string(base) + "-" + std::to_string(i);
  std::string dir = std::string(BK_DIR) + "/" + name;
  if (!fs_mkdirs(dir + "/bg")) return msg = "Ordner auf der SD-Karte konnte nicht angelegt werden", false;

  bool ok = true;
  for (const char *f : LFS_FILES) {
    std::string src = std::string(LFS_MOUNT "/") + f;
    if (fs_exists(src)) ok &= fs_copy(src, dir + "/" + f);
  }

  JsonDocument nv;
  for (const char *ns : NVS_SPACES) nvs_dump(ns, nv[ns].to<JsonObject>());
  std::string s;
  serializeJsonPretty(nv, s);
  ok &= fs_write(dir + "/nvs.json", s.data(), s.size());

  Layout l = layout_get();
  int bg = 0;
  for (auto &p : l.pages) {
    std::string src = bg_jpg_path(p.id);
    if (src.empty()) continue;
    ok &= fs_copy(src, dir + "/bg/" + p.id + ".jpg");
    bg++;
  }

  JsonDocument info;
  info["fw"] = FW_VERSION;
  info["created"] = net_time_valid() ? name.substr(0, 16) : "";
  info["hostname"] = settings_copy().hostname.c_str();
  info["pages"] = (int)l.pages.size();
  info["bg"] = bg;
  s.clear();
  serializeJsonPretty(info, s);
  ok &= fs_write(dir + "/info.json", s.data(), s.size());

  if (!ok) {
    fs_rmtree(dir);
    return msg = "Schreiben auf die SD-Karte fehlgeschlagen", false;
  }
  log_i("Sicherung %s angelegt (%d Hintergründe)", name.c_str(), bg);
  return true;
}

// ---------------------------------------------------------------- Wiederherstellen
bool backup_restore(const std::string &name, std::string &msg) {
  Busy busy;
  if (!busy.ok) return msg = "Es läuft bereits eine Sicherung", false;
  if (!sd_ok()) return msg = "Keine SD-Karte im Panel", false;
  if (!backup_valid_name(name)) return msg = "Ungültiger Name", false;
  std::string dir = std::string(BK_DIR) + "/" + name;
  if (!fs_exists(dir + "/nvs.json") || !fs_exists(dir + "/layout.json")) return msg = "Sicherung ist unvollständig", false;

  // Erst alles pruefen, dann schreiben
  size_t len;
  uint8_t *buf = fs_read_all(dir + "/nvs.json", 256 * 1024, len);
  if (!buf) return msg = "nvs.json nicht lesbar", false;
  JsonDocument nv;
  DeserializationError e = deserializeJson(nv, (const char *)buf, len);
  heap_caps_free(buf);
  if (e) return msg = "nvs.json ist beschädigt", false;

  bool ok = true;
  for (const char *f : LFS_FILES) {
    std::string src = dir + "/" + f, dst = std::string(LFS_MOUNT "/") + f;
    if (fs_exists(src)) ok &= fs_copy(src, dst);
    else unlink(dst.c_str());
  }

  // Hintergruende: alte entfernen, die aus der Sicherung einspielen
  Layout cur = layout_get();
  for (auto &p : cur.pages) bg_remove(p.id);
  for (auto &n : fs_list(SD_DIR "/bg", false)) unlink((std::string(SD_DIR "/bg/") + n).c_str());
  for (auto &n : fs_list(dir + "/bg", false)) {
    if (n.size() < 5 || n.compare(n.size() - 4, 4, ".jpg") != 0) continue;
    std::string id = n.substr(0, n.size() - 4);
    ok &= fs_copy(dir + "/bg/" + n, bg_store_path(id));
  }

  for (const char *ns : NVS_SPACES) {
    JsonObject o = nv[ns];
    if (!o.isNull()) ok &= nvs_load(ns, o);
  }
  if (!ok) msg = "Nicht alles ließ sich zurückschreiben";
  log_i("Sicherung %s eingespielt%s", name.c_str(), ok ? "" : " (mit Fehlern)");
  return ok;
}

bool backup_delete(const std::string &name, std::string &msg) {
  Busy busy;
  if (!busy.ok) return msg = "Es läuft bereits eine Sicherung", false;
  if (!sd_ok()) return msg = "Keine SD-Karte im Panel", false;
  if (!backup_valid_name(name)) return msg = "Ungültiger Name", false;
  if (!fs_rmtree(std::string(BK_DIR) + "/" + name)) return msg = "Löschen fehlgeschlagen", false;
  return true;
}
