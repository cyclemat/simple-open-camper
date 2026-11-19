# SmartShunt-Satellit – ESP32-C3 Super Mini + INA228

Batteriewächter wie ein Victron SmartShunt, als Satellit von **Simple Open Camper**: misst Spannung und Strom am
externen Shunt, rechnet Ladezustand (SOC) und Restzeit und schickt alles ans Panel (UDP/MQTT) und an Home Assistant.
Auf Wunsch zusätzlich **VE.Direct** an einen Victron GX. Details zur Auswertung: [`docs/smartshunt.md`](../docs/smartshunt.md).

> **Beta, an echter Hardware noch ungetestet.** Keine Schutzfunktion: Hauptsicherung, BMS und Schutzgeräte bleiben
> nötig. Hochstrom (Sammelschienen, Kabel, Shunt-Wärme – bei 1000 A sind es 50 W) gehört in fachkundige Hände.

## Bauteile

| Teil | Hinweis |
|---|---|
| ESP32-C3 Super Mini | 4 MB Flash, USB-C |
| INA228-Modul (I²C) | z. B. Adafruit 5832 oder ähnliche; **eingebauten Shunt entfernen**, VBUS getrennt führen (s. u.) |
| Externer Shunt | 50–2000 A, 50–100 mV (z. B. 500 A / 50 mV) |
| DC/DC-Wandler 12/24 V → 5 V | klein (100 mA reichen), eigene Sicherung |
| 2 × Feinsicherung 0,5–1 A | für VBUS-Messleitung und Versorgung, direkt am Batterie-Plus |
| optional: Digital-Isolator / Optokoppler | für VE.Direct an den GX (Masse-Trennung, siehe unten) |
| optional: Zusatzsensoren | BME280, SHT, AHT20, DHT22 … (Karte „Weitere Sensoren“) |

## Pinout ESP32-C3 Super Mini

Draufsicht, USB-C oben. Die Lage der Reihen kann je nach Hersteller abweichen – **Aufdruck auf der Platine beachten**.

```text
                 ┌────[USB-C]────┐
          5V  ── │ 5V        GP5 │ ── SDA  (INA228, Standard)
         GND  ── │ GND       GP6 │ ── SCL  (INA228, Standard)
         3V3  ── │ 3V3       GP7 │
               ─ │ GP4       GP8 │ ─  blaue LED (aktiv LOW), Strapping – nicht für I²C nehmen
               ─ │ GP3       GP9 │ ─  BOOT-Taste (3 s = Einrichten per Bluetooth), Strapping
               ─ │ GP2      GP10 │
               ─ │ GP1    GP20 RX│ ── VE.Direct RX (optional, Standard)
               ─ │ GP0    GP21 TX│ ── VE.Direct TX (Standard)
                 └───────────────┘
   Analog (ADC): GP0–GP4 · Strapping: GP2, GP8, GP9 · Konsole/Log: über USB
```

Alle Pins sind in der Weboberfläche umstellbar (Karte „SmartShunt“); belegte Pins werden gegen GPIO-Anschlüsse und
Zusatzsensoren geprüft.

## Schaltplan (Low-Side-Messung)

```text
 BATTERIE +  ──┬── HAUPTSICHERUNG ─────────────────────────────┬──► Verbraucher / Lader (+)
               │                                               │
               ├── F1 0,5 A ──► INA228 VBUS  (Batteriespannung messen)
               │
               └── F2 1 A ──► DC/DC 12/24 V → 5 V ──► Super Mini 5V
                                          GND ──┐
                                                │
 BATTERIE −  ──► [ BATT ─┤ SHUNT ├─ SYS ] ──────┴──────────────────────► System-Minus
                        │          │                                       (ALLE Rückleiter,
                  IN+ ◄─┘          └─► IN−    Kelvin-Leitungen,             auch Lader/GX/USB!)
                  (INA228)          (INA228)  dünn, verdrillt, direkt an
                                              die Messschrauben des Shunts

  INA228 VCC ◄── Super Mini 3V3        INA228 GND ◄── System-Minus (= Super Mini GND)
  INA228 SDA ◄── GP5                   INA228 SCL ◄── GP6           (Adresse ab Werk 0x40)
```

