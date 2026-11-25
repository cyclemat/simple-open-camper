# Changelog – Simple Open Camper

Alle Firmwares (Panels, Satelliten, Victron-Display, Zigbee-Modul) tragen dieselbe Versionsnummer
(`src/soc_version.h`). Neueste Version oben. *Beta* = eingebaut, aber noch nicht an echter Hardware getestet.

## 0.2.9 – in Arbeit

**Neu**
- ⚓ **Symbole für Boote und Technik** (Wunsch eines Nutzers) – 22 neue Kachel-Symbole im Editor, eigene Gruppe „Boot und
  Technik“ unter den Wohn-Symbolen: Anker, Boot, Segelboot, Motor, Kraftstoff, Wellen (Bilgepumpe), Wasser, Dusche, WC,
  Rohr/Ventil, Kompass, Leuchtturm, Rettungsring, Sirene, Funk, Wind, Markise, Leiter, Fisch, Taschenlampe, Solar, Kühlen.
  Radar und Propeller gibt es im Phosphor-Satz nicht (Ersatz: Funk bzw. Motor). Alle Panels und Zusatzdisplays
  (Symbolschriften neu erzeugt, ~45 KB mehr Flash je Panel).
- 🖼️ **Eigene Symbole** *(Beta)* – PNG-Dateien als Kachel-Symbole: Hochladen im Editor (Symbolauswahl **+** oder Einstellungen →
  Anzeige → Eigene Symbole, der Browser bringt das Bild auf 96 × 96 Pixel) oder Dateien im Ordner `icons` auf der SD-Karte
  (P4, Waveshare). Durchsichtiger Hintergrund oder schwarz auf weiß; das Panel zeigt sie in der Kachelfarbe wie die
  eingebauten. Je Symbol 3,5 KB PSRAM, kein interner RAM; bis zu 32. Technik: PNG-Dekoder (lodepng aus LVGL, nur
  Dekoder) und eine Laufzeit-Schrift mit Rückfall auf die eingebaute Symbolschrift. `/api/icons`, `/api/icon?name=`.
  Zusatzdisplays und App zeigen stattdessen das automatische Symbol. Wiki-Seite „Symbole für Kacheln“.
- 📶 **Funkmodul-Update am P4 per Knopf** *(Beta)* – der Funkchip ESP32-C6 (WLAN/Bluetooth, ESP-Hosted) hat eine eigene
  Firmware. Passt sie nicht zur Panel-Firmware, zeigen Editor → Einstellungen → Info und das Panel unter Info den Stand
  („2.12.3 – passend wäre 2.12.13“) und den Knopf **Funkmodul aktualisieren**: Das Panel lädt die Datei aus dem Internet und
  schreibt sie selbst in einem eigenen Task (etwa eine Minute), danach Neustart. Ohne Internet: `c6.bin` auf der SD-Karte,
  Knopf „Aus SD-Datei“. Keine Automatik. `POST /api/c6`, Stand in `/api/info` „c6“. Der Online-Flasher kann den Funkchip
  nicht erreichen (er hängt per SDIO hinter dem P4), darum macht es das Panel. Serielle Befehle `c6update`/`c6datei`
  blockieren nicht mehr.
- 🛠️ **Symbol-Generator ohne Node:** `python tools/gen_icons.py` lädt die Phosphor-SVGs, rendert sie mit Chrome headless und
  baut Schriften und Editor-Pfade (Ersatz für `tools/gen_icons.js`).

## 0.2.8 – 2026-11-24

**Neu**
- ⚡ **Votronic** *(Beta, ungetestet)* – eigener Menüpunkt **Einstellungen → Votronic** (Panel und Editor), zwei Wege,
  ab Werk aus: **Bluetooth Connector S-BC** (Batteriecomputer/Smart Shunt und Solarregler, Kopplung mit PIN 173928) und
  **Display-Link per Kabel** (RJ11, 1000 Baud: Solarregler, Ladegeräte VBCS/VAC, Ladebooster VCC, Batteriecomputer).
  Kacheln wie Victron (`vot:bat`, `vot:solar`, `vot:charger`, `vot:booster`), „Kacheln anlegen“, serieller Befehl
  `votronic`, `/api/votronic`. P4, rundes Panel, Waveshare (nicht in der Victron-Edition). Protokollwissen aus
  syssi/esphome-votronic (Apache-2.0), [`docs/protokolle/votronic.md`](docs/protokolle/votronic.md).
- 🔌 **Orion XS / Orion Smart bedienen** *(Beta, ungetestet)* – Kachelfenster wie beim MultiPlus: **Ein/Aus** und
  **Ladestrom (max.)** mit −/+. Zwei Wege: **per Bluetooth direkt** (kurze Verbindung mit der PIN aus VictronConnect,
  ab Werk 000000 – Knopf „PIN“ im Fenster; Register 0x0200 Gerätemodus und 0xEDF0 maximaler Ladestrom wie im
  offiziellen Orion-XS-HEX-Dokument) oder **über den Cerbo GX** (Ein/Aus über `dcdc/<n>/Mode`, wenn der GX einen Schalter
  meldet). Auch auf Zusatzdisplays. Die Notfall-Rückladung lässt sich (noch) nicht starten – Victron hat dafür keinen
  Befehl veröffentlicht. Serieller Befehl `vctl`.
