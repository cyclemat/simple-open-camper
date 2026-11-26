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

### 🔌 Anschlüsse und freie GPIOs am CrowPanel P4

Ausgelesen aus dem Eagle-Schaltplan **V1.2** von Elecrow
([Schaltplan als PDF](https://github.com/Elecrow-RD/CrowPanel-Advanced-7inch-ESP32-P4-HMI-AI-Display-1024x600-IPS-Touch-Screen/blob/master/Eagle_SCH%26PCB/1.2/ESP32-P4%20Display%207.0%20inch%20V1.2.pdf) ·
[Ordner mit Eagle-Dateien](https://github.com/Elecrow-RD/CrowPanel-Advanced-7inch-ESP32-P4-HMI-AI-Display-1024x600-IPS-Touch-Screen/tree/master/Eagle_SCH%26PCB/1.2)).
Andere Platinen-Revisionen vor dem Löten mit der Beschriftung auf der Platine vergleichen. Alle Signale **3,3 V** – außer
wo es anders steht. Die 5-V-Pins sind nur zur Versorgung, **nie an einen GPIO**.

**GPIO-Steckleiste J7 (2 × 12 Pins, oben)** – elf GPIOs **direkt am P4**, ohne Reihenwiderstände oder Pull-ups:

| Pin | Signal | Pin | Signal |
|---|---|---|---|
| 1 | 3V3 | 2 | 5 V |
| 3 | 3V3 | 4 | 5 V |
| 5 | 3V3 | 6 | 5 V |
| 7 | GND | 8 | GND |
| 9 | **GPIO 2** | 10 | GND |
| 11 | **GPIO 3** | 12 | **GPIO 54** (ADC) |
| 13 | **GPIO 4** | 14 | **GPIO 53** (ADC) |
| 15 | **GPIO 5** | 16 | **GPIO 52** (ADC) |
| 17 | **GPIO 25** | 18 | **GPIO 51** (ADC) |
| 19 | **GPIO 49** (ADC) | 20 | **GPIO 50** (ADC) |
| 21 | GND | 22 | GND |
| 23 | GND | 24 | GND |

Pin-Nummern wie im Schaltplan (ungerade Reihe / gerade Reihe); maßgeblich ist die Beschriftung auf der Platine.

- **GPIO 49–54:** ADC2 Kanal 0–5 (analog bis ~3,1 V). GPIO 49 ist der vorgesehene Pin für den Akku-Spannungsteiler
  (siehe Tabelle oben), die anderen fünf sind frei.
- **GPIO 2–5:** Low-Power-Pins (Aufwachen aus dem Tiefschlaf, Touch-Pads), nur digital. **GPIO 25:** digital.
- Jeder dieser Pins kann über die GPIO-Matrix jede Schnittstelle bekommen: UART, I²C, SPI, CAN (TWAI), PWM, 1-Wire.
  Freie Hardware-UARTs: **1, 2 und 4** (0 = USB-Konsole, 3 = Zigbee-Funkmodul). Belastung: wenige mA je Pin, Relais
  und LEDs immer über Treiber.

**In der Firmware nutzbar** (Stand 0.2.9): LIN für CP Plus/Alde (Pins in `p4/platformio.ini`, `[truma]`/`[alde]`),
Hobby CI-BUS und Votronic Display-Link (Einstellungen → Fahrzeug bzw. Votronic: UART und Pins wählen), Akkumessung
(GPIO 49). Eigene Sensoren (DS18B20, Tanksonden …) hängen weiterhin an **Satelliten** – direkt am Panel gibt es dafür
noch keine Einstellung.

**Weitere Buchsen**

| Buchse | Pins | GPIO / Hinweis |
|---|---|---|
| **UART1** (HY2.0-4P, J2) | 1 RX, 2 TX, 3 3V3, 4 GND | **GPIO 48 (RX) / 47 (TX)**, direkt am P4, 3,3 V – frei, z. B. für Hobby, Votronic oder einen LIN-Transceiver |
| **I2C** (HY2.0-4P, J13) | 1 SCL, 2 SDA, 3 3V3, 4 GND | I²C1 **GPIO 46 (SCL) / 45 (SDA)** über Pegelwandler (BSS138), 3,3 V; derselbe Bus wie Touch (0x5D) und Modul-Steckplatz – weitere Geräte mit freier Adresse möglich, die Firmware liest dort aber noch nichts aus |
| **UART3-IN** (XH2.54-4P, J10) | 1 TX, 2 RX, 3 5 V ein, 4 GND | **GPIO 34 (TX) / 33 (RX)** über MOSFET-Pegelwandler mit **5-V-Pegel**; zugleich 5-V-Versorgung des Panels. GPIO 34 ist ein Strapping-Pin – ein Gerät, das die Leitung beim Start auf Low zieht, kann den Start stören |
| **Modul-Steckplatz** (2 × 7 Pins, J9/J11) | SPI, UART, I²C, 3V3 (eigener Regler), 5 V | SPI2 **GPIO 8/7/6** (SCK/MISO/MOSI), **GPIO 9/10**, UART2 **GPIO 27 (TX) / 28 (RX)**, I²C1. Mit gestecktem Zigbee-Modul (H2) belegt – ohne Modul sind GPIO 6–10, 27 und 28 über die Buchsenleisten frei |
| Lautsprecher (2 × PH2.0, J4/J6) | links / rechts | I²S **GPIO 22/21/23**, Verstärker-Enable **GPIO 30** – siehe [Ton & Musik](Ton-und-Musik) |
| Akku (PH2.0, J3) | +, – | Lader TP4059; die Spannung misst nur der Hilfscontroller STC8, nicht der P4 → Spannungsteiler an GPIO 49 |
| microSD (J5) | – | SDMMC **1 Bit**: CLK 43, CMD 44, D0 39 (D1/D2 sind nicht am P4) |
| USB-C „UART“ (J1) / „USB“ (J16) | – | CH340K → **GPIO 37/38** (UART0: Konsole, Flashen) / USB des P4 (von der Firmware nicht genutzt) |
| Kamera (FPC 24-polig) | – | MIPI-CSI, I²C2 **GPIO 12/13**, Reset **GPIO 11** – nicht genutzt |
| STC8 (1 × 4, J8) | TX, RX, 3V3, GND | Programmierleiste des Hilfscontrollers STC8H1K08 (Akku, Laden) – **kein GPIO-Port** |

**Fest belegt** (nicht verwenden): 0/1 Uhrenquarz · 6–10, 27/28 Modul-Steckplatz · 11–13 Kamera · 14–19 SDIO zum
Funkchip C6 · 20 Mikrofon · 21–24, 26 I²S · 29 Hintergrundlicht-Versorgung · 30 Verstärker · 31 Hintergrundlicht (PWM) ·
32 C6-Reset · 33/34 UART3-Buchse · 35 BOOT-Taste · 36 Download-Strapping · 37/38 UART0 · 39/43/44 SD-Karte ·
40/42 Touch · 41 LCD-Reset · 45/46 I²C1 · 47/48 UART1-Buchse.

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
