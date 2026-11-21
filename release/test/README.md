# Testfassung: P4 mit Home-Assistant-Anbindung („HA Addon Test“)

**Nur zum Testen** – keine reguläre Version. Basis 0.2.6 plus:

- **Home Assistant** über die Integration [simple-open-camper-HA-Integration](https://github.com/cyclemat/simple-open-camper-HA-Integration)
  (Beta): HA verbindet sich per WebSocket mit dem Panel, beide Richtungen, jede Seite bekommt nur das Ausgewählte.
  Am Panel im Editor: **Einstellungen → Netzwerk → Home Assistant** einschalten und den Zugangscode kopieren.
- **ESPHome, Tasmota & Co.** (MQTT-Discovery): Einstellungen → MQTT → „Geräte aus MQTT-Discovery anzeigen“.

| Datei | Wofür |
|---|---|
| [panel-crowpanel-p4-HA-Addon-Test.bin](panel-crowpanel-p4-HA-Addon-Test.bin) | komplett per USB an Adresse `0x0` – **löscht die Einstellungen** |
| [panel-crowpanel-p4-HA-Addon-Test-update.bin](panel-crowpanel-p4-HA-Addon-Test-update.bin) | Update über die Weboberfläche bzw. per USB an `0x10000` – Einstellungen bleiben |

Nur für das **Elecrow CrowPanel Advanced 7" ESP32-P4 (EC-P4-7-V1.2)**. Zurück zur regulären Version: die Dateien eine
Ebene höher (`release/`). Fehler bitte als [GitHub-Issue](https://github.com/cyclemat/simple-open-camper/issues) melden.
