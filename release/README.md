# Fertige Firmware – Version 0.2.6

Gebaut aus Commit c77dad2 (2026-11-21 04:38 UTC).
Die jeweils vorige Fassung liegt in [`vorher/`](vorher/).

**Per USB (komplett, Adresse 0x0):** die Datei des Boards in diesem Ordner, z. B.
`esptool.py write_flash 0x0 panel-waveshare-7b.bin` – fuer neue Geraete. **Achtung: loescht die Einstellungen**
(WLAN, Geraete, Kacheln), weil die Datei auch den Einstellungsbereich enthaelt.

**Per USB, Einstellungen behalten:** die gleichnamige Datei aus `ota/` an Adresse 0x10000, z. B.
`esptool.py write_flash 0x10000 ota/panel-waveshare-7b.bin`.

**Ueber die Weboberflaeche (Update):** die gleichnamige Datei in `ota/`.

| Board | Version | Komplett (USB) | Update (Weboberflaeche) | vorige Fassung |
|---|---|---|---|---|
| panel-crowpanel-146 | 0.2.6 (c77dad2) | [panel-crowpanel-146.bin](panel-crowpanel-146.bin) (3650 KB) | [ota/panel-crowpanel-146.bin](ota/panel-crowpanel-146.bin) (3586 KB) | 0.2.0 (7e6a2e6): [komplett](vorher/panel-crowpanel-146.bin), [Update](vorher/ota/panel-crowpanel-146.bin) |
| panel-crowpanel-p4 | 0.2.6 (c77dad2) | [panel-crowpanel-p4.bin](panel-crowpanel-p4.bin) (4054 KB) | [ota/panel-crowpanel-p4.bin](ota/panel-crowpanel-p4.bin) (3990 KB) | 0.2.0 (7e6a2e6): [komplett](vorher/panel-crowpanel-p4.bin), [Update](vorher/ota/panel-crowpanel-p4.bin) |
| panel-waveshare-7b | 0.2.6 (c77dad2) | [panel-waveshare-7b.bin](panel-waveshare-7b.bin) (3389 KB) | [ota/panel-waveshare-7b.bin](ota/panel-waveshare-7b.bin) (3325 KB) | 0.2.0 (7e6a2e6): [komplett](vorher/panel-waveshare-7b.bin), [Update](vorher/ota/panel-waveshare-7b.bin) |
| satellit-cyd | 0.2.6 (c77dad2) | [satellit-cyd.bin](satellit-cyd.bin) (2079 KB) | [ota/satellit-cyd.bin](ota/satellit-cyd.bin) (2015 KB) | 0.2.0 (e6449eb): [komplett](vorher/satellit-cyd.bin), [Update](vorher/ota/satellit-cyd.bin) |
| satellit-esp32-c3-oled | 0.2.6 (c77dad2) | [satellit-esp32-c3-oled.bin](satellit-esp32-c3-oled.bin) (1983 KB) | [ota/satellit-esp32-c3-oled.bin](ota/satellit-esp32-c3-oled.bin) (1919 KB) | – |
| satellit-esp32-c3 | 0.2.6 (c77dad2) | [satellit-esp32-c3.bin](satellit-esp32-c3.bin) (1955 KB) | [ota/satellit-esp32-c3.bin](ota/satellit-esp32-c3.bin) (1891 KB) | 0.2.0 (e6449eb): [komplett](vorher/satellit-esp32-c3.bin), [Update](vorher/ota/satellit-esp32-c3.bin) |
| satellit-esp32-c6 | 0.2.6 (c77dad2) | [satellit-esp32-c6.bin](satellit-esp32-c6.bin) (2023 KB) | [ota/satellit-esp32-c6.bin](ota/satellit-esp32-c6.bin) (1959 KB) | 0.2.0 (e6449eb): [komplett](vorher/satellit-esp32-c6.bin), [Update](vorher/ota/satellit-esp32-c6.bin) |
| satellit-esp32-s3 | 0.2.6 (c77dad2) | [satellit-esp32-s3.bin](satellit-esp32-s3.bin) (1770 KB) | [ota/satellit-esp32-s3.bin](ota/satellit-esp32-s3.bin) (1706 KB) | 0.2.0 (e6449eb): [komplett](vorher/satellit-esp32-s3.bin), [Update](vorher/ota/satellit-esp32-s3.bin) |
| satellit-esp32 | 0.2.6 (c77dad2) | [satellit-esp32.bin](satellit-esp32.bin) (1839 KB) | [ota/satellit-esp32.bin](ota/satellit-esp32.bin) (1775 KB) | 0.2.0 (e6449eb): [komplett](vorher/satellit-esp32.bin), [Update](vorher/ota/satellit-esp32.bin) |
| satellit-relay16 | 0.2.6 (c77dad2) | [satellit-relay16.bin](satellit-relay16.bin) (1840 KB) | [ota/satellit-relay16.bin](ota/satellit-relay16.bin) (1776 KB) | 0.2.0 (e6449eb): [komplett](vorher/satellit-relay16.bin), [Update](vorher/ota/satellit-relay16.bin) |
| satellit-shunt-c3 | 0.2.6 (c77dad2) | [satellit-shunt-c3.bin](satellit-shunt-c3.bin) (1954 KB) | [ota/satellit-shunt-c3.bin](ota/satellit-shunt-c3.bin) (1890 KB) | – |
| updater-esp32 | 0.2.6 (c77dad2) | – | [ota/updater-esp32.bin](ota/updater-esp32.bin) (936 KB) | 0.2.0 (7e6a2e6): [komplett](vorher/updater-esp32.bin), [Update](vorher/ota/updater-esp32.bin) |
| zigbee-modul-h2 | 0.2.6 (c77dad2) | [zigbee-modul-h2.bin](zigbee-modul-h2.bin) (3549 KB) | [ota/zigbee-modul-h2.bin](ota/zigbee-modul-h2.bin) (826 KB) | 0.2.0 (7e6a2e6): [komplett](vorher/zigbee-modul-h2.bin), [Update](vorher/ota/zigbee-modul-h2.bin) |
| zusatzdisplay-146 | 0.2.6 (c77dad2) | [zusatzdisplay-146.bin](zusatzdisplay-146.bin) (2180 KB) | [ota/zusatzdisplay-146.bin](ota/zusatzdisplay-146.bin) (2116 KB) | – |
| zusatzdisplay-cyd | 0.2.6 (c77dad2) | [zusatzdisplay-cyd.bin](zusatzdisplay-cyd.bin) (3944 KB) | [ota/zusatzdisplay-cyd.bin](ota/zusatzdisplay-cyd.bin) (2253 KB) | 0.2.0 (7e6a2e6): [komplett](vorher/zusatzdisplay-cyd.bin), [Update](vorher/ota/zusatzdisplay-cyd.bin) |

**Zigbee-Modul (ESP32-H2):** [zb_drivers.bin](zb_drivers.bin) (1565 KB) = Geraetetreiber allein,
per USB nach `0x1F0000` – das Programm (`ota/zigbee-modul-h2.bin` nach `0x10000`) bleibt dabei unberuehrt.

**Android-App „S.O.C. Live“:** [soc-live.apk](soc-live.apk) (7677 KB) – Live-Ansicht des Panels als App,
Android 7 oder neuer, 64 Bit; Installation aus unbekannten Quellen erlauben.
