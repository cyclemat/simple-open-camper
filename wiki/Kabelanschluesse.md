# Kabelanschlüsse (LIN, CI-BUS, Zigbee-Modul)

### 🔌 Kabelanschlüsse (LIN-Bus und CI-BUS)

Truma CP Plus, Alde und der Hobby-CI-BUS hängen per Kabel am Panel. **Hymer Connect braucht kein Kabel** – das
läuft nur per Bluetooth zur SCU (siehe oben).

**Grundregeln**

- Die Pins des ESP32 vertragen nur **3,3 V**. Den Bus nie direkt anschließen: LIN (12 V) nur über einen
  **LIN-Transceiver**, CI-BUS (RS232, ±12 V) nur über einen **RS232-TTL-Wandler**.
- **Masse (GND)** von Panel, Wandler und Fahrzeug verbinden.
- **RX** des Panels an den **Ausgang** des Wandlers, **TX** des Panels an dessen **Eingang**. Kommt nichts an:
  RX und TX tauschen.
- Jedes Gerät braucht eine **eigene serielle Schnittstelle** (UART). UART0 ist für Log/USB reserviert.

| Gerät | Bus und Stecker | Wandler | Baud | Master | Einstellen |
|---|---|---|---|---|---|
| Truma CP Plus | LIN, RJ12: **Pin 3 = LIN, Pin 5 = Masse** (am Combi oder per Verteiler) | LIN-Transceiver | 9600 | CP Plus | `platformio.ini` → `[truma]` |
| Alde 3020/3030 | LIN, **gelbe** RJ12-Buchse: **Pin 4 = LIN, Pin 2 = Masse** | LIN-Transceiver + Master-Widerstand | 19200 | **Panel** | `platformio.ini` → `[alde]` |
| Hobby (CI-BUS) | RS232 am LSG, Kabelsatz „Bluetooth-Modul“ von Hobby | RS232-TTL-Wandler (MAX3232) | 115200 | LSG | am Panel: **Einstellungen → Fahrzeug** |
| Hymer Connect | – (Bluetooth) | – | – | – | Einstellungen → Fahrzeug |
| Votronic (Display-Link) | RJ11 6-polig: **Pin 1 = TXD, Pin 6 = Masse** (Pin 3 frei) | Spannungsteiler, falls über 3,3 V | 1000 | Votronic-Gerät | am Panel: **Einstellungen → Votronic** |

**LIN-Transceiver** (TJA1020/TJA1021, fertiges Modul; MCP2025 sinngemäß):

| Transceiver | Anschluss |
|---|---|
| VBAT / VS | +12 V Bordnetz (über eine Sicherung) |
| GND | Fahrzeugmasse **und** GND des Panels |
| LIN | LIN-Leitung (Pin siehe Tabelle oben) |
| RXD | RX-Pin des Panels – offener Ausgang: Pull-up 4,7 kΩ nach 3,3 V (auf fertigen Modulen meist schon drauf) |
| TXD | TX-Pin des Panels |
| SLP / NSLP (MCP2025: CS) | an 3,3 V – oder an einen GPIO und diesen als `lin_en` eintragen (das Panel setzt ihn auf High) |

- **Alde:** Das Panel ist LIN-Master. Dafür gehört ein **1-kΩ-Widerstand mit Diode** (z. B. 1N4148, Anode an +12 V)
  von +12 V an die LIN-Leitung – viele Module haben dafür eine Brücke „Master“.
- **CP Plus:** Das CP Plus ist Master, das Panel antwortet nur – **kein** 1-kΩ-Widerstand, Brücke „Master“ offen.
- Alde 3030 (optional): Der interne rote Bus (Glykol-/Warmwassertemperatur) lässt sich mit einem zweiten Transceiver
  nur mithören (`red_uart`, `red_rx`).

**RS232-TTL-Wandler für den Hobby-CI-BUS** (z. B. MAX3232-Modul):

| Wandler (TTL-Seite) | Panel |
|---|---|
| VCC | **3,3 V** (nicht 5 V – sonst liegen 5 V am ESP32) |
| GND | GND |
| TXD (Ausgang) | RX-Pin |
| RXD (Eingang) | TX-Pin |

Die RS232-Seite (D-Sub 9) kommt an den Kabelsatz „Bluetooth-Modul“ von Hobby (TX, RX, Masse des LSG). Die Belegung
des Kabelsatzes steht im Projekt [BastyJuice/Hobby_Caravan_Serial_2_CI-BUS_Connectivity](https://github.com/BastyJuice/Hobby_Caravan_Serial_2_CI-BUS_Connectivity).
Kommt nichts an, RX und TX auf der RS232-Seite tauschen (Nullmodem). Schnittstelle und Pins werden am Panel gewählt
(kein Neubau nötig); der Status zeigt, ob das LSG antwortet.

**Pins je Panel**

| Panel | Anschluss | Hinweis |
|---|---|---|
| Waveshare 7B | UART-Stiftleiste, **GPIO 15/16** (Umschalter auf „UART“, nicht RS485) | Freie Pins sind knapp – die Stiftleiste reicht für **ein** Gerät (CP Plus, Alde **oder** Hobby). Doku und Wiki widersprechen sich bei der Richtung: kommt nichts an, RX/TX tauschen. |
| CrowPanel P4 | freie GPIOs der Steckleisten | Pins in `p4/platformio.ini` bzw. (Hobby) am Panel eintragen. **UART3 / GPIO 27/28** belegt das Zigbee-Funkmodul (H2) – nicht verwenden, wenn es gesteckt ist. |
| Rundes Panel 1,46" | UART-Buchse hinten (1,25 mm, 4-polig): **1 = RX (GPIO 44), 2 = TX (GPIO 43), 3 = 5 V, 4 = GND** | Dort hängt das **Zigbee-Funkmodul (H2)**, siehe unten. Die Buchse hat **5-V-Pegel** (Pegelwandler auf der Platine). |

**Zigbee-Funkmodul (ESP32-H2) am runden Panel** *(Beta, ungetestet)*: Das Modul ist für den 3,3-V-Steckplatz des P4
gebaut, die Buchse hinten am runden Panel liefert aber 5 V und 5-V-Pegel. Dazwischen gehören:

| Von der Buchse | Über | Zum H2-Modul |
|---|---|---|
| Pin 3 (5 V) | 3,3-V-Regler (z. B. AMS1117-3.3, HT7333) | 3V3 |
| Pin 4 (GND) | – | GND |
| Pin 2 (TX des Panels) | Pegelwandler 5 V → 3,3 V (z. B. BSS138-Modul) | RX des Moduls |
| Pin 1 (RX des Panels) | Pegelwandler 3,3 V → 5 V | TX des Moduls |

Danach im Browser-Editor **Einstellungen → Zigbee → „Zigbee-Modul verwenden“** einschalten (ab Werk aus) und Geräte
anlernen; dort lassen sie sich auch umbenennen, einstellen und entfernen. Neue Geräte bekommen von selbst eine Kachel.
Die Belegung stammt aus Elecrows Schaltplan – vor dem Löten nachmessen. **Nie 5 V oder 5-V-Pegel direkt an das H2!**

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
