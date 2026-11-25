# Fertige Firmware – Version 0.2.8

Gebaut aus Commit e03c8a9 (2026-11-25 09:05 UTC).

**Per USB (komplett, Adresse 0x0):** die Datei des Boards in diesem Ordner, z. B.
`esptool.py write_flash 0x0 panel-waveshare-7b.bin` – fuer neue Geraete. **Achtung: loescht die Einstellungen**
(WLAN, Geraete, Kacheln), weil die Datei auch den Einstellungsbereich enthaelt.

**Per USB, Einstellungen behalten:** die gleichnamige Datei aus `ota/` an Adresse 0x10000, z. B.
`esptool.py write_flash 0x10000 ota/panel-waveshare-7b.bin`.

**Ueber die Weboberflaeche (Update):** die gleichnamige Datei in `ota/`.

| Board | Version | Komplett (USB) | Update (Weboberflaeche) | vorige Fassung |
|---|---|---|---|---|
| panel-crowpanel-146-es | 0.2.8 (e03c8a9) | [panel-crowpanel-146-es.bin](panel-crowpanel-146-es.bin) (3858 KB) | [ota/panel-crowpanel-146-es.bin](ota/panel-crowpanel-146-es.bin) (3794 KB) | – |
| panel-crowpanel-146-fr | 0.2.8 (e03c8a9) | [panel-crowpanel-146-fr.bin](panel-crowpanel-146-fr.bin) (3860 KB) | [ota/panel-crowpanel-146-fr.bin](ota/panel-crowpanel-146-fr.bin) (3796 KB) | – |
| panel-crowpanel-146 | 0.2.8 (e03c8a9) | [panel-crowpanel-146.bin](panel-crowpanel-146.bin) (3785 KB) | [ota/panel-crowpanel-146.bin](ota/panel-crowpanel-146.bin) (3721 KB) | – |
| panel-crowpanel-p4-es | 0.2.8 (e03c8a9) | [panel-crowpanel-p4-es.bin](panel-crowpanel-p4-es.bin) (4348 KB) | [ota/panel-crowpanel-p4-es.bin](ota/panel-crowpanel-p4-es.bin) (4284 KB) | – |
| panel-crowpanel-p4-fr | 0.2.8 (e03c8a9) | [panel-crowpanel-p4-fr.bin](panel-crowpanel-p4-fr.bin) (4351 KB) | [ota/panel-crowpanel-p4-fr.bin](ota/panel-crowpanel-p4-fr.bin) (4287 KB) | – |
| panel-crowpanel-p4 | 0.2.8 (e03c8a9) | [panel-crowpanel-p4.bin](panel-crowpanel-p4.bin) (4273 KB) | [ota/panel-crowpanel-p4.bin](ota/panel-crowpanel-p4.bin) (4209 KB) | – |
| panel-waveshare-7b | 0.2.8 (e03c8a9) | [panel-waveshare-7b.bin](panel-waveshare-7b.bin) (3522 KB) | [ota/panel-waveshare-7b.bin](ota/panel-waveshare-7b.bin) (3458 KB) | – |
| satellit-cyd | 0.2.8 (e03c8a9) | [satellit-cyd.bin](satellit-cyd.bin) (2082 KB) | [ota/satellit-cyd.bin](ota/satellit-cyd.bin) (2018 KB) | – |
| satellit-esp32-c3-oled | 0.2.8 (e03c8a9) | [satellit-esp32-c3-oled.bin](satellit-esp32-c3-oled.bin) (1987 KB) | [ota/satellit-esp32-c3-oled.bin](ota/satellit-esp32-c3-oled.bin) (1923 KB) | – |
| satellit-esp32-c3 | 0.2.8 (e03c8a9) | [satellit-esp32-c3.bin](satellit-esp32-c3.bin) (1959 KB) | [ota/satellit-esp32-c3.bin](ota/satellit-esp32-c3.bin) (1895 KB) | – |
| satellit-esp32-c6 | 0.2.8 (e03c8a9) | [satellit-esp32-c6.bin](satellit-esp32-c6.bin) (2028 KB) | [ota/satellit-esp32-c6.bin](ota/satellit-esp32-c6.bin) (1964 KB) | – |
| satellit-esp32-s3 | 0.2.8 (e03c8a9) | [satellit-esp32-s3.bin](satellit-esp32-s3.bin) (1774 KB) | [ota/satellit-esp32-s3.bin](ota/satellit-esp32-s3.bin) (1710 KB) | – |
| satellit-esp32 | 0.2.8 (e03c8a9) | [satellit-esp32.bin](satellit-esp32.bin) (1842 KB) | [ota/satellit-esp32.bin](ota/satellit-esp32.bin) (1778 KB) | – |
| satellit-relay16 | 0.2.8 (e03c8a9) | [satellit-relay16.bin](satellit-relay16.bin) (1843 KB) | [ota/satellit-relay16.bin](ota/satellit-relay16.bin) (1779 KB) | – |
| satellit-shunt-c3 | 0.2.8 (e03c8a9) | [satellit-shunt-c3.bin](satellit-shunt-c3.bin) (1958 KB) | [ota/satellit-shunt-c3.bin](ota/satellit-shunt-c3.bin) (1894 KB) | – |
| updater-esp32 | 0.2.8 (e03c8a9) | – | [ota/updater-esp32.bin](ota/updater-esp32.bin) (936 KB) | – |
| zigbee-modul-h2 | 0.2.8 (e03c8a9) | [zigbee-modul-h2.bin](zigbee-modul-h2.bin) (3549 KB) | [ota/zigbee-modul-h2.bin](ota/zigbee-modul-h2.bin) (826 KB) | – |
| zusatzdisplay-146 | 0.2.8 (e03c8a9) | [zusatzdisplay-146.bin](zusatzdisplay-146.bin) (2184 KB) | [ota/zusatzdisplay-146.bin](ota/zusatzdisplay-146.bin) (2120 KB) | – |
| zusatzdisplay-cyd | 0.2.8 (e03c8a9) | [zusatzdisplay-cyd.bin](zusatzdisplay-cyd.bin) (3944 KB) | [ota/zusatzdisplay-cyd.bin](ota/zusatzdisplay-cyd.bin) (2257 KB) | – |

**P4 – neue Speicheraufteilung seit 0.2.7** (2 x 6 MB Programm): Umstieg ohne Einstellungsverlust per USB
`esptool.py --chip esp32p4 write_flash 0x2000 ota/p4-bootloader.bin 0x8000 ota/p4-partitionen.bin 0x10000 ota/panel-crowpanel-p4.bin`
(bzw. Online-Flasher "Nur Firmware (Update)"). Das Dateisystem (Kachel-Anordnung, Regeln, Bilder) ist danach leer.

**Zigbee-Modul (ESP32-H2):** [zb_drivers.bin](zb_drivers.bin) (1565 KB) = Geraetetreiber allein,
per USB nach `0x1F0000` – das Programm (`ota/zigbee-modul-h2.bin` nach `0x10000`) bleibt dabei unberuehrt.

**Android-App „S.O.C. Live“:** [soc-live.apk](soc-live.apk) (7677 KB) – Live-Ansicht des Panels als App,
Android 7 oder neuer, 64 Bit; Installation aus unbekannten Quellen erlauben.
