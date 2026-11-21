# Simple Open Camper – Handbuch

Bedienpanel für Wohnmobil und Wohnwagen: Licht, Gas, Heizung und Batterie auf einem Bildschirm – per MQTT, Bluetooth, Zigbee und Kabel.

Ein Wand-Panel mit 7-Zoll-Touch (1024 × 600), das Lampen und Schalter aus **Zigbee2MQTT direkt per MQTT** steuert –
ohne Home Assistant dazwischen. Dazu liest es Gasflaschen, Tanks, Victron-Geräte und die Heizung im Wohnmobil.
Das Layout baust du im Browser-Editor, den das Panel selbst ausliefert.

> 💡 **So fing es an:** Im Wohnwagen nutze ich selbst Home Assistant – aber für Licht und ein paar Werte braucht
> das System auf Dauer zu viel Strom. Gesucht habe ich etwas Sparsameres. So entstand das Panel als
> **Lichtsteuerung für den SMHUB Nano**: ein Bildschirm an der Wand, der die Zigbee-Lampen direkt schaltet.
>
> Auf die Idee, mehr daraus zu machen, hat mich meine
> [Truma-LevelControl-Integration für Home Assistant](https://github.com/cyclemat/HAOS-Truma-Levelcontrol-Integration)
> gebracht: Wenn das Panel den Gasstand selbst per Bluetooth lesen kann, braucht es dafür kein Home Assistant.
> Danach kamen Tanks, Victron, die Truma-Heizung, Alde und Satelliten dazu – Stück für Stück ist daraus das
> Bordpanel für den ganzen Camper gewachsen. Daher auch der neue Name: aus dem „Zigbee-Panel“ wurde
> **Simple Open Camper**.

- 🏠 Alles läuft **lokal** auf dem Panel – keine Cloud, kein Konto
- 📶 Eigenes WLAN möglich (Router-Modus) – kein Heimnetz nötig, ideal fürs Wohnmobil
- 🧩 Jedes Zusatzmodul lässt sich in `platformio.ini` aus- oder einbauen

---

## Inhalt

### Start
- [Funktionen](Funktionen)
- [Panels und Voraussetzungen](Panels-und-Voraussetzungen)
- [Installation und Flashen](Installation)
- [Konfiguration (Module, Sprachen)](Konfiguration)

### Geräte
- [Gas und Tanks](Gas-und-Tanks)
- [Akkus, Power Stations und Victron](Akkus-und-Victron)
- [Sensoren (BTHome, Ruuvi, SwitchBot)](Sensoren)
- [Heizung und Kühlbox](Heizung-und-Kuehlbox)
- [Fahrzeug: Hymer und Hobby](Fahrzeug)
- [Startbild und WLED](Startbild-und-WLED)

### Satelliten und Displays
- [Satelliten](Satelliten)
- [Victron-Display (eigene Firmware)](Victron-Display)

### Bedienen und Fernzugriff
- [Live-Ansicht](Live-Ansicht)
- [Tailscale – Fernzugriff](Tailscale)
- [Home Assistant](Home-Assistant)

### Technik
- [Kabelanschlüsse (LIN, CI-BUS, Zigbee-Modul)](Kabelanschluesse)
- [Fehlersuche und Wartung](Fehlersuche)
- [REST-API](REST-API)

### Projekt
- [Quellen und Lizenzen](Quellen-und-Lizenzen)
- [Mitmachen und Unterstützen](Mitmachen)

---
[Code und fertige Firmware](https://github.com/cyclemat/simple-open-camper) · [Änderungen](https://github.com/cyclemat/simple-open-camper/blob/main/CHANGELOG.md) · [English README](https://github.com/cyclemat/simple-open-camper/blob/main/README_EN.md)
