# Fahrzeug: Hymer und Hobby

### 🚐 Hymer Connect

- Für Fahrzeuge der Erwin Hymer Group mit **HYMER Connect** (Hymer, Bürstner, Dethleffs, Eriba, LMC, Carado …):
  Licht, Hauptschalter, Wasserpumpe, Batterie, Lithium-BMS, Frisch-/Abwasser, Solar, Landstrom, Außentemperatur –
  direkt per Bluetooth von der SCU, **ohne Cloud**.
- Am Panel unter **Einstellungen → Fahrzeug** „Hymer Connect“ einschalten, **Hymer koppeln** tippen und dann am
  Bedienteil im Fahrzeug **CONNECTION** drücken (3 Minuten Zeit). Am Satelliten geht das genauso in der Weboberfläche.
- Die Werte erscheinen im Editor wie die Anschlüsse eines Satelliten namens **„Hymer“** und lassen sich aufs Panel
  ziehen; Lichter und Schalter sind per Tippen schaltbar.
- Nur Bluetooth – ein Kabelanschluss an die SCU ist nicht vorgesehen.
- Noch **nicht an einer echten SCU getestet** – Rückmeldungen sehr willkommen.

### 🏕️ Hobby (HobbyConnect-Box oder CI-BUS) *(Beta, ungetestet)*

- Für Wohnwagen und Reisemobile von **Hobby**: Lichter (schaltbar), Bordbatterie (Spannung, Strom, Ladezustand,
  Restzeit, Temperatur), Frischwasser, Innen-/Außentemperatur, Landstrom, Heizung, Warmwasser, Kühlschrank, Klimaanlage,
  Hauptschalter – direkt am Panel, ohne App und ohne Cloud.
- **Zwei Wege**, einzeln schaltbar und **ab Werk aus** – unter **Einstellungen → Fahrzeug**:
  - **HobbyConnect-Box per Bluetooth:** einschalten, die gefundene Box antippen. Die Hobby-App auf dem Handy vorher
    trennen (die Box spricht nur mit einem Gerät).
  - **CI-BUS am LSG per Kabel** (ersetzt die Box): RS232-TTL-Wandler (z. B. MAX3232, **3,3 V**) an eine freie serielle
    Schnittstelle, Schnittstelle (UART1, UART2 …) sowie RX-/TX-Pin am Panel wählen. 115200 Baud; die Nummern der Werte
    holt das Panel selbst vom LSG. Kabelsatz „Bluetooth-Modul“ von Hobby plus Stecker für die serielle Seite.
    Waveshare 7": UART-Stiftleiste (GPIO 15/16). **Niemals RS232 (±12 V) direkt an den ESP32!**
    Verdrahtung: [Kabelanschlüsse](Kabelanschluesse).
- Die Werte erscheinen im Editor wie die Anschlüsse eines Satelliten **„Hobby“**; die Bordbatterie zusätzlich als
  **Batteriewächter-Kachel**. Am Panel legt **„Kacheln anlegen“** alles auf einmal an. Der **Hauptschalter** geht nur
  über das Kabel – Vorsicht, wenn das Panel selbst hinter dem Hauptschalter hängt.
- `LIGHT_DUSCHE`/`LIGHT_WASCH` sind je nach Fahrzeug vertauscht – Kachel einfach umbenennen. Serieller Befehl `hobby`
  zeigt alle empfangenen Werte (auch unbekannte) – bitte bei anderen Modellen mitschicken.
- Protokoll: [`docs/protokolle/hobby_ble_cibus.md`](https://github.com/cyclemat/simple-open-camper/blob/main/docs/protokolle/hobby_ble_cibus.md).

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
