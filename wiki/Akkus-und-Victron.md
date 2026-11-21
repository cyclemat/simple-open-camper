# Akkus, Power Stations und Victron

### 🔋 Akkus und Power Stations

- Am Panel **Einstellungen → Akkus (BMS)** bzw. **Power Stations** → „Suchen“, im Editor Reiter **Akkus**.
- **App des Herstellers vorher schließen** – die meisten Akkus sprechen nur mit einem Gerät.
- Verlangt der Akku eine **Kopplungs-PIN** (z. B. Super B) oder hat er in seiner App ein **Passwort** (z. B.
  CS-BATTERIES), dieses im Feld „PIN / Passwort“ eintragen. Das Panel zeigt an, wenn der Akku danach fragt.
- **CS-BATTERIES** *(Beta, ungetestet)*: JBD-BMS mit Anmeldung – die Anmeldung der App erledigt das Panel selbst,
  nur ein eigenes Passwort muss eingetragen werden.
- **Zellspannungen:** Bei vielen Akkus sieht man jede einzelne Zelle – JBD/Xiaoxiang, CS-BATTERIES, JK-BMS, Daly/Bulltron,
  ANT, Eco-Worthy, Renogy, Vatrer und Power Quarry/Shentec. **Akku-Kachel lange drücken**: Fenster mit allen Werten,
  die der Akku liefert (Spannung, Strom, Leistung, Restzeit, Temperatur, Zyklen, Gesundheit, Kapazität, Alarme) und
  allen Zellen als Kästchen – die höchste orange, die niedrigste blau umrandet, dazu die Differenz (Δ).
- PIN/Passwort und Entfernen gibt es nur in den Einstellungen, nicht im Fenster der Kachel.
- Nur lesen – am Akku wird nichts geschaltet oder eingestellt. Power Stations: AC-/DC-Ausgang schaltbar.

### ☀️ Victron

- In VictronConnect **Instant Readout** aktivieren (**Einstellungen → Produktinfo → Instant Readout**).
- Pro Gerät den Schlüssel aus **Details anzeigen** im Editor einfügen oder am Panel eintippen.
- Nur Anzeige – Schalten geht über Instant Readout nicht.

### 🔋 Victron Cerbo GX

- Am GX **Einstellungen → Dienste → MQTT** einschalten (unverschlüsselt, Port 1883).
- Am Panel unter **Einstellungen → Victron** oder im Editor die IP-Adresse des GX eintragen. Die Portal-ID
  erkennt das Panel selbst; ein Passwort ist nur nötig, wenn am GX eins für das lokale Netz gesetzt ist.
- Das Panel verbindet sich direkt mit dem GX (eigene MQTT-Verbindung, unabhängig vom Zigbee2MQTT-Broker),
  abonniert nur die benötigten Werte und hält den GX per Keepalive am Senden.
- Fünf Kacheln: **Solar** (PV-Leistung, Ladephase, Ertrag heute), **Netz** (Leistung, Spannung, Strombegrenzung),
  **Ladegerät** (Multi/Quattro oder Netzladegerät: Zustand, Schalter, Leistung), **Batterie** (SOC, Spannung,
  Leistung, Restlaufzeit) und **DC-System** (Verbraucher-Leistung).
- **MultiPlus/Quattro und Ladegeräte bedienen:** im Detailfenster (Kachel lange drücken) Netzstrom-Begrenzung und
  Schalter (Ein, Aus, Nur Laden, Nur Wechselrichter) – nur wenn der GX das erlaubt.
- **DC-DC-Lader am GX** (Orion XS, Orion Smart) und **Lichtmaschinenregler** (z. B. Wakespeed): je Gerät eine eigene
  Kachel mit Zustand, Eingang und Ausgang (Name vom GX). Per Bluetooth ohne GX gehen Orion XS/Smart sowieso.
- **Ein- und Ausgänge des GX** – nur was dort eingerichtet ist: **Relais** (schaltbar, wenn am GX die Funktion
  „Manuell“ gewählt ist), **Tanks** (Füllstand, Inhalt in Litern), **Temperatursensoren** (auch Luftfeuchte, z. B.
  Ruuvi) und **Digitaleingänge** (Tür, Bilge, Alarm, Generator). Sie erscheinen im Editor wie die Anschlüsse eines
  Satelliten namens **„GX“** und lassen sich aufs Panel ziehen.

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
