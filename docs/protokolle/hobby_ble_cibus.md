# Hobby: HobbyConnect-Box (Bluetooth) und CI-BUS am LSG (Kabel)

Umsetzung: `src/hobby.cpp` (Panel, `WITH_HOBBY`, ab Werk beide Wege **aus**). Werte erscheinen als Satellit „Hobby“
(`satio:Hobby/<punkt>`), die Bordbatterie (IBS) zusätzlich als Batteriewächter-Kachel `hobby:bat`.
Einstellungen: Panel > Einstellungen > **Fahrzeug** (zusammen mit Hymer Connect). Serieller Befehl `hobby` zeigt alle
empfangenen Variablen (auch nicht ausgewertete) – hilfreich für andere Modelle.

Quellen (nur Protokollwissen, eigene Umsetzung):
- AndreasKirsten79/hobby-caravan-pi – `BLE-PROTOKOLL.md`, `ble_bridge.py` (ohne Lizenzangabe → nichts übernommen)
- paveltresnak/hobby-caravan-esphome-ble, esphome PR #13327 (dort zitiert)
- BastyJuice/Hobby_Caravan_Serial_2_CI-BUS_Connectivity – CI-BUS seriell (MIT), Variablentabelle

## Bluetooth (HobbyConnect-Box)

| | |
|---|---|
| Name | `HobbyConnect Data` |
| Dienst | `C7841029-FE7C-4894-8532-F97908EF1AE4` |
| Kanal | `00000001-0000-1000-8000-00805f9b34fb` (NOTIFY + WRITE) |
| Anmeldung | `net-BT_ID-<eigene BT-MAC, klein>` dann `net-BT_VARS` (alle Variablen schicken) |
| Daten | Text `KEY:WERT`; endet ein Paket auf `@`, folgt der Rest im nächsten Paket |
| Schreiben | höchstens 17 Zeichen am Stück: länger → `erste 17 + "@"`, 100 ms, Rest |
| Schalten | `cmd-tgl:<KEY>` (nur Umschalten! → zustandsbewusst: nur senden, wenn Soll ≠ Ist) |
| Wert setzen | `net-<KEY>-<WERT>` (Dimmer `LIGHT_DIMn` 0…15, `FRIDGE_SOURCE` 0–3, `FRIDGE_TEMP` 1–5) |
| Sitzung | nach 300 s neu verbinden (wie die Bridge), 200 ms zwischen Befehlen |

Werte: `24,0^C` → `°C`; `WATER_LEVEL` 0–4 Stufen (×25 %) oder `37:37` (Teil vor `:`). Neuere Firmware (V012000) hat
keine `LIGHT_DIM*`, sondern nur Ein/Aus-Lichter. `LIGHT_DUSCHE`/`LIGHT_WASCH` sind je nach Fahrzeug vertauscht
(Kachel umbenennen).

## CI-BUS seriell am LSG (ersetzt die Box)

RS232-Pegel (±12 V) → nur über RS232-TTL-Wandler (MAX3232, 3,3 V) an den ESP32. 115200 Baud, 8N1. Kabelsätze
„Bluetooth-Modul“ von Hobby (Wohnwagen bzw. Reisemobil) + Stecker für die serielle Seite.

Ablauf beim Start (und nach 2 min ohne Zeile): `!sys`, 0,5 s, `!sof`, 0,5 s, `!gvl` → Variablenliste bis `!end`
(max. 20 s), dann `!son` (Datenstrom an). Zeilen enden mit `\r\n`.

- Daten: `$<nr>:<wert>`, z. B. `$2:14,2 V`
- Variablenliste: Format nicht genau bekannt – ausgewertet wird je Zeile die erste Zahl und der erste Name
  (`[A-Z][A-Z0-9_]+`), danach optional `:`/`=` und ein Wert. Klappt das nicht, gelten die Nummern der Tabelle
  aus dem BastyJuice-README (in `DEFS[].cid`).
- Umschalten: `!tgl:<nr>`, Wert setzen: `!set:<nr>=<wert>` (z. B. `!set:10=Cool`, `!set:93=60`)
- Hauptschalter: `!run:30` ein, `!run:31` aus (kein Umschalten). **Vorsicht:** hängt das Panel am Bordnetz hinter dem
  Hauptschalter, schaltet es sich damit selbst aus.

Wichtige Nummern (laut Tabelle, je Fahrzeug verschieden → `!gvl`): 1 IBS0_IBAT, 2 IBS0_UBAT, 3 IBS0_SOC2,
4 IBS0_REMAINING_TIME, 5 IBS0_TEMPERATURE, 9 AC_DOM_FJ_ENABLE, 17 LINE_EN, 18 HS_EN, 23 TEMP_IN, 24 TEMP_OUT, 25 UBAT,
29 FRESH_WATER_LEVEL, 34–39 HEATER_*, 61–76 LIGHT_*, 90–93 TH_*, 104–109 FRIDGE_*.

## Baugruppen

Gibt es `<GRUPPE>AVAILABLE` = 0 (FRIDGE_, HEATER_, AC_DOM_FJ_, AC_TRUMA_, ULTRAHEAT_, IBS0_, CHARGER0_, TH_), werden die
Werte dieser Gruppe nicht angezeigt.

## Offen

Textwerte (Klima-Modus/Lüfter, Kühlschrank-Quelle) noch nicht als Kachel/Bedienung; Klima-Soll, Warmwasser 40/60 °C
setzen; Editor-Reiter (Einrichtung derzeit nur am Panel). Ungetestet an echter Box/echtem LSG.
