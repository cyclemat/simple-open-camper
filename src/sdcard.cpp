#include "sdcard.h"
#include "app_config.h"
#include "board.h"

#include <Arduino.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_heap_caps.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#if BOARD_P4
#include "driver/sdmmc_host.h"
#include "board_p4.h"
#endif

static sdmmc_card_t *s_card = nullptr;
static bool s_ok = false;
static const char *s_status = "nicht geprüft";

static bool sd_mounted();

#if BOARD_P4
// ESP32-P4: Karte direkt am SDMMC (Slot 0, 1 Bit – D1..D3 sind auf dem Board anders belegt). Versorgung ueber LDO4,
// den board_init() schon eingeschaltet hat.
bool sd_begin() {
  const P4BoardDef &b = p4_board();
  if (b.sd_clk < 0) {
    s_status = "kein Kartenleser";
    return false;
  }
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.slot = SDMMC_HOST_SLOT_0;
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
  sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
  slot.width = 1;
  slot.clk = (gpio_num_t)b.sd_clk;
  slot.cmd = (gpio_num_t)b.sd_cmd;
  slot.d0 = (gpio_num_t)b.sd_d0;
  slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
  esp_vfs_fat_mount_config_t mc = {};
  mc.format_if_mount_failed = false;   // fremde Karten nie ungefragt formatieren
  mc.max_files = 6;
  mc.allocation_unit_size = 16 * 1024;
  esp_err_t e = esp_vfs_fat_sdmmc_mount(SD_MOUNT, &host, &slot, &mc, &s_card);
  if (e != ESP_OK) {
    s_status = e == ESP_FAIL ? "Karte nicht lesbar (FAT32 oder exFAT nötig)" : "keine Karte eingesteckt";
    log_w("SD: %s (%s)", s_status, esp_err_to_name(e));
    return false;
  }
  return sd_mounted();
}
#else
bool sd_begin() {
  spi_bus_config_t bus = {};
  bus.mosi_io_num = PIN_SD_MOSI;
  bus.miso_io_num = PIN_SD_MISO;
  bus.sclk_io_num = PIN_SD_SCK;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = 16 * 1024;
  if (spi_bus_initialize(SD_SPI_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
    s_status = "SPI-Bus nicht verfügbar";
    log_e("SD: %s", s_status);
    return false;
  }

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SD_SPI_HOST;
  host.max_freq_khz = SD_FREQ_KHZ;
  sdspi_device_config_t dev = SDSPI_DEVICE_CONFIG_DEFAULT();
  dev.host_id = SD_SPI_HOST;
  dev.gpio_cs = SDSPI_SLOT_NO_CS;   // CS liegt am IO-Expander und bleibt dauerhaft aktiv
  esp_vfs_fat_mount_config_t mc = {};
  mc.format_if_mount_failed = false;   // fremde Karten nie ungefragt formatieren
  mc.max_files = 6;
  mc.allocation_unit_size = 16 * 1024;

  board_sd_select(true);
  esp_err_t e = ESP_FAIL;
  for (int i = 0; i < 2 && e != ESP_OK; i++) {
    if (i) delay(50);
    e = esp_vfs_fat_sdspi_mount(SD_MOUNT, &host, &dev, &mc, &s_card);
  }
  if (e != ESP_OK) {
    s_status = e == ESP_FAIL ? "Karte nicht lesbar (FAT32 oder exFAT nötig)" : "keine Karte eingesteckt";
    log_w("SD: %s (%s)", s_status, esp_err_to_name(e));
    board_sd_select(false);
    spi_bus_free(SD_SPI_HOST);
    return false;
  }
  return sd_mounted();
}
#endif

static bool sd_mounted() {
  s_ok = true;
  s_status = "bereit";
  fs_mkdirs(SD_DIR "/bg");
  fs_mkdirs(SD_DIR "/backup");
  log_i("SD: %s, %llu MB", s_card->cid.name, (unsigned long long)(sd_total() >> 20));
  return true;
}

bool sd_ok() { return s_ok; }
const char *sd_status() { return s_status; }

uint64_t sd_total() {
  uint64_t t = 0, f = 0;
  if (s_ok) esp_vfs_fat_info(SD_MOUNT, &t, &f);
  return t;
}
uint64_t sd_free() {
  uint64_t t = 0, f = 0;
  if (s_ok) esp_vfs_fat_info(SD_MOUNT, &t, &f);
  return f;
}

// ---------------------------------------------------------------- Datei-Helfer
bool fs_exists(const std::string &path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0;
}

long fs_size(const std::string &path) {
  struct stat st;
  if (stat(path.c_str(), &st) != 0) return -1;
  return (long)st.st_size;
}

bool fs_copy(const std::string &src, const std::string &dst) {
  FILE *in = fopen(src.c_str(), "rb");
  if (!in) return false;
  std::string tmp = dst + ".tmp";
  FILE *out = fopen(tmp.c_str(), "wb");
  if (!out) {
    fclose(in);
    return false;
  }
  char *buf = (char *)malloc(8192);
  bool ok = buf != nullptr;
  size_t n;
  while (ok && (n = fread(buf, 1, 8192, in)) > 0) ok = fwrite(buf, 1, n, out) == n;
  if (ok && ferror(in)) ok = false;
  free(buf);
  fclose(in);
  if (fclose(out) != 0) ok = false;
  if (!ok) {
    unlink(tmp.c_str());
    return false;
  }
  unlink(dst.c_str());
  return rename(tmp.c_str(), dst.c_str()) == 0;
}

bool fs_write(const std::string &path, const void *data, size_t len) {
  std::string tmp = path + ".tmp";
  FILE *f = fopen(tmp.c_str(), "wb");
  if (!f) return false;
  bool ok = fwrite(data, 1, len, f) == len;
  if (fclose(f) != 0) ok = false;
  if (!ok) {
    unlink(tmp.c_str());
    return false;
  }
  unlink(path.c_str());
  return rename(tmp.c_str(), path.c_str()) == 0;
}

uint8_t *fs_read_all(const std::string &path, size_t max, size_t &len) {
  len = 0;
  long n = fs_size(path);
  if (n <= 0 || (size_t)n > max) return nullptr;
  FILE *f = fopen(path.c_str(), "rb");
  if (!f) return nullptr;
  uint8_t *buf = (uint8_t *)heap_caps_malloc(n + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (buf && fread(buf, 1, n, f) == (size_t)n) {
    buf[n] = 0;
    len = n;
  } else if (buf) {
    heap_caps_free(buf);
    buf = nullptr;
  }
  fclose(f);
  return buf;
}

bool fs_mkdirs(const std::string &dir) {
  for (size_t i = 1; i <= dir.size(); i++) {
    if (i == dir.size() || dir[i] == '/') {
      std::string part = dir.substr(0, i);
      if (!fs_exists(part) && mkdir(part.c_str(), 0777) != 0) return false;
    }
  }
  return true;
}

std::vector<std::string> fs_list(const std::string &dir, bool dirs) {
  std::vector<std::string> out;
  DIR *d = opendir(dir.c_str());
  if (!d) return out;
  struct dirent *e;
  while ((e = readdir(d)) != nullptr) {
    std::string n = e->d_name;
    if (n == "." || n == "..") continue;
    bool is_dir = e->d_type == DT_DIR;
    if (is_dir == dirs) out.push_back(n);
  }
  closedir(d);
  return out;
}

bool fs_rmtree(const std::string &path) {
  struct stat st;
  if (stat(path.c_str(), &st) != 0) return true;
  if (!S_ISDIR(st.st_mode)) return unlink(path.c_str()) == 0;
  for (auto &n : fs_list(path, true)) fs_rmtree(path + "/" + n);
  for (auto &n : fs_list(path, false)) unlink((path + "/" + n).c_str());
  return rmdir(path.c_str()) == 0;
}
