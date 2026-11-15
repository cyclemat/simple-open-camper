<p align="center">
  <img src="assets/banner_small.jpg" alt="Simply Open Camper (S.O.C.)" width="100%">
</p>

<h1 align="center"><img src="assets/van-right.svg" height="32" alt=""> Simply Open Camper – Victron-Display <img src="assets/van-left.svg" height="32" alt=""></h1>

<h3 align="center">Victron und Gas im Camper auf einem 2,8"-Touch-Display – Cerbo GX per WLAN, Victron-Geräte und Truma LevelControl direkt per Bluetooth. Lokal, ohne Cloud.</h3>

<p align="center">
  <img src="https://img.shields.io/badge/Hardware-ESP32--2432S028R%20(CYD)-0089D0" alt="Hardware">
  <img src="https://img.shields.io/badge/Display-320%C3%97240%20Touch-blue" alt="Display">
  <img src="https://img.shields.io/badge/Victron-Cerbo%20GX%20%7C%20Instant%20Readout-005CA9" alt="Victron">
  <img src="https://img.shields.io/badge/Gas-Truma%20LevelControl-E30613" alt="Truma LevelControl">
  <img src="https://img.shields.io/badge/Cloud-keine-brightgreen" alt="Ohne Cloud">
  <img src="https://img.shields.io/badge/Lizenz-GPL--3.0-blue" alt="Lizenz GPL-3.0">
  <img src="https://img.shields.io/badge/Status-Beta-yellow" alt="Status">
</p>

