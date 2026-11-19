# China-Dieselheizungen per Bluetooth (Vevor, Hcalory, Sunster, HeaterCC/AirHeater)

Quellen (Protokollwissen, eigene Umsetzung): **Spettacolo83/diesel-heater-ble** (MIT) und **mSoftMS/AirHeater-BLE**
(MIT, PROTOCOL.md). Nicht übernommen: bderleta/vevor-ble-bridge (GPL-3.0). Stand 2026-11-19, nicht am Gerät getestet.

**Sicherheit:** Gerät mit Brennstoff. Nur Ein/Aus, Stufe und Solltemperatur – keine Pumpen-/Einstellparameter.
Mindestens 500 ms zwischen Befehlen, nach jedem Befehl Status abwarten, bei Fehlercode keine weiteren Befehle.
Die Heizung meldet nur auf Anfrage (Status-Abfrage schreiben, dann kommt die Antwort per Notify).

## Varianten (Erkennung am ersten Statusrahmen)

| Art | Erkennung | Länge | Wo |
|---|---|---|---|
| AA55 | Kopf `AA 55` | 18–20 | Vevor/Hcalory „AirHeaterBLE“, Dienst FFE0, Kennlinie FFE1 (Notify + Schreiben) |
| AA55 verschlüsselt | 48 Byte, nach XOR Kopf `AA 55` | 48 | dto. |
| AA66 | Kopf `AA 66` | 20 | BYD-Variante |
| AA66 verschlüsselt | 48 Byte, nach XOR Kopf `AA 66` | 48 | dto. |
| ABBA (HeaterCC/AirHeater) | Kopf `AB BA` | ≥ 21 | Dienst FFF0, Notify FFF1, Schreiben FFF2 |
| CBFF (Sunster v2.1) | Kopf `CB FF` | 47 | doppelt XOR mit Seriennummer – **später** |
| Hcalory MVP1/MVP2 | eigene Pakete, Dienst FFF0 bzw. BD39 | – | **später** |

XOR-Schlüssel (AA55/AA66 verschlüsselt): `"password"` (70 61 73 73 77 6F 72 64), Byte i ^= key[i % 8], für 6×8 Byte.

## Befehle Vevor (AA55/AA66, auch verschlüsselt – Befehl selbst immer unverschlüsselt 8 Byte)

`AA 55 P1 P2 CMD ARG_L ARG_H SUM` – P1 = Passkey / 100, P2 = Passkey % 100 (ab Werk 1234 → 0C 22),
SUM = (P1 + P2 + CMD + ARG_L + ARG_H) & 0xFF.

| CMD | ARG | Bedeutung |
|---|---|---|
| 1 | 0 | Status abfragen |
| 2 | 1 = Stufe, 2 = Temperatur | Betriebsart |
| 3 | 1 = an, 0 = aus | Ein/Aus |
| 4 | Stufe 1–10 bzw. °C 8–36 | Sollwert (je nach Betriebsart) |

## Status AA55 (unverschlüsselt)

| Byte | Bedeutung |
|---|---|
| 3 | Laufzustand 0 aus / 1 an |
| 4 | Fehlercode (0 = keiner, siehe unten) |
| 5 | Schritt 0 Bereit, 1 Selbsttest, 2 Zündung, 3 läuft, 4 Abkühlen, 6 Lüften |
| 6–7 | Höhe (LE) |
| 8 | Betriebsart 0 manuell, 1 Stufe, 2 Temperatur |
| 9 | Stufe (Art 1) bzw. Soll-°C (Art 2) |
| 10 | Stufe − 1 (Art 0/2) |
| 11–12 | Versorgung in 0,1 V (LE) |
| 13–14 | Gehäuse-/Brennkammertemperatur °C (LE, mit Vorzeichen) |
| 15–16 | Innenraumtemperatur °C (LE, mit Vorzeichen) |

**AA66:** wie AA55, aber 6 = Höhe (1 Byte), 9 = Stufe/Soll, 11–12 Spannung 0,1 V LE, 13–14 Gehäuse (> 350 → ÷10),
15 = Innenraum °C (1 Byte).

**AA55 verschlüsselt (nach XOR):** 3 Lauf, 4 Fehler, 5 Schritt, 6–7 Höhe (BE, ÷10), 8 Art, 9 Soll-°C, 10 Stufe,
11–12 Spannung 0,1 V (**BE**), 13–14 Gehäuse (BE, Vorzeichen), 32–33 Innenraum 0,1 °C (BE, Vorzeichen),
37 = 1 → 38–39 CO ppm (BE).
**AA66 verschlüsselt:** wie AA55 verschlüsselt, aber Fehlercode in Byte **35**, 27 = Einheit (1 = °F, Soll dann
umrechnen), 31 = Auto-Start/Stopp.

Fehlercodes Vevor: 1 Startfehler, 2 kein Brennstoff, 3 Überspannung, 4 Auslassfühler, 5 Einlassfühler,
6 Pumpe, 7 Gebläse, 8 Zündung, 9 Überhitzung, 10 Überhitzungsfühler.

## ABBA / HeaterCC / AirHeater (Dienst FFF0, Notify FFF1, Schreiben FFF2)

Befehl: `BA AB 04 CMD D1 D2 D3 SUM`, SUM = Summe Byte 0–6.

| Befehl | Bytes (ohne SUM) |
|---|---|
| Status abfragen | `BA AB 04 CC 00 00 00` (SUM 0x35) |
| Heizen an/aus (umschalten!) | `BA AB 04 BB A1 00 00` |
| Lüften | `BA AB 04 BB A4 00 00` |
| Stufe + / − | `BA AB 04 BB A2 00 00` / `… A3 …` |
| Solltemperatur | `BA AB 04 DB <°C> 00 00` |
| Konstant-Temperatur / andere Betriebsart | `… BB AC …` / `… BB AD …` |

Status (≥ 21 Byte): 4 Zustand (0 aus, 1 heizt, 2 kühlt ab, 4 lüftet, 6 Bereit), 5 Art (0 Stufe, 1 Temperatur,
0xFF Fehler → Byte 6 = Fehlercode), 6 Stufe bzw. Soll-°C, 8 Auto-Start/Stopp, 9 Versorgung V (ganze Volt),
10 Einheit (0 °C, 1 °F), 11 Innenraum (−30 bei °C, −22 bei °F), 12–13 Gehäuse °C (BE), 16–17 Höhe (LE).
Fehler ABBA: 2 Spannung, 3 Glühstift, 4 Pumpe, 5 Übertemperatur, 6 Gebläse, 7 Kommunikation, 8 Flamme aus,
9 Fühler, 10 Startfehler, 192 CO-Alarm.
Achtung: „Heizen an“ ist ein Umschalter – vor dem Senden Zustand prüfen (nur senden, wenn er wirklich umschalten soll).
