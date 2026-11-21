# Home Assistant

## 🏠 Home Assistant

Das Panel braucht kein Home Assistant – arbeitet aber gut mit ihm zusammen. Wer HA nutzt, kann das Panel auch als
**Bedienteil für Home Assistant** einsetzen.

| Richtung | Was | Stand |
|---|---|---|
| **Satelliten → HA** | Alle Satelliten melden sich per MQTT-Discovery selbst an: Tanksonden, Temperaturen, Relais (schaltbar), Eingänge, Wasserwaage, Zusatzsensoren und ihre Bluetooth-Sensoren | ✅ funktioniert |
| **Zigbee2MQTT → HA** | wie gewohnt über Zigbee2MQTT selbst | ✅ funktioniert |
| **Panel → HA** | Über die Integration [simple-open-camper-HA-Integration](https://github.com/cyclemat/simple-open-camper-HA-Integration): ausgewählte Panel-Werte (Lampen, Schalter, Akkus, Victron, Gas, Satelliten-Tanks und -Temperaturen …) als Sensoren, Schalter und Lampen in HA | 🧪 Beta |
| **HA → Panel** | Über dieselbe Integration: ausgewählte HA-Entitäten (Lampen, Schalter, Sensoren …) erscheinen auf dem Panel wie ein Satellit „Home Assistant“ – als Kacheln, in Regeln, App und Live-Ansicht | 🧪 Beta |

### 🔌 Integration „Simple Open Camper“ für Home Assistant *(Beta)*

Home Assistant verbindet sich selbst mit dem Panel – **eine** WebSocket-Verbindung, lokal oder über Tailscale, ohne
MQTT-Broker. In der Integration wählst du, **welche Panel-Werte HA bekommt** und **welche HA-Entitäten das Panel zeigt**;
jede Seite bekommt nur das Ausgewählte.

1. **Panel:** Browser-Editor → **Einstellungen → Netzwerk → Home Assistant** → „Home Assistant darf sich verbinden“
   einschalten und den **Zugangscode** kopieren. (Firmware: P4 bis zur nächsten Version die Testfassung aus
   [`release/test/`](https://github.com/cyclemat/simple-open-camper/blob/main/release/test/), danach jede reguläre Version.)
2. **HA – installieren:** HACS → ⋮ → **Benutzerdefinierte Repositories** →
   `https://github.com/cyclemat/simple-open-camper-HA-Integration`, Typ **Integration** → herunterladen → HA neu starten.
   (Ohne HACS: den Ordner `custom_components/simple_open_camper` nach `config/custom_components/` kopieren.)
3. **HA – einrichten:** Einstellungen → Geräte & Dienste → **Integration hinzufügen → Simple Open Camper** → Adresse des
   Panels (IP im Netz, Tailscale-IP oder Name im Tailnet) und den Zugangscode eintragen.
4. **HA – auswählen:** bei der Integration **Konfigurieren** →
   Schritt 1 „Vom Panel übernehmen“: Panel-Werte anhaken (erscheinen in HA, gruppiert wie am Panel) →
   Schritt 2 „Dem Panel bereitstellen“: HA-Entitäten wählen (erscheinen am Panel in der Geräteliste, eine Gruppe je Bereich).

Liegt das Panel nicht im selben Netz wie HA (z. B. im Camper unterwegs), beide ins Tailnet bringen: am Panel
[Tailscale](Tailscale), in HA das Tailscale-Add-on – dann als
Adresse die 100.x-Adresse des Panels eintragen.

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
