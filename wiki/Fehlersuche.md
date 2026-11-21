# Fehlersuche und Wartung

### 🛠 Wartung und Diagnose

- **Debug-Log (Fehlersuche):** in der Weboberfläche einschalten (Panel: Editor → Einstellungen → Info; Satelliten,
  Victron-Display, Zusatzdisplays: Karte „Fehlersuche“), **ab Werk aus**. Schreibt alle Meldungen der Firmware mit
  Uhrzeit mit, dazu alle 60 s Speicher, WLAN und Verbindungen (MQTT, Cerbo …) und beim Start den Grund des letzten
  Neustarts (Absturz, Watchdog, Spannungseinbruch). Mit SD-Karte (P4, Waveshare, CYD mit Kartenslot) als Dateien
  `log-NNNNN.txt` (je bis 1 MB, die letzten 10; Panel: `panel/logs`, CYD: `logs`), sonst die letzten 8–64 KB im
  Arbeitsspeicher. Ansehen, herunterladen und löschen direkt im Browser. Ausgeschaltet belegt er keinen Speicher.
- `tools/embed_web.py` packt bei jedem Build den Editor gzip-komprimiert in die Firmware.
- `tools/upload_wait.py` wartet vor dem Hochladen auf Enter (`PANEL_UPLOAD_NOWAIT=1` schaltet es ab).
- `pio run -e check-all` baut zur Probe eine Variante mit **allen** Modulen (nicht zum Hochladen gedacht).
- Nur nötig, wenn du Schriften, Icons oder den Editor änderst:

```sh
node tools/gen_icons.js        # Icons (react-icons + sharp nötig)
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