- **Strom + = Laden, − = Entladen.** Zeigt die Weboberfläche beim Laden Minus: „Strom umkehren“ anhaken.
- **Batteriespannung** = VBUS − Shuntspannung (rechnet die Firmware). VBUS am Modul **nicht** mit IN− verbinden
  (bei Breakouts oft per Lötbrücke verbunden → auftrennen), sondern über F1 an Batterie-Plus.
- **Keine Rückleitung am Shunt vorbei** – sonst fehlt dieser Strom in der Bilanz (auch Masse über Chassis, USB, GX).
- Die Elektronik wird über den Shunt versorgt (DC/DC-Minus an System-Minus) und damit mitgezählt.
- Erst mit kleiner Last testen, Nullpunktabgleich nur bei wirklich stromlosem Shunt.

## VE.Direct an einen Victron GX (optional)

Sendet die Werte im VE.Direct-Text-Format (19200 Baud, 8N1) wie ein Batteriewächter, jede Sekunde abwechselnd
Messwerte und Historie. Ab Werk an **TX = GP21** (RX = GP20, wird nicht ausgewertet – kein HEX-Protokoll).

```text
 Super Mini                    Isolator (empfohlen)              VE.Direct-Buchse (JST-PH 2,0 mm, 4-polig)
 GP21 TX  ─────────────────►  IN ─────── OUT  ─────────────────►  Pin 3  TX   (Daten zum GX)
 GP20 RX  ◄─────────────────  OUT ────── IN   ◄─────────────────  Pin 2  RX   (optional)
 GND      ─────────────────   GND1      GND2  ─────────────────   Pin 1  GND
 3V3      ─────────────────   VCC1      VCC2  ◄─────────────────  Pin 4  +V   (Versorgung der GX-Seite)
```

- Mit einem normalen VE.Direct-Kabel 1:1 an einen VE.Direct-Anschluss des GX – die Belegung oben ist die eines
  Victron-Geräts (Pin 3 = dessen Sendeleitung).
- Ohne Isolator entsteht über die GX-Masse eine zweite Rückleitung am Shunt vorbei → Messfehler. Darum Isolator
  oder Optokoppler (Pegel 3,3 V/5 V nach GX-Seite).
- **Produkt-ID:** ab Werk unsere eigene Kennung **0xA38F** (nicht von Victron vergeben). Der GX erkennt Geräte an
  der ID; wer möchte, trägt in der Weboberfläche eine Victron-ID ein (SmartShunt 500 A `0xA389`, 1000 A `0xA38A`,
  2000 A `0xA38B`, BMV-712 `0xA381`) – auf eigene Verantwortung. Ob der GX die eigene Kennung annimmt, ist noch
  nicht geprüft.
- Gesendet: `PID V I P CE SOC TTG Alarm Relay AR FW MON` bzw. `H1–H12 H17 H18`, Prüfsumme je Block.

## Bauen und Flashen

```bash
pio run -d shunt -e shunt-c3 -t upload        # bzw. im Panel-Menü Umgebung "shunt-c3"
```

Erste Einrichtung wie jeder Satellit: WLAN **„SOC-Satellite-…“** (Passwort `panelsat`), Seite öffnet sich von
selbst (sonst http://4.3.2.1/), oder vom Panel aus per Knopf (BOOT 3 s gedrückt halten). Dann in der Karte
**„SmartShunt“** Shunt-Nennwerte, Kapazität, Ladespannung usw. eintragen und „Speichern“.

## Einstellungen (Auswahl)

| Feld | Bedeutung |
|---|---|
| Shunt Nennstrom / Nennspannung | z. B. 500 A / 50 mV |
| Kapazität, Bezugszeit | z. B. 200 Ah, C20 = 20 h |
| Ladespannung, Schweifstrom, Erkennungszeit | Vollerkennung → 100 % (z. B. LiFePO4 14,0 V, 4 %, 3 min) |
| Entladeboden | für Restzeit und Alarm (keine Abschaltung) |
| Peukert, Ladewirkungsgrad | LiFePO4 ≈ 1,01 / 99 %, AGM ≈ 1,15 / 95 % |
| Stromschwelle | kleinere Ströme zählen für SOC/Anzeige als 0 (Zähler bleiben genau) |
| Alarme | Ladezustand, Unter-/Überspannung (mit Hysterese) |
| VE.Direct | an/aus, TX/RX-Pin, Produkt-ID |
