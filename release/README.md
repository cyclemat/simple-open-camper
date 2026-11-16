# Fertige Firmware – Version 0.2.0

Automatisch von GitHub gebaut (zuletzt aus Commit e15c080, 2026-11-16 23:58 UTC).
Die jeweils vorige Fassung liegt in [`vorher/`](vorher/).

**Per USB (komplett, Adresse 0x0):** die Datei des Boards in diesem Ordner, z. B.
`esptool.py write_flash 0x0 panel-waveshare-7b.bin` – fuer neue Geraete. **Achtung: loescht die Einstellungen**
(WLAN, Geraete, Kacheln), weil die Datei auch den Einstellungsbereich enthaelt.

**Per USB, Einstellungen behalten:** die gleichnamige Datei aus `ota/` an Adresse 0x10000, z. B.
`esptool.py write_flash 0x10000 ota/panel-waveshare-7b.bin`.

**Ueber die Weboberflaeche (Update):** die gleichnamige Datei in `ota/`.

| Board | Version | Komplett (USB) | Update (Weboberflaeche) | vorige Fassung |
|---|---|---|---|---|
| panel-crowpanel-146 | 0.2.0 (e15c080) | [panel-crowpanel-146.bin](panel-crowpanel-146.bin) (3000 KB) | [ota/panel-crowpanel-146.bin](ota/panel-crowpanel-146.bin) (2936 KB) | 0.2.0 (328c2a8): [komplett](vorher/panel-crowpanel-146.bin), [Update](vorher/ota/panel-crowpanel-146.bin) |
| panel-crowpanel-p4 | 0.2.0 (e15c080) | [panel-crowpanel-p4.bin](panel-crowpanel-p4.bin) (3397 KB) | [ota/panel-crowpanel-p4.bin](ota/panel-crowpanel-p4.bin) (3333 KB) | 0.2.0 (328c2a8): [komplett](vorher/panel-crowpanel-p4.bin), [Update](vorher/ota/panel-crowpanel-p4.bin) |
| panel-waveshare-7b | 0.2.0 (e15c080) | [panel-waveshare-7b.bin](panel-waveshare-7b.bin) (3169 KB) | [ota/panel-waveshare-7b.bin](ota/panel-waveshare-7b.bin) (3105 KB) | 0.2.0 (328c2a8): [komplett](vorher/panel-waveshare-7b.bin), [Update](vorher/ota/panel-waveshare-7b.bin) |
| satellit-cyd | 0.2.0 (e15c080) | [satellit-cyd.bin](satellit-cyd.bin) (2010 KB) | [ota/satellit-cyd.bin](ota/satellit-cyd.bin) (1946 KB) | 0.2.0 (328c2a8): [komplett](vorher/satellit-cyd.bin), [Update](vorher/ota/satellit-cyd.bin) |
| satellit-esp32-c3 | 0.2.0 (e15c080) | [satellit-esp32-c3.bin](satellit-esp32-c3.bin) (1856 KB) | [ota/satellit-esp32-c3.bin](ota/satellit-esp32-c3.bin) (1792 KB) | 0.2.0 (328c2a8): [komplett](vorher/satellit-esp32-c3.bin), [Update](vorher/ota/satellit-esp32-c3.bin) |
| satellit-esp32-c6 | 0.2.0 (e15c080) | [satellit-esp32-c6.bin](satellit-esp32-c6.bin) (2013 KB) | [ota/satellit-esp32-c6.bin](ota/satellit-esp32-c6.bin) (1949 KB) | 0.2.0 (328c2a8): [komplett](vorher/satellit-esp32-c6.bin), [Update](vorher/ota/satellit-esp32-c6.bin) |
| satellit-esp32-s3 | 0.2.0 (e15c080) | [satellit-esp32-s3.bin](satellit-esp32-s3.bin) (1679 KB) | [ota/satellit-esp32-s3.bin](ota/satellit-esp32-s3.bin) (1615 KB) | 0.2.0 (328c2a8): [komplett](vorher/satellit-esp32-s3.bin), [Update](vorher/ota/satellit-esp32-s3.bin) |
| satellit-esp32 | 0.2.0 (e15c080) | [satellit-esp32.bin](satellit-esp32.bin) (1745 KB) | [ota/satellit-esp32.bin](ota/satellit-esp32.bin) (1681 KB) | 0.2.0 (328c2a8): [komplett](vorher/satellit-esp32.bin), [Update](vorher/ota/satellit-esp32.bin) |
| satellit-relay16 | 0.2.0 (e15c080) | [satellit-relay16.bin](satellit-relay16.bin) (1747 KB) | [ota/satellit-relay16.bin](ota/satellit-relay16.bin) (1683 KB) | 0.2.0 (328c2a8): [komplett](vorher/satellit-relay16.bin), [Update](vorher/ota/satellit-relay16.bin) |
| updater-esp32 | 0.2.0 (e15c080) | – | [ota/updater-esp32.bin](ota/updater-esp32.bin) (936 KB) | 0.2.0 (328c2a8): [komplett](vorher/updater-esp32.bin), [Update](vorher/ota/updater-esp32.bin) |
| victron-display-cyd | 0.2.0 (e15c080) | [victron-display-cyd.bin](victron-display-cyd.bin) (3944 KB) | [ota/victron-display-cyd.bin](ota/victron-display-cyd.bin) (2313 KB) | 0.2.0 (328c2a8): [komplett](vorher/victron-display-cyd.bin), [Update](vorher/ota/victron-display-cyd.bin) |
| zusatzdisplay-cyd | 0.2.0 (e15c080) | [zusatzdisplay-cyd.bin](zusatzdisplay-cyd.bin) (3944 KB) | [ota/zusatzdisplay-cyd.bin](ota/zusatzdisplay-cyd.bin) (2116 KB) | – |
