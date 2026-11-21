# Quellen und Lizenzen

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
| Tailscale (`src/tailscale.cpp`, `src/third_party/microlink/`) | [CamM2325/microlink](https://github.com/CamM2325/microlink) (enthält wireguard-lwip, BSD-3-Clause) | MIT | Tailscale-kompatibler Client, unverändert übernommen (nur Einstellungen statt Kconfig) |
| Hobby (`src/hobby.cpp`) | [AndreasKirsten79/hobby-caravan-pi](https://github.com/AndreasKirsten79/hobby-caravan-pi) (mit [paveltresnak/hobby-caravan-esphome-ble](https://github.com/paveltresnak/hobby-caravan-esphome-ble)) | keine Angabe | nur Protokollwissen: BLE-Dienst, Anmeldung, Teilstücke, Befehle |
| | [BastyJuice/Hobby_Caravan_Serial_2_CI-BUS_Connectivity](https://github.com/BastyJuice/Hobby_Caravan_Serial_2_CI-BUS_Connectivity) | MIT | CI-BUS seriell: Anmeldung, Befehle, Variablentabelle |
| WLED (`src/wled.cpp`) | [WLED – MQTT-Schnittstelle](https://kno.wled.ge/interfaces/mqtt/) | EUPL-1.2 (WLED) | Topics und Befehle |
| Cerbo GX (`src/cerbo.cpp`) | [victronenergy/dbus-flashmq](https://github.com/victronenergy/dbus-flashmq), [dbus-API im Victron-Wiki](https://github.com/victronenergy/venus/wiki/dbus) | siehe Projekt | Topics, Keepalive, Pfade der Werte |
| Victron Smart Lithium, Smart BatteryProtect (`src/victron.cpp`) | [keshavdv/victron-ble](https://github.com/keshavdv/victron-ble) | Unlicense | Aufbau der Datensätze 0x05 und 0x09 |
| Akkus mit BMS (`src/bms.cpp`) | [patman15/aiobmsble](https://github.com/patman15/aiobmsble) | Apache-2.0 | Erkennung, Rahmen und Werte für Daly/Bulltron, JBD, JK, LiTime/Redodo, Eco-Worthy, ANT, Vatrer, Super B Epsilon V2 |
| | [mr-manuel/venus-os_dbus-serialbattery](https://github.com/mr-manuel/venus-os_dbus-serialbattery) | AGPL-3.0 | Protokolle JBD, JK-BMS (JK02), LiTime |
| | Unterlagen vom Hersteller bzw. eigene Analyse der Apps | – | Super B (ältere Akkus), Power Quarry/Shentec, CS-BATTERIES (Anmeldung), SuperSense |
| Ruuvi (`src/bms.cpp`) | [ruuvi/ruuvi-sensor-protocols](https://github.com/ruuvi/ruuvi-sensor-protocols) | siehe Projekt | Datenformate 3, 5 und 6 (Werbepakete) |
| Simarine STRx (`src/bms.cpp`) | [Simarine/strx-ble-broadcast-protocol](https://github.com/Simarine/strx-ble-broadcast-protocol) | siehe Projekt | Werbepakete, Metadaten, AES-CCM-Verschlüsselung |
| Dieselheizung (`src/bms.cpp`) | [Spettacolo83/diesel-heater-ble](https://github.com/Spettacolo83/diesel-heater-ble), [homeassistant-diesel-heater](https://github.com/Spettacolo83/homeassistant-diesel-heater), [mSoftMS/AirHeater-BLE](https://github.com/mSoftMS/AirHeater-BLE) | MIT | Rahmen AA55/AA66/ABBA, XOR-Schlüssel, Befehle, Erkennung |
| Kühlbox (`src/bms.cpp`) | [klightspeed/BrassMonkeyFridgeMonitor](https://github.com/klightspeed/BrassMonkeyFridgeMonitor), [deeteeppg/bodega-bluetooth-le-hass](https://github.com/deeteeppg/bodega-bluetooth-le-hass) | MIT | Rahmen, Status, Befehle |
| | Analyse der App IceCube / Plug-in Festivals (Unterlage des Nutzers) | – | Set-Aufbau nach Modell, Bind |
| BTHome (`src/bms.cpp`) | [bthome.io](https://bthome.io), [Bluetooth-Devices/bthome-ble](https://github.com/Bluetooth-Devices/bthome-ble) | MIT | Objekttabelle, Verschlüsselung |
| Senso4s (`src/mopeka.cpp`) | [ksanislo/senso4s_ble](https://github.com/ksanislo/senso4s_ble) | Apache-2.0 | Werbepakete, Fehler- und Warncodes |
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

## 📜 Lizenz

**[PolyForm Noncommercial 1.0.0](https://github.com/cyclemat/simple-open-camper/blob/main/LICENSE)** – Copyright © 2026 cyclemat.

- ✅ **Erlaubt:** privat nutzen, ändern, weitergeben – auch für Vereine, Forschung und Bildung, jeweils ohne
  kommerziellen Zweck.
- ❌ **Nicht erlaubt ohne gesonderte Lizenz:** die Firmware oder Geräte mit dieser Firmware verkaufen, gewerblich
  einbauen oder als Dienstleistung anbieten. **Kommerzielle Lizenzen vergibt nur der Urheber** – bei Interesse bitte
  ein Issue eröffnen.
- Teile in `src/third_party/` stehen unter eigenen, freizügigen Lizenzen (siehe [dort](https://github.com/cyclemat/simple-open-camper/blob/main/src/third_party/README.md)).
- Beiträge: siehe [CONTRIBUTING.md](https://github.com/cyclemat/simple-open-camper/blob/main/CONTRIBUTING.md).
- Versionen, die früher unter GPL-3.0 weitergegeben wurden, bleiben für deren Empfänger GPL-3.0.

## 📄 Markenhinweis

**Privates, von der Community getragenes Hobbyprojekt.**

- Nicht verbunden mit Truma, Alde, Mopeka, Victron Energy, WLED, Shelly, Waveshare, Zigbee2MQTT, Ruuvi, SwitchBot,
  CS-BATTERIES, SuperSense (comworks), Simarine oder den genannten Akku- und Power-Station-Herstellern.
- „Truma“, „LevelControl“, „iNet“, „Alde“, „Mopeka“, „Victron“, „WLED“, „Shelly“, „Ruuvi“, „SwitchBot“, „CS-BATTERIES“,
  „SuperSense“ und „Simarine“ sind Marken bzw. Namen ihrer
  Inhaber und werden nur zur Beschreibung der Kompatibilität genannt.
- Das Projekt stellt ausschließlich die Verbindung zu deinen eigenen Geräten her.
- Nutzung auf eigene Gefahr – insbesondere beim Eingriff in Heizungs- und Gasanlagen.


<p align="center"><strong>CycleMat · Dein Licht. Deine Heizung. Dein Panel.</strong></p>

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
