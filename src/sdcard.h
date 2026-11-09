// microSD-Karte (SPI, CS haengt am CH32V003) – eingebunden unter /sd als FAT
#pragma once
#include <stdint.h>
#include <string>
#include <vector>

#define SD_MOUNT "/sd"
#define SD_DIR   "/sd/panel"            // alles vom Panel liegt in diesem Ordner
#define LFS_MOUNT "/littlefs"           // LittleFS ueber stdio (Arduino-Standardpfad)

bool sd_begin();                         // nach board_init() aufrufen
bool sd_ok();
const char *sd_status();                 // Klartext fuer Editor
uint64_t sd_total();
uint64_t sd_free();

// Datei-Helfer ueber stdio (funktionieren fuer /sd/... und /littlefs/...)
bool fs_exists(const std::string &path);
long fs_size(const std::string &path);    // -1 = fehlt
bool fs_copy(const std::string &src, const std::string &dst);
bool fs_write(const std::string &path, const void *data, size_t len);   // ueber .tmp, dann umbenennen
uint8_t *fs_read_all(const std::string &path, size_t max, size_t &len); // PSRAM, mit heap_caps_free freigeben
bool fs_mkdirs(const std::string &dir);   // legt fehlende Ordner an
bool fs_rmtree(const std::string &path);  // Datei oder Ordner rekursiv loeschen
std::vector<std::string> fs_list(const std::string &dir, bool dirs);   // Namen ohne Pfad
