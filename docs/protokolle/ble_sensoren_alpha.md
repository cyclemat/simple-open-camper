# Weitere Bluetooth-Sensoren – ALPHA (noch nicht umgesetzt)

> Wunsch des Nutzers (2026-11-19): Daten schon hinterlegen, im Hauptstrang noch nicht anzeigen. Quelle: Victrons
> Venus-OS-Treiber `victronenergy/dbus-ble-sensors` (src/*.c, Stand 2026-11). Das Repo hat **keine Lizenz** – also nur
> Protokollwissen übernehmen, eigenen Code schreiben (wie bei Hymer/aiobmsble). Alle senden **nur Werbepakete**
> (Herstellerdaten, keine Verbindung, kein Schlüssel) – Umsetzung später wie `src/mopeka.cpp` bzw. `src/tpms.cpp`.
> Offsets = Bytes **nach** der 2-Byte-Hersteller-ID, Little Endian, wenn nicht anders angegeben.

| Sensor | Hersteller-ID | Länge | Art |
|---|---|---|---|
| Gobius C (Tank, Ultraschall von außen) | `0x0F53` | 14 | Tank, misst Abstand von oben |
| Garnet SeeLevel 709-BT | `0x0CC0` | ≥ 12 | bis 8 Tanks in % + Batteriespannung |
| Safiery Star-Tank (Radar) | `0x0067` | 10 | Tank, misst Abstand von oben |
| Ruuvi Tag / Ruuvi Air | `0x0499` | 24 / 20 | Temperatur, Feuchte, Luftdruck (Air: Feinstaub, CO₂ …) |
| (Mopeka, schon umgesetzt) | `0x0059` | – | – |

## Gobius C – `0x0F53`, genau 14 Byte (Format v1.1.1)

| Offset | Bedeutung |
|---:|---|
| 0 | Hardware-ID (7 Bit) |
| 1 | Temperatur: Bits 0..6, °C = Wert − 40 (Bit 7 reserviert) |
| 2–3 | Abstand in mm (uint16); `0xFFFF` = Fehler, `0xFFFE` = startet |
| 4–6 | letzte 3 Byte der Funkadresse (addr[2], addr[1], addr[0]) – Prüfung, dass das Paket vom Sender stammt |
| 7–9 | Firmware major.middle.minor |
| 10 | Status-Flags (von Victron ignoriert) |
| 11–13 | frei (0) |

Füllstand: Abstand von oben → Tankhöhe leer/voll vom Nutzer (wie Mopeka „eigene Maße“). Name bei Victron
„Gobius C XX:XX:XX“.

## Garnet SeeLevel 709-BT – `0x0CC0`, mindestens 12 Byte

| Offset | Bedeutung |
|---:|---|
| 0–2 | Seriennummer (24 Bit) |
| 3 | Frisch 1 % |
| 4 | Grau 1 % |
| 5 | Schwarz 1 % |
| 6 | Frisch 2 % |
| 7 | Grau 2 % |
| 8 | Schwarz 2 % |
| 9 | Galley % |
| 10 | LPG % |
| 11 | Batteriespannung × 10 (141 = 14,1 V) |

Werte 0..100 = %, über 100 = Fehlercode: 101 Kurzschluss, 102 offen, 103 Bitfehler, 104 Stapel falsch, 105 oben
fehlt, 106 unten fehlt, 107 nicht kalibriert, 108 Prüfsumme, 109 außerhalb, **110 = Kanal nicht benutzt**
(überspringen), 111 startet, 112 falsche ID. Je benutztem Kanal eine Tank-Kachel („SeeLevel <Seriennr.> Frisch 1“).

## Safiery Star-Tank – `0x0067`, genau 10 Byte

| Offset | Bedeutung |
|---:|---|
| 0 | Hardware-ID (7 Bit), 10 = „TopDown BLE“ |
| 1 | Batterie: Bits 0..6 / 32 = Volt |
| 2 | Temperatur: Bits 0..6, °C = Wert − 40; Bit 7 = Sync-Taste gedrückt |
| 3–4 | Abstand (uint16, 14 Bit) in mm → /10 = cm |
| 5–7 | letzte 3 Byte der Funkadresse (Prüfung wie Gobius) |
| 8–9 | Beschleunigung X/Y (int8 / 1024 g) – Victron liest auch Z an 10 (liegt außerhalb der 10 Byte) |

Füllstand von oben wie Gobius. Name „StarTank XX:XX:XX“. Sync-Taste → bei der Suche wie Mopeka anzeigen.

## Ruuvi – `0x0499`, erstes Byte = Format

**Format 5 (RAWv2), 24 Byte** (Big Endian):

| Offset | Bedeutung |
|---:|---|
| 0 | 5 |
| 1–2 | Temperatur int16 × 0,005 °C |
| 3–4 | Feuchte uint16 × 0,0025 % |
| 5–6 | Luftdruck uint16 + 50000 Pa |
| 7–12 | Beschleunigung X/Y/Z int16 mG |
| 13–14 | Bits 15..5: Batterie mV − 1600; Bits 4..0: Sendeleistung (×2 − 40 dBm) |
| 15 | Bewegungszähler |
| 16–17 | Folgenummer |
| 18–23 | MAC |

Neigung aus X/Y/Z → Wasserwaage (Victron hat dafür einen Zweig „ruuvi-angle-support-for-leveling“).

**Format 6 (Ruuvi Air), 20 Byte**: 1–2 Temperatur ×0,005 °C, 3–4 Feuchte ×0,0025 %, 5–6 Luftdruck +50000 Pa,
7–8 PM2,5 ×0,1 µg/m³, 9–10 CO₂ ppm, 11 VOC, 12 NOx (je mit Zusatzbit in den Flags an 16), 13 Helligkeit
(logarithmisch: lux = e^(Wert × 16 / ln2 / 254) − 1 laut Victron), 15 Folgenummer, 16 Flags.
Genaues Bitlayout von VOC/NOx vor der Umsetzung in der offiziellen Ruuvi-Doku nachsehen.

## Umsetzungsidee

Ein gemeinsames Modul `src/blesens.cpp` (Schalter `WITH_BLESENS`, je Typ ein Unterschalter) für reine Werbepakete:
Suche (3 min), Einrichten, NVS, Kacheln „Tank“/„Klima“. Tanks mit Abstand (Gobius, Star-Tank) brauchen Tankhöhe
leer/voll wie Mopeka. Erst einbauen, wenn jemand ein Gerät zum Testen hat.
