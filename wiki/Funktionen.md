# Funktionen

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
- 🧩 **ESPHome, Tasmota & Co.** – Geräte mit MQTT-Discovery erscheinen von selbst, wie Satelliten (Sensoren, Schalter, Lampen)
- 🌈 **WLED** über denselben Broker – Ein/Aus, Helligkeit, Farbe
- ⛽ **Truma LevelControl** per Bluetooth – bis zu vier Gasflaschen
- 🛢️ **Mopeka** per Bluetooth – bis zu vier Gas- und Tanksensoren
- ⚖️ **Senso4s** *(Beta)* – Gasflaschen-Waage Basic/Plus per Bluetooth, Füllstand in % und kg
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
- 🔥 **Dieselheizung** *(Beta)* – Vevor/Hcalory, HeaterCC/AirHeater: anzeigen und bedienen (ein/aus, Stufe, Temperatur)
- 🧊 **Kühlbox** *(Beta)* – Alpicool, IceCube/Plug-in Festivals, Bodega u. a.: anzeigen und bedienen (ein/aus, Soll je Zone)
- 📡 **BTHome** *(Beta)* – Shelly BLU (Tür, Bewegung, Taster, H&T), Xiaomi mit pvvx-Firmware u. v. a., auch verschlüsselt
- 🐝 **Zigbee direkt** *(Beta, P4 und rundes Panel)* – mit dem ESP32-H2-Funkmodul ganz ohne Zigbee2MQTT: Geräte anlernen,
  Lampen, Steckdosen (mit Leistung und Verbrauch), Schalter und Sensoren als Kacheln
- 📲 **App „S.O.C. Live“** *(Beta, Android)* – das Panel aufs Handy, Tailscale eingebaut: Kacheln wie am Panel, antippen schaltet bzw. zeigt alle Werte
- 📱 **Live-Ansicht** – das Panel im Browser bzw. am Handy bedienen (`/live`), mit Passwort
- 🔐 **Tailscale** *(Beta, P4 und rundes Panel)* – Editor und Weboberfläche von unterwegs, ohne offenen Port und ohne
  Cloud: Panel per Link-Anmeldung ins eigene Tailnet
- 🧩 **Regeln** *(Beta, P4 und rundes Panel)* – Automationen mit grafischen Bausteinen (Blockly) im Editor, z. B. Bewegungslicht,
  Licht per Taster, Dimmen per Taster

**Drei Betriebsarten**

| Modus | Beschreibung |
|---|---|
| Netzwerk | Panel und Hub im Camp-WLAN |
| Router | Panel öffnet ein eigenes WLAN mit DHCP; der SMHUB Nano bucht sich ein, das Panel findet den Broker automatisch |
| Kombi | Eigenes WLAN für den Hub **und** Camp-WLAN, mit Internet-Weiterleitung (NAT) |

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