> Die abgespeckte Version von [Simply Open Camper](https://github.com/cyclemat/simply-open-camper) (S.O.C.) für alle,
> die **nur Victron und den Gasvorrat** sehen und bedienen wollen – auf einem Display für rund 15 €, ohne großes Panel.

---

## 🚧 Beta

Die Firmware läuft auf dem Board. **Noch nicht an jeder Anlage getestet** sind: Cerbo GX mit vielen Geräten,
MultiPlus-Steuerung (Strombegrenzung/Schalter) und Bluetooth-Ladegeräte. Rückmeldungen und Fotos willkommen!

## ✨ Was es kann

| | |
|---|---|
| 🔋 **Cerbo GX** (optional) | Batterie, Solar (Summe und jeder Laderegler einzeln), Netz/Landstrom, Ladegerät, Wechselrichter, DC-System – per MQTT im WLAN, der GX wird automatisch gefunden |
| 🔥 **Truma LevelControl** | Füllstand der Gasflaschen (%, kg, Akku des Sensors) per Bluetooth, bis zu 4 Sensoren |
| 📶 **Victron Bluetooth** | SmartSolar, SmartShunt/BMV, Orion/DC-DC, Blue Smart/Phoenix Smart Ladegeräte – „Instant Readout“, auch ganz ohne Cerbo |
| 🎛️ **MultiPlus steuern** | Netzstrom-Begrenzung (− / +) und Schalter (Ein, Aus, Nur Laden, Nur Wechselrichter); eigenständige Ladegeräte: Begrenzung und Ein/Aus |
| 🧩 **Nur echte Werte** | Kacheln entstehen automatisch – nur für Geräte, die wirklich Werte liefern. Keine leeren Kacheln |
| 🚦 **Ampel-LED** | Die RGB-LED auf der Rückseite zeigt Batterie oder Gas: bis 10 % rot, bis 50 % orange, darüber grün |
| 👆 **Bedienung** | 4 oder 2 Kacheln pro Seite, Wischen zum Blättern, Antippen zum Bedienen, Dimmen nach Wunsch |
| 🌐 **Weboberfläche** | WLAN, Cerbo, Bluetooth-Geräte, Reihenfolge und Namen der Kacheln, Helligkeit, Firmware-Update |
| 🔒 **Sicher eingerichtet** | Einrichtungsseite öffnet sich von selbst (wie bei WLED), eigenes Zugangspasswort ist Pflicht |

## 🛒 Hardware

Nur ein Board, keine Verkabelung: **ESP32-2432S028R** („Cheap Yellow Display“, CYD) – ESP32 mit 2,8"-Display
320×240 und resistivem Touch, Micro-USB. Strom über USB (5 V) aus dem Bordnetz, z. B. mit einem USB-Einbau-Lader.

| | |
|---|---|
| Chip | ESP32-WROOM-32, 4 MB Flash, kein PSRAM |
| Display | ILI9341 320×240 an HSPI (SCLK 14, MOSI 13, MISO 12, CS 15, DC 2), Hintergrundlicht GPIO 21 |
| Touch | XPT2046 (CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36) – ein Finger (kein Multitouch) |
| RGB-LED | Rückseite, rot GPIO 4, grün GPIO 16, blau GPIO 17 (Low = an) – Ampel-Anzeige |
| Lichtsensor | LDR vorne neben dem Display, GPIO 34 – automatische Helligkeit |

## 🔑 Die zwei Passwörter

| | Wofür | Ab Werk | Ändern |
|---|---|---|---|
| **Zugangspasswort** | Anmelden an der Weboberfläche (Einstellungen ändern) | `panelsat` – **muss beim ersten Öffnen geändert werden** | Weboberfläche → „Zugangspasswort ändern“ |
| **WLAN-Passwort** | Verbinden mit dem Einrichtungs-WLAN „SOC-Victron-…“ | `panelsat`, eingeschaltet | Weboberfläche → Einrichtungs-WLAN: eigenes Passwort oder ganz ausschalten (offenes WLAN) |

Die beiden sind **unabhängig**: Wer das Zugangspasswort ändert, ändert das WLAN-Passwort nicht. Auch bei offenem
Einrichtungs-WLAN kann ohne Zugangspasswort niemand etwas einstellen.

## 🚀 Erste Einrichtung

1. **Firmware aufspielen** (siehe unten).
2. Am Handy oder Laptop mit dem WLAN **SOC-Victron-XXXXXX** verbinden – WLAN-Passwort ab Werk **`panelsat`**.
3. Die **Einstellungsseite öffnet sich von selbst** (wie bei einem Gast-WLAN). Falls nicht: im Browser
   **http://4.3.2.1/** öffnen. Meldet das Handy „kein Internet“, trotzdem verbunden bleiben.
4. **Eigenes Zugangspasswort vergeben:** Standardpasswort `panelsat` eingeben, neues Zugangspasswort
   (mindestens 8 Zeichen) zweimal eingeben. Damit meldest du dich künftig an der Weboberfläche an.
5. Optional unter **Einrichtungs-WLAN**: eigenes WLAN-Passwort setzen oder den Passwortschutz ausschalten.
6. **WLAN eintragen:** „Netze suchen“, Netz antippen, Passwort eingeben, Speichern. Das kann das eigene WLAN des
   Cerbo GX sein („venus-…“) oder das WLAN im Fahrzeug/zuhause, in dem der Cerbo hängt.
7. Danach erreichst du die Weboberfläche im WLAN unter **http://soc-victron-xxxxxx.local/** oder über die IP-Adresse:
   am Display von oben nach unten wischen (oder die Kachel „Einrichten“ antippen) – dann stehen Name, WLAN und
   Adresse groß auf dem Bildschirm.

Ohne WLAN-Verbindung öffnet das Display nach 30 Sekunden wieder sein Einrichtungs-WLAN.

### Cerbo GX verbinden (optional)

1. Am Cerbo GX: *Einstellungen → Dienste → MQTT im LAN (Klartext)* einschalten.
2. In der Weboberfläche bei **Cerbo GX**: „Cerbo GX verwenden“ anhaken und speichern.
3. „Automatisch suchen“ findet den GX unter 172.24.24.1 (eigenes WLAN des Cerbo), am Router (Gateway) oder als
   `venus.local` / `einstein.local`. Alternativ die Adresse von Hand eintragen.
4. Alles, was der GX liefert, erscheint automatisch als Kachel.

Ab Werk ist der Cerbo **aus** – wer nur Bluetooth-Geräte hat, braucht ihn nicht.

### Victron-Geräte per Bluetooth

1. In der Weboberfläche bei **Victron per Bluetooth**: „Geräte suchen“ (3 Minuten).
2. Gefundenes Gerät „Hinzufügen“ und den **Schlüssel** eintragen. Den findest du in VictronConnect:
   *Gerät → Einstellungen (Zahnrad) → ⋮ → Produktinfo → Instant Readout → Details anzeigen → Verschlüsselungsschlüssel*.
3. Sobald das erste Paket ankommt, erscheint die Kachel.

Bluetooth startet erst, wenn ein Gerät eingerichtet ist oder gesucht wird.

### Truma LevelControl (Gasflaschen)

1. **Truma-App schließen** – der Sensor spricht immer nur mit einem Gerät.
2. In der Weboberfläche bei **Truma LevelControl**: „Sensoren suchen“, dann am Sensor kurz die Taste drücken
   (LED blinkt).
3. Gefundenen Sensor „Hinzufügen“ und benennen, z. B. „Flasche links“.
4. Gemessen wird automatisch alle 10 Minuten (einstellbar 2–240) oder per „Jetzt messen“.
   Die Kachel erscheint mit der ersten Messung.

### Ampel-LED auf der Rückseite

Die farbige LED auf der Rückseite des Boards zeigt einen Füllstand auf einen Blick:

| Füllstand | LED |
|---|---|
| bis 10 % | 🔴 rot |
| 11–50 % | 🟠 orange |
| über 50 % | 🟢 grün |
| kein Messwert | aus |

In der Weboberfläche unter **Ampel-LED** wählst du, was sie zeigt: **Automatisch** (Ladezustand der Batterie,
ohne Batteriemonitor das Gas), **Batterie** (SmartShunt/BMV per Bluetooth oder Cerbo GX), **Gas** (Truma
LevelControl, die leerste Flasche) oder **Aus**.

### MultiPlus / Ladegerät bedienen

Kachel **Netz**, **Ladegerät** oder **Wechselrichter** **2 Sekunden gedrückt halten** → Fenster mit

- **Netzstrom-Begrenzung**: − / + in ganzen Ampere (wird kurz nach dem letzten Tippen an den GX geschickt),
- **Schalter**: Ein, Aus, Nur Laden, Nur Wechselrichter (beim eigenständigen Ladegerät: Ein/Aus).

Erscheint nur, wenn der GX den Wert liefert **und** als einstellbar meldet (z. B. nicht, wenn ein Digital
Multi Control die Begrenzung festlegt). Dasselbe gibt es in der Weboberfläche beim Cerbo.

## 👆 Bedienung am Display

| Geste | Wirkung |
|---|---|
| Wischen links/rechts | nächste/vorige Seite (Punkte unten zeigen die Seite) |
| Kachel 2 s gedrückt halten | MultiPlus/Ladegerät bedienen (wo möglich) |
| Von oben nach unten wischen | Name, WLAN und Adresse der Weboberfläche groß anzeigen (antippen schließt) |
| Berühren nach dem Dimmen | weckt nur auf |
| Beim Einschalten **2 s** drücken, dann loslassen | **Touch kalibrieren** (vier Pfeilspitzen antippen) |
| Beim Einschalten **10 s** drücken | **Zugangs- und WLAN-Passwort** auf `panelsat` zurücksetzen |

Welche Kacheln in welcher Reihenfolge und unter welchem Namen erscheinen, wie viele pro Seite, Helligkeit und
Dimmzeit stellst du in der Weboberfläche unter **Display** ein. Dort gibt es auch „Touch kalibrieren“.

**Helligkeit automatisch:** Der Lichtsensor vorne neben dem Display schaltet zwischen der normalen Helligkeit und
der „Helligkeit bei Dunkelheit“ um (ab Werk an). Nachts wird auch die Ampel-LED gedimmt. Unter **Display** steht der
aktuelle Messwert (größer = dunkler): Raum abdunkeln, Wert ablesen und etwas darunter als „Dunkel ab Messwert“
eintragen. Der Sensor ist auf diesen Boards eher grob – er unterscheidet hell und dunkel, mehr nicht.

## 🆘 Notfall: Einrichtungs-WLAN erzwingen

Auch ohne Touch und ohne Passwort kommst du immer wieder ran – über **schnelles Aus- und Einschalten**
(jeweils kürzer als 10 Sekunden laufen lassen, auch per Reset-Taste):

| | |
|---|---|
| **3×** hintereinander | Einrichtungs-WLAN geht sofort auf und bleibt bis zum nächsten Neustart an (WLAN-Daten bleiben) |
| **5×** hintereinander | WLAN-Verbindung gelöscht, Zugangs- und WLAN-Passwort auf Werkseinstellung (`panelsat`) – Geräte und Kacheln bleiben |

## 🔧 Firmware aufspielen

**Fertige Dateien** liegen im Ordner [`release/`](release/) dieses Repositorys – immer die neueste Fassung, die vorige
in `release/vorher/`:

- `release/victron-display-cyd.bin` – für das **erste Aufspielen per USB**, komplett (Bootloader, Partitionen,
  Firmware und der kleine Updater) an Adresse **0x0**, z. B. mit
  `esptool.py --chip esp32 write_flash 0x0 victron-display-cyd.bin` oder einem ESP-Web-Flasher,
- `release/ota/victron-display-cyd.bin` – für spätere **Updates über die Weboberfläche** (**Firmware → Hochladen**).

**Selbst bauen** mit [PlatformIO](https://platformio.org/) (VS Code):

```
pio run -t upload
```

Der erste Build lädt Framework und Werkzeuge in `.pio/packages` (eigener Paketordner) und dauert entsprechend.
Eigenes Startbild: `splash_cyd.jpg` (320×240, JPEG „baseline“) in den Projektordner legen.

## ❓ Häufige Fragen

- **Seite öffnet sich nicht von selbst?** Am Handy das WLAN „SOC-Victron-…“ einmal *vergessen* und neu
  verbinden, dann http://4.3.2.1/ im Browser öffnen. Mobile Daten kurz ausschalten hilft manchen Handys.
- **Touch trifft daneben?** Touch kalibrieren (beim Einschalten 2 s drücken oder in der Weboberfläche).
- **Kachel fehlt?** Es erscheinen nur Geräte mit echten Werten. Victron-Bluetooth: Schlüssel prüfen (Status in der
  Weboberfläche). Cerbo: MQTT im LAN am GX an? „Cerbo GX verwenden“ angehakt? LevelControl: Truma-App zu?
  „Jetzt messen“ drücken.
- **Cerbo trennt ab und zu?** Meist schwaches WLAN – näher an den GX oder dessen eigenes WLAN verwenden.

## 🔗 Teil von Simply Open Camper

Dieses Repository wird automatisch aus dem Hauptprojekt
[cyclemat/simply-open-camper](https://github.com/cyclemat/simply-open-camper) erzeugt
(`tools/export_victron.py`). Fehler und Wünsche bitte dort melden – Änderungen hier werden beim nächsten
Abgleich überschrieben.

---

<details>
<summary>🇬🇧 English (short)</summary>

Lean firmware for the ESP32-2432S028R "Cheap Yellow Display": shows all Victron values and gas levels as tiles –
from a Cerbo GX via WiFi (MQTT on LAN, auto-discovery, optional), from Victron devices via Bluetooth (Instant
Readout) and from Truma LevelControl gas sensors via Bluetooth. Tap a
tile to set the MultiPlus input current limit and mode. Setup: join WiFi `SOC-Victron-XXXXXX` (default password
`panelsat`), the setup page opens automatically (captive portal, http://4.3.2.1/), set your own access password
(web UI), then your WiFi. The setup WiFi password is separate (default `panelsat`, can be changed or switched off).
Emergency: power-cycle 3× quickly = setup WiFi on, 5× = reset WiFi and both passwords. Build with
PlatformIO: `pio run -t upload`. German UI, beta.
</details>

## 📄 Lizenz

GPL-3.0 – siehe [LICENSE](LICENSE). Victron Energy, Cerbo GX, MultiPlus, SmartSolar usw. sind Marken der
Victron Energy B.V., Truma und LevelControl Marken der Truma Gerätetechnik GmbH & Co. KG; dieses Projekt ist kein
offizielles Produkt dieser Firmen und steht in keiner Verbindung zu ihnen.
