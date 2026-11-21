# Konfiguration (Module, Sprachen)

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
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
