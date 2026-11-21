# Home Assistant

## 🏠 Home Assistant

Das Panel braucht kein Home Assistant – arbeitet aber gut mit ihm zusammen. Wer HA nutzt, kann das Panel auch als
**Bedienteil für Home Assistant** einsetzen.

| Richtung | Was | Stand |
|---|---|---|
| **Satelliten → HA** | Alle Satelliten melden sich per MQTT-Discovery selbst an: Tanksonden, Temperaturen, Relais (schaltbar), Eingänge, Wasserwaage, Zusatzsensoren und ihre Bluetooth-Sensoren | ✅ funktioniert |
| **Zigbee2MQTT → HA** | wie gewohnt über Zigbee2MQTT selbst | ✅ funktioniert |
| **Panel → HA** | Das Panel meldet seine **lokalen** Geräte per MQTT-Discovery: Gasflaschen (Truma LevelControl, Mopeka), Victron per Bluetooth, Akkus und Power Stations, Zigbee-Geräte am H2-Funkmodul, Heizung (iNet X, CP Plus, Alde), Hymer, Regeln (an/aus) | 🚧 in Arbeit |
| **HA → Panel** | Das Panel als HA-Bedienteil: Lampen, Schalter, Szenen, Skripte und Sensoren aus Home Assistant als Kacheln – über die HA-API mit Zugangstoken, auch in den Regeln nutzbar | 🚧 in Arbeit |

### 🔁 Kein Daten-Kreislauf

Damit kein Wert im Kreis läuft (Panel → HA → Panel → HA …), gelten feste Regeln:

- Das Panel meldet an HA **nur, was direkt am Panel hängt** (Bluetooth, H2-Funkmodul, Heizungsbus, Regeln).
- Was das Panel **schon per MQTT bekommt**, meldet es **nicht** weiter: Zigbee2MQTT-Geräte, Cerbo GX, Satelliten und
  Zusatzdisplays (die melden sich selbst bzw. haben eigene HA-Integrationen).
- Werte, die das Panel **aus Home Assistant** holt, gehen **nie** zurück an HA.
- Topics unter `homeassistant/` liest das Panel nie als Eingang.
- In der HA-Auswahl im Editor sind Geräte ausgeblendet, die vom Panel selbst oder von seinen Satelliten stammen –
  die gibt es auf dem Panel ja schon direkt.

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
