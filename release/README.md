# Fertige Firmware – Version 0.2.0

Automatisch von GitHub gebaut (zuletzt aus Commit a2d18dc, 2026-11-18 13:20 UTC).
Die jeweils vorige Fassung liegt in [`vorher/`](vorher/).

**Per USB (komplett, Adresse 0x0):** die Datei des Boards in diesem Ordner, z. B.
`esptool.py write_flash 0x0 panel-waveshare-7b.bin` – fuer neue Geraete. **Achtung: loescht die Einstellungen**
(WLAN, Geraete, Kacheln), weil die Datei auch den Einstellungsbereich enthaelt.

**Per USB, Einstellungen behalten:** die gleichnamige Datei aus `ota/` an Adresse 0x10000, z. B.
`esptool.py write_flash 0x10000 ota/panel-waveshare-7b.bin`.

**Ueber die Weboberflaeche (Update):** die gleichnamige Datei in `ota/`.

| Board | Version | Komplett (USB) | Update (Weboberflaeche) | vorige Fassung |
|---|---|---|---|---|
| panel-crowpanel-146 | 0.2.0 (a2d18dc) | [panel-crowpanel-146.bin](panel-crowpanel-146.bin) (2997 KB) | [ota/panel-crowpanel-146.bin](ota/panel-crowpanel-146.bin) (2933 KB) | 0.2.0 (f61d015): [komplett](vorher/panel-crowpanel-146.bin), [Update](vorher/ota/panel-crowpanel-146.bin) |
| panel-crowpanel-p4 | 0.2.0 (a2d18dc) | [panel-crowpanel-p4.bin](panel-crowpanel-p4.bin) (3470 KB) | [ota/panel-crowpanel-p4.bin](ota/panel-crowpanel-p4.bin) (3406 KB) | 0.2.0 (f61d015): [komplett](vorher/panel-crowpanel-p4.bin), [Update](vorher/ota/panel-crowpanel-p4.bin) |
| panel-waveshare-7b | 0.2.0 (a2d18dc) | [panel-waveshare-7b.bin](panel-waveshare-7b.bin) (3170 KB) | [ota/panel-waveshare-7b.bin](ota/panel-waveshare-7b.bin) (3106 KB) | 0.2.0 (fd760c9): [komplett](vorher/panel-waveshare-7b.bin), [Update](vorher/ota/panel-waveshare-7b.bin) |
| satellit-cyd | 0.2.0 (a2d18dc) | [satellit-cyd.bin](satellit-cyd.bin) (2045 KB) | [ota/satellit-cyd.bin](ota/satellit-cyd.bin) (1981 KB) | 0.2.0 (f61d015): [komplett](vorher/satellit-cyd.bin), [Update](vorher/ota/satellit-cyd.bin) |
| satellit-esp32-c3 | 0.2.0 (a2d18dc) | [satellit-esp32-c3.bin](satellit-esp32-c3.bin) (1900 KB) | [ota/satellit-esp32-c3.bin](ota/satellit-esp32-c3.bin) (1836 KB) | 0.2.0 (f61d015): [komplett](vorher/satellit-esp32-c3.bin), [Update](vorher/ota/satellit-esp32-c3.bin) |
| satellit-esp32-c6 | 0.2.0 (a2d18dc) | [satellit-esp32-c6.bin](satellit-esp32-c6.bin) (2060 KB) | [ota/satellit-esp32-c6.bin](ota/satellit-esp32-c6.bin) (1996 KB) | 0.2.0 (f61d015): [komplett](vorher/satellit-esp32-c6.bin), [Update](vorher/ota/satellit-esp32-c6.bin) |
| satellit-esp32-s3 | 0.2.0 (a2d18dc) | [satellit-esp32-s3.bin](satellit-esp32-s3.bin) (1709 KB) | [ota/satellit-esp32-s3.bin](ota/satellit-esp32-s3.bin) (1645 KB) | 0.2.0 (f61d015): [komplett](vorher/satellit-esp32-s3.bin), [Update](vorher/ota/satellit-esp32-s3.bin) |
| satellit-esp32 | 0.2.0 (a2d18dc) | [satellit-esp32.bin](satellit-esp32.bin) (1799 KB) | [ota/satellit-esp32.bin](ota/satellit-esp32.bin) (1735 KB) | 0.2.0 (f61d015): [komplett](vorher/satellit-esp32.bin), [Update](vorher/ota/satellit-esp32.bin) |
| satellit-relay16 | 0.2.0 (a2d18dc) | [satellit-relay16.bin](satellit-relay16.bin) (1800 KB) | [ota/satellit-relay16.bin](ota/satellit-relay16.bin) (1736 KB) | 0.2.0 (f61d015): [komplett](vorher/satellit-relay16.bin), [Update](vorher/ota/satellit-relay16.bin) |
| updater-esp32 | 0.2.0 (a2d18dc) | – | [ota/updater-esp32.bin](ota/updater-esp32.bin) (936 KB) | 0.2.0 (fd760c9): [komplett](vorher/updater-esp32.bin), [Update](vorher/ota/updater-esp32.bin) |
| victron-display-cyd-en | 0.2.0 (a2d18dc) | [victron-display-cyd-en.bin](victron-display-cyd-en.bin) (3944 KB) | [ota/victron-display-cyd-en.bin](ota/victron-display-cyd-en.bin) (2359 KB) | – |
| victron-display-cyd | 0.2.0 (a2d18dc) | [victron-display-cyd.bin](victron-display-cyd.bin) (3944 KB) | [ota/victron-display-cyd.bin](ota/victron-display-cyd.bin) (2319 KB) | 0.2.0 (fd760c9): [komplett](vorher/victron-display-cyd.bin), [Update](vorher/ota/victron-display-cyd.bin) |
| zusatzdisplay-cyd | 0.2.0 (a2d18dc) | [zusatzdisplay-cyd.bin](zusatzdisplay-cyd.bin) (3944 KB) | [ota/zusatzdisplay-cyd.bin](ota/zusatzdisplay-cyd.bin) (2223 KB) | 0.2.0 (fd760c9): [komplett](vorher/zusatzdisplay-cyd.bin), [Update](vorher/ota/zusatzdisplay-cyd.bin) |
