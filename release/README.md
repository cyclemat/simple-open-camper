# Fertige Firmware – Version 0.2.7

Gebaut aus Commit 21869d8 (2026-11-22 15:47 UTC).

**Per USB (komplett, Adresse 0x0):** die Datei des Boards in diesem Ordner, z. B.
`esptool.py write_flash 0x0 panel-waveshare-7b.bin` – fuer neue Geraete. **Achtung: loescht die Einstellungen**
(WLAN, Geraete, Kacheln), weil die Datei auch den Einstellungsbereich enthaelt.

**Per USB, Einstellungen behalten:** die gleichnamige Datei aus `ota/` an Adresse 0x10000, z. B.
`esptool.py write_flash 0x10000 ota/panel-waveshare-7b.bin`.

**Ueber die Weboberflaeche (Update):** die gleichnamige Datei in `ota/`.

| Board | Version | Komplett (USB) | Update (Weboberflaeche) | vorige Fassung |
|---|---|---|---|---|
| panel-crowpanel-146-es | 0.2.7 (21869d8) | [panel-crowpanel-146-es.bin](panel-crowpanel-146-es.bin) (3764 KB) | [ota/panel-crowpanel-146-es.bin](ota/panel-crowpanel-146-es.bin) (3700 KB) | – |
| panel-crowpanel-146-fr | 0.2.7 (21869d8) | [panel-crowpanel-146-fr.bin](panel-crowpanel-146-fr.bin) (3766 KB) | [ota/panel-crowpanel-146-fr.bin](ota/panel-crowpanel-146-fr.bin) (3702 KB) | – |
| panel-crowpanel-146 | 0.2.7 (21869d8) | [panel-crowpanel-146.bin](panel-crowpanel-146.bin) (3698 KB) | [ota/panel-crowpanel-146.bin](ota/panel-crowpanel-146.bin) (3634 KB) | – |
| panel-crowpanel-p4-es | 0.2.7 (21869d8) | [panel-crowpanel-p4-es.bin](panel-crowpanel-p4-es.bin) (4172 KB) | [ota/panel-crowpanel-p4-es.bin](ota/panel-crowpanel-p4-es.bin) (4108 KB) | – |
| panel-crowpanel-p4-fr | 0.2.7 (21869d8) | [panel-crowpanel-p4-fr.bin](panel-crowpanel-p4-fr.bin) (4175 KB) | [ota/panel-crowpanel-p4-fr.bin](ota/panel-crowpanel-p4-fr.bin) (4111 KB) | – |
| panel-crowpanel-p4 | 0.2.7 (21869d8) | [panel-crowpanel-p4.bin](panel-crowpanel-p4.bin) (4104 KB) | [ota/panel-crowpanel-p4.bin](ota/panel-crowpanel-p4.bin) (4040 KB) | – |
| panel-waveshare-7b | 0.2.7 (21869d8) | [panel-waveshare-7b.bin](panel-waveshare-7b.bin) (3435 KB) | [ota/panel-waveshare-7b.bin](ota/panel-waveshare-7b.bin) (3371 KB) | – |
| satellit-cyd | 0.2.7 (21869d8) | [satellit-cyd.bin](satellit-cyd.bin) (2079 KB) | [ota/satellit-cyd.bin](ota/satellit-cyd.bin) (2015 KB) | – |
| satellit-esp32-c3-oled | 0.2.7 (21869d8) | [satellit-esp32-c3-oled.bin](satellit-esp32-c3-oled.bin) (1983 KB) | [ota/satellit-esp32-c3-oled.bin](ota/satellit-esp32-c3-oled.bin) (1919 KB) | – |
| satellit-esp32-c3 | 0.2.7 (21869d8) | [satellit-esp32-c3.bin](satellit-esp32-c3.bin) (1955 KB) | [ota/satellit-esp32-c3.bin](ota/satellit-esp32-c3.bin) (1891 KB) | – |
| satellit-esp32-c6 | 0.2.7 (21869d8) | [satellit-esp32-c6.bin](satellit-esp32-c6.bin) (2023 KB) | [ota/satellit-esp32-c6.bin](ota/satellit-esp32-c6.bin) (1959 KB) | – |
| satellit-esp32-s3 | 0.2.7 (21869d8) | [satellit-esp32-s3.bin](satellit-esp32-s3.bin) (1770 KB) | [ota/satellit-esp32-s3.bin](ota/satellit-esp32-s3.bin) (1706 KB) | – |
| satellit-esp32 | 0.2.7 (21869d8) | [satellit-esp32.bin](satellit-esp32.bin) (1839 KB) | [ota/satellit-esp32.bin](ota/satellit-esp32.bin) (1775 KB) | – |
| satellit-relay16 | 0.2.7 (21869d8) | [satellit-relay16.bin](satellit-relay16.bin) (1840 KB) | [ota/satellit-relay16.bin](ota/satellit-relay16.bin) (1776 KB) | – |
| satellit-shunt-c3 | 0.2.7 (21869d8) | [satellit-shunt-c3.bin](satellit-shunt-c3.bin) (1954 KB) | [ota/satellit-shunt-c3.bin](ota/satellit-shunt-c3.bin) (1890 KB) | – |
| updater-esp32 | 0.2.7 (21869d8) | – | [ota/updater-esp32.bin](ota/updater-esp32.bin) (936 KB) | – |
| zigbee-modul-h2 | 0.2.7 (21869d8) | [zigbee-modul-h2.bin](zigbee-modul-h2.bin) (3549 KB) | [ota/zigbee-modul-h2.bin](ota/zigbee-modul-h2.bin) (826 KB) | – |
| zusatzdisplay-146 | 0.2.7 (21869d8) | [zusatzdisplay-146.bin](zusatzdisplay-146.bin) (2180 KB) | [ota/zusatzdisplay-146.bin](ota/zusatzdisplay-146.bin) (2116 KB) | – |
| zusatzdisplay-cyd | 0.2.7 (21869d8) | [zusatzdisplay-cyd.bin](zusatzdisplay-cyd.bin) (3944 KB) | [ota/zusatzdisplay-cyd.bin](ota/zusatzdisplay-cyd.bin) (2253 KB) | – |

**P4 – neue Speicheraufteilung seit 0.2.7** (2 x 6 MB Programm): Umstieg ohne Einstellungsverlust per USB
`esptool.py --chip esp32p4 write_flash 0x2000 ota/p4-bootloader.bin 0x8000 ota/p4-partitionen.bin 0x10000 ota/panel-crowpanel-p4.bin`
(bzw. Online-Flasher "Nur Firmware (Update)"). Das Dateisystem (Kachel-Anordnung, Regeln, Bilder) ist danach leer.

**Zigbee-Modul (ESP32-H2):** [zb_drivers.bin](zb_drivers.bin) (1565 KB) = Geraetetreiber allein,
per USB nach `0x1F0000` – das Programm (`ota/zigbee-modul-h2.bin` nach `0x10000`) bleibt dabei unberuehrt.

**Android-App „S.O.C. Live“:** [soc-live.apk](soc-live.apk) (7677 KB) – Live-Ansicht des Panels als App,
Android 7 oder neuer, 64 Bit; Installation aus unbekannten Quellen erlauben.
