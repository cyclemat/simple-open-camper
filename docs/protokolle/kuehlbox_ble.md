# Kompressor-Kühlboxen per Bluetooth (Alpicool, Bodega, Brass Monkey, Setpower u. a.)

Quellen (Protokollwissen, eigene Umsetzung): **klightspeed/BrassMonkeyFridgeMonitor** (MIT) und
**deeteeppg/bodega-bluetooth-le-hass** (MIT). Gleiches Protokoll wie die App „Alpicool“ / „Car Fridge Freezer“.
Stand 2026-11-19, nicht am Gerät getestet. Andere Kühlbox-Apps (Cfridge, Outdoor Joy/ICECO, A-Premium …) können
andere Protokolle haben – erst mit Unterlagen/Mitschnitt.

## Erkennung und Verbindung

- Name beginnt mit `WT-`, `A1-`, `AK1-`, `AK2-`, `AK3-`.
- Dienst `00001234-0000-1000-8000-00805f9b34fb`, Schreiben `…1235…`, Notify `…1236…`.
- Solange verbunden, wirbt die Box nicht und spricht mit keinem anderen Gerät (App schließen).
- Ablauf: verbinden, Notify abonnieren, **Bind** senden, dann **Query**; Antwort kommt per Notify.

## Rahmen

`FE FE LEN DATA… SUM_H SUM_L`, LEN = Anzahl Bytes nach LEN (Daten + 2), SUM = 16-Bit-Summe aller Bytes davor (BE).
Manche Boxen schicken die doppelte Summe – beides gilt.

| Befehl | DATA |
|---|---|
| Bind | `00 01` (bzw. nur `00`) |
| Query | `01` |
| Set (alle Einstellungen) | `02` + Einstellungsblock (wie Status-Bytes 0–13, bei zwei Zonen + Zone 2) |
| Zone 1 Soll | `05 <°C als int8>` |
| Zone 2 Soll | `06 <°C als int8>` |

## Status (Antwort auf Query, Daten nach dem Befehlsbyte)

| Byte | Bedeutung |
|---|---|
| 0 | Tasten gesperrt |
| 1 | eingeschaltet |
| 2 | Betrieb 0 Max, 1 Eco |
| 3 | Batterieschutz 0 niedrig, 1 mittel, 2 hoch |
| 4 | Zone 1 Soll (int8) |
| 5 / 6 | höchste / niedrigste wählbare Temperatur |
| 7 | Hysterese Zone 1 |
| 8 | Startverzögerung |
| 9 | Einheit 0 °C, 1 °F |
| 10–13 | Korrekturwerte Zone 1 |
| 14 | Zone 1 Ist (int8) |
| 15 | Akku % (0x7F = keiner) |
| 16 / 17 | Spannung ganze V / Zehntel |
| 18 | Zone 2 Soll (nur Zwei-Zonen-Box, Daten ≥ 28 Byte) |
| 21–25 | Hysterese/Korrektur Zone 2 |
| 26 | Zone 2 Ist |
| 27 | Laufzustand |

Ein/Aus: Set-Befehl mit dem zuletzt gelesenen Block und geändertem Byte 1 (nie einzelne Felder raten).
