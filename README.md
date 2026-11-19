<p align="center">
  <img src="assets/banner.png" alt="Simple Open Camper (S.O.C.) – Touch-Panel für den Camper" width="100%">
</p>

<h1 align="center"><img src="assets/van-right.svg" height="36" alt=""> Simple Open Camper <img src="assets/van-left.svg" height="36" alt=""></h1>
<h3 align="center">S.O.C. – Touch-Panel für den Camper: Zigbee2MQTT, Gas, Wasser, Heizung und Bordtechnik – lokal, ohne Cloud, ohne Home Assistant</h3>

<p align="center">
  <img src="https://img.shields.io/badge/Hardware-ESP32--P4%20%7C%20ESP32--S3-0089D0" alt="Hardware">
  <img src="https://img.shields.io/badge/Display-1024%C3%97600%20Touch-blue" alt="Display">
  <img src="https://img.shields.io/badge/Framework-Arduino%20%2F%20ESP--IDF%205-orange" alt="Framework">
  <img src="https://img.shields.io/badge/Verbindung-MQTT%20%7C%20Bluetooth%20LE%20%7C%20LIN-blueviolet" alt="Verbindung">
  <img src="https://img.shields.io/badge/Satelliten-ESP32-0075A9" alt="Satelliten">
  <img src="https://img.shields.io/badge/Cloud-keine-brightgreen" alt="Ohne Cloud">
  <img src="https://img.shields.io/badge/Lizenz-PolyForm%20Noncommercial-blue" alt="Lizenz PolyForm Noncommercial">
  <img src="https://img.shields.io/badge/Status-Beta%20%2F%20Testing-yellow" alt="Status">
</p>

<p align="center">
  <strong>🇩🇪 Deutsch</strong> · <a href="README_EN.md">🇬🇧 English</a>
</p>

> **Licht, Gas, Heizung und Batterie auf einem Bildschirm – direkt per MQTT, Bluetooth und LIN-Bus.**

---

## 🚧 Beta – Tester und Ideen willkommen!

Das Panel läuft im Alltag mit Zigbee2MQTT (getestet mit **131 Geräten / 73 Kacheln**). Einige Module sind nach
offenen Protokollbeschreibungen gebaut, aber **noch nicht an echter Hardware getestet**:
Mopeka mit echtem Sensor, Truma CP Plus, Truma iNet X, Alde, WLED und Cerbo GX, die meisten Akku-Typen und
Power Stations sowie die neuen Sensoren **SuperSense**, **Simarine STRx**, **Ruuvi**, **SwitchBot** und Akkus von
**CS-BATTERIES**.

Wer eines davon hat und testen mag: Rückmeldungen, Logs und Ideen sind sehr willkommen!

---

## 🚀 Über das Projekt

Ein Wand-Panel mit 7-Zoll-Touch (1024 × 600), das Lampen und Schalter aus **Zigbee2MQTT direkt per MQTT** steuert –
ohne Home Assistant dazwischen. Dazu liest es Gasflaschen, Tanks, Victron-Geräte und die Heizung im Wohnmobil.
Das Layout baust du im Browser-Editor, den das Panel selbst ausliefert.

