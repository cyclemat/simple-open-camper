# Changelog – Simple Open Camper

Alle Firmwares (Panels, Satelliten, Victron-Display, Zigbee-Modul) tragen dieselbe Versionsnummer
(`src/soc_version.h`). Neueste Version oben. *Beta* = eingebaut, aber noch nicht an echter Hardware getestet.

## In Arbeit (nächste Version)

**Neu**
- 🔋 **Akku-Fenster** (Kachel lange drücken): nur die Werte, die der Akku wirklich liefert – Spannung, Strom,
  Leistung, Restzeit, Temperatur, Zyklen, Gesundheit, Kapazität (Rest/Nenn), Zellen min–max mit Δ, Alarmcode,
  Zustand – und alle **Zellen als Kästchen**, höchste orange, niedrigste blau umrandet.
- 🔋 **Zellspannungen** jetzt auch bei JK-BMS, ANT, Eco-Worthy, Renogy-Akku und Vatrer.
- Rundes Panel: je Zelle eine Datenseite. CYD: Akku-Übersicht scrollbar, mit allen Zellen.
- 🔥 **Dieselheizung** *(Beta)* – Vevor/Hcalory (AA55/AA66, auch verschlüsselt), HeaterCC/AirHeater (ABBA):
  Zustand, Stufe/Soll, Innen-/Brennkammertemperatur, Spannung, Fehler; ein/aus, Stufe ±, Temperatur ±.
- 🧊 **Kühlbox** *(Beta)* – Alpicool, IceCube/Plug-in Festivals, Bodega, Brass Monkey: Ist/Soll je Zone, Spannung;
  ein/aus, Soll ±.
- 📡 **BTHome** *(Beta)* – Shelly BLU, Xiaomi mit pvvx-Firmware u. v. a.: Temperatur/Feuchte bzw. Zustand
  (Tür, Bewegung, Nässe, Taster), auch verschlüsselt.
- ⚖️ **Senso4s** *(Beta)* – Gasflaschen-Waage Basic/Plus per Bluetooth (wie Mopeka): Füllstand in % und kg,
  Hinweise (Nullabgleich, Einstellfehler, Batterien leer, Warnungen beim Plus).

**Geändert**
- Im Kachel-Fenster eines Akkus gibt es keine Knöpfe „PIN“ und „Entfernen“ mehr – das geht nur noch über die
  Einstellungen.

## 0.2.3 – 2026-11-19

**Neu**
- 💧 **Simarine STRx-Tanksensoren** *(Beta)* – STR1/STR2/STR3 per Bluetooth-Broadcast, offen oder verschlüsselt
  (AES-CCM) mit dem Schlüssel des Sensors aus der Simarine-App; Füllstand, Inhalt in Litern, Tankart.

**Verbessert**
- PIN-/Passwort-Felder nehmen bis zu 32 Zeichen an (Simarine-Schlüssel).
- GitHub-Build legt die Firmware auch beim Start von Hand nach `release/`.

## 0.2.1 – 2026-11-19

**Neu**
- 🌡️ **Klima-Sensoren** *(Beta)* – **Ruuvi** Tag/Air (Temperatur, Feuchte, Luftdruck, CO₂, Feinstaub, Batterie) und
  **SwitchBot** Meter/Meter Plus/Outdoor Meter (Temperatur, Feuchte, Batterie). Nur Mithören, keine Kopplung.
- 🔋 **CS-BATTERIES** *(Beta)* – JBD-BMS mit Anmeldung: App-Key automatisch, eigenes Passwort im Feld „PIN / Passwort“.
- 🔋 **JBD-Akkus**: Zellspannungen, weitere Dienstvariante FFE0.
- 🐝 **Zigbee direkt** (P4 mit ESP32-H2): Lampen, Steckdosen (mit Leistung, Spannung, Strom, Verbrauch) und Schalter
  als Kacheln; neue Geräte bekommen automatisch eine Kachel. Taster und Fernbedienungen, eigene Gerätenamen.
- 🧩 **Regeln** mit grafischen Bausteinen (Blockly) im Editor, mit Vorlagen (Bewegungslicht, Licht per Taster, Dimmen).
- 📟 **Victron-Display**: Mopeka, SuperSense-Tankmodul *(Beta)*; Edition für das **Waveshare 7"** mit Einstellungen am
  Bildschirm, Shelly und eingebautem MQTT-Broker.
- 🔧 Serielle Befehle `zb refresh`, `zb trace`, `zb diag` zur Fehlersuche am Zigbee-Modul.

**Verbessert**
- Schalten mit festem An/Aus statt Umschalten – Kachel und Lampe laufen nicht mehr auseinander.
- MQTT-Befehle über eigene Sendeaufgabe – kein Hängen der Oberfläche bei schlechtem WLAN.
- Nach Neustart oder Stromausfall fragt das Panel den Zustand der Zigbee-Geräte von selbst neu ab.
- PIN-/Passwort-Felder nehmen auch Buchstaben an.
- Kachel zeigt bei Geräten am Zigbee-Modul „nicht verfügbar“ statt „nicht in Zigbee2MQTT“.

**Behoben**
- Regel-Vorlage „Bewegungslicht“ ließ sich nicht speichern („Ungültiges JSON“).
- Bewegungslicht flackerte (Melder meldet mehrmals je Sekunde).
- Steckdosen mit unvollständigem Treiber lieferten keine Werte.
- Zigbee-Treiberliste: Generator brach an einem Gerät ab.

## 0.2.0 – 2026-11-16

**Neu**
- Satelliten per Knopf einrichten (Bluetooth, verschlüsselt), Zusatzdisplays, eingebauter MQTT-Broker (P4).
- Rundes Panel CrowPanel 1,46" mit Drehknopf und LED-Ring.
- Akkus mit Bluetooth-BMS und Power Stations, Kopplungs-PIN.
- Victron-Display auf Englisch, Web-Flasher.

## Frühere Versionen (0.0.x – 0.1.x)

Panel mit Zigbee2MQTT, Shelly, WLED, Truma LevelControl, Mopeka, Victron (Bluetooth und Cerbo GX), Truma-Heizung,
Alde, Satelliten, Victron-Display für das CYD.