- 📐 **Wasserwaage** *(Beta, ungetestet)* – Kachel „Wasserwaage“ für **Wohnwagen** (welche Seite wie viele cm anheben,
  Stützrad hoch/runter) oder **Reisemobil** (Keilhöhe je Rad, rot wenn höher als die Auffahrkeile). Sensor: Satellit mit
  MPU-6050 oder Ruuvi Tag (Beschleunigung aus Datenformat 5, jetzt ausgewertet). Detailfenster mit Draufsicht und Libelle.
  Einrichten nur im Editor (Einstellungen → Wasserwaage): Fahrzeugmaße, Nullpunkt, Einbaulage (Kante nach vorne/rechts/
  hinten/links, links/rechts vertauscht) und ein **Assistent**, der die Einbaulage selbst bestimmt (Front anheben, optional
  rechte Seite). Wo der Sensor sitzt, ist egal. P4, rundes Panel, Waveshare. `/api/level`.
- 🔊 **Ton & Musik** *(Beta, nur P4; Musik am P4 V1.2 mit Lautsprecher getestet)* – Kern sind **Warn- und Hinweistöne**,
  die Musikwiedergabe ist nur ein **Gimmick** (Teil der Warnton-Abspielfunktion). MP3-Wiedergabe von der SD-Karte (Ordner `music`) über den
  Lautsprecher-Anschluss des CrowPanel, Kachel „Musik“ (kurz tippen = Abspielen/Pause, lang = Player-Fenster; *Test:* „CampAmp“
  im Retro-Stil mit Laufzeit, Spektrum-Balken, Lauftext, kbps/kHz, Lautstärke, Spulen, Zufall, Wiederholen, Titelliste). Hinweistöne (Gong, Piepton, Alarm) und eigene Ansagen (Ordner `sounds`), Musik wird
  dafür kurz angehalten. Neue Regel-Aktionen „Ton abspielen“ und „Musik“ (z. B. Wecker, Gas-Warnung, Klingel).
  Editor: Einstellungen → Ton & Musik (ab Werk aus). Decoder minimp3 (CC0). Audio-Task mit Stapel im PSRAM, Puffer
  nur während der Wiedergabe. `/api/audio`.
- 🔌 **32 statt 16 Netzwerk-Sockets** (P4 und rundes Panel) – die veröffentlichten Firmware-Dateien kommen jetzt aus den
  Umgebungen `crowpanel-p4-sock32` und `crowpanel-146-sock32` (eigene Projektordner `p4-sock32/`, `crowpanel146-sock32/`).
  Webserver, Broker, Tailscale, Home Assistant und UDP teilen sich den Vorrat; mit 16 hing der Webserver nach einer
  Minute („error in accept (23)“). Der erste Build übersetzt die Framework-Bibliotheken neu (1–2 h); beim P4 hält
  `tools/pin_components.py` die Versionen der Funkchip-Bausteine fest (esp_hosted 2.12.13, esp_wifi_remote 1.6.4).
  Dazu Bluetooth-Host-Speicher und (P4) lwIP-Netzwerkpuffer im PSRAM, größerer Host-Stapel, rundes Panel 5 statt 3
  Bluetooth-Verbindungen; beim runden Panel ist BLE-Mesh abgeschaltet (ungenutzt, ließ den Neubau scheitern).
  **P4 getestet** (ein Abend mit Tailscale, Musik, App, Editor; 0.2.8 danach 1 h stabil mit 150 KB internem RAM
  frei statt 75 KB); **rundes Panel ungetestet**
  *(Beta)*. Die 16-Socket-Umgebungen `crowpanel-p4` / `crowpanel-146` bleiben als Rückfall baubar. Macht es Probleme,
  gehen wir dorthin zurück.

**Verbessert**
- ✏️ **Umbenennen gilt nur noch für das Panel** – Editor und Panel schicken keine neuen Namen mehr an Zigbee2MQTT
  (der Bereich „In Zigbee2MQTT umbenennen“, das Häkchen am Panel und `/api/rename` sind entfernt). Zigbee2MQTT und Home
  Assistant behalten ihre Namen.
- 📖 README: Unterschied zwischen Instant-Readout-Schlüssel (lesen) und Bluetooth-PIN (Orion schalten) erklärt.
- 🧠 **Verschlüsselung (TLS) im PSRAM** (P4 und rundes Panel) – mit verbundenem Tailscale blieben am P4 nur ~15 KB
  interner Speicher, Bluetooth und Funkchip fielen aus. Jetzt mit Tailscale, Musik, App und Editor gleichzeitig
  ~110 KB intern frei.
