# SmartShunt-Satellit (ESP32-C3 Super Mini + INA228)

Batteriewächter wie ein Victron SmartShunt, als Satellit: misst am externen Shunt, rechnet Ladezustand und Restzeit
und schickt alles ans Panel (UDP/MQTT) und an Home Assistant (MQTT-Discovery). Grundlage: Spezifikation
„CycleMat Open SmartShunt v0.1“ (Nutzer, 2026-11-19). **Beta, ungetestet an echter Hardware.**
Keine Schutzfunktion – Sicherungen, BMS und Schutzgeräte bleiben nötig.

## Bauen

Eigenes Projekt `shunt/` (Quellen aus `src/`, Pakete aus `satellite/`): `pio run -d shunt -e shunt-c3 -t upload`,
im Panel-Menü Umgebung `shunt-c3`. Satelliten-Firmware mit `-DSAT_SHUNT=1` und `-DSAT_BLE_SENS=0` (keine
Bluetooth-Sensoren; Bluetooth nur zum Einrichten per Knopf). GPIO-Anschlüsse und Zusatzsensoren bleiben.

## Anschluss (Low-Side)

```text
BAT+ ── Hauptsicherung ──┬── Verbraucher/Lader (+)
                         ├─ kleine Sicherung → VBUS (INA228)
                         └─ kleine Sicherung → DC/DC 3,3 V → ESP32-C3, INA228
BAT− ──[ BATT | SHUNT | SYS ]── System-Minus (alle Rückleiter)
             IN+      IN−       INA228, Kelvin-Leitungen direkt an die Messschrauben
INA228 SDA/SCL → GPIO 5 / 6 (einstellbar; nicht GPIO 9 = BOOT-Taste)
```

Strom + = Laden. Falsch herum: „Strom umkehren“. Batteriespannung = VBUS − Shuntspannung. Beim Breakout den
eingebauten Shunt entfernen. Keine Rückleitung darf den Shunt umgehen.

## Umgesetzt (`src/sat/sat_shunt.cpp`)

- INA228: Kennung prüfen, ±163,84 mV, dauernd Bus/Shunt/Temperatur, 16-fach gemittelt; Messung alle 100 ms.
- Strom aus Shunt-Nennwerten (A, mV), Offset/Faktor für Strom und Spannung, Nullpunktabgleich (nur < 2 % Nennstrom).
- Zähler (Lebenszeit): geladene/entladene Ah und Wh – tatsächlich gemessen, ohne Schwelle.
- SOC: Stromschwelle, Ladewirkungsgrad nur beim Laden, Peukert beim Entladen (Bezugszeit C20 o. ä.);
  SOC unbekannt bis zur ersten Vollladung oder bis er von Hand gesetzt wird.
- Vollerkennung: Spannung ≥ Ladespannung und 0 ≤ I ≤ Schweifstrom ununterbrochen über die Erkennungszeit →
  100 % (einmal je Ereignis, danach erst wieder nach Verlassen der Bedingungen).
- Restzeit bis zum Entladeboden aus gleitendem Mittel des Stroms (Peukert); bei Laden/Ruhe keine Angabe.
- Historie: tiefste/letzte Entladung, Zyklen (unter 65 % → über 90 %), vollständige Entladungen, Synchronisierungen
  (automatisch/von Hand), Min/Max Spannung, max. Strom/Leistung, Alarmzahl.
- Alarme mit Hysterese: Ladezustand niedrig, Unter-/Überspannung, Messfehler.
- Speicher: Einstellungen und Zustand im NVS (Zustand alle 60 s, einstellbar). FRAM noch nicht.
- Punkte: analog51 Spannung, 52 Strom, 53 Leistung, 54 Ladezustand, 55 Verbraucht (Ah, negativ), 56 Restzeit (h),
  57/58 geladen/entladen gesamt (kWh), 59 Zyklen, 60 Chiptemperatur.
- Weboberfläche: Karte „SmartShunt“ (/api/shunt, /api/shunt/action: sync, soc, zero, reset_hist).

## Offen

FRAM, Temperaturfühler für Batterie/Shunt, Starter-/Mittelpunktspannung, Trend-Ringpuffer, Shunt-TCR,
INA228-Ladungsakkumulator als zweite Referenz, VE.Direct-Ausgabe (siehe Notizen), Darstellung als
Batteriewächter-Kachel am Panel (statt einzelner Punkte).
