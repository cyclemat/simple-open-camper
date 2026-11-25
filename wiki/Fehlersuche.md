# Fehlersuche und Wartung

### 🛠 Wartung und Diagnose

- **Debug-Log (Fehlersuche):** in der Weboberfläche einschalten (Panel: Editor → Einstellungen → Info; Satelliten,
  Victron-Display, Zusatzdisplays: Karte „Fehlersuche“), **ab Werk aus**. Schreibt alle Meldungen der Firmware mit
  Uhrzeit mit, dazu alle 60 s Speicher, WLAN und Verbindungen (MQTT, Cerbo …) und beim Start den Grund des letzten
  Neustarts (Absturz, Watchdog, Spannungseinbruch). Mit SD-Karte (P4, Waveshare, CYD mit Kartenslot) als Dateien
  `log-NNNNN.txt` (je bis 1 MB, die letzten 10; Panel: `panel/logs`, CYD: `logs`), sonst die letzten 8–64 KB im
  Arbeitsspeicher. Ansehen, herunterladen und löschen direkt im Browser. Ausgeschaltet belegt er keinen Speicher.
- **Funkmodul (P4):** Der Funkchip ESP32-C6 (WLAN/Bluetooth, ESP-Hosted) hat eine eigene Firmware. Passt sie nicht zur
  Panel-Firmware, zeigen Editor → Einstellungen → Info und das Panel unter Info den Stand und den Knopf **Funkmodul
  aktualisieren**: Das Panel lädt die passende Datei aus dem Internet und schreibt sie selbst (etwa eine Minute), danach
  Neustart. Ohne Internet: Datei als `c6.bin` auf die SD-Karte, Knopf „Aus SD-Datei“. Keine Automatik. Der Online-Flasher
  kann den Funkchip nicht erreichen (er hängt hinter dem P4 am SDIO-Bus).
- `tools/embed_web.py` packt bei jedem Build den Editor gzip-komprimiert in die Firmware.
- `tools/upload_wait.py` wartet vor dem Hochladen auf Enter (`PANEL_UPLOAD_NOWAIT=1` schaltet es ab).
- `pio run -e check-all` baut zur Probe eine Variante mit **allen** Modulen (nicht zum Hochladen gedacht).
- **32 statt 16 Netzwerk-Sockets (P4 und rundes Panel)**: Die veröffentlichten Firmware-Dateien kommen seit 0.2.8 aus den
  Umgebungen `crowpanel-p4-sock32` und `crowpanel-146-sock32` (eigene Projektordner `p4-sock32/` und
  `crowpanel146-sock32/`). Webserver, eingebauter MQTT-Broker, Tailscale, Home Assistant und UDP teilen sich den
  Socket-Vorrat; mit 16 hing der Webserver bei vielen Diensten gleichzeitig. Die Socket-Zahl steckt in den fertig
  übersetzten Framework-Bibliotheken, darum übersetzt der **erste Build** sie neu (**1–2 Stunden**, eigener Paketordner,
  die normalen Umgebungen bleiben unberührt). Beim P4 werden dabei die Versionen der Funkchip-Bausteine festgehalten
  (`tools/pin_components.py`, sonst kein WLAN). Außerdem liegen dort der Bluetooth-Host-Speicher und (P4) die
  Netzwerkpuffer im PSRAM, der Bluetooth-Host-Task hat mehr Stapel, das runde Panel erlaubt 5 statt 3 gleichzeitige
  Bluetooth-Verbindungen. Die Umgebungen `crowpanel-p4` und `crowpanel-146` (16 Sockets, 3–4 Minuten Build) bleiben als
  Rückfall. Am P4 getestet, am runden Panel noch nicht *(Beta)* – macht es Probleme, gehen wir auf 16 zurück.
- **Bluetooth und WLAN teilen sich die Antenne.** Der Dauerscan (Victron, Mopeka, Ruuvi …) lauscht deshalb nur 12–15 %
  der Zeit; neue Werte kommen etwa alle 5–10 s, die Kachel zeigt das Alter. Läuft keine Suche, hört das Panel nur noch
  auf die eingerichteten Geräte (Whitelist im Controller, bis 12 Geräte) – fremde Geräte auf dem Platz belasten es dann
  nicht mehr. Nach dem Start und alle 10 Minuten (falls ein Gerät fehlt) lauscht es eine Minute offen. Während der
  Editor oder die App benutzt wird, scannt der P4 sparsam weiter (5 %), die S3-Panels pausieren den Scan. *(Beta)*
- Nur nötig, wenn du Schriften, Icons oder den Editor änderst:

```sh
python3 tools/gen_icons.py     # Icons: lädt Phosphor-SVGs, rendert mit Chrome headless (oder node tools/gen_icons.js mit react-icons + sharp)
python3 tools/gen_fonts.py     # LVGL-Schriften aus Inter + Icons
python3 tools/build_editor.py  # web/editor.src.html -> web/editor.html
```

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
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
