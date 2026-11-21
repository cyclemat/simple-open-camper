# Startbild und WLED

### 🖼️ Startbild

Beim Start zeigt das Panel 6 Sekunden lang ein Startbild und gibt den Speicher danach wieder frei. Standard ist das
Banner (`assets/splash.jpg`); eine eigene **`splash.jpg`** im Projektordner hat beim Bauen Vorrang. Das Bild als JPEG „baseline“ speichern (nicht „progressive“), am besten 1024×600 –
größere Bilder werden um 1/2, 1/4 oder 1/8 verkleinert.

### 🌈 WLED

- In WLED **Config → Sync Interfaces → MQTT** aktivieren, denselben Broker eintragen, Device Topic
  `wled/<name>` lassen. Das Panel findet die Geräte von selbst.
- Farbtemperatur und Effekte gehen über MQTT nicht – die stellst du weiter in der WLED-App ein.

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
