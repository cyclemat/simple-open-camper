# Installation und Flashen

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

**Web-Flasher (am einfachsten):** Doppelklick auf **`flasher.bat`** im Projektordner (bzw. `python tools/flasher.py`).
Das startet einen kleinen Webserver nur für diesen Rechner und öffnet **http://localhost:8765/flasher/** – mit den
Bereichen **Panels** und **Satelliten und Zubehör**. Je Gerät wählbar: Firmware aus dem **GitHub-Release** oder die
hier **lokal gebaute** (neuester Stand), und **„Nur Firmware (Update)“** (Einstellungen bleiben) oder **„Komplett (neu
einrichten)“** (alles gelöscht). Gerät per USB anschließen, Knopf drücken, Anschluss wählen – Chrome oder Edge am PC.

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

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
