# Shelly, ESPHome, Tasmota und WLED

### 🔌 Shelly per Bluetooth *(Beta, ungetestet)*

- Für **Shelly Gen2, Gen3 und Gen4** (Plus, Pro, Mini …) – ohne WLAN-Anbindung und ohne MQTT-Broker. P4, rundes Panel
  (EC-S3-1.46) und Waveshare 7B. Gen1 hat kein Bluetooth: dafür weiter Shelly über MQTT.
- Bluetooth am Shelly einschalten (Shelly-App → Einstellungen → Bluetooth).
- Panel: **Einstellungen → Shelly (BT)** einschalten, „Shellys suchen“, gefundenen Shelly hinzufügen (Name, Passwort nur
  wenn am Shelly gesetzt). Im Editor dieselbe Seite unter Einstellungen.
- Das Panel verbindet sich kurz: Stand alle 30 s, Schalten sofort (1–2 s). Schalter, Licht/Dimmer, Leistung und Energie
  erscheinen als Kacheln wie die MQTT-Shellys und gehen auch in Regeln und auf Zusatzdisplays.

### 🧩 ESPHome, Tasmota & Co. (MQTT-Discovery)

Geräte, die sich am Broker per **Home-Assistant-Discovery** anmelden, zeigt das Panel automatisch an – ohne Home
Assistant. Jedes Gerät erscheint in der Geräteliste **wie ein Satellit** (eigene Gruppe mit seinem Namen); daraus
Kacheln aufs Panel ziehen. Unterstützt: **Sensoren** (Wert + Einheit), **Binärsensoren** (Tür, Bewegung, Wasser …),
**Schalter** und **Lampen** (ein/aus). Die Werte gehen auch in Regeln, die App, die Live-Ansicht und auf
Zusatzdisplays.

1. Panel: **Einstellungen → MQTT → „Geräte aus MQTT-Discovery anzeigen“** (ab Werk aus; Präfix `homeassistant`,
   im Editor änderbar).
2. Gerät mit demselben Broker verbinden wie das Panel – im Camper am einfachsten mit dem **eingebauten Broker** des
   Panels. Beispiel für ESPHome:

   ```yaml
   mqtt:
     broker: 192.168.4.1        # IP des Panels (Panel-WLAN) bzw. seine Adresse im Netz
     username: !secret mqtt_user   # nur, wenn am Panel-Broker ein Benutzer gesetzt ist
     password: !secret mqtt_pass
   ```

   Tasmota: unter *Konfiguration → MQTT* den Broker eintragen und `SetOption19 1` (Home-Assistant-Discovery) setzen.
3. Nach wenigen Sekunden steht unter dem Schalter „Gefunden: … Werte von … Geräten“.

Eigene Satelliten und Zigbee2MQTT-Geräte werden übergangen (die kennt das Panel schon direkt); das Panel meldet aus
Discovery-Geräten nichts weiter – kein Kreislauf. Wert-Vorlagen werden in der einfachen Form verstanden
(`{{ value }}`, `{{ value_json.ENERGY.Power }}`); Regler wie Zahlen, Auswahlen oder Klima kommen später.

### 🌈 WLED

- In WLED **Config → Sync Interfaces → MQTT** aktivieren, denselben Broker eintragen, Device Topic
  `wled/<name>` lassen. Das Panel findet die Geräte von selbst.
- Farbtemperatur und Effekte gehen über MQTT nicht – die stellst du weiter in der WLED-App ein.

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
