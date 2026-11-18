// Geraetetreiber fuer das Zigbee-Funkmodul (ESP32-H2): Format der Datei zb_drivers.bin
//
// Erzeugt von tools/z2m_drivers.js (laeuft beim GitHub-Build) aus der Geraetedatenbank von Zigbee2MQTT
// (zigbee-herdsman-converters, MIT-Lizenz). Liegt im H2 im eigenen Bereich "zbdrv" (0x1F0000, h2/partitions_h2.csv)
// und laesst sich unabhaengig vom Programm ueberschreiben: write_flash 0x1F0000 zb_drivers.bin
//
// Alle Zahlen little-endian. Zeichenketten: nullterminiert (UTF-8) im Textbereich, angegeben als Abstand ab
// dessen Anfang (ZBD_NONE = keine).
//
// Kopf (48 Byte):
//   0  char[4]  "SOCZ"
//   4  u16      Format (ZBD_FORMAT)
//   6  u16      frei
//   8  u32      Anzahl Geraetedefinitionen
//  12  u32      Anzahl Indexeintraege (eine Definition kann mehrere Modell-/Herstellerkennungen haben)
//  16  u32      Abstand Index ab Dateianfang
//  20  u32      Abstand Textbereich ab Dateianfang
//  24  u32      Gesamtgroesse
//  28  u32      CRC-32 ueber Byte 48 bis Gesamtgroesse
//  32  u32      Text: Version der Quelle (z. B. "zigbee-herdsman-converters 26.133.0")
//  36  u32      Erzeugt (Unix-Zeit)
//  40  u8[8]    frei
//
// Index (je 12 Byte, sortiert nach Modell-Hash, dann Hersteller-Hash):
//   u32 FNV-1a der Modellkennung (Basic 0x0005), u32 FNV-1a des Herstellernamens (Basic 0x0004) oder 0 = jeder,
//   u32 Abstand der Definition ab Dateianfang
//   Suche: Modell-Hash; unter den Treffern zuerst passender Hersteller-Hash, sonst 0.
//
// Definition:
//   u32 Hersteller, u32 Modell, u32 Beschreibung (Texte)
//   u8 Anzahl Werte (Exposes), u8 Anzahl Tuya-Datenpunkte, u8 Merkmale (ZBD_F_*), u8 Anzahl fz-Namen
//   je fz-Name: u32 Text (Name des Zigbee2MQTT-Umrechners, z. B. "fz.on_off", fuer spaeteren Code im Modul)
//   je Wert (ZbdExpose, 28 Byte) + Anzahl Werte * u32 Text (Auswahlwerte bzw. Ein/Aus-Werte)
//   je Datenpunkt (ZbdDp, 12 Byte) + Anzahl * ZbdDpMap (8 Byte)
#pragma once
#include <stdint.h>

#define ZBD_MAGIC "SOCZ"
#define ZBD_FORMAT 1
#define ZBD_NONE 0xFFFFFFFFu
#define ZBD_HEADER_SIZE 48

// Merkmale einer Definition
#define ZBD_F_TUYA 0x01      // Tuya-Datenpunkte (Cluster 0xEF00)
#define ZBD_F_LUMI 0x02      // Aqara/Xiaomi (Herstellerattribute)
#define ZBD_F_DYNAMIC 0x04   // Werte haengen vom Geraet ab (Zigbee2MQTT erzeugt sie zur Laufzeit) – Liste unvollstaendig
#define ZBD_F_MULTI_EP 0x08  // mehrere Endpunkte (z. B. Doppelschalter)

// Art eines Wertes
enum ZbdType : uint8_t { ZBD_T_OTHER = 0, ZBD_T_BINARY, ZBD_T_NUMERIC, ZBD_T_ENUM, ZBD_T_TEXT };
// Gruppe (aus welchem Zigbee2MQTT-Baustein der Wert stammt) – Hinweis fuer Kacheln
enum ZbdGroup : uint8_t {
  ZBD_G_NONE = 0, ZBD_G_LIGHT, ZBD_G_SWITCH, ZBD_G_CLIMATE, ZBD_G_COVER, ZBD_G_LOCK, ZBD_G_FAN, ZBD_G_OTHER
};
// Zugriff (wie Zigbee2MQTT): 1 = meldet, 2 = setzbar, 4 = abfragbar
#define ZBD_A_STATE 1
#define ZBD_A_SET 2
#define ZBD_A_GET 4

#pragma pack(push, 1)
struct ZbdHeader {
  char magic[4];
  uint16_t format, reserved;
  uint32_t defs, index_n, index_off, str_off, total, crc;
  uint32_t source, created;
  uint8_t free[8];
};
struct ZbdIndex {
  uint32_t h_model, h_mf, rec;
};
struct ZbdRecord {
  uint32_t vendor, model, description;
  uint8_t n_exposes, n_dps, flags, n_fz;
};
struct ZbdExpose {
  uint32_t property;   // Name des Wertes ("temperature", "state", …)
  uint32_t unit;       // Einheit ("°C", "%") oder ZBD_NONE
  float min, max, step;   // nur ZBD_T_NUMERIC (sonst 0)
  uint8_t type;        // ZbdType
  uint8_t access;      // ZBD_A_*
  uint8_t group;       // ZbdGroup
  uint8_t n_values;    // folgende u32-Texte: Auswahlwerte (enum) bzw. [Ein, Aus] (binary)
  uint32_t endpoint;   // Endpunkt-Name ("l1", "l2" …) oder ZBD_NONE
};
// Art eines Tuya-Datenpunkts (vom Generator durch Probieren der Zigbee2MQTT-Umrechnung bestimmt)
enum ZbdDpKind : uint8_t {
  ZBD_DP_UNKNOWN = 0,   // Umrechnung nicht erkannt (Rohwert weitergeben)
  ZBD_DP_NUMBER,        // Zahl * factor
  ZBD_DP_BOOL,          // Zuordnung Rohwert -> "true"/"false" in der Tabelle
  ZBD_DP_ENUM,          // Zuordnung Rohwert -> Text in der Tabelle
};
struct ZbdDp {
  uint8_t dp;          // Datenpunkt-Nummer
  uint8_t kind;        // ZbdDpKind
  uint8_t n_map;       // folgende ZbdDpMap-Eintraege
  uint8_t reserved;
  uint32_t name;       // Name des Wertes (wie Expose.property)
  float factor;        // ZBD_DP_NUMBER: Wert = Rohwert * factor
};
struct ZbdDpMap {
  int32_t raw;
  uint32_t text;
};
#pragma pack(pop)

// FNV-1a (32 Bit) – identisch im Generator (tools/z2m_drivers.js)
static inline uint32_t zbd_hash(const char *s) {
  uint32_t h = 2166136261u;
  while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
  return h;
}
