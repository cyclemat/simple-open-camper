# Sensoren (BTHome, Ruuvi, SwitchBot)

### 📡 BTHome *(Beta, ungetestet)*

- Offener Standard für viele günstige Funk-Sensoren: **Shelly BLU** (Tür/Fenster, Bewegung, Taster, H&T),
  **Xiaomi-Thermometer mit pvvx-Firmware** und viele andere. Das Panel hört nur mit, keine Kopplung.
- Einrichten unter **Sensoren → Sensoren suchen** (Editor: Reiter Sensoren), das Gerät erscheint als „BTHome“.
- Kachel: Temperatur und Feuchte, bzw. bei Tür-/Bewegungsmeldern der Zustand („Tür offen“, „Bewegung“,
  „Nässe!“ …). Taster-Ereignisse („Taster: kurz/doppelt/lang“) stehen im Zustand.
- **Verschlüsselt** (z. B. Shelly BLU mit Schlüssel): den Schlüssel (32 Zeichen, aus der Shelly-App) ins Feld
  **PIN / Passwort** eintragen.

### 🌡️ Ruuvi und SwitchBot *(Beta, ungetestet)*

- **Ruuvi Tag / Ruuvi Air** und **SwitchBot Meter, Meter Plus, Outdoor Meter** senden ihre Werte offen per Bluetooth.
  Keine Kopplung, kein Passwort, die Hersteller-App darf offen bleiben, ein SwitchBot-Hub ist nicht nötig.
- Einrichten unter **Sensoren → Sensoren suchen** (Editor: Reiter Sensoren) – der Sensor erscheint als „Ruuvi“ bzw. „SwitchBot Meter“.
- Kachel: Temperatur groß, Luftfeuchte darunter (Ruuvi Air: CO₂). Im Detail Luftdruck, Feinstaub und Batterie.

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
