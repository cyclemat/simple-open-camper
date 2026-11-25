# Panels und Voraussetzungen

## 📋 Voraussetzungen

- Ein Panel – Vergleich siehe [Welches Panel?](#welches-panel):
  **Elecrow CrowPanel Advanced 7" (ESP32-P4)** (EC-P4-7-V1.2), **Waveshare ESP32-S3-Touch-LCD-7B** (WS-S3-7B) oder als
  kleines rundes Panel das **Elecrow CrowPanel 1,46" Rotary** (EC-S3-1.46)
- MQTT-Broker mit **Zigbee2MQTT** (z. B. SMHUB Nano)
- VS Code mit **PlatformIO** (pioarduino, Arduino-ESP32 3.x / ESP-IDF 5.x)
- Optional: microSD-Karte (FAT32/exFAT) für Hintergründe und Sicherungen
- Für CP Plus oder Alde: ein **LIN-Transceiver** (TJA1020/TJA1021/MCP2025) und ein RJ12-Kabel


<a id="welches-panel"></a>

## 🖥️ Welches Panel? P4 oder Waveshare

**Gerätekürzel** – im Text und in den Dateien kurz so genannt: Hersteller – Chip – Zoll – Platinen-Revision
(die Revision nur, wo sie für die Firmware zählt).

| Kürzel | Gerät |
|---|---|
| **EC-P4-7-V1.2** | Elecrow CrowPanel Advanced 7" HMI (ESP32-P4), Platine V1.2 – spätere Revisionen entsprechend V1.3 usw. |
| **WS-S3-7B** | Waveshare ESP32-S3-Touch-LCD-7B |
| **EC-S3-1.46** | Elecrow CrowPanel 1,46" Rotary (rund, ESP32-S3) |
| **CYD-ESP32-2.8** | „Cheap Yellow Display“ ESP32-2432S028R (2,8", ESP32) – Victron-Display, Satellit, Zusatzdisplay |

Beide große Panels haben 7 Zoll und 1024 × 600 Bildpunkte und laufen mit derselben Firmware und demselben Editor.
Der Unterschied liegt im Chip – und damit darin, wie viel gleichzeitig laufen kann.

> ℹ️ **Elecrow CrowPanel Advanced 9" und 10,1" (ESP32-P4): noch nicht getestet.** Beide haben laut Elecrow ebenfalls
> 1024 × 600 Bildpunkte, denselben Chip (ESP32-P4 + Funkchip ESP32-C6) und dieselbe Beschaltung wie das 7"-Modell.
> Ich gehe deshalb davon aus, dass die **7"-Firmware** (`panel-crowpanel-p4.bin`, im Online-Flasher „CrowPanel
> Advanced 7"“) auch darauf läuft. Wer eins hat: bitte ausprobieren und als
> [GitHub-Issue](https://github.com/cyclemat/simple-open-camper/issues) melden (Bild, Touch, WLAN, Bluetooth, SD-Karte).

| | **Elecrow CrowPanel Advanced 7" (ESP32-P4)** (EC-P4-7-V1.2) | **Waveshare ESP32-S3-Touch-LCD-7B** (WS-S3-7B) |
|---|---|---|
| Chip | ESP32-P4, 2 Kerne RISC-V, 400 MHz | ESP32-S3, 2 Kerne Xtensa LX7, 240 MHz |
| Interner RAM | 768 KB – im Betrieb **~270 KB frei** | 512 KB – im Betrieb **~45 KB frei** (mit WLAN, Zigbee, Cerbo und Bluetooth) |
| PSRAM | **32 MB** | 8 MB (Octal, 80 MHz) |
| Flash | 16 MB | 16 MB |
| WLAN / Bluetooth | eigener Funkchip **ESP32-C6** (WLAN 6, BLE 5) – entlastet den Hauptchip | im Chip selbst – WLAN und Bluetooth teilen sich RAM und Antenne mit allem anderen |
| Display | IPS, MIPI-DSI, Grafikbeschleuniger (DMA2D, Hardware-JPEG) | paralleles RGB, das Bild wird ständig aus dem PSRAM gelesen |
| Akku | kein Messpunkt am P4 – nur über eigenen Spannungsteiler (2 × 100 kΩ an GPIO 49) | Akkuanschluss mit Spannungsmessung |
| Firmware-Größe | ~66 % des App-Bereichs (6 MB seit 0.2.7) | ~75 % des App-Bereichs |
| Bauen | ~1–2 min (fertige Framework-Bibliotheken) | erster Build 10–20 min (Framework wird neu gebaut) |
| Umgebung | `crowpanel-p4` (Ordner `p4/`) | `waveshare-7b` |
| Bezugsquelle | [bei Amazon](https://link.amazon/B04j2pDnV) | [bei Amazon](https://www.amazon.de/dp/B0FDQXKFJ5) |

**P4 – Vorteile:** viel Reserve, alle Module gleichzeitig möglich; eingebauter MQTT-Broker (P4 und rundes Panel); Funk auf eigenem
Chip; ruhiges Bild ohne PSRAM-Engpass beim Bildaufbau; geplant: Zigbee direkt über das ESP32-H2-Funkmodul.
**P4 – Nachteile:** teurer; Akku nicht ab Werk messbar; der Funkchip C6 braucht ggf. ein Firmware-Update
(Editor → Einstellungen → Info, Knopf „Funkmodul aktualisieren“, oder serieller Befehl `c6update`); frühe Chips (Revision v1.x) rechnen Bluetooth-Kopplungen mit dem Hardware-ECC falsch –
die Firmware rechnet diese Schlüssel deshalb automatisch in Software.

**Waveshare – Vorteile:** günstig und verbreitet; alles auf einem Chip; Akkuanschluss mit Messung, SD-Karte, Uhrenchip.
**Waveshare – Nachteile:** **interner RAM sehr knapp**; das Bild wird aus demselben PSRAM gelesen, in dem das Panel
zeichnet – bei vollen Seiten und viel Bewegung sind Streifen/Bildfehler möglich (Bildwiederholrate deshalb 25 Hz);
WLAN und Bluetooth teilen sich Speicher und Antenne.

> ⚠️ **Waveshare: nicht alles einbauen!** Der ESP32-S3 ist mit Oberfläche, WLAN, Zigbee2MQTT, Cerbo GX und Bluetooth
> **fast voll** (Stand 0.2.0: ~45 KB interner RAM frei). Bau nur die Module ein, die du wirklich brauchst
> (`[features]`, `[truma]`, `[alde]` in `platformio.ini`) – `check-all` ist nur zum Probebauen, nicht zum Hochladen.
> Schalte am Panel außerdem Bluetooth-Protokolle ab, die du nicht nutzt (z. B. iNet X ohne Kopplung): Abgeschaltete
> Module belegen keinen RAM. Wer viele Module gleichzeitig nutzen möchte, nimmt den **P4**.

**Rundes Panel – Elecrow CrowPanel 1,46" Rotary** (EC-S3-1.46; ESP32-S3R8, 8 MB PSRAM, 16 MB Flash, rund 360 × 360,
Drehknopf, LED-Ring; Umgebung `crowpanel-146`, Ordner `crowpanel146/`): kleines Panel bzw. Zusatzdisplay für eine
Kachel je Bildschirm. Ohne Einstellungsmenü – eingerichtet wird im Browser (eigenes WLAN mit Captive Portal).
Bezugsquelle: [bei Amazon](https://link.amazon/B02xEzSrj).

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
