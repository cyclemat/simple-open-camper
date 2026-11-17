# Fertige Firmware – Version 0.2.0

Automatisch von GitHub gebaut (zuletzt aus Commit a035d64, 2026-11-17 22:52 UTC).
Die jeweils vorige Fassung liegt in [`vorher/`](vorher/).

**Per USB (komplett, Adresse 0x0):** die Datei des Boards in diesem Ordner, z. B.
`esptool.py write_flash 0x0 panel-waveshare-7b.bin` – fuer neue Geraete. **Achtung: loescht die Einstellungen**
(WLAN, Geraete, Kacheln), weil die Datei auch den Einstellungsbereich enthaelt.

**Per USB, Einstellungen behalten:** die gleichnamige Datei aus `ota/` an Adresse 0x10000, z. B.
`esptool.py write_flash 0x10000 ota/panel-waveshare-7b.bin`.

**Ueber die Weboberflaeche (Update):** die gleichnamige Datei in `ota/`.

| Board | Version | Komplett (USB) | Update (Weboberflaeche) | vorige Fassung |
|---|---|---|---|---|
| panel-crowpanel-146 | 0.2.0 (a035d64) | [panel-crowpanel-146.bin](panel-crowpanel-146.bin) (2997 KB) | [ota/panel-crowpanel-146.bin](ota/panel-crowpanel-146.bin) (2933 KB) | 0.2.0 (dfec943): [komplett](vorher/panel-crowpanel-146.bin), [Update](vorher/ota/panel-crowpanel-146.bin) |
| panel-crowpanel-p4 | 0.2.0 (a035d64) | [panel-crowpanel-p4.bin](panel-crowpanel-p4.bin) (3470 KB) | [ota/panel-crowpanel-p4.bin](ota/panel-crowpanel-p4.bin) (3406 KB) | 0.2.0 (092cfbb): [komplett](vorher/panel-crowpanel-p4.bin), [Update](vorher/ota/panel-crowpanel-p4.bin) |
| panel-waveshare-7b | 0.2.0 (a035d64) | [panel-waveshare-7b.bin](panel-waveshare-7b.bin) (3190 KB) | [ota/panel-waveshare-7b.bin](ota/panel-waveshare-7b.bin) (3126 KB) | 0.2.0 (211a593): [komplett](vorher/panel-waveshare-7b.bin), [Update](vorher/ota/panel-waveshare-7b.bin) |
| satellit-cyd | 0.2.0 (092cfbb) | [satellit-cyd.bin](satellit-cyd.bin) (2017 KB) | [ota/satellit-cyd.bin](ota/satellit-cyd.bin) (1953 KB) | 0.2.0 (75b1398): [komplett](vorher/satellit-cyd.bin), [Update](vorher/ota/satellit-cyd.bin) |
| satellit-esp32-c3 | 0.2.0 (092cfbb) | [satellit-esp32-c3.bin](satellit-esp32-c3.bin) (1863 KB) | [ota/satellit-esp32-c3.bin](ota/satellit-esp32-c3.bin) (1799 KB) | 0.2.0 (75b1398): [komplett](vorher/satellit-esp32-c3.bin), [Update](vorher/ota/satellit-esp32-c3.bin) |
| satellit-esp32-c6 | 0.2.0 (092cfbb) | [satellit-esp32-c6.bin](satellit-esp32-c6.bin) (2019 KB) | [ota/satellit-esp32-c6.bin](ota/satellit-esp32-c6.bin) (1955 KB) | 0.2.0 (75b1398): [komplett](vorher/satellit-esp32-c6.bin), [Update](vorher/ota/satellit-esp32-c6.bin) |
| satellit-esp32-s3 | 0.2.0 (092cfbb) | [satellit-esp32-s3.bin](satellit-esp32-s3.bin) (1684 KB) | [ota/satellit-esp32-s3.bin](ota/satellit-esp32-s3.bin) (1620 KB) | 0.2.0 (75b1398): [komplett](vorher/satellit-esp32-s3.bin), [Update](vorher/ota/satellit-esp32-s3.bin) |
| satellit-esp32 | 0.2.0 (092cfbb) | [satellit-esp32.bin](satellit-esp32.bin) (1751 KB) | [ota/satellit-esp32.bin](ota/satellit-esp32.bin) (1687 KB) | 0.2.0 (75b1398): [komplett](vorher/satellit-esp32.bin), [Update](vorher/ota/satellit-esp32.bin) |
| satellit-relay16 | 0.2.0 (092cfbb) | [satellit-relay16.bin](satellit-relay16.bin) (1752 KB) | [ota/satellit-relay16.bin](ota/satellit-relay16.bin) (1688 KB) | 0.2.0 (75b1398): [komplett](vorher/satellit-relay16.bin), [Update](vorher/ota/satellit-relay16.bin) |
| updater-esp32 | 0.2.0 (a035d64) | – | [ota/updater-esp32.bin](ota/updater-esp32.bin) (936 KB) | 0.2.0 (211a593): [komplett](vorher/updater-esp32.bin), [Update](vorher/ota/updater-esp32.bin) |
| victron-display-cyd | 0.2.0 (a035d64) | [victron-display-cyd.bin](victron-display-cyd.bin) (3944 KB) | [ota/victron-display-cyd.bin](ota/victron-display-cyd.bin) (2316 KB) | 0.2.0 (211a593): [komplett](vorher/victron-display-cyd.bin), [Update](vorher/ota/victron-display-cyd.bin) |
| zusatzdisplay-cyd | 0.2.0 (a035d64) | [zusatzdisplay-cyd.bin](zusatzdisplay-cyd.bin) (3944 KB) | [ota/zusatzdisplay-cyd.bin](ota/zusatzdisplay-cyd.bin) (2223 KB) | 0.2.0 (211a593): [komplett](vorher/zusatzdisplay-cyd.bin), [Update](vorher/ota/zusatzdisplay-cyd.bin) |
