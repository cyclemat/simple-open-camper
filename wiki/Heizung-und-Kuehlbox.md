# Heizung und Kühlbox

### 🔥 Dieselheizung *(Beta, ungetestet)*

- China-Dieselheizungen mit Bluetooth: **Vevor/Hcalory** mit der App „AirHeaterBLE“ (auch verschlüsselte Variante) und
  **HeaterCC/AirHeater**. Namen wie BYD-…, Heater…, „Air Heater“, HC-…, BAC-…, CRONUS…, Hcalory…, boygu… werden erkannt.
- Einrichten unter **Gas/Heizung → Heizung/Kühlbox suchen** (Editor: Reiter Gas/Heizung), erscheint als „Dieselheizung“. App vorher schließen.
- Kachel: an/aus, Stufe bzw. Solltemperatur, Innenraumtemperatur; Fehler (E1…E10) rot.
- **Bedienen:** Kachel lange drücken → **Ein/Aus**, **Stufe ±**, **Solltemperatur ±**. Auf dem Victron-Display in
  der Weboberfläche (Akku-Karte).
- Sicherheit: nur Ein/Aus, Stufe und Temperatur – keine Pumpen- oder Einstellwerte. Meldet die Heizung einen Fehler,
  ist nur noch Ausschalten möglich. Abstand zwischen Befehlen mind. 0,7 s.
- Passkey der App ist ab Werk 1234; wer ihn geändert hat, trägt ihn im Feld **PIN / Passwort** ein.
- Noch nicht: Hcalory MVP1/MVP2 und Sunster (andere Protokolle).

### 🧊 Kühlbox *(Beta, ungetestet)*

- Kompressor-Kühlboxen mit den Apps **Alpicool / Car Fridge Freezer** und **IceCube / Plug-in Festivals**, auch
  Bodega, Brass Monkey u. a. (Namen WT-…, A1-…, AK1/2/3-…), ein- und zweizonig.
- Einrichten unter **Gas/Heizung → Heizung/Kühlbox suchen** (Editor: Reiter Gas/Heizung), erscheint als „Kühlbox“. Beim ersten Verbinden ggf. die
  **Settings-Taste an der Box** drücken. App vorher schließen – solange sie verbunden ist, ist die Box für andere stumm.
- Kachel: Innentemperatur groß, Soll darunter (zweite Zone daneben).
- **Bedienen:** Kachel lange drücken → **Ein/Aus**, **Soll ±** je Zone. Einstellungen werden immer aus dem zuletzt
  gelesenen Stand geschrieben, nichts geraten; Reset wird nie gesendet.

### 🔥 Truma-Heizung

- **CP Plus:** LIN-Transceiver an die UART-Stiftleiste (Schalter auf „UART“), LIN an RJ12 Pin 3, Masse an Pin 5
  (am Combi oder per Verteiler). Danach am CP Plus einmal **PR SET → Init** ausführen.
  Verdrahtung im Detail: [Kabelanschlüsse](Kabelanschluesse).
- **iNet X:** In den Einstellungen das Protokoll einschalten, **„iNet X koppeln“** tippen und dann am Bedienteil
  **„Gerät hinzufügen“** öffnen (3 Minuten Zeit). Das iNet X merkt sich höchstens vier Geräte.

### ♨️ Alde

- LIN-Leitung an die **gelbe RJ12-Buchse** (Pin 4, Masse Pin 2), Modell und Pins unter `[alde]` eintragen.
  Das Panel ist hier LIN-Master (1-kΩ-Widerstand nötig) – siehe [Kabelanschlüsse](Kabelanschluesse).
- Das Panel fragt den Kessel alle 500 ms ab und sendet nur bei Änderungen – Einstellungen am Alde-Bedienteil
  bleiben gültig.
- Compact 3020 HE: nach bisherigem Stand lässt sich nur lesen, nicht steuern – nicht unterstützt.

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
