# Sensoren (BTHome, Ruuvi, SwitchBot) und Wasserwaage

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

### 📐 Wasserwaage (Wohnwagen und Reisemobil) *(Beta, ungetestet)*

Zeigt auf dem Panel, wie schief das Fahrzeug steht und **was zu tun ist**:

- **Wohnwagen:** welche Seite wie viele Zentimeter anheben (Spurweite) und das **Stützrad** wie viele Zentimeter
  hoch- oder runterkurbeln (Abstand Achse–Stützrad).
- **Reisemobil:** die **Keilhöhe je Rad** in Zentimetern (Radstand, Spurweite). Ist die nötige Höhe größer als die
  höchste Stufe der Auffahrkeile, wird es rot („Keile zu klein“). Ein Rad bleibt immer bei 0 – das ist der höchste Punkt.
- Kachel: größte Neigung in Grad, darunter der nächste Schritt („Vorne rechts 6,5 cm auffahren“). Grün = gerade
  (ab Werk bis 0,3°), Bernstein = fast gerade (bis 1,5°), Rot = schief bzw. Keile zu klein. Kachel antippen =
  Draufsicht mit Zentimetern an den Rädern und Libelle.

**Sensor:** ein **Satellit mit MPU-6050** (siehe [Satelliten](#️-satelliten)) oder ein **Ruuvi Tag** (unter
Sensoren hinzufügen; die Wasserwaage nutzt dessen Beschleunigungssensor, Datenformat 5).

**Wo** der Sensor sitzt, ist egal – das Fahrzeug ist starr, die Neigung ist überall gleich. Die Zentimeter rechnet das
Panel nur aus dem Winkel und den Fahrzeugmaßen. Wichtig ist nur, **wie herum** er eingebaut ist.

**Einrichten – nur im Editor** (Einstellungen → **Wasserwaage**), absichtlich nicht am Panel, damit niemand den
Nullpunkt aus Versehen verstellt:

1. Einschalten, **Sensor** und **Fahrzeug** (Wohnwagen/Reisemobil) wählen, **Maße** eintragen (Spurweite; Wohnwagen:
   Achse bis Stützrad; Reisemobil: Radstand und höchste Keilstufe).
2. Fahrzeug auf einem ebenen Platz gerade hinstellen (mit einer echten Wasserwaage prüfen) und **Nullpunkt setzen**.
   Danach muss der Sensor nicht genau gerade eingebaut sein – auch schräg oder (Ruuvi) senkrecht an der Wand geht.
3. **Einbaulage:** angeben, wohin die Kante/der Pfeil des Sensors zeigt (vorne, rechts, hinten, links) und ob links/
   rechts vertauscht ist – **oder den Assistenten nehmen**: „Assistent starten“, dann die Front anheben (Reisemobil:
   mit beiden Vorderrädern auf die Keile fahren; Wohnwagen: Stützrad hochkurbeln) und „Weiter“. Optional danach die
   rechte Seite anheben und „Weiter“ – damit prüft er auch links/rechts. Der Assistent stellt die Einbaulage selbst
   ein, auch wenn der Sensor schräg zur Fahrtrichtung sitzt.

Vorzeichen in der Weboberfläche: längs + = Front tiefer, quer + = rechte Seite höher. API: `/api/level`.

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
