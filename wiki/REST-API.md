# REST-API

## 🔌 REST-API

Vom Editor genutzt, auch für eigene Skripte:

| Endpunkt | Zweck |
|---|---|
| `GET /api/info`, `GET /api/devices` | Zustand des Panels, Geräteliste |
| `GET\|POST /api/layout` | Layout (`?force=1` bei Konflikt) |
| `POST /api/rename` | `{ieee, name}` in Zigbee2MQTT umbenennen |
| `GET\|POST /api/bg?page=<id>&fmt=jpg` | Hintergrund (JPEG 1024 × 600, `&delete=1`) |
| `GET\|POST /api/backup` | `{"create":true}`, `{"restore":"<name>"}`, `{"delete":"<name>"}` |
| `GET\|POST /api/settings` | Einstellungen, inkl. `{"ble":{"truma","mopeka","victron","inetx"}}` |
| `POST /api/wifi`, `/api/time`, `/api/refresh`, `/api/reboot`, `/api/saver` | WLAN, Uhrzeit, Neu laden, Neustart, Ruhezustand |
| `POST /api/ota` | Firmware-Update (firmware.bin) |
| `GET\|POST /api/saver_logo?fmt=bin\|png` | Eigenes Logo (`&delete=1`) |
| `GET\|POST /api/truma` | `{"scan":true}`, `{"measure":"<mac>"}`, `{"add":{mac,type,name}}`, `{"remove"}`, `{"rename"}`, `{"interval"}` |
| `GET\|POST /api/mopeka` | `{"scan":true}`, `{"add":{mac,name,tank}}`, `{"remove"}`, `{"rename"}`, `{"tank":{mac,tank,empty,full,capacity}}` |
| `GET\|POST /api/victron` | `{"scan":true}`, `{"add":{mac,key,name}}`, `{"key":{mac,key}}`, `{"remove"}`, `{"rename"}` |
| `GET\|POST /api/heat` | `{"src":"cpplus"\|"inetx"\|"alde", "heat":{on,target,mode}, "water", "energy":{e,power}, "clock", "pair", "forget"}` |

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
