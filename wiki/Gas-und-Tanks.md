# Gas und Tanks

### ⛽ Truma LevelControl

- Der Sensor sendet nur alle paar Minuten und erlaubt **eine Verbindung gleichzeitig**. Ist die Truma-App,
  Home Assistant oder der Victron GX gerade verbunden, versucht es das Panel beim nächsten Durchgang erneut.
- Die Suche läuft bis zu drei Minuten; Abfrageintervall 5 bis 60 Minuten (Standard 10).
- Die letzte Messung bleibt über Neustarts erhalten.

### 🛢️ Mopeka

- Unterstützt: Pro Check, Pro Plus, Pro Check Universal, Pro Check H2O, Lippert BottleCheck, Pro-200, TD40/TD200
  und die älteren Standard-Sensoren. Das Panel hört nur mit – die Mopeka-App bleibt nutzbar.
- Beim Suchen kurz die **Sync-Taste** am Sensor drücken – er wird dann als „Taste gedrückt“ markiert.
- Flaschentyp in der Detailansicht wählen: 6/11/14 kg (Europa), 20/30/40 lb (USA) oder eigene Werte (mm, kg/l).

### ⚖️ Senso4s *(Beta, ungetestet)*

- Die Gasflaschen-Waage **Senso4s Basic / Plus** steht unter der Flasche und sendet den Füllstand in % offen per
  Bluetooth (alle 15 Minuten neu). Keine Kopplung, kein Passwort, die Senso4s-App bleibt nutzbar.
- Flasche (Leergewicht, Inhalt) und Nullabgleich einmal in der **Senso4s-App** einrichten.
- Einrichten wie ein Mopeka-Sensor: Bluetooth-Protokoll **Mopeka** einschalten, **Mopeka → Suchen** – die Waage
  erscheint als „Senso4s Basic“ bzw. „Senso4s Plus“. Flaschengröße wählen, dann rechnet das Panel die kg aus.
- Hinweise der Waage stehen im Status: Nullabgleich nötig, Einstellfehler, Batterien leer, beim Plus Warnungen zu
  Temperatur, Neigung oder Bewegung.

### 💧 Simarine STRx-Tanksensoren *(Beta, ungetestet)*

Die Simarine-Tanksensoren **STR1** (analog), **STR2** (NMEA 2000) und **STR3** (RV-C) können ihren Füllstand per
Bluetooth senden („Broadcast“). Das Panel hört nur mit – keine Kopplung, **kein App-Key nötig**.

1. In der App **Simarine Connect** den Tank einrichten (Höhe, Inhalt, Tankart, Kalibrierung).
2. Je Sensor **Device Management → Broadcast Settings → Broadcast Mode** einschalten – **ab Werk ist er aus**.
3. Modus wählen:
   - **Open** – Füllstand, Inhalt und Tankart für jeden lesbar. Nichts weiter einzutragen; am einfachsten zum Ausprobieren.
   - **Low / Medium / High Security** – Füllstand verschlüsselt. Jeder Sensor hat **seinen eigenen Schlüssel**: in der
     App **Show Encryption Key** antippen und die **32 Zeichen** beim Hinzufügen bzw. später im Feld **PIN / Passwort**
     eintragen. Der Schlüssel ist nicht die 4-stellige Geräte-PIN der App. Bei Medium/High sendet der Sensor Inhalt und
     Tankart nicht mit – dann zeigt das Panel nur Prozent.
4. **Simarine-App schließen bzw. vom Sensor trennen** – solange ein Handy verbunden ist, sendet der Sensor nicht.
5. Am Panel **Sensoren → Sensoren suchen** (Editor: Reiter Sensoren). Der Sensor erscheint als „Simarine STRx“. Hinzufügen –
   Kachel mit Füllstand, im Detail Inhalt in Litern, Tankart und ob der Funk offen oder verschlüsselt ist.

Fehlt der Wert, steht der Grund im Status: Broadcast aus, Schlüssel fehlt, Schlüssel passt nicht. Wird der Schlüssel in
der Simarine-App neu erzeugt, den neuen eintragen. Nicht als einzige Messung für sicherheitskritische Füllstände nutzen.

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
