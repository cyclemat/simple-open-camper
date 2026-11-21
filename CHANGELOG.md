# Changelog – Simple Open Camper

Alle Firmwares (Panels, Satelliten, Victron-Display, Zigbee-Modul) tragen dieselbe Versionsnummer
(`src/soc_version.h`). Neueste Version oben. *Beta* = eingebaut, aber noch nicht an echter Hardware getestet.

## In Arbeit (nächste Version)

**Neu**
- 🧩 **ESPHome, Tasmota & Co.** (MQTT-Discovery) 🧪 *Test – nur in [`panel-crowpanel-p4-HA-Addon-Test.bin`](release/test/) (P4)* – Geräte, die sich per Home-Assistant-Discovery am Broker melden,
  erscheinen auf dem Panel wie Satelliten (eine Gruppe je Gerät): Sensoren, Binärsensoren, Schalter, Lampen (ein/aus).
  Ohne Home Assistant, mit dem eingebauten Broker. Eigene Satelliten und Zigbee2MQTT werden übergangen. Einstellungen →
  MQTT, ab Werk aus. P4, rundes Panel, Waveshare. Getestet am P4 mit einem Gerät im ESPHome-Format.
- 🏠 **Home Assistant** *(Beta)* 🧪 *Test – nur in [`panel-crowpanel-p4-HA-Addon-Test.bin`](release/test/) (P4)* – Integration [simple-open-camper-HA-Integration](https://github.com/cyclemat/simple-open-camper-HA-Integration)
  (MIT, HACS): HA verbindet sich per WebSocket mit dem Panel (`/api/ha`, Zugangscode, auch über Tailscale). HA übernimmt
  nur ausgewählte Panel-Werte, das Panel zeigt nur ausgewählte HA-Entitäten (wie ein Satellit „Home Assistant“). Am Panel:
  Einstellungen → Netzwerk → Home Assistant. Testfassung für den P4 in `release/test/`.

## 0.2.6 – 2026-11-21

**Neu**
- 📱 **Live-Ansicht** unter `http://<panel>/live`: das Panel im Browser bedienen – gleiche Seiten und Kacheln, Werte alle
  1,5 s, Antippen schaltet, lange drücken = Helligkeit. Mit Zugangspasswort, am Handy „Zum Startbildschirm“ wie eine
  App, über Tailscale auch von unterwegs. Knopf „Live“ im Editor.
- ⚡ **Lokaler Web-Flasher** fürs Hauptprojekt (`flasher.bat`): Bereiche „Panels“ und „Satelliten und Zubehör“, je Gerät
  GitHub-Release oder lokal gebaut, „Nur Firmware“ (Einstellungen bleiben) oder „Komplett“.
- 🔐 **Tailscale am P4** *(Beta)* – das Panel kommt per [microlink](https://github.com/CamM2325/microlink) (MIT) ins
  Tailnet: Editor und Weboberfläche von unterwegs unter der 100.x-Adresse bzw. dem Namen (MagicDNS). Anmeldung per Link
  oder Auth-Key (bleibt im Panel), ab Werk aus – Editor: Einstellungen → Netzwerk → Tailscale. Getestet: direkt im
  WLAN, Editor über Tailnet-IP und -Namen erreichbar.
- 🔌 Debug-Log auch am Bildschirm schaltbar (Einstellungen → Info), dort auch GitHub-Adresse und „Unterstütze das Projekt“.
- 🔌 **Shelly per Bluetooth** *(Beta, ungetestet)* – Shelly Gen2/Gen3/Gen4 (Plus, Pro, Mini …) ohne WLAN und ohne Broker
  schalten und ablesen (Schalter, Licht/Dimmer, Leistung, Energie), Passwort am Shelly wird unterstützt. P4, rundes Panel
  und Waveshare; eigener Menüpunkt „Shelly (BT)“ am Panel und im Editor, ab Werk aus. Kurze Verbindungen: Stand alle
  30 s, Schalten sofort. Gen1 hat kein Bluetooth (dafür Shelly über MQTT).
- 🔐 Tailscale auch am runden Panel *(Beta)*; Tailnet-Adresse am Panel jetzt unter Einstellungen → Netzwerk.
- 🧩 Regeln (Blockly) und eingebauter MQTT-Broker jetzt auch am runden Panel – einrichten im Browser-Editor.
- 📲 **Android-App „S.O.C. Live“** *(Beta)* – das Panel aufs Handy (`release/soc-live.apk`): **Tailscale eingebaut**
  (Anmeldung im Browser, App sucht das Panel im Tailnet – ohne Tailscale-App), Kacheln genau wie am Panel (quer, ganzer
  Bildschirm), Antippen schaltet bzw. zeigt alle Werte, Passwort wird gemerkt. Alternativ direkt im WLAN.
- 📱 Live-Ansicht zeigt die Kacheln jetzt so, wie das Panel sie berechnet (`GET /api/live`), inkl. Akku, Victron, Gas,
  Heizung; Antippen einer Messwert-Kachel öffnet ein Fenster mit allen Werten.

**Behoben** (Live/App)
- Webserver: verschwundene Gegenstellen (Handy weg) belegten Verbindungen, bis keine neue mehr ging (Editor vom PC
  nicht mehr erreichbar) – jetzt TCP-Keepalive, frei nach ~45 s.

**Geändert**
- Panel → Einstellungen → Geräte: Liste nach Herkunft gruppiert (Zigbee2MQTT, Zigbee eingebaut, Shelly MQTT, Shelly
  Bluetooth, WLED) – vorher stand alles unter „Zigbee2MQTT“. Fehlt ein Gerät, heißt es nur bei Zigbee2MQTT-Geräten
  „nicht in Zigbee2MQTT“, sonst „nicht verfügbar“ (Panel und Editor).
- Zusatzdisplay (CYD): Abruf der Geräteliste beim Hauptpanel ohne die große HTTP-Bibliothek (passte sonst nicht mehr in
  den schnellen Programmspeicher). Hauptpanels müssen dafür nicht neu.
- Satellit ESP32-C6: ohne Hymer und ohne Debug-Log – Flash war voll. Hymer-Satellit mit ESP32, C3 oder S3.

**Behoben**
- Debug-Log: Neustart-Schleife, wenn er beim Start schon an war (Zustandszeile fragte Module vor deren Start ab).
- Eingebauter MQTT-Broker: kein Hinweis „MQTT-Broker eintragen“ mehr, keine Suche nach fremden Brokern im Router-Modus;
  Hinweis heißt jetzt „Kein MQTT-Broker eingerichtet“ (ohne Zigbee2MQTT).
- Hymer erscheint im Log/Status nur noch, wenn es eingeschaltet ist.
- P4: Akkumessung an GPIO 49 – Dämpfung griff nicht („Pin is not configured as analog channel“).
- 🐞 **Debug-Log auf allen Geräten** – in der Weboberfläche ein- und ausschaltbar, ab Werk aus: alle Meldungen mit
  Uhrzeit, alle 60 s Speicher/WLAN/Verbindungen, Grund des letzten Neustarts. Dateien auf der SD-Karte (P4, Waveshare,
  CYD mit Kartenslot), sonst im Arbeitsspeicher; ansehen, herunterladen und löschen im Browser.
- 🐝 **Zigbee direkt am runden Panel** *(Beta, ungetestet)* – ESP32-H2-Funkmodul an der UART-Buchse hinten (GPIO 43/44,
  UART2; 3,3-V-Regler und Pegelwandler nötig, siehe README), ab Werk aus. Einschalten, anlernen, umbenennen,
  einstellen und entfernen im Browser-Editor (Einstellungen → Zigbee) – das geht jetzt auf allen Panels auch dort.

## 0.2.5 – 2026-11-20

**Neu**
- 🔋 **SmartShunt-Satellit** *(Beta)* – ESP32-C3 Super Mini + INA228 + externer Shunt (50–2000 A): Spannung, Strom,
  Leistung, Ladezustand (Peukert, Ladewirkungsgrad, automatische Vollerkennung), Restzeit, Historie, Alarme;
  eigenes Projekt `shunt/`, ohne Bluetooth-Sensoren. Siehe `docs/smartshunt.md` und `shunt/README.md`
  (Schaltplan, Pinout Super Mini).
- 🔌 **VE.Direct-Ausgabe** am SmartShunt-Satelliten (Text-Protokoll, 19200 Baud, TX/RX-Pins des Super Mini) mit
  einstellbarer Produkt-ID – ab Werk eigene Kennung 0xA38F.
- 🔋 **SmartShunt als Batteriewächter-Kachel** auf allen Panels (Schlüssel `satshunt:<Satellit>`): Spannung, Strom,
  Leistung, Ladezustand, Verbraucht, Restzeit – wie ein Victron SmartShunt (Kachel, Detailfenster, Editor-Liste).

- 🏕️ **Hobby** *(Beta, ungetestet)* – Wohnwagen/Reisemobile von Hobby direkt am Panel: über die **HobbyConnect-Box
  per Bluetooth** oder per **Kabel am LSG (CI-BUS, RS232-Wandler, Hardware-UART und Pins wählbar)**; beide Wege ab Werk
  aus. Lichter schaltbar, Bordbatterie als Batteriewächter-Kachel, Wasser, Temperaturen, Heizung, Kühlschrank, Klima,
  Hauptschalter (nur Kabel); Knopf „Kacheln anlegen“; serieller Befehl `hobby`.

**Verbessert**
- **Editor wie das Panel:** Einstellungen mit **Seitenleiste** (Netzwerk, MQTT, Anzeige, Geräte, Gas/Heizung, Victron,
  Akkus, Power Stations, Sensoren, Fahrzeug, Zigbee, Sicherung, Info) statt Reitern; Satelliten und Zusatzdisplays
  zusammen unter **Geräte**, Hymer und **Hobby** unter **Fahrzeug** (Hobby jetzt auch im Browser einstellbar,
  `/api/hobby`), neue Seite **Zigbee** (Stand, Anlernen, Geräteliste). **Regeln** haben einen eigenen Knopf oben
  neben „Einstellungen“.
- **Editor am Handy:** dieselbe Seite passt sich an schmale Bildschirme an – unten eine Leiste **Geräte · Panel ·
  Einstellungen · Regeln**; Geräte antippen setzt sie aufs Panel bzw. zeigt die Kachel, unter „Panel“ Vorschau, Seiten
  und Kacheln als Liste mit den Eigenschaften darunter, Dialoge als Vollbild. Frei verschieben bleibt dem PC vorbehalten.
- **Rundes Panel (CrowPanel 1,46"):** eigene **Bedienseite** für Dieselheizung, Kühlbox und Power Station (Kachel
  antippen; Drehknopf oder −/+ stellt Stufe/Solltemperatur, Ein/Aus bzw. AC/DC), lange Werte und Namen werden
  kleiner statt abgeschnitten, die Zeile darunter darf zwei Zeilen haben, Meldungen passen in den Kreis.
- README (DE/EN): neuer Abschnitt **Kabelanschlüsse** – LIN-Transceiver (CP Plus, Alde inkl. Master-Widerstand),
  RS232-TTL-Wandler für den Hobby-CI-BUS, Steckerbelegung, Pins je Panel. Englische README: Hobby-Abschnitt ergänzt,
  Hymer-Pfad korrigiert (Einstellungen → Fahrzeug).
- **Eigene Rubriken statt alles unter „Akkus“:** neuer Menüpunkt/Editor-Reiter **„Sensoren“** (Ruuvi, SwitchBot,
  BTHome, Simarine, SuperSense-Tank); **Dieselheizung und Kühlbox** stehen unter **Gas/Heizung** – jeweils mit eigener
  Suche. Bis zu **12** Bluetooth-Geräte (Akkus, Sensoren …) statt 6 auf den großen Panels.
- Einstellungen: linke Leiste scrollt jetzt (feste Zeilenhöhe statt immer kleiner werdender Punkte); „Hymer“ heißt
  jetzt **„Fahrzeug“** (Hymer Connect und Hobby).
- Victron-Edition (Waveshare): kein Hinweis auf das Zigbee-Modul mehr im Programm.

**Behoben**
- GitHub-Build: NimBLE-Arduino fest auf 2.5.2 (neuere Version baut mit diesem Framework nicht).

## 0.2.4 – 2026-11-19

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
