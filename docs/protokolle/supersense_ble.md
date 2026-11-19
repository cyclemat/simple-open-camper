# SuperSense (comworks) – BLE-Protokoll, rekonstruiert aus der Android-App

> Quelle: Disassemblieren der Android-App `de.comworks.supersense` Version **2.13.121** (APKPure), bereitgestellt
> vom Nutzer 2026-11-19. Nur Protokollwissen – der Code im Projekt ist eine eigene Umsetzung
> (`src/bms.cpp` BMS_SS_TANK, `src/tpms.cpp`). **[sicher]** = steht so im Bytecode, **[unsicher]** = nicht eindeutig.
> Stand im Projekt: BETA in der Victron-Beta, im Hauptstrang (Panel) per Build-Schalter aus. Ungetestet am Gerät.

## 1. Gerätefamilien

| Gerätetyp (App-intern) | BLE-Name beginnt mit | Kommunikation |
|---|---|---|
| `FluidTank` (Tankmodul) | `SSBT` | GATT-Verbindung + Notifications |
| `MotionDetector` | `SSAS` | GATT-Verbindung + Notifications |
| `TirePressure` (TPMS) | `TPMS…`, `SSPS…` oder Service-Data `0xEEFD`/`0xFAF0` | nur Advertising |

Keine Verschlüsselung, kein Pairing/Bonding, keine Authentifizierung. Die App schreibt genau einen Befehl (Init). [sicher]

## 2. Scan

Scan-Filter „Gerät hinzufügen“ (ODER) [sicher]:
1. Service-UUID `a0eeb190-6b2a-45fb-966e-d8e49323691f` (SSBT/SSAS)
2. Service-UUID `0xEEFD` (TPMS Typ A)
3. Service-UUID `0xFAF0` (TPMS Typ A)
4. nur Entwicklermodus: Service `0000fbb0-…` und Hersteller-ID `0x0100`

Zuordnung in dieser Reihenfolge [sicher]:
1. Service Data `0xEEFD`/`0xFAF0` → TPMS Typ A. Anzeige-ID = `"0"` + Hex der MAC-Bytes 3..5, Name `SSPS-PS-0DDEEFF`.
2. Name `SSBT…` → Tankmodul. 3. Name `SSAS…` → Bewegungsmelder. 4. Name `TPMS…`/`SSPS…` → TPMS Typ B.

TPMS-Namen [sicher]: `^(TPMS\d)_((\d)[0-9a-f]{5})$` (Ziffer = Rad: 1 VL, 2 VR, 3 HL, 4 HR) und
`^(SSPS)-(FL|FR|RL|RR|PS)-\d?((\d)[0-9a-f]{5})$` (`PS` = keine Position).

Normalbetrieb: SSBT/SSAS über die gespeicherte MAC; TPMS A: Service Data, Bytes 0..5 = MAC rückwärts;
TPMS B: Hersteller-ID `0x0100`, 16 Byte, MAC an Offset 0 oder 3-Byte-ID an 3..5.

## 3. TPMS – nur Advertising

Aktiv, solange innerhalb von **70 s** ein gültiges Paket kam. [sicher]

### Typ A – Service Data `0xEEFD`/`0xFAF0`, genau 11 Byte [sicher]

| Offset | Länge | Typ | Bedeutung |
|---:|---:|---|---|
| 0 | 6 | bytes | MAC des Sensors, umgekehrte Reihenfolge |
| 6 | 2 | int16 LE | Druck kPa (< 0 ignoriert), bar = kPa / 100 |
| 8 | 2 | int16 LE | Batterie, vermutlich mV [unsicher] |
| 10 | 1 | uint8 | Temperatur °C = Wert − 50, `0xFF` ungültig |

### Typ B – Herstellerdaten Firma `0x0100`, genau 16 Byte ohne Firmen-ID, mit Gerätename [sicher]

