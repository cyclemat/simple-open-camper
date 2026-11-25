# Symbole für Kacheln (eingebaute und eigene)

### 🎨 Symbole für Kacheln

Jede Kachel bekommt im Editor ein Symbol (Eigenschaften → Symbol): **Auto** wählt nach Gerätetyp, darunter die
eingebauten Gruppen **Wohnen** (Lampen, Steckdose, Ventilator, Heizung, Räume …) und **Boot und Technik** (Anker, Boot,
Segelboot, Motor, Kraftstoff, Wellen für die Bilgepumpe, Wasser, Dusche, WC, Rohr, Kompass, Leuchtturm, Rettungsring,
Sirene, Funk, Wind, Markise, Leiter, Fisch, Taschenlampe, Solar, Kühlen). Die Symbole stammen aus dem Phosphor-Satz (MIT).

**Eigene Symbole** (Gruppe „Eigene“, bis zu 32 je Panel):
- **Hochladen im Editor:** in der Symbolauswahl auf **+** oder unter Einstellungen → Anzeige → Eigene Symbole
  „PNG hochladen“. Der Browser bringt das Bild auf 96 × 96 Pixel und schickt es ins Panel (Flash). Name = Dateiname
  ohne Endung (klein, a–z, 0–9, - und _).
- **SD-Karte** (P4 und Waveshare): PNG-Dateien in den Ordner `icons` legen, Dateiname = Name des Symbols. Nach dem
  Einstecken unter Einstellungen → Anzeige „SD-Karte neu einlesen“ oder das Panel neu starten.
- **Bildvorgabe:** durchsichtiger Hintergrund, Zeichnung in einer Farbe (weiß oder schwarz), quadratisch, 96 bis
  512 Pixel, höchstens 300 KB. Ohne Durchsichtigkeit gilt „dunkel = Symbol“ (schwarze Zeichnung auf weißem Grund). Das
  Panel zeigt das Symbol in der jeweiligen Kachelfarbe, genau wie die eingebauten.
- **Speicher:** je Symbol etwa 3,5 KB PSRAM, kein interner RAM. Zusatzdisplays und die App zeigen für eigene Symbole
  das automatische Symbol der Kachel.
- **Entfernen:** Einstellungen → Anzeige → Eigene Symbole (nur Flash; Dateien auf der SD-Karte am PC löschen).

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
