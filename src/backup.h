// Sicherung auf der SD-Karte: SD_DIR/backup/<name>/ mit Layout, Einstellungen (NVS), Logo und Hintergruenden
#pragma once
#include <string>
#include <vector>

struct BackupInfo {
  std::string name;      // Ordnername, z. B. 2026-11-08_1432
  std::string title;     // lesbar, z. B. "08.11.2026, 14:32 Uhr"
  std::string fw;
  int bg = 0;            // Anzahl Hintergruende
  bool complete = false;
};
std::vector<BackupInfo> backup_list();                 // neueste zuerst

std::string backup_list_json();                        // {"sd":{...},"backups":[...]}
bool backup_create(std::string &name, std::string &msg);
bool backup_restore(const std::string &name, std::string &msg);   // danach Neustart noetig
bool backup_delete(const std::string &name, std::string &msg);
bool backup_valid_name(const std::string &name);
