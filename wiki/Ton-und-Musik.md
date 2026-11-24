# Ton & Musik (P4)

### 🔊 Ton & Musik (nur P4) *(Beta)*

Der P4 hat einen Lautsprecher-Anschluss (I2S-Verstärker). Das Panel spielt darüber **MP3-Dateien von seiner SD-Karte**
– ganz ohne Internet und ohne Handy.

> ℹ️ **Kern der Funktion sind Warn- und Hinweistöne** (z. B. eine Ansage, wenn die Gasflasche fast leer ist). Die
> **Musikwiedergabe ist nur ein Gimmick**: Sie ist ein Teil der Warnton-Abspielfunktion und nutzt denselben
> Abspieler – kein vollwertiger Musikplayer.

- **Musik:** MP3s in den Ordner **`music`** der SD-Karte legen. Kachel „Musik“ (Geräteliste → Musik): **kurz tippen =
  Abspielen/Pause**, **lang drücken** = Player-Fenster mit Titel, Bedienung und Lautstärke. Die Titel laufen der Reihe
  nach (alphabetisch), am Ende wieder von vorn.
- **Hinweistöne und Ansagen:** eingebaute Töne **Gong, Piepton, Alarm** oder eigene MP3s im Ordner **`sounds`**
  (z. B. „Gas fast leer.mp3“). Läuft gerade Musik, wird sie dafür kurz angehalten und läuft danach weiter.
- **In den Regeln:** Aktionen **„Ton abspielen“** (Ton oder Ansage) und **„Musik“** (abspielen, Pause, stoppen,
  nächster/voriger Titel, Lautstärke). Damit gehen z. B. ein **Wecker** (Auslöser „täglich um 7:00“ → Musik abspielen),
  eine **Gas-Warnung** (Kachel Gas unter 15 % → Ansage) oder eine **Klingel** (Zigbee-Taster → Gong).
- **Einstellungen nur im Editor** (Einstellungen → **Ton & Musik**): ein/aus (ab Werk aus), Lautstärke für Musik und
  für Hinweistöne getrennt, Titelliste zum Antippen, Töne zum Probehören.
- Sparsam: Der MP3-Decoder ist [minimp3](https://github.com/lieff/minimp3) (gemeinfrei, rund 30 KB). Der Audio-Teil
  liegt im PSRAM (Puffer und Arbeitsspeicher des Decoders), braucht vom knappen internen Speicher nur wenige KB und
  gibt seine Puffer 20 s nach dem Abspielen wieder frei. Der Verstärker ist aus, solange nichts spielt (kein Rauschen).
- Anschluss: Lautsprecher (z. B. 4–8 Ω, 2–3 W) an die Lautsprecher-Buchse des CrowPanel. Pins laut Elecrow-Beispielen:
  BCLK GPIO 22, LRCK GPIO 21, Daten GPIO 23, Verstärker an GPIO 30 (Low = an) – am P4 V1.2 mit Lautsprecher geprüft.

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