| Offset | Länge | Typ | Bedeutung |
|---:|---:|---|---|
| 0 | 6 | bytes | MAC/ID (normale Reihenfolge) |
| 6 | 4 | int32 LE | Druck Pa → bar = / 100000 |
| 10 | 4 | int32 LE | Temperatur 0,01 °C |
| 14 | 1 | int8 | Batterie-Code |
| 15 | 1 | – | nicht ausgewertet (Alarm?) [unsicher] |

Batterie-Code → mV (App, Code < 0 ungültig):
```
code 0..4  : v = code/4*224 + 1136
code 5..28 : v = (code-4)/24*224 + 1360
code >= 29 : v = (code-28)/72*121 + 1584
mV = round(v / 2 / 1023 * 3.6 * 1000)      → 0 = 1999 mV, 100 = 3000 mV
```
Die App hält kPa intern, speichert auf 10 kPa abgerundet, zieht keinen Luftdruck ab. Absolut oder relativ: [unsicher].

## 4. SSBT (Tankmodul) und SSAS (Bewegungsmelder) – GATT

| Rolle | UUID | Properties |
|---|---|---|
| Service | `a0eeb190-6b2a-45fb-966e-d8e49323691f` | – |
| Command | `8d81b2f7-e56f-44bf-94ee-94083f5ea6d7` | WRITE |
| Daten | `8215dd0f-cc20-4d2a-8b64-73907ec4bd16` | READ + NOTIFY |
| Temperatur (nur SSBT) | `0bd4c5d4-62e3-4946-82ea-d7643834d368` | READ + NOTIFY |

Ablauf der App [sicher]: verbinden, Dienste suchen, **1000 ms warten**, Init `0E 01 02 03 04 05 06` schreiben (mit
Antwort), optional 500 ms warten und Daten lesen, Notifications Daten (und Temperatur bei SSBT) einschalten.
Verbindungs-Timeout bei Benutzeraktion 15 s, sonst Auto-Reconnect.

Datenformat [sicher]: genau 8 Byte = 4 Werte BCD (`12 34` → 1234, Nibble A–F → ungültig), `99 99` = kein Sensor.
Leeres Paket direkt nach dem Verbinden → App trennt.

SSBT Füllstand (Kanal 1..4): `% = clamp(raw / 2475 * 100, 0, 100)`; Option „große Schritte“: raw auf Vielfaches von
125 abrunden; „invertiert“: 100 − %. Mit Kalibrierung (`[level0, raw0, level1, raw1, …]`): Punkte nach raw absteigend,
stückweise linear, außerhalb Randwert, große Schritte auf 5 % abrunden. Gasflasche (Wiegen):
`((% × calibrated_weight / 100) − tara_weight) / capacity × 100`.

SSBT Temperatur: `°C = raw × (5 / 1023) × 100 − 50` (raw 153 → 24,8 °C).

SSAS: W1 links, W2 rechts, W3 vorne, W4 hinten (9999 = fehlt); Bedeutung der Zahl [unsicher], in der App toter Code.

## 5. Umsetzung im Projekt (Stand 2026-11-19)

- Tankmodul: `src/bms.cpp` Typ `BMS_SS_TANK` (Schalter `BMS_SUPERSENSE_ON`), kurz verbinden alle 20 s, Init
  schreiben, Daten + Temperatur **lesen** (statt Notify), trennen. Ohne Kalibrierung/Invertierung.
- Reifendruck: `src/tpms.cpp` (Schalter `WITH_TPMS`), Typ A und B, Kennung = MAC aus den Daten. **ALPHA – wird
  nirgends kompiliert** (Wunsch des Nutzers 2026-11-19); Karte/Kacheln im Victron-Display liegen bereit (#if WITH_TPMS).
- Bewegungsmelder: nicht umgesetzt.
- Offen am Gerät: Batterie Typ A (mV?), Druck absolut/relativ, Byte 15 Typ B, ob SSBT ohne Init Werte schickt,
  ob der SuperSense PRO Laser mit eigenem Bluetooth als `SSBT…` wirbt.
