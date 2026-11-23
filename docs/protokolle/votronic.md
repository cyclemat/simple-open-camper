# Votronic – Bluetooth (S-BC) und Display-Link (Kabel)

Quelle: [syssi/esphome-votronic](https://github.com/syssi/esphome-votronic) (Apache-2.0), Stand 2026-11-21 (7c10fa4).
Umsetzung im Panel: `src/votronic.*` (eigener Code nach diesem Protokollwissen). **Beta – nicht an echten Geräten getestet.**

## Bluetooth Connector S-BC

Unterstützt laut Vorlage: Solarregler SR/MPP ab 2014 (S/N 14Vxx.xxxxx), Batteriecomputer S + Smart Shunt.
Ungetestet: VBCS-Triple, VPC Jupiter + Smart Shunt.

- Kopplung: am S-BC die Taste kurz drücken, dann verbinden. Verschlüsselung mit MITM, das Panel gibt die PIN ein
  (Passkey, ab Werk **173928**). Gelungen = beide LEDs leuchten gleichzeitig.
- Dienste:
  - Kopplung `70521e61-022d-f899-d046-4885a76acbd0`
  - Messwerte `d0cb6aa7-8548-46d0-99f8-2d02611e5270`
    - Batteriecomputer `9a082a4e-5bcc-4b1d-9958-a97cfccfa5ec` (Notify, 20 Byte)
    - Solarregler `971ccec2-521d-42fd-b570-cf46fe5ceb65` (Notify, 19 Byte)
  - Aufzeichnungen `ae64a924-1184-4554-8bbc-295db9f2324a` (Befehl `ac12f485-…`, Daten `b8a37ffe-…`) – nicht genutzt
- Zahlen Little Endian.

Batteriecomputer (20 Byte):

| Byte | Inhalt |
|---|---|
| 0–1 | Batteriespannung, 10 mV |
| 2–3 | zweite Batterie (Starter), 10 mV |
| 4–5 | Restkapazität, Ah |
| 8 | Ladezustand, % |
| 10–12 | Strom, vorzeichenbehaftet 24 Bit, mA (+ = laden) |
| 13–14 | Nennkapazität, 0,1 Ah |

Solarregler (19 Byte):

| Byte | Inhalt |
|---|---|
| 0–1 | Batteriespannung, 10 mV |
| 2–3 | PV-Spannung, 10 mV |
| 4–5 | PV-Strom, 100 mA |
| 8 | Batteriestatus (Bit 0 I, 1 U1, 2 U2, 3 U3; 0 = Standby) |
| 9 | Reglertemperatur, °C (gültig, wenn Byte 12 ≠ 0) |
| 12 | Reglerstatus (Bit 3 aktiv, 4 reduziert, 5 AES aktiv) |
| 13–14 | geladen seit Zurücksetzen, Ah |
| 15–16 | Energie seit Zurücksetzen, 10 Wh |
| 17–18 | PV-Leistung, 0,1 W |

## Display-Link (Kabel)

Geräte laut Vorlage: Solarregler SR/MPP/CI (ab Mitte 2014), VBCS Triple, VAC Triple, VCC 1212-30 / 1290, Smart Shunt.

- Westernstecker RJ11, 6-polig (Draufsicht): 1 TXD (zum ESP-RX), 2 VBAT+ (nicht zur Versorgung!), 3 **frei lassen**,
  4 GND, 5 RXD, 6 GND. Höchstens 50 mA je Pin.
- UART 1000 Baud, 8N1. Das Panel liest nur (RX).
- Rahmen 16 Byte: `AA <Typ> <13 Byte Daten> <XOR über Byte 1..14>`. Lücke > 150 ms = neuer Rahmen.

| Typ | Gerät | Inhalt (Byte) |
|---|---|---|
| `1A` | Solarregler | 2–3 Batterie 10 mV, 4–5 PV 10 mV, 6–7 PV-Strom 100 mA, 11 Temperatur °C, 12 Lademodus, 13 Batteriestatus, 14 Reglerstatus |
| `3A` | Ladegerät (VBCS/VAC) | 2–3 Batterie, 4–5 Batterie 2, 6–7 Strom s16 100 mA, 10 Auslastung %, 11 Temperatur 0,1 °C, 12 Lademodus, 13/14 Status |
| `7A` | Ladebooster (VCC) | wie `3A`, Byte 11 Batterietemperatur |
| `CA` | Batteriecomputer 1 | 2–3 Batterie, 4–5 Batterie 2, 6–7 Rest-Ah, 10 Ladezustand %, 12–14 Strom s24 mA |
| `DA` | Batteriecomputer 2 | 6–7 Nennkapazität 0,1 Ah, 12 Batterietyp, 13 Batteriestatus |
| `FA` | Batteriecomputer 3 | unbekannt |
| `4A` | leer | wird übergangen |

Ladegerät-Status (Byte 14): Bit 2 lädt Batterie 1, 3 lädt Batterie 2, 4 reduziert, 5 Netzleistung begrenzt.

Lademodus (DIP-Schalter, 12-V- bzw. 24-V-System): 24/124 Blei-Säure, 53/153 Gel, 34/134 AGM 14,4 V, 47/147 AGM 14,7–14,8 V,
89/189 LiFePO4 13,9 V, 82/182 14,2 V, 84/184 14,4 V, 86/186 14,6 V, 88/188 14,8 V.