- 🌐 **Webserver hängt nicht mehr bei vielen Anfragen** – der Editor schickt seine Anfragen nacheinander (eine
  Verbindung statt bis zu sechs), der Webserver hält höchstens 3 Verbindungen und schließt bei einer vierten die älteste.
- 🔍 Diagnose: serielle Befehle `sockets` (belegte Netzwerk-Sockets mit Gegenstelle) und `log`/`log zeigen`, Zustandszeile
  des Debug-Logs mit „Sockets n/16“, bei fast vollem Vorrat Liste ins Log.
- 📡 **Bluetooth und WLAN kommen sich weniger in die Quere** *(Beta)* – beide teilen sich die Antenne. Der Dauerscan für
  Victron lauscht nur noch 15 % statt 30 % der Zeit (ein Wert alle ~7 statt ~3 s, die Kachel zeigt das Alter), das bringt
  WLAN-Durchsatz für Editor und App. Beim P4 läuft der Scan während Editor/App-Zugriffen sparsam weiter (5 %) statt zu
  pausieren – die Werte in der App bleiben nicht mehr stehen.
- 🎯 **Nur bekannte Geräte hören** *(Beta)* – läuft keine Suche oder Kopplung, bekommt der Bluetooth-Controller eine
  Liste der eingerichteten Geräte (bis 12) und verwirft alle anderen Werbepakete, bevor sie den Prozessor erreichen
  (beim P4 schon im Funkchip). Auf vollen Plätzen sind das leicht 100 Pakete pro Sekunde weniger. Nach dem Start und
  alle 10 Minuten (wenn ein Gerät fehlt) hört das Panel eine Minute offen. Mit mehr als 12 Geräten wie bisher.
- 🧠 Bluetooth-Host-Speicher (~25 KB) und beim P4 die lwIP-Netzwerkpuffer (bis 64 KB je Verbindung) im PSRAM, größerer
  Host-Stapel – siehe „32 Sockets“ oben (nur in diesen Umgebungen, weil es einen Framework-Neubau braucht).

**Behoben**
- Kacheln **Musik**, **Wasserwaage** und **Votronic** zeigten am Panel „nicht verfügbar“ (Kachelart wurde nicht erkannt).

## 0.2.7 – 2026-11-22

**Neu**
- 🧩 **ESPHome, Tasmota & Co.** (MQTT-Discovery) *(Beta)* – Geräte, die sich per Home-Assistant-Discovery am Broker melden,
  erscheinen auf dem Panel wie Satelliten (eine Gruppe je Gerät): Sensoren, Binärsensoren, Schalter, Lampen (ein/aus).
  Ohne Home Assistant, mit dem eingebauten Broker. Eigene Satelliten und Zigbee2MQTT werden übergangen. Einstellungen →
  MQTT, ab Werk aus. P4, rundes Panel, Waveshare. Getestet am P4 mit einem Gerät im ESPHome-Format.
- 🏠 **Home Assistant** *(Beta)* – Integration [simple-open-camper-HA-Integration](https://github.com/cyclemat/simple-open-camper-HA-Integration)
  (MIT, HACS): HA verbindet sich per WebSocket mit dem Panel (`/api/ha`, Zugangscode, auch über Tailscale). HA übernimmt
  nur ausgewählte Panel-Werte, das Panel zeigt nur ausgewählte HA-Entitäten (wie ein Satellit „Home Assistant“). Am Panel:
  Einstellungen → Netzwerk → Home Assistant. P4, rundes Panel, Waveshare.

**Verbessert**
- 🌍 Spanisch und Französisch für die Panel-Oberfläche vollständig (1603 Texte, auch Shelly per Bluetooth, Zigbee,
  Regeln, Sensoren, Heizung/Kühlbox, Hobby, Debug-Log). Veraltete Einträge aus den Wörterbüchern entfernt.
- 🌍 **P4 und rundes Panel auch auf Spanisch und Französisch** als fertige Firmware (Online-Flasher: Español /
  Français). Der Web-Editor ist bei diesen Sprachen englisch. Umgebungen `crowpanel-p4-es/-fr`, `crowpanel-146-es/-fr`.
- 💾 **P4: neue Speicheraufteilung** (`partitions_p4.csv`: 2 × 6 MB Programm statt 2 × 4 MB, Dateisystem 3,9 MB) – die
  Firmware passte nicht mehr in 4 MB. Umstieg einmal per Online-Flasher „Nur Firmware (Update)“: Einstellungen bleiben,
  Dateisystem (Kachel-Anordnung, Regeln, Bilder) ist danach leer – vorher auf SD sichern. Update über die Weboberfläche
  geht erst nach dem Umstieg (vorher „Datei zu groß“).
- 🧹 Testfassungen (`release/test/`) und ältere Firmware-Dateien (`release/vorher/`) entfernt.

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
