# Fertige Firmware – Version 0.2.0

Automatisch von GitHub gebaut (zuletzt aus Commit 328c2a8, 2026-11-16 21:23 UTC).
Die jeweils vorige Fassung liegt in [`vorher/`](vorher/).

**Per USB (komplett, Adresse 0x0):** die Datei des Boards in diesem Ordner, z. B.
`esptool.py write_flash 0x0 panel-waveshare-7b.bin` – fuer neue Geraete. **Achtung: loescht die Einstellungen**
(WLAN, Geraete, Kacheln), weil die Datei auch den Einstellungsbereich enthaelt.

**Per USB, Einstellungen behalten:** die gleichnamige Datei aus `ota/` an Adresse 0x10000, z. B.
`esptool.py write_flash 0x10000 ota/panel-waveshare-7b.bin`.

**Ueber die Weboberflaeche (Update):** die gleichnamige Datei in `ota/`.

| Board | Version | Komplett (USB) | Update (Weboberflaeche) | vorige Fassung |
|---|---|---|---|---|
| panel-crowpanel-146 | 0.2.0 (328c2a8) | [panel-crowpanel-146.bin](panel-crowpanel-146.bin) (2963 KB) | [ota/panel-crowpanel-146.bin](ota/panel-crowpanel-146.bin) (2899 KB) | 0.2.0 (4ad429b): [komplett](vorher/panel-crowpanel-146.bin), [Update](vorher/ota/panel-crowpanel-146.bin) |
| panel-crowpanel-p4 | 0.2.0 (328c2a8) | [panel-crowpanel-p4.bin](panel-crowpanel-p4.bin) (3354 KB) | [ota/panel-crowpanel-p4.bin](ota/panel-crowpanel-p4.bin) (3290 KB) | 0.2.0 (4ad429b): [komplett](vorher/panel-crowpanel-p4.bin), [Update](vorher/ota/panel-crowpanel-p4.bin) |
| panel-waveshare-7b | 0.2.0 (328c2a8) | [panel-waveshare-7b.bin](panel-waveshare-7b.bin) (3136 KB) | [ota/panel-waveshare-7b.bin](ota/panel-waveshare-7b.bin) (3072 KB) | 0.2.0 (4ad429b): [komplett](vorher/panel-waveshare-7b.bin), [Update](vorher/ota/panel-waveshare-7b.bin) |
| satellit-cyd | 0.2.0 (328c2a8) | [satellit-cyd.bin](satellit-cyd.bin) (2005 KB) | [ota/satellit-cyd.bin](ota/satellit-cyd.bin) (1941 KB) | 0.2.0 (4ad429b): [komplett](vorher/satellit-cyd.bin), [Update](vorher/ota/satellit-cyd.bin) |
| satellit-esp32-c3 | 0.2.0 (328c2a8) | [satellit-esp32-c3.bin](satellit-esp32-c3.bin) (1852 KB) | [ota/satellit-esp32-c3.bin](ota/satellit-esp32-c3.bin) (1788 KB) | 0.2.0 (4ad429b): [komplett](vorher/satellit-esp32-c3.bin), [Update](vorher/ota/satellit-esp32-c3.bin) |
| satellit-esp32-c6 | 0.2.0 (328c2a8) | [satellit-esp32-c6.bin](satellit-esp32-c6.bin) (2008 KB) | [ota/satellit-esp32-c6.bin](ota/satellit-esp32-c6.bin) (1944 KB) | 0.2.0 (4ad429b): [komplett](vorher/satellit-esp32-c6.bin), [Update](vorher/ota/satellit-esp32-c6.bin) |
| satellit-esp32-s3 | 0.2.0 (328c2a8) | [satellit-esp32-s3.bin](satellit-esp32-s3.bin) (1674 KB) | [ota/satellit-esp32-s3.bin](ota/satellit-esp32-s3.bin) (1610 KB) | 0.2.0 (4ad429b): [komplett](vorher/satellit-esp32-s3.bin), [Update](vorher/ota/satellit-esp32-s3.bin) |
| satellit-esp32 | 0.2.0 (328c2a8) | [satellit-esp32.bin](satellit-esp32.bin) (1741 KB) | [ota/satellit-esp32.bin](ota/satellit-esp32.bin) (1677 KB) | 0.2.0 (4ad429b): [komplett](vorher/satellit-esp32.bin), [Update](vorher/ota/satellit-esp32.bin) |
| satellit-relay16 | 0.2.0 (328c2a8) | [satellit-relay16.bin](satellit-relay16.bin) (1743 KB) | [ota/satellit-relay16.bin](ota/satellit-relay16.bin) (1679 KB) | 0.2.0 (4ad429b): [komplett](vorher/satellit-relay16.bin), [Update](vorher/ota/satellit-relay16.bin) |
| updater-esp32 | 0.2.0 (328c2a8) | – | [ota/updater-esp32.bin](ota/updater-esp32.bin) (936 KB) | 0.2.0 (4ad429b): [komplett](vorher/updater-esp32.bin), [Update](vorher/ota/updater-esp32.bin) |
| victron-display-cyd | 0.2.0 (328c2a8) | [victron-display-cyd.bin](victron-display-cyd.bin) (3944 KB) | [ota/victron-display-cyd.bin](ota/victron-display-cyd.bin) (2312 KB) | 0.2.0 (4ad429b): [komplett](vorher/victron-display-cyd.bin), [Update](vorher/ota/victron-display-cyd.bin) |