> 💡 **So fing es an:** Im Wohnwagen nutze ich selbst Home Assistant – aber für Licht und ein paar Werte braucht
> das System auf Dauer zu viel Strom. Gesucht habe ich etwas Sparsameres. So entstand das Panel als
> **Lichtsteuerung für den SMHUB Nano**: ein Bildschirm an der Wand, der die Zigbee-Lampen direkt schaltet.
>
> Auf die Idee, mehr daraus zu machen, hat mich meine
> [Truma-LevelControl-Integration für Home Assistant](https://github.com/cyclemat/HAOS-Truma-Levelcontrol-Integration)
> gebracht: Wenn das Panel den Gasstand selbst per Bluetooth lesen kann, braucht es dafür kein Home Assistant.
> Danach kamen Tanks, Victron, die Truma-Heizung, Alde und Satelliten dazu – Stück für Stück ist daraus das
> Bordpanel für den ganzen Camper gewachsen. Daher auch der neue Name: aus dem „Zigbee-Panel“ wurde
> **Simple Open Camper**.

- 🏠 Alles läuft **lokal** auf dem Panel – keine Cloud, kein Konto
- 📶 Eigenes WLAN möglich (Router-Modus) – kein Heimnetz nötig, ideal fürs Wohnmobil
- 🧩 Jedes Zusatzmodul lässt sich in `platformio.ini` aus- oder einbauen

---

## ✨ Funktionen

**Bedienung**
- 💡 **Kacheln** zeigen die echte Lichtfarbe (Farbe, Weißton, Helligkeitsbalken). Tippen schaltet, langes Drücken
  öffnet Helligkeit, Farbtemperatur, Sättigung und Farbrad – jeweils nur, wenn das Gerät es kann.
- 🗂️ **Mehrere Seiten** (Wischen oder Reiter), jede auf Wunsch mit eigenem Hintergrundbild (z. B. Grundriss).
- ✏️ **Web-Editor** unter `http://<panel>/`: Geräte per Drag & Drop anordnen, Größen, Symbole, Seiten,
  Hintergründe, Namen, Einstellungen, Firmware-Update übers WLAN. Ohne Panel-Verbindung startet er mit Demo-Daten.
- 🔒 **Zugangspasswort** für Editor und Weboberfläche – beim ersten Öffnen wird ein eigenes festgelegt.
- ⌨️ **Einstellungen am Panel** mit deutscher Bildschirmtastatur (QWERTZ, ä ö ü ß).
- 🌍 **Oberfläche auf Deutsch, Englisch, Spanisch oder Französisch** – weitere Sprachen leicht ergänzbar.
- 🌙 **Ruhezustand**: Abdunkeln, Schwarz (schont das LCD), dunkle Uhr oder hüpfendes Logo (eigenes PNG möglich).
- 💾 **Sicherung auf der SD-Karte**: Layout, Namen, alle Einstellungen, Sensoren, Logo und Hintergründe.
- 🕐 **Uhr von Hand stellen** für den Betrieb ohne Internet; sonst per Internet oder Browser.
- 🔋 **Akkuanzeige** in der Leiste (Ladestand, geschätzt: Akkubetrieb / lädt / USB voll).

**Geräte**
- 🐝 **Zigbee2MQTT** – Lampen und Schalter, auch große Netze
- 🔌 **Shelly** (Gen1 und Gen2+) über denselben Broker
- 🌈 **WLED** über denselben Broker – Ein/Aus, Helligkeit, Farbe
- ⛽ **Truma LevelControl** per Bluetooth – bis zu vier Gasflaschen
- 🛢️ **Mopeka** per Bluetooth – bis zu vier Gas- und Tanksensoren
- ☀️ **Victron** per Bluetooth („Instant Readout“) – Solarregler, Batteriemonitor, Ladebooster
- 🔋 **Victron Cerbo GX** (Venus OS) per MQTT – Solar, Netz/Landstrom, Ladegerät, Batterie (SOC) und DC-System
- 🔥 **Truma-Heizung** (Combi/VarioHeat) – über **CP Plus** (LIN-Bus) oder **iNet X** (Bluetooth)
- ♨️ **Alde-Warmwasserheizung** (Compact 3020, 3030, 3030 Plus) über den LIN-Bus
- 🔋 **Akkus mit Bluetooth-BMS** – Super B, Power Quarry/Shentec, Daly/Bulltron, JBD/Xiaoxiang, CS-BATTERIES *(Beta)*,
  JK-BMS, LiTime/Redodo/PowerQueen, Renogy, Eco-Worthy, ANT, Vatrer: Ladezustand, Spannung, Strom, Temperatur, Zellen –
  nur lesen; Kopplungs-PIN bzw. Passwort aus der Hersteller-App möglich
- ⚡ **Power Stations** per Bluetooth – Bluetti, Allpowers, Anker SOLIX, EcoFlow: Ladestand, Leistung, AC-/DC-Ausgang schalten
- 💧 **SuperSense-Tankmodul** *(Beta)* – bis zu vier Tanksensoren per Bluetooth
- 💧 **Simarine STRx** *(Beta)* – Tanksensoren STR1/STR2/STR3 per Bluetooth-Broadcast, offen oder verschlüsselt
- 🌡️ **Klima-Sensoren** *(Beta)* – **Ruuvi** Tag/Air und **SwitchBot** Meter/Meter Plus/Outdoor Meter: Temperatur,
  Luftfeuchte, Luftdruck, beim Ruuvi Air auch CO₂ und Feinstaub; das Panel hört nur mit, keine Kopplung
- 🐝 **Zigbee direkt** *(Beta, nur P4)* – mit dem ESP32-H2-Funkmodul ganz ohne Zigbee2MQTT: Geräte anlernen,
  Lampen, Steckdosen (mit Leistung und Verbrauch), Schalter und Sensoren als Kacheln
- 🧩 **Regeln** *(Beta, nur P4)* – Automationen mit grafischen Bausteinen (Blockly) im Editor, z. B. Bewegungslicht,
  Licht per Taster, Dimmen per Taster

**Drei Betriebsarten**

| Modus | Beschreibung |
|---|---|
| Netzwerk | Panel und Hub im Camp-WLAN |
| Router | Panel öffnet ein eigenes WLAN mit DHCP; der SMHUB Nano bucht sich ein, das Panel findet den Broker automatisch |
| Kombi | Eigenes WLAN für den Hub **und** Camp-WLAN, mit Internet-Weiterleitung (NAT) |

---

## 📋 Voraussetzungen

- Ein Panel – Vergleich siehe [Welches Panel?](#welches-panel):
  **Elecrow CrowPanel Advanced 7" (ESP32-P4)**, **Waveshare ESP32-S3-Touch-LCD-7B** oder als kleines rundes Panel
  das **Elecrow CrowPanel 1,46" Rotary**
- MQTT-Broker mit **Zigbee2MQTT** (z. B. SMHUB Nano)
- VS Code mit **PlatformIO** (pioarduino, Arduino-ESP32 3.x / ESP-IDF 5.x)
- Optional: microSD-Karte (FAT32/exFAT) für Hintergründe und Sicherungen
- Für CP Plus oder Alde: ein **LIN-Transceiver** (TJA1020/TJA1021/MCP2025) und ein RJ12-Kabel

---

<a id="welches-panel"></a>

## 🖥️ Welches Panel? P4 oder Waveshare

Beide große Panels haben 7 Zoll und 1024 × 600 Bildpunkte und laufen mit derselben Firmware und demselben Editor.
Der Unterschied liegt im Chip – und damit darin, wie viel gleichzeitig laufen kann.

| | **Elecrow CrowPanel Advanced 7" (ESP32-P4)** | **Waveshare ESP32-S3-Touch-LCD-7B** |
|---|---|---|
| Chip | ESP32-P4, 2 Kerne RISC-V, 400 MHz | ESP32-S3, 2 Kerne Xtensa LX7, 240 MHz |
| Interner RAM | 768 KB – im Betrieb **~270 KB frei** | 512 KB – im Betrieb **~45 KB frei** (mit WLAN, Zigbee, Cerbo und Bluetooth) |
| PSRAM | **32 MB** | 8 MB (Octal, 80 MHz) |
| Flash | 16 MB | 16 MB |
| WLAN / Bluetooth | eigener Funkchip **ESP32-C6** (WLAN 6, BLE 5) – entlastet den Hauptchip | im Chip selbst – WLAN und Bluetooth teilen sich RAM und Antenne mit allem anderen |
| Display | IPS, MIPI-DSI, Grafikbeschleuniger (DMA2D, Hardware-JPEG) | paralleles RGB, das Bild wird ständig aus dem PSRAM gelesen |
| Akku | kein Messpunkt am P4 – nur über eigenen Spannungsteiler (2 × 100 kΩ an GPIO 49) | Akkuanschluss mit Spannungsmessung |
| Firmware-Größe | ~78 % des App-Bereichs | ~75 % des App-Bereichs |
| Bauen | ~1–2 min (fertige Framework-Bibliotheken) | erster Build 10–20 min (Framework wird neu gebaut) |
| Umgebung | `crowpanel-p4` (Ordner `p4/`) | `waveshare-7b` |
| Bezugsquelle | [bei Amazon](https://link.amazon/B04j2pDnV) | [bei Amazon](https://www.amazon.de/dp/B0FDQXKFJ5) |

**P4 – Vorteile:** viel Reserve, alle Module gleichzeitig möglich; eingebauter MQTT-Broker (nur P4); Funk auf eigenem
Chip; ruhiges Bild ohne PSRAM-Engpass beim Bildaufbau; geplant: Zigbee direkt über das ESP32-H2-Funkmodul.
**P4 – Nachteile:** teurer; Akku nicht ab Werk messbar; der Funkchip C6 braucht ggf. ein Firmware-Update
(serieller Befehl `c6update`); frühe Chips (Revision v1.x) rechnen Bluetooth-Kopplungen mit dem Hardware-ECC falsch –
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

**Rundes Panel – Elecrow CrowPanel 1,46" Rotary** (ESP32-S3R8, 8 MB PSRAM, 16 MB Flash, rund 360 × 360,
Drehknopf, LED-Ring; Umgebung `crowpanel-146`, Ordner `crowpanel146/`): kleines Panel bzw. Zusatzdisplay für eine
Kachel je Bildschirm. Ohne Einstellungsmenü – eingerichtet wird im Browser (eigenes WLAN mit Captive Portal).
Bezugsquelle: [bei Amazon](https://link.amazon/B02xEzSrj).

---

## 📦 Installation

1. VS Code und PlatformIO installieren, diesen Ordner öffnen (Pfad ohne Leerzeichen, z. B. `C:\zp\simply-open-camper`).
2. Board über die USB-C-Buchse **„UART“** anschließen – dort kommen auch die Logs an.
3. Umgebung wählen: `waveshare-7b`, `crowpanel-p4` oder `crowpanel-146` (im PlatformIO-Menü), dann
   **PlatformIO: Upload**. Beim Waveshare dauert der **erste Build 10–20 Minuten**, weil das Arduino-Framework mit
   eigenen Einstellungen neu gebaut wird (XIP aus PSRAM gegen Bildflackern, NAT für den Kombi-Modus).
4. Vor dem Hochladen fragt das Terminal nach **Enter** – Zeit zum Anstecken.
5. Danach führt das Panel durch die Einrichtung.
6. Beim ersten Öffnen des Editors (`http://<panel>/`) ein eigenes **Zugangspasswort** festlegen (ab Werk
   `panelsat`). Vergessen? Am Panel unter **Einstellungen → Netzwerk** zurücksetzen (rundes Panel: seriell `webpw`).

### 📁 Fertige Firmware ohne neu zu bauen

Jeder Build legt seine Dateien zusätzlich in **`release/lokal/`** ab (nicht im Git) – je Umgebung immer nur die
neueste:

| Datei | Wofür |
|---|---|
| `<umgebung>.bin` | Komplettdatei (Bootloader, Partitionen, App) – mit esptool ab Adresse `0x0` schreiben. **Löscht die Einstellungen** (nur für neue Geräte) |
| `<umgebung>-update.bin` | nur die App – für Updates über die Weboberfläche des Geräts, oder per USB an Adresse `0x10000` (Einstellungen bleiben) |
| `esp32-updater@0x2F0000.bin` | kleiner Updater der 4-MB-Geräte, an die angegebene Adresse |
| `stand.txt` | Version und Zeitpunkt je Umgebung |

Beispiele: neues Gerät `esptool --port COM11 write-flash 0x0 release/lokal/esp32-relay16.bin`,
eingerichtetes Gerät (Einstellungen behalten) `esptool --port COM11 write-flash 0x10000 release/lokal/esp32-relay16-update.bin`

### 🛠 Wartung und Diagnose

- `tools/embed_web.py` packt bei jedem Build den Editor gzip-komprimiert in die Firmware.
- `tools/upload_wait.py` wartet vor dem Hochladen auf Enter (`PANEL_UPLOAD_NOWAIT=1` schaltet es ab).
- `pio run -e check-all` baut zur Probe eine Variante mit **allen** Modulen (nicht zum Hochladen gedacht).
- Nur nötig, wenn du Schriften, Icons oder den Editor änderst:

```sh
node tools/gen_icons.js        # Icons (react-icons + sharp nötig)
python3 tools/gen_fonts.py     # LVGL-Schriften aus Inter + Icons
python3 tools/build_editor.py  # web/editor.src.html -> web/editor.html
```

---

## ⚙ Konfiguration

### Module ein- und ausschalten (`platformio.ini`)

| Abschnitt | Schalter | Bedeutung |
|---|---|---|
| `[features]` | `wled = 1` | WLED über MQTT |
| | `cerbo = 1` | Victron Cerbo GX über MQTT |
| | `lang = de` | Sprache der Panel-Oberfläche: `de`, `en`, `es`, `fr` |
| | `hymer = 1` | Hymer Connect über Bluetooth (an/aus dann in den Einstellungen; Satellit: `[sat] hymer`) |
| `[truma]` | `cpplus = 0` | Truma CP Plus über LIN (Transceiver nötig) |
| | `lin_uart`, `lin_rx`, `lin_tx`, `lin_en` | LIN-Anschluss fürs CP Plus (Standard UART2) |
| | `inetx = 1` | Truma iNet X über Bluetooth (an/aus dann in den Einstellungen) |
| `[alde]` | `enabled = 0` | Alde über LIN (das Panel ist LIN-Master) |
| | `model` | `3020`, `3030` oder `3031` (= 3030 Plus) |
| | `baud`, `checksum` | 19200 / automatisch; beim 3020 ggf. 9600 |
| | `lin_uart`, `lin_rx`, `lin_tx`, `lin_en` | LIN-Anschluss (CP Plus und Alde brauchen getrennte UARTs) |
| | `red_uart`, `red_rx` | nur 3030: interner Bus zum Mitlesen (−1 = aus) |

Truma LevelControl, Mopeka und Victron sind immer eingebaut und werden am Panel unter
**Einstellungen → Gas/Heizung** bzw. **Victron** ein- und ausgeschaltet. Was aus ist, wird nicht gesucht und
belastet Funk und WLAN nicht.

### 🌍 Sprachen & Wörterbücher

| Bereich | Sprachen | Auswahl | Wörterbuch | Werkzeug |
|---|---|---|---|---|
| **Panel** (Bildschirm) | Deutsch, Englisch, Spanisch, Französisch | `lang` unter `[features]` in `platformio.ini` | `src/i18n/<kürzel>.h` | `tools/i18n_update.py` |
| **Browser-Editor** | Deutsch, Englisch | automatisch wie das Panel, oder `http://<panel>/?lang=en` | `web/i18n/<kürzel>.json` | `tools/i18n_editor.py` |

Die Texte im Quellcode sind deutsch – sie sind gleichzeitig der **Schlüssel** im Wörterbuch. Fehlt eine
Übersetzung, erscheint einfach der deutsche Text; es geht nichts kaputt. Gerätenamen werden nie übersetzt.

#### 📖 Wörterbuch des Panels (`src/i18n/<kürzel>.h`)

Eine Zeile pro Text: links deutsch (nicht ändern), rechts die Übersetzung, dahinter als Hilfe die Fundstelle.

```c
{"Einstellungen", "Settings"},                       // src/ui/ui_settings.cpp:1424
{"vor %ld Min.", "%ld min ago"},                     // src/ui/ui_gas.cpp:29
{"Kein Echo – Sensor richtig angebracht?", ""},      // leer = bleibt deutsch
```

**Neue Sprache** (z. B. Italienisch):

1. `python tools/i18n_update.py it` – legt `src/i18n/it.h` mit allen Texten an (rechts leer).
2. Die rechte Seite übersetzen.
3. In `platformio.ini` `lang = it` setzen und bauen.

#### 📖 Wörterbuch des Editors (`web/i18n/<kürzel>.json`)

Ein JSON-Objekt: deutscher Text → Übersetzung. `{}` steht für einen veränderlichen Teil (Zahl, Name …) und wird
in der Übersetzung in derselben Reihenfolge eingesetzt.

```json
{
 "Einstellungen": "Settings",
 "vor {} Min.": "{} min ago",
 "„{}“ heißt danach überall „{}“ – auch in Home Assistant.": "“{}” will then be called “{}” everywhere – including Home Assistant."
}
```

**Neue Sprache** (z. B. Französisch):

1. `python tools/i18n_editor.py fr` – legt `web/i18n/fr.json` mit allen Texten an (Übersetzungen leer).
2. Die Werte übersetzen.
3. `python tools/i18n_editor.py` erneut ausführen – baut die Wörterbücher in `web/editor.src.html` und
   `web/editor.html` ein. Danach bauen und hochladen.

#### ✍️ Regeln für Übersetzer

- **Links bzw. den Schlüssel nie ändern** – daran wird der Text im Code erkannt.
- **Platzhalter beibehalten:** im Panel `%s`, `%d`, `%u`, `%ld`, `%.1f`; im Editor `{}`. Anzahl und Reihenfolge
  müssen passen.
- **Leerzeichen am Anfang/Ende beibehalten** (z. B. `"  ·  Boiler "`) – solche Stücke werden mit anderen Texten
  zusammengesetzt.
- Die Schriften des Panels enthalten den Latin-1-Bereich (á é í ó ú ñ ç à è ê ü ß …), aber z. B. kein „œ“
  und keine nicht-lateinischen Schriften.
- **Neue Texte im Code:** `python tools/i18n_update.py` bzw. `python tools/i18n_editor.py` tragen sie in alle
  Wörterbücher ein; mit `--report` sehen sie nur nach, was noch fehlt.

Pull Requests mit neuen oder verbesserten Übersetzungen sind sehr willkommen!

### SMHUB Nano

- **Netzwerk-Modus:** Am Nano unter **Einstellungen → MQTT** „Allow External“ einschalten. Am Panel die IP des
  Nano eintragen (Port 1883, Basis-Topic `zigbee2mqtt`).
- **Router-Modus:** Am Panel **Netzwerk → Router** wählen (Panel startet neu). Am Nano unter WLAN das Panel-WLAN
  wählen (2,4 GHz) – Name und Passwort stehen am Panel. Der Editor ist dann unter `http://192.168.4.1/` erreichbar.
- **Funkkanäle:** WLAN und Zigbee teilen sich 2,4 GHz. Zigbee2MQTT nutzt standardmäßig Kanal 11 → Panel-WLAN
  auf 13 lassen. Im Kombi-Modus folgt das Panel-WLAN dem Kanal des Camp-WLANs.

---

## 📡 Geräte einrichten

### ⛽ Truma LevelControl

- Der Sensor sendet nur alle paar Minuten und erlaubt **eine Verbindung gleichzeitig**. Ist die Truma-App,
  Home Assistant oder der Victron GX gerade verbunden, versucht es das Panel beim nächsten Durchgang erneut.
- Die Suche läuft bis zu drei Minuten; Abfrageintervall 5 bis 60 Minuten (Standard 10).
- Die letzte Messung bleibt über Neustarts erhalten.

### 🛢️ Mopeka

- Unterstützt: Pro Check, Pro Plus, Pro Check Universal, Pro Check H2O, Lippert BottleCheck, Pro-200, TD40/TD200
  und die älteren Standard-Sensoren. Das Panel hört nur mit – die Mopeka-App bleibt nutzbar.
- Beim Suchen kurz die **Sync-Taste** am Sensor drücken – er wird dann als „Taste gedrückt“ markiert.
- Flaschentyp in der Detailansicht wählen: 6/11/14 kg (Europa), 20/30/40 lb (USA) oder eigene Werte (mm, kg/l).

### 🔋 Akkus und Power Stations

- Am Panel **Einstellungen → Akkus (BMS)** bzw. **Power Stations** → „Suchen“, im Editor Reiter **Akkus**.
- **App des Herstellers vorher schließen** – die meisten Akkus sprechen nur mit einem Gerät.
- Verlangt der Akku eine **Kopplungs-PIN** (z. B. Super B) oder hat er in seiner App ein **Passwort** (z. B.
  CS-BATTERIES), dieses im Feld „PIN / Passwort“ eintragen. Das Panel zeigt an, wenn der Akku danach fragt.
- **CS-BATTERIES** *(Beta, ungetestet)*: JBD-BMS mit Anmeldung – die Anmeldung der App erledigt das Panel selbst,
  nur ein eigenes Passwort muss eingetragen werden.
- **Zellspannungen:** Bei vielen Akkus sieht man jede einzelne Zelle – JBD/Xiaoxiang, CS-BATTERIES, JK-BMS, Daly/Bulltron,
  ANT, Eco-Worthy, Renogy, Vatrer und Power Quarry/Shentec. **Akku-Kachel lange drücken**: Fenster mit allen Werten,
  die der Akku liefert (Spannung, Strom, Leistung, Restzeit, Temperatur, Zyklen, Gesundheit, Kapazität, Alarme) und
  allen Zellen als Kästchen – die höchste orange, die niedrigste blau umrandet, dazu die Differenz (Δ).
- PIN/Passwort und Entfernen gibt es nur in den Einstellungen, nicht im Fenster der Kachel.
- Nur lesen – am Akku wird nichts geschaltet oder eingestellt. Power Stations: AC-/DC-Ausgang schaltbar.

### 💧 Simarine STRx-Tanksensoren *(Beta, ungetestet)*

Die Simarine-Tanksensoren **STR1** (analog), **STR2** (NMEA 2000) und **STR3** (RV-C) können ihren Füllstand per
Bluetooth senden („Broadcast“). Das Panel hört nur mit – keine Kopplung, **kein App-Key nötig**.

1. In der App **Simarine Connect** den Tank einrichten (Höhe, Inhalt, Tankart, Kalibrierung).
2. Je Sensor **Device Management → Broadcast Settings → Broadcast Mode** einschalten – **ab Werk ist er aus**.
3. Modus wählen:
   - **Open** – Füllstand, Inhalt und Tankart für jeden lesbar. Nichts weiter einzutragen; am einfachsten zum Ausprobieren.
   - **Low / Medium / High Security** – Füllstand verschlüsselt. Jeder Sensor hat **seinen eigenen Schlüssel**: in der
     App **Show Encryption Key** antippen und die **32 Zeichen** beim Hinzufügen bzw. später im Feld **PIN / Passwort**
     eintragen. Der Schlüssel ist nicht die 4-stellige Geräte-PIN der App. Bei Medium/High sendet der Sensor Inhalt und
     Tankart nicht mit – dann zeigt das Panel nur Prozent.
4. **Simarine-App schließen bzw. vom Sensor trennen** – solange ein Handy verbunden ist, sendet der Sensor nicht.
5. Am Panel **Akkus (BMS) → Suchen** (Editor: Reiter Akkus). Der Sensor erscheint als „Simarine STRx“. Hinzufügen –
   Kachel mit Füllstand, im Detail Inhalt in Litern, Tankart und ob der Funk offen oder verschlüsselt ist.

Fehlt der Wert, steht der Grund im Status: Broadcast aus, Schlüssel fehlt, Schlüssel passt nicht. Wird der Schlüssel in
der Simarine-App neu erzeugt, den neuen eintragen. Nicht als einzige Messung für sicherheitskritische Füllstände nutzen.

### 🌡️ Ruuvi und SwitchBot *(Beta, ungetestet)*

- **Ruuvi Tag / Ruuvi Air** und **SwitchBot Meter, Meter Plus, Outdoor Meter** senden ihre Werte offen per Bluetooth.
  Keine Kopplung, kein Passwort, die Hersteller-App darf offen bleiben, ein SwitchBot-Hub ist nicht nötig.
- Einrichten wie ein Akku: **Akkus (BMS) → Suchen** – der Sensor erscheint als „Ruuvi“ bzw. „SwitchBot Meter“.
- Kachel: Temperatur groß, Luftfeuchte darunter (Ruuvi Air: CO₂). Im Detail Luftdruck, Feinstaub und Batterie.

### ☀️ Victron

- In VictronConnect **Instant Readout** aktivieren (**Einstellungen → Produktinfo → Instant Readout**).
- Pro Gerät den Schlüssel aus **Details anzeigen** im Editor einfügen oder am Panel eintippen.
- Nur Anzeige – Schalten geht über Instant Readout nicht.

### 🔋 Victron Cerbo GX

- Am GX **Einstellungen → Dienste → MQTT** einschalten (unverschlüsselt, Port 1883).
- Am Panel unter **Einstellungen → Victron** oder im Editor die IP-Adresse des GX eintragen. Die Portal-ID
  erkennt das Panel selbst; ein Passwort ist nur nötig, wenn am GX eins für das lokale Netz gesetzt ist.
- Das Panel verbindet sich direkt mit dem GX (eigene MQTT-Verbindung, unabhängig vom Zigbee2MQTT-Broker),
  abonniert nur die benötigten Werte und hält den GX per Keepalive am Senden.
- Fünf Kacheln: **Solar** (PV-Leistung, Ladephase, Ertrag heute), **Netz** (Leistung, Spannung, Strombegrenzung),
  **Ladegerät** (Multi/Quattro oder Netzladegerät: Zustand, Schalter, Leistung), **Batterie** (SOC, Spannung,
  Leistung, Restlaufzeit) und **DC-System** (Verbraucher-Leistung).
- **MultiPlus/Quattro und Ladegeräte bedienen:** im Detailfenster (Kachel lange drücken) Netzstrom-Begrenzung und
  Schalter (Ein, Aus, Nur Laden, Nur Wechselrichter) – nur wenn der GX das erlaubt.
- **DC-DC-Lader am GX** (Orion XS, Orion Smart) und **Lichtmaschinenregler** (z. B. Wakespeed): je Gerät eine eigene
  Kachel mit Zustand, Eingang und Ausgang (Name vom GX). Per Bluetooth ohne GX gehen Orion XS/Smart sowieso.
- **Ein- und Ausgänge des GX** – nur was dort eingerichtet ist: **Relais** (schaltbar, wenn am GX die Funktion
  „Manuell“ gewählt ist), **Tanks** (Füllstand, Inhalt in Litern), **Temperatursensoren** (auch Luftfeuchte, z. B.
  Ruuvi) und **Digitaleingänge** (Tür, Bilge, Alarm, Generator). Sie erscheinen im Editor wie die Anschlüsse eines
  Satelliten namens **„GX“** und lassen sich aufs Panel ziehen.

### 🔥 Truma-Heizung

- **CP Plus:** LIN-Transceiver an die UART-Stiftleiste (Schalter auf „UART“), LIN an RJ12 Pin 3, Masse an Pin 5
  (am Combi oder per Verteiler). Danach am CP Plus einmal **PR SET → Init** ausführen.
- **iNet X:** In den Einstellungen das Protokoll einschalten, **„iNet X koppeln“** tippen und dann am Bedienteil
  **„Gerät hinzufügen“** öffnen (3 Minuten Zeit). Das iNet X merkt sich höchstens vier Geräte.

### ♨️ Alde

- LIN-Leitung an die **gelbe RJ12-Buchse** (Pin 4, Masse Pin 2), Modell und Pins unter `[alde]` eintragen.
- Das Panel fragt den Kessel alle 500 ms ab und sendet nur bei Änderungen – Einstellungen am Alde-Bedienteil
  bleiben gültig.
- Compact 3020 HE: nach bisherigem Stand lässt sich nur lesen, nicht steuern – nicht unterstützt.

### 🚐 Hymer Connect

- Für Fahrzeuge der Erwin Hymer Group mit **HYMER Connect** (Hymer, Bürstner, Dethleffs, Eriba, LMC, Carado …):
  Licht, Hauptschalter, Wasserpumpe, Batterie, Lithium-BMS, Frisch-/Abwasser, Solar, Landstrom, Außentemperatur –
  direkt per Bluetooth von der SCU, **ohne Cloud**.
- Am Panel unter **Einstellungen → Gas/Heizung** „Hymer Connect“ einschalten, **Hymer koppeln** tippen und dann am
  Bedienteil im Fahrzeug **CONNECTION** drücken (3 Minuten Zeit). Am Satelliten geht das genauso in der Weboberfläche.
- Die Werte erscheinen im Editor wie die Anschlüsse eines Satelliten namens **„Hymer“** und lassen sich aufs Panel
  ziehen; Lichter und Schalter sind per Tippen schaltbar.
- Noch **nicht an einer echten SCU getestet** – Rückmeldungen sehr willkommen.

### 🖼️ Startbild

Beim Start zeigt das Panel 6 Sekunden lang ein Startbild und gibt den Speicher danach wieder frei. Standard ist das
Banner (`assets/splash.jpg`); eine eigene **`splash.jpg`** im Projektordner hat beim Bauen Vorrang. Das Bild als JPEG „baseline“ speichern (nicht „progressive“), am besten 1024×600 –
größere Bilder werden um 1/2, 1/4 oder 1/8 verkleinert.

### 🌈 WLED

- In WLED **Config → Sync Interfaces → MQTT** aktivieren, denselben Broker eintragen, Device Topic
  `wled/<name>` lassen. Das Panel findet die Geräte von selbst.
- Farbtemperatur und Effekte gehen über MQTT nicht – die stellst du weiter in der WLED-App ein.

---

## 🛰️ Satelliten

Ein Satellit ist ein günstiger **ESP32**, den du irgendwo im Fahrzeug verbaust – am
Gaskasten, unter dem Bett beim Frischwassertank, im Technikfach. Er liest Bluetooth-Geräte und eigene Sensoren und
schickt die Werte ans Panel. Das spart im Panel Arbeitsspeicher und Funkzeit (Bluetooth und WLAN teilen sich dort
eine Antenne) und überbrückt Strecken, die Bluetooth allein nicht schafft.

**Unterstützte Chips** – je Chip eine eigene Umgebung im PlatformIO-Menü:

| Umgebung | Chip / Board | Analog (ADC1) | I²C (Standard) | Hinweise |
|---|---|---|---|---|
| `satellite` | ESP32 (WROOM, esp32dev) | GPIO 32–39 | 21 / 22 | die meisten Pins, GPIO 34–39 nur Eingang |
| `satellite-c3` | ESP32-C3 (Super Mini, DevKitM-1) – [bei Amazon](https://link.amazon/B04vjKxhb) | GPIO 0–4 | 8 / 9 | klein, 13 freie Pins, Log über USB |
| `satellite-c3-oled` | ESP32-C3 mit 0,42"-OLED (ABrobot, 72 × 40) – [bei Amazon](https://link.amazon/B00lVYc7g) | GPIO 0–4 | 5 / 6 (mit dem OLED) | zeigt einen Sensorwert bzw. die Wasserwaage, siehe unten |
| `satellite-s3` | ESP32-S3 (DevKitC-1) | GPIO 1–10 | 8 / 9 | Log über die Buchse „UART“ |
| `satellite-c6` | ESP32-C6 (DevKitC-1, Super Mini) – [bei Amazon](https://link.amazon/B0gJnV7Pv) | GPIO 0–6 | 23 / 22 | Log über USB |

Alle brauchen mindestens 4 MB Flash. Das Web-IF zeigt nur die Pins, die der jeweilige Chip frei hat.

**ESP32-C3 mit OLED** (`satellite-c3-oled`): Das kleine Display zeigt beim Start abwechselnd einen Camper oder einen
Wohnwagen mit „S.O.C.“, danach **automatisch** – mit Lagesensor MPU-6050 eine **Wasserwaage in zwei Richtungen**
(Blase im Kreis, Neigung längs und quer), sonst die angeschlossenen Sensoren nacheinander (z. B. Temperatur), ohne
Sensor Name und Adresse des Satelliten. Fest einstellbar in der Weboberfläche (Karte „OLED“), ebenso Helligkeit und
Drehen. Der MPU-6050 kommt an dieselben Pins wie das OLED (SDA GPIO 5, SCL GPIO 6). Zum Schonen wird das OLED alle
20 Minuten kurz ganz schwarz und ganz weiß.

**16-fach-Relaisplatine** (Umgebung `satellite-relay16`, ESP32-WROOM-32E mit 2× 74HC595) –
[Relaisplatine bei Amazon](https://link.amazon/B08IzxSMo), [USB-TTL-Wandler bei Amazon](https://link.amazon/B02ciSu8U):

- Die Platine hat **keinen USB-Anschluss** – zum Flashen brauchst du einen **USB-TTL-Wandler** mit 3,3-V-Pegel:
  TX des Wandlers an RX der Platine, RX an TX, GND an GND. Zum Flashen **IO0 beim Einschalten auf GND** halten
  (bzw. BOOT drücken), danach loslassen.
- Sie braucht eine **eigene 5-V-Versorgung** – der Wandler reicht dafür nicht (beim Schalten der Relais bricht
  sonst die Spannung ein bzw. die USB-Verbindung ab). GND von Platine und Wandler verbinden.
- Spätere Updates gehen über die Weboberfläche des Satelliten, ohne Wandler.

**Was ein Satellit liest und schaltet**

| Art | Umfang | Hinweise |
|---|---|---|
| ⛽ Truma LevelControl, 🛢️ Mopeka, ☀️ Victron | per Bluetooth | dieselben Module wie im Panel |
| 💧 Tanksonden | bis 4 Tanks, je bis 5 Sonden | Stangen verschiedener Länge, jede mit eigener Prozentstufe |
| 🌡️ DS18B20 | bis 8 Fühler an einem GPIO | 1-Wire, Fühler per Knopf suchen, Korrektur je Fühler |
| 📈 Analog | bis 4 Eingänge (ADC1, siehe oben) | zwei Punkte zum Umrechnen von mV in den Wert |
| 📐 MPU-6050 (GY-521) | Neigung längs und quer | Nullpunkt per Knopf, Beruhigung siehe unten |
| 🔌 Schalter | bis 8 Ausgänge (Relais), 16 mit der Relaisplatine (`satellite-relay16`, 2× 74HC595) | aktiv Low wählbar, Zustand nach dem Start: aus / an / wie zuletzt |
| 🔘 Eingänge | bis 8 Kontakte oder Taster | Pull-up und Umkehren wählbar |

**Zwei Wege zum Panel** – am Satelliten einzeln einschaltbar (ab Werk ist beides aus):

- **UDP** im WLAN direkt ans Panel (Port 47811, Rundruf oder feste Panel-IP) – **ohne Server**. Werte und
  Schaltbefehle sind mit dem gemeinsamen **Satelliten-Passwort** verschlüsselt (AES-CTR). Die Einrichtung per Knopf
  (unten) schaltet UDP automatisch ein.
- **MQTT** über einen Broker, mit **Home-Assistant-Discovery** – die Werte erscheinen dann auch in Home Assistant.
  Topics: `panelsat/<id>/sensor/<punkt>`, schalten mit `panelsat/<id>/switch/<punkt>/set` (`ON` / `OFF` / `TOGGLE`).

Bluetooth dient nur noch zum **Einrichten** – als Datenweg war es zu langsam. Auch die ausgewerteten
Bluetooth-Geräte (Truma, Mopeka, Victron) sind ab Werk aus: am Satelliten nur einschalten, was er lesen soll.

Im Panel muss dafür `satellites = 1` unter `[features]` in der `platformio.ini` stehen (Standard).

### Einrichten

1. Firmware aufspielen: im PlatformIO-Menü die passende Umgebung wählen (`satellite`, `satellite-c3` …) und
   **Upload** klicken – oder `pio run -e satellite-c3 -t upload`. Der Satellit ist intern ein eigenes
   PlatformIO-Projekt im Ordner `satellite/` mit eigenem Paketordner; die Einträge im Panel-Menü reichen nur
   weiter. So installieren sich Panel und Satellit nicht gegenseitig das Framework neu.
2. Mit dem Panel verbinden – am einfachsten **per Knopf** (siehe unten), sonst über das eigene WLAN des Satelliten.
3. Im Web-IF des Satelliten die Bluetooth-Geräte und die GPIOs belegen. Das Web-IF zeigt zu jedem Pin, was er kann,
   warnt bei Startpins, Boot-Taste und LED und bei doppelter Belegung.

#### 🔘 Per Knopf mit dem Panel verbinden (empfohlen)

So kommt ein neuer Satellit ohne Tippen von WLAN-Daten ins Netz – ähnlich wie bei Shelly oder Hue:

1. Am Panel unter **Einstellungen → Geräte → Satelliten** auf **„Neuen Satelliten hinzufügen“** tippen (im Editor:
   Reiter **Satelliten → „Neuen Satelliten suchen“**). Das Panel sucht dann 3 Minuten lang per Bluetooth.
2. Am Satelliten die **BOOT-Taste 3 Sekunden halten** (ESP32/S3: GPIO 0, C3/C6: GPIO 9). Hat er keine Taste:
   **3× schnell hintereinander Strom aus/an**. Er ist dann **3 Minuten** lang per Bluetooth zum Einrichten sichtbar.
3. Er erscheint als **„Satellite-…“** – antippen bzw. **„Einrichten“**.
4. Das Panel verbindet sich per Bluetooth, handelt mit dem Satelliten einen geheimen Schlüssel aus (ECDH P-256) und
   überträgt **verschlüsselt** (AES-256):
   - den **WLAN-Zugang** – passend zur Betriebsart des Panels:

     | Panel-Betriebsart | Der Satellit bekommt |
     |---|---|
     | Camp-WLAN (Panel ist Teilnehmer) | Zugang zu **diesem** Netz |
     | Router (Panel spannt das Netz auf) | Zugang zum **Panel-WLAN** |
     | Kombi | das **Panel-WLAN** (bleibt gleich, auch auf einem anderen Campingplatz) |
     | WLAN aus | **keine Einrichtung** – Satelliten brauchen ein WLAN |

   - das **Satelliten-Passwort** des Panels, damit beide sofort zusammenpassen,
   - den **MQTT-Broker** des Panels (Adresse, Port, Benutzer, Passwort) – nur wenn das Panel einen nutzt. Der
     Satellit sendet dann zusätzlich per MQTT (auch an Home Assistant).
5. Der Satellit verbindet sich, **meldet seine IP-Adresse** zurück (das Panel zeigt sie an), startet neu und spricht
   das Panel ab dann per **UDP** an (mit Broker zusätzlich per MQTT).

Die Daten gehen nur nach dem Tastendruck am Gerät und nur verschlüsselt über die Luft – aus der Ferne lässt sich
einem Satelliten so kein fremdes WLAN unterschieben. Ändert sich am Panel die Betriebsart oder das Netz: am
Satelliten einfach erneut BOOT halten und neu einrichten. Abschaltbar beim Bauen mit `-DSAT_PAIR=0` (spart Flash).

#### Über das eigene WLAN des Satelliten

Ohne WLAN-Zugang öffnet der Satellit das WLAN **„SOC-Satellite-…“** (Passwort `panelsat`). Verbinden – die Seite
öffnet sich meist von selbst, sonst **http://4.3.2.1/**. Beim ersten Öffnen ein eigenes Zugangspasswort vergeben, dann
WLAN, Weg zum Panel (UDP) und das Satelliten-Passwort einstellen (dasselbe wie am Panel). Das eigene WLAN des
Satelliten öffnet sich auch jederzeit mit 3× schnell aus/an.

Am Panel nimmt **Antippen** eines Satelliten in der Liste ihn auch wieder heraus (**Entfernen**), im Editor heißt das
**Vergessen**.

### 💧 Tanksonden

Mehrere Edelstahl-Stangen verschiedener Länge im Tank plus eine gemeinsame Masse-Stange. Steht Wasser an einer Sonde,
leitet es zur Masse; angezeigt wird die höchste Stufe mit Kontakt (z. B. 20 / 40 / 60 / 80 / 100 %).

- **Je Sonde ein GPIO** – am einfachsten. Die Masse-Stange kommt an GND oder an einen eigenen GPIO.
  Für Module mit Transistor gibt es „Kontakt = High“.
- **Widerstandsleiter an einem Analog-Pin** – alle Sonden über Widerstände an einem Pin. Im Web-IF trägst du je Stufe
  die gemessene Spannung ein, der Satellit wählt die nächstliegende.
- **Gegen Korrosion** bekommen die Sonden nur beim Messen kurz Strom (eigenes Intervall, Standard 60 s) und sind
  sonst stromlos. Jede Sonde wird dreimal gelesen, es zählt die Mehrheit.
- Ist eine höhere Sonde nass und eine niedrigere trocken, meldet der Satellit den Wert als **unplausibel** – meist
  eine verschmutzte Stange oder ein Kabelfehler.

### 📐 Neigung beruhigen (MPU-6050)

Im Camper wackelt immer etwas: Jemand läuft herum, der Wind drückt. Damit der Wert nicht springt, glättet der
Satellit in mehreren Stufen – alles im Web-IF einstellbar:

| Stufe | Standard | Wirkung |
|---|---|---|
| Tiefpass im Sensor | 5 Hz | filtert Vibrationen schon im MPU-6050 |
| Median | letzte 5 Werte | einzelne Ausreißer fallen weg |
| Zeitfilter | 2 s | gleitender Mittelwert (0 = aus) |
| Totband | 0,2° | kleinere Änderungen werden nicht gemeldet |
| Nur bei Stillstand | ab 3 °/s | dreht sich der Sensor (Gyro), bleibt der letzte Wert stehen, bis 1 s Ruhe ist |

Den **Nullpunkt** setzt du per Knopf, wenn das Fahrzeug gerade steht. Achsen tauschen und umkehren geht ebenfalls,
falls die Platine anders herum eingebaut ist.

---

## 🏠 Home Assistant

Das Panel braucht kein Home Assistant – arbeitet aber gut mit ihm zusammen. Wer HA nutzt, kann das Panel auch als
**Bedienteil für Home Assistant** einsetzen.

| Richtung | Was | Stand |
|---|---|---|
| **Satelliten → HA** | Alle Satelliten melden sich per MQTT-Discovery selbst an: Tanksonden, Temperaturen, Relais (schaltbar), Eingänge, Wasserwaage, Zusatzsensoren und ihre Bluetooth-Sensoren | ✅ funktioniert |
| **Zigbee2MQTT → HA** | wie gewohnt über Zigbee2MQTT selbst | ✅ funktioniert |
| **Panel → HA** | Das Panel meldet seine **lokalen** Geräte per MQTT-Discovery: Gasflaschen (Truma LevelControl, Mopeka), Victron per Bluetooth, Akkus und Power Stations, Zigbee-Geräte am H2-Funkmodul, Heizung (iNet X, CP Plus, Alde), Hymer, Regeln (an/aus) | 🚧 in Arbeit |
| **HA → Panel** | Das Panel als HA-Bedienteil: Lampen, Schalter, Szenen, Skripte und Sensoren aus Home Assistant als Kacheln – über die HA-API mit Zugangstoken, auch in den Regeln nutzbar | 🚧 in Arbeit |

### 🔁 Kein Daten-Kreislauf

Damit kein Wert im Kreis läuft (Panel → HA → Panel → HA …), gelten feste Regeln:

- Das Panel meldet an HA **nur, was direkt am Panel hängt** (Bluetooth, H2-Funkmodul, Heizungsbus, Regeln).
- Was das Panel **schon per MQTT bekommt**, meldet es **nicht** weiter: Zigbee2MQTT-Geräte, Cerbo GX, Satelliten und
  Zusatzdisplays (die melden sich selbst bzw. haben eigene HA-Integrationen).
- Werte, die das Panel **aus Home Assistant** holt, gehen **nie** zurück an HA.
- Topics unter `homeassistant/` liest das Panel nie als Eingang.
- In der HA-Auswahl im Editor sind Geräte ausgeblendet, die vom Panel selbst oder von seinen Satelliten stammen –
  die gibt es auf dem Panel ja schon direkt.

---

## 📟 Victron-Display (eigene kleine Firmware)

Für alle, die nur **Victron und den Gasvorrat** sehen wollen, gibt es eine schlanke Firmware für das 2,8"-Display
ESP32-2432S028R („Cheap Yellow Display“, rund 15 €): Cerbo GX per WLAN, Victron-Geräte, Truma LevelControl, Mopeka,
Akkus mit Bluetooth-BMS und Power Stations, dazu als Beta SuperSense, Simarine STRx, Ruuvi und SwitchBot – als Kacheln, ohne großes
Panel. **Deutsch oder Englisch.** Dieselbe Firmware gibt es auch für das Waveshare 7" mit Einstellungen am Bildschirm.

- **⚡ [Web-Flasher](https://cyclemat.github.io/simply-open-camper-victron-beta/)** – im Browser (Chrome/Edge) aufspielen,
  Sprache dort wählen
- Code, Anleitung und fertige Dateien: [simply-open-camper-victron-beta](https://github.com/cyclemat/simply-open-camper-victron-beta)
- Hier im Projekt: Umgebungen `satellite-cyd-victron` (Deutsch) und `satellite-cyd-victron-en` (Englisch)

## ⚠ Bekannte Einschränkungen

| Problem | Stellschraube |
|---|---|
| Bild flackert/driftet | `LCD_PCLK_HZ` in `src/app_config.h` auf 21 oder 18 MHz senken |
| Touch gespiegelt/gedreht | `TOUCH_SWAP_XY`, `TOUCH_INVERT_X/Y` in `src/app_config.h` |
| Bounce-Buffer-Artefakte | `LCD_BOUNCE_BUFFER_PX` auf 0 setzen |
| Kein MQTT im Router-Modus | Am Nano „Allow External“ an? Läuft der Broker? Status steht unter **Info** |
| SD-Karte wird nicht erkannt | FAT32/exFAT formatieren, Panel neu starten; notfalls `SD_FREQ_KHZ` senken |
| Panel startet bei großen Zigbee-Netzen neu | Log über „UART“ ansehen; „Interner RAM knapp“ zeigt, wo es eng wird |
| Kein Log am PC | Kabel an die Buchse **„UART“**, nicht an „USB“ |
| Umlaute fehlen | `python3 tools/gen_fonts.py` erneut ausführen |
| `IncompatiblePlatform … 6.2.0-post.1` | Im PlatformIO-Python `pioarduino==6.2.0` installieren (pioarduino-Issue #563) |
| `whitespace character in project paths` | Projekt in einen Pfad ohne Leerzeichen legen |

- Die Platine meldet nicht, ob USB-Strom anliegt – die Anzeige „USB / Akku“ ist eine Schätzung aus dem
  Spannungsverlauf. Einen Uhrenchip hat die 7B nicht; ohne Internet geht die Zeit bei Stromausfall verloren.
- Die UART-Stiftleiste der 7B teilt sich die Schnittstelle mit RS485 (Umschalter auf der Platine).

---

## 🔌 REST-API

Vom Editor genutzt, auch für eigene Skripte:

| Endpunkt | Zweck |
|---|---|
| `GET /api/info`, `GET /api/devices` | Zustand des Panels, Geräteliste |
| `GET\|POST /api/layout` | Layout (`?force=1` bei Konflikt) |
| `POST /api/rename` | `{ieee, name}` in Zigbee2MQTT umbenennen |
| `GET\|POST /api/bg?page=<id>&fmt=jpg` | Hintergrund (JPEG 1024 × 600, `&delete=1`) |
| `GET\|POST /api/backup` | `{"create":true}`, `{"restore":"<name>"}`, `{"delete":"<name>"}` |
| `GET\|POST /api/settings` | Einstellungen, inkl. `{"ble":{"truma","mopeka","victron","inetx"}}` |
| `POST /api/wifi`, `/api/time`, `/api/refresh`, `/api/reboot`, `/api/saver` | WLAN, Uhrzeit, Neu laden, Neustart, Ruhezustand |
| `POST /api/ota` | Firmware-Update (firmware.bin) |
| `GET\|POST /api/saver_logo?fmt=bin\|png` | Eigenes Logo (`&delete=1`) |
| `GET\|POST /api/truma` | `{"scan":true}`, `{"measure":"<mac>"}`, `{"add":{mac,type,name}}`, `{"remove"}`, `{"rename"}`, `{"interval"}` |
| `GET\|POST /api/mopeka` | `{"scan":true}`, `{"add":{mac,name,tank}}`, `{"remove"}`, `{"rename"}`, `{"tank":{mac,tank,empty,full,capacity}}` |
| `GET\|POST /api/victron` | `{"scan":true}`, `{"add":{mac,key,name}}`, `{"key":{mac,key}}`, `{"remove"}`, `{"rename"}` |
| `GET\|POST /api/heat` | `{"src":"cpplus"\|"inetx"\|"alde", "heat":{on,target,mode}, "water", "energy":{e,power}, "clock", "pair", "forget"}` |

---

## 🧩 Protokoll & verwandte Projekte

Für die Geräte-Protokolle dienten diese offenen Projekte als Vorlage. Übernommen wurde jeweils die
**Beschreibung des Protokolls** (Byte-Aufbau, Konstanten, Abläufe) – der Code hier ist eine eigene Umsetzung in C++.
Ausnahmen stehen in `src/third_party/` (fremder Code bzw. Daten, unverändert mit Lizenzhinweis) und sind unten
gekennzeichnet. Vielen Dank an die Autorinnen und Autoren!

| Modul (Datei) | Vorlage | Lizenz der Vorlage | Genutzt für |
|---|---|---|---|
| Truma LevelControl (`src/truma.cpp`) | [cyclemat/HAOS-Truma-Levelcontrol-Integration](https://github.com/cyclemat/HAOS-Truma-Levelcontrol-Integration) | GPL-3.0 | Passwort/RC4-Schlüssel, Mess-Ablauf |
| Mopeka (`src/mopeka.cpp`) | [esphome/esphome](https://github.com/esphome/esphome) – `mopeka_pro_check`, `mopeka_std_check` | GPL-3.0 (C++) / MIT (Python) | Paketaufbau Pro/Standard, Temperaturkorrektur, Echo-Auswertung, Flaschenmaße |
| | [Bluetooth-Devices/mopeka-iot-ble](https://github.com/Bluetooth-Devices/mopeka-iot-ble) | MIT | Modellliste, Medien-Koeffizienten (Luft, Wasser) |
| Truma CP Plus (`src/truma_cpplus.cpp`) | [danielfett/inetbox.py](https://github.com/danielfett/inetbox.py) | GPL-3.0 | LIN-Rahmen, Diagnosedienste, Statuspuffer, Kodierung |
| | [Fabian-Schmidt/esphome-truma_inetbox](https://github.com/Fabian-Schmidt/esphome-truma_inetbox) | GPL-3.0 | Anmeldung, Puffer-Layouts, Timing auf dem ESP32 |
| | [muccc/WomoLIN](https://github.com/muccc/WomoLIN) | siehe Projekt | frühe Entschlüsselung des Truma-LIN-Protokolls (über die beiden obigen) |
| Truma iNet X (`src/truma_inetx.cpp`) | [rpodgorny/hass-truma-inetx](https://github.com/rpodgorny/hass-truma-inetx) | GPL-3.0; die enthaltenen Protokolldateien stammen aus [daaaaan/truma-inetx-ble](https://github.com/daaaaan/truma-inetx-ble) **ohne Lizenzangabe** | GATT-Merkmale, Übertragungsablauf, Rahmen, Themen/Parameter |
| Alde (`src/alde.cpp`) | [daviddootson/alde_3030_plus](https://github.com/daviddootson/alde_3030_plus) | MIT | Bus-Aufbau, Steuer- und Infoblock, roter Bus (Glykol/Warmwasser) |
| | [holdestmade/esphome-alde-3020](https://github.com/holdestmade/esphome-alde-3020) | MIT | 3020-Besonderheiten, Prüfsummen-Erkennung, Zeitverhalten |
| | [darenshaw/truma-inet-x-to-alde-3020](https://github.com/darenshaw/truma-inet-x-to-alde-3020), [inetbox.py Issue #28](https://github.com/danielfett/inetbox.py/issues/28) | GPL-3.0 / – | Hinweise zu Modellen und Grenzen (3020 HE) |
| Hymer Connect (`src/hymer.cpp`, `src/hymer_tls.cpp`) | [Facetti/hymer-connect-ha-ble](https://github.com/Facetti/hymer-connect-ha-ble), [BetaHydri/hymer-connect-ha](https://github.com/BetaHydri/hymer-connect-ha) | MIT | GATT-Dienste, TLS-Profil, PIA-Rahmen, Sensor-Karten; die sieben Abo-Nachrichten sind wörtlich übernommen |
| WLED (`src/wled.cpp`) | [WLED – MQTT-Schnittstelle](https://kno.wled.ge/interfaces/mqtt/) | EUPL-1.2 (WLED) | Topics und Befehle |
| Cerbo GX (`src/cerbo.cpp`) | [victronenergy/dbus-flashmq](https://github.com/victronenergy/dbus-flashmq), [dbus-API im Victron-Wiki](https://github.com/victronenergy/venus/wiki/dbus) | siehe Projekt | Topics, Keepalive, Pfade der Werte |
| Victron Smart Lithium, Smart BatteryProtect (`src/victron.cpp`) | [keshavdv/victron-ble](https://github.com/keshavdv/victron-ble) | Unlicense | Aufbau der Datensätze 0x05 und 0x09 |
| Akkus mit BMS (`src/bms.cpp`) | [patman15/aiobmsble](https://github.com/patman15/aiobmsble) | Apache-2.0 | Erkennung, Rahmen und Werte für Daly/Bulltron, JBD, JK, LiTime/Redodo, Eco-Worthy, ANT, Vatrer, Super B Epsilon V2 |
| | [mr-manuel/venus-os_dbus-serialbattery](https://github.com/mr-manuel/venus-os_dbus-serialbattery) | AGPL-3.0 | Protokolle JBD, JK-BMS (JK02), LiTime |
| | Unterlagen vom Hersteller bzw. eigene Analyse der Apps | – | Super B (ältere Akkus), Power Quarry/Shentec, CS-BATTERIES (Anmeldung), SuperSense |
| Ruuvi (`src/bms.cpp`) | [ruuvi/ruuvi-sensor-protocols](https://github.com/ruuvi/ruuvi-sensor-protocols) | siehe Projekt | Datenformate 3, 5 und 6 (Werbepakete) |
| Simarine STRx (`src/bms.cpp`) | [Simarine/strx-ble-broadcast-protocol](https://github.com/Simarine/strx-ble-broadcast-protocol) | siehe Projekt | Werbepakete, Metadaten, AES-CCM-Verschlüsselung |
| SwitchBot (`src/bms.cpp`) | [OpenWonderLabs/SwitchBotAPI-BLE](https://github.com/OpenWonderLabs/SwitchBotAPI-BLE) | siehe Projekt | Werbepakete der Thermo-Hygrometer |
| Renogy (`src/bms.cpp`) | [cyrils/renogy-bt](https://github.com/cyrils/renogy-bt) | GPL-3.0 | Modbus-Register Laderegler, DC-DC-Lader, Akku, Wechselrichter |
| | [IAmTheMitchell/renogy-ble](https://github.com/IAmTheMitchell/renogy-ble) | Apache-2.0 | Geräteerkennung, Smart Shunt 300, Akku Pro |
| Power Stations (`src/bms_ps.inc`) | [Patrick762/bluetti-bt-lib](https://github.com/Patrick762/bluetti-bt-lib), [warhammerkid/bluetti_mqtt](https://github.com/warhammerkid/bluetti_mqtt) | MIT | Bluetti: Modbus-Register, Verschlüsselung neuerer Modelle |
| | [madninjaskillz/allpowers-ble](https://github.com/madninjaskillz/allpowers-ble) | MIT | Allpowers |
| | [flip-dots/SolixBLE](https://github.com/flip-dots/SolixBLE) | MIT | Anker SOLIX: Anmeldung, Pakete, Feldzuordnung je Modell |
| | [rabits/ha-ef-ble](https://github.com/rabits/ha-ef-ble) | Apache-2.0 | EcoFlow: Anmeldung, Verschlüsselung, Datenstrukturen; **Schlüsseltabelle übernommen** (`src/third_party/ecoflow/ef_keydata.h`) |
| Rundes Panel (`src/board_crowpanel146.cpp`) | [Elecrow-RD/CrowPanel-1.46inch-HMI-ESP32-Rotary-Display](https://github.com/Elecrow-RD/CrowPanel-1.46inch-HMI-ESP32-Rotary-Display) | keine Lizenzangabe | Pinbelegung, Startbefehle des Displays |

Für die Projektlizenz siehe [Lizenz](#lizenz). Die Lizenzen der Vorlagen betreffen deren Code, nicht die
eigene Umsetzung hier. Beim iNet X ist die Herkunft
der ursprünglichen Protokoll-Analyse ohne Lizenz. Das ist keine Rechtsberatung.

### 📚 Verwendete Bibliotheken, Schriften und Symbole

| Name | Quelle | Lizenz | Wofür |
|---|---|---|---|
| LVGL 8.4 | [lvgl/lvgl](https://github.com/lvgl/lvgl) | MIT | Oberfläche aller Panels und Displays |
| ArduinoJson 7 | [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson) | MIT | JSON (MQTT, Web-API, Einstellungen) |
| NimBLE-Arduino | [h2zero/NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | Apache-2.0 | Bluetooth (Waveshare, rundes Panel, Satelliten, Victron-Display) |
| esp-nimble-cpp | [h2zero/esp-nimble-cpp](https://github.com/h2zero/esp-nimble-cpp) | Apache-2.0 | Bluetooth am P4 (über den Funkchip C6) |
| LovyanGFX | [lovyan03/LovyanGFX](https://github.com/lovyan03/LovyanGFX) | FreeBSD (BSD-2-Clause) | Display-Treiber rundes Panel und CYD |
| micro-ecc | [kmackay/micro-ecc](https://github.com/kmackay/micro-ecc) | BSD-2-Clause | **eingebunden** in `src/third_party/`: EcoFlow-Schlüsseltausch, Bluetooth-Kopplung am P4 |
| Arduino-ESP32 | [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32) | LGPL-2.1 | Framework |
| ESP-IDF | [espressif/esp-idf](https://github.com/espressif/esp-idf) | Apache-2.0 | Framework, u. a. MQTT, HTTP-Server, NimBLE-Stack |
| ESP-Hosted | [espressif/esp-hosted-mcu](https://github.com/espressif/esp-hosted-mcu) | siehe Projekt | WLAN/Bluetooth über den C6 (P4) |
| pioarduino | [pioarduino/platform-espressif32](https://github.com/pioarduino/platform-espressif32) | Apache-2.0 | PlatformIO-Plattform |
| Inter | [rsms/inter](https://github.com/rsms/inter) | SIL OFL 1.1 | Schrift (in `src/fonts/` umgewandelt) |
| Phosphor Icons | [phosphor-icons/core](https://github.com/phosphor-icons/core) | MIT | Symbole (über react-icons, `tools/gen_icons.js`) |

---

## 📝 Änderungen

Was sich von Version zu Version geändert hat: **[CHANGELOG.md](CHANGELOG.md)**

## 🛠 Feedback & Beiträge

- Fehler und Ideen bitte als Issue melden – gern mit Log von der Buchse „UART“
- Pull Requests sind willkommen
- Mopeka, CP Plus, iNet X, Alde, WLED, Cerbo GX, Akkus, Power Stations, SuperSense, Simarine, Ruuvi oder SwitchBot getestet?
  Bitte Bescheid geben – auch wenn es nicht klappt!

---

## ❤ Support & Spenden

Das Projekt entsteht in der Freizeit. Wenn es dir hilft, freue ich mich über einen Kaffee:

**[☕ PayPal – CycleMat unterstützen](https://paypal.me/cyclemat)**

Kein Druck, keine Werbung, keine Bezahlschranke – einfach Open Source mit Leidenschaft ❤

---

<a id="lizenz"></a>

## 📜 Lizenz

**[PolyForm Noncommercial 1.0.0](LICENSE)** – Copyright © 2026 cyclemat.

- ✅ **Erlaubt:** privat nutzen, ändern, weitergeben – auch für Vereine, Forschung und Bildung, jeweils ohne
  kommerziellen Zweck.
- ❌ **Nicht erlaubt ohne gesonderte Lizenz:** die Firmware oder Geräte mit dieser Firmware verkaufen, gewerblich
  einbauen oder als Dienstleistung anbieten. **Kommerzielle Lizenzen vergibt nur der Urheber** – bei Interesse bitte
  ein Issue eröffnen.
- Teile in `src/third_party/` stehen unter eigenen, freizügigen Lizenzen (siehe [dort](src/third_party/README.md)).
- Beiträge: siehe [CONTRIBUTING.md](CONTRIBUTING.md).
- Versionen, die früher unter GPL-3.0 weitergegeben wurden, bleiben für deren Empfänger GPL-3.0.

---

## 📄 Markenhinweis

**Privates, von der Community getragenes Hobbyprojekt.**

- Nicht verbunden mit Truma, Alde, Mopeka, Victron Energy, WLED, Shelly, Waveshare, Zigbee2MQTT, Ruuvi, SwitchBot,
  CS-BATTERIES, SuperSense (comworks), Simarine oder den genannten Akku- und Power-Station-Herstellern.
- „Truma“, „LevelControl“, „iNet“, „Alde“, „Mopeka“, „Victron“, „WLED“, „Shelly“, „Ruuvi“, „SwitchBot“, „CS-BATTERIES“,
  „SuperSense“ und „Simarine“ sind Marken bzw. Namen ihrer
  Inhaber und werden nur zur Beschreibung der Kompatibilität genannt.
- Das Projekt stellt ausschließlich die Verbindung zu deinen eigenen Geräten her.
- Nutzung auf eigene Gefahr – insbesondere beim Eingriff in Heizungs- und Gasanlagen.

---

<p align="center"><strong>CycleMat · Dein Licht. Deine Heizung. Dein Panel.</strong></p>
