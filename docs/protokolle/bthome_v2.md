# BTHome v2 (offener Standard, bthome.io)

Quelle: Spezifikation bthome.io und Referenz **Bluetooth-Devices/bthome-ble** (MIT). Geräte z. B. Shelly BLU
(Tür/Fenster, Bewegung, Taster, H&T), Xiaomi-Thermometer mit pvvx-Firmware, viele Eigenbauten. Nur Werbepakete.

## Rahmen

Dienstdaten mit UUID **0xFCD2**: `[Geräteinfo] [Objekt-ID Wert…]…`
- Geräteinfo: Bit 0 = verschlüsselt, Bit 2 = sendet nur bei Ereignis (Trigger), Bit 5–7 = Version (2).
- Objekte stehen nach ID aufsteigend; Werte Little Endian; Länge je ID fest (Tabelle).
- Unbekannte ID → Rest des Pakets nicht auswertbar (Länge unbekannt) → abbrechen.
- 0x00 Paket-ID: gleiche ID = Wiederholung, nicht doppelt auswerten (wichtig für Taster).

## Verschlüsselung (Bit 0)

AES-128-CCM, Prüfwert 4 Byte. Hinter den verschlüsselten Objekten: Zähler (4 Byte LE) + MIC (4 Byte).
Nonce (13 Byte) = MAC (6, in Schreibreihenfolge) + `D2 FC` + Geräteinfo + Zähler (4). Schlüssel 16 Byte je Gerät
(32 Hex-Zeichen, z. B. aus der Shelly-App).

## Objekte (Auswahl, Länge in Byte, Faktor)

| ID | Wert | Länge | Faktor |
|---|---|---|---|
| 0x00 | Paket-ID | 1 | – |
| 0x01 | Batterie % | 1 | 1 |
| 0x02 | Temperatur °C (s) | 2 | 0,01 |
| 0x03 | Feuchte % | 2 | 0,01 |
| 0x04 | Luftdruck hPa | 3 | 0,01 |
| 0x05 | Helligkeit lx | 3 | 0,01 |
| 0x08 | Taupunkt °C (s) | 2 | 0,01 |
| 0x09 | Zähler | 1 | 1 |
| 0x0A | Energie kWh | 3 | 0,001 |
| 0x0B | Leistung W | 3 | 0,01 |
| 0x0C | Spannung V | 2 | 0,001 |
| 0x0D / 0x0E | PM2,5 / PM10 µg/m³ | 2 | 1 |
| 0x12 | CO₂ ppm | 2 | 1 |
| 0x13 | VOC µg/m³ | 2 | 1 |
| 0x14 | Feuchte (Boden) % | 2 | 0,01 |
| 0x2E | Feuchte % | 1 | 1 |
| 0x2F | Feuchte (Boden) % | 1 | 1 |
| 0x3A | Taster-Ereignis (1 kurz, 2 doppelt, 3 dreifach, 4 lang, …) | 1 | – |
| 0x3C | Dimmer | 2 | – |
| 0x3D / 0x3E | Zähler | 2 / 4 | 1 |
| 0x3F | Drehung ° (s) | 2 | 0,1 |
| 0x40 / 0x41 | Abstand mm / m | 2 | 1 / 0,1 |
| 0x42 | Dauer s | 3 | 0,001 |
| 0x43 | Strom A | 2 | 0,001 |
| 0x44 | Geschwindigkeit m/s | 2 | 0,01 |
| 0x45 | Temperatur °C (s) | 2 | 0,1 |
| 0x46 | UV-Index | 1 | 0,1 |
| 0x47 / 0x48 | Volumen l / ml | 2 | 0,1 / 1 |
| 0x49 | Durchfluss m³/h | 2 | 0,001 |
| 0x4A | Spannung V | 2 | 0,1 |
| 0x4B / 0x4C | Gas m³ | 3 / 4 | 0,001 |
| 0x4D | Energie kWh | 4 | 0,001 |
| 0x4E / 0x4F | Volumen / Wasser l | 4 | 0,001 |
| 0x50 | Zeitstempel | 4 | – |
| 0x51 / 0x52 | Beschleunigung / Drehrate | 2 | 0,001 |
| 0x53 / 0x54 | Text / Rohdaten | 1 + n (erstes Byte = Länge) | – |
| 0x55 | Speichervolumen l | 4 | 0,001 |
| 0x56 | Leitfähigkeit µS/cm | 2 | 1 |
| 0x57 / 0x58 | Temperatur °C (s) | 1 | 1 / 0,35 |
| 0x59 / 0x5A / 0x5B | Zähler (s) | 1 / 2 / 4 | 1 |
| 0x5C | Leistung W (s) | 4 | 0,01 |
| 0x5D | Strom A (s) | 2 | 0,001 |
| 0x5E | Richtung ° | 2 | 0,01 |
| 0x5F | Niederschlag mm | 2 | 0,1 |
| 0x60 | Kanal | 1 | – |
| 0x61 | Drehzahl U/min | 2 | 1 |
| 0x62 / 0x63 | Geschwindigkeit / Beschleunigung (s) | 4 | 0,000001 |
| 0x64 / 0x65 | Lichtstufe / Einstellungsstand | 1 | – |
| 0xF0 / 0xF1 / 0xF2 | Gerätetyp / Firmware | 2 / 4 / 3 | – |

Zustände (1 Byte, 0/1): 0x0F allgemein, 0x10 Strom, 0x11 offen, 0x15 Batterie schwach, 0x16 lädt, 0x17 CO,
0x18 kalt, 0x19 verbunden, 0x1A Tür, 0x1B Garagentor, 0x1C Gas, 0x1D Hitze, 0x1E Licht, 0x1F Schloss, 0x20 Nässe,
0x21 Bewegung, 0x22 bewegt sich, 0x23 belegt, 0x24 Stecker, 0x25 Anwesenheit, 0x26 Problem, 0x27 läuft,
0x28 Sicherheit, 0x29 Rauch, 0x2A Geräusch, 0x2B Manipulation, 0x2C Erschütterung, 0x2D Fenster.
