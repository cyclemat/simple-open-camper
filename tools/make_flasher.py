#!/usr/bin/env python3
"""Erzeugt die Geraeteliste und die Manifeste fuer den lokalen Web-Flasher (flasher/index.html, ESP Web Tools).

Quellen je Geraet:
  release   – fertige Dateien aus dem GitHub-Build (release/*.bin, Updates in release/ota/, Stand in release/versionen.txt)
  lokal     – hier gebaute Dateien (release/lokal/<env>.bin bzw. <env>-update.bin, Stand in release/lokal/stand.txt,
              von tools/upload_wait.py nach jedem Build abgelegt)
Je Quelle zwei Arten:
  komplett  – Bootloader, Partitionen, Firmware (CYD zusaetzlich Updater) ab 0x0: neu einrichten, Einstellungen weg
  update    – nur die Firmware (0x10000) + Startauswahl (0xe000, boot_app0): Einstellungen bleiben
Ergebnis: flasher/gen/geraete.json und flasher/gen/m/*.json (nicht im Repo, wird beim Start neu erzeugt).
Aufruf: python tools/make_flasher.py   (macht tools/flasher.py automatisch)
"""
import json
import os
import re
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN = os.path.join(ROOT, 'flasher', 'gen')

# id, Bereich, Name, Beschreibung, Chip (ESP Web Tools), Datei im Release, Umgebung (lokal), Updater bei 0x2F0000
DEVICES = [
    ('p4', 'panel', 'CrowPanel Advanced 7"', 'ESP32-P4, 1024 × 600 – das große Panel mit allem (Zigbee-Modul, Regeln, Tailscale)',
     'ESP32-P4', 'panel-crowpanel-p4.bin', 'crowpanel-p4', False),
    ('ws7', 'panel', 'Waveshare 7" (7B)', 'ESP32-S3, 1024 × 600', 'ESP32-S3', 'panel-waveshare-7b.bin', 'waveshare-7b', False),
    ('ws7v', 'panel', 'Waveshare 7" – Victron-Edition', 'ESP32-S3, nur Victron/Gas/Akkus/Shelly (Beta)', 'ESP32-S3', None,
     'waveshare-7b-victron', False),
    ('rund', 'panel', 'CrowPanel 1,46" rund', 'ESP32-S3, 360 × 360 mit Drehknopf', 'ESP32-S3', 'panel-crowpanel-146.bin',
     'crowpanel-146', False),
    ('sat32', 'sat', 'Satellit ESP32', 'ESP32 (z. B. DevKit, WROOM)', 'ESP32', 'satellit-esp32.bin', 'esp32', False),
    ('satc3', 'sat', 'Satellit ESP32-C3', 'ESP32-C3 (z. B. Super Mini)', 'ESP32-C3', 'satellit-esp32-c3.bin', 'esp32-c3', False),
    ('satc3o', 'sat', 'Satellit ESP32-C3 mit OLED', 'ESP32-C3 mit 0,42"-OLED (72 × 40)', 'ESP32-C3', 'satellit-esp32-c3-oled.bin', 'esp32-c3-oled', False),
    ('satc6', 'sat', 'Satellit ESP32-C6', 'ESP32-C6', 'ESP32-C6', 'satellit-esp32-c6.bin', 'esp32-c6', False),
    ('sats3', 'sat', 'Satellit ESP32-S3', 'ESP32-S3 (DevKitC-1)', 'ESP32-S3', 'satellit-esp32-s3.bin', 'esp32-s3', False),
    ('relay', 'sat', '16-Relais-Platine', 'ESP32 mit 2 × 74HC595', 'ESP32', 'satellit-relay16.bin', 'esp32-relay16', False),
    ('shunt', 'sat', 'SmartShunt-Satellit', 'ESP32-C3 Super Mini + INA228 (Beta)', 'ESP32-C3', 'satellit-shunt-c3.bin', 'shunt-c3', False),
    ('satcyd', 'sat', 'CYD-Satellit 2,8"', 'ESP32-2432S028R mit Anzeige', 'ESP32', 'satellit-cyd.bin', 'esp32-cyd', True),
    ('rdcyd', 'sat', 'Zusatzdisplay CYD 2,8"', 'zeigt Kacheln des Hauptpanels', 'ESP32', 'zusatzdisplay-cyd.bin', 'esp32-cyd-remote', True),
    ('rdrund', 'sat', 'Zusatzdisplay rund 1,46"', 'CrowPanel 1,46" als Zusatzdisplay', 'ESP32-S3', 'zusatzdisplay-146.bin', 'crowpanel-146-remote', False),
    ('vdcyd', 'sat', 'Victron-Display CYD 2,8"', 'eigenständig: Victron, Gas, Akkus (Deutsch)', 'ESP32', 'victron-display-cyd.bin',
     'esp32-cyd-victron', True),
    ('vdcyden', 'sat', 'Victron display CYD 2.8" (English)', 'standalone: Victron, gas, batteries', 'ESP32',
     'victron-display-cyd-en.bin', 'esp32-cyd-victron-en', True),
    ('h2', 'sat', 'Zigbee-Funkmodul ESP32-H2', 'Steckmodul für den P4 bzw. das runde Panel (eigener USB-Anschluss)', 'ESP32-H2',
     'zigbee-modul-h2.bin', None, False),
]
APP_OFFSET = 0x10000
OTADATA_OFFSET = 0xE000
UPDATER_OFFSET = 0x2F0000


def read_versions():
    rel, lok = {}, {}
    p = os.path.join(ROOT, 'release', 'versionen.txt')
    if os.path.exists(p):
        for line in open(p, encoding='utf-8'):
            m = re.match(r'(\S+)\s+(\S+)', line)
            if m:
                rel[m.group(1)] = m.group(2)
    p = os.path.join(ROOT, 'release', 'lokal', 'stand.txt')
    if os.path.exists(p):
        for line in open(p, encoding='utf-8'):
            parts = re.split(r'\s{2,}', line.strip())
            if len(parts) >= 3:
                lok[parts[0]] = (parts[1], parts[2])
    return rel, lok


def manifest(name, version, chip, parts, erase):
    return {
        'name': name,
        'version': version,
        'new_install_prompt_erase': erase,
        'new_install_improv_wait_time': 0,
        'builds': [{'chipFamily': chip, 'parts': [{'path': p, 'offset': o} for p, o in parts]}],
    }


def main():
    if os.path.isdir(GEN):
        shutil.rmtree(GEN)
    os.makedirs(os.path.join(GEN, 'm'))
    # Startauswahl (otadata mit app0): sorgt dafuer, dass nach einem Netz-Update (app1) wieder app0 startet
    boot = os.path.join(os.path.expanduser('~'), '.platformio', 'packages', 'framework-arduinoespressif32', 'tools',
                        'partitions', 'boot_app0.bin')
    have_boot = os.path.exists(boot)
    if have_boot:
        shutil.copyfile(boot, os.path.join(GEN, 'boot_app0.bin'))
    rel, lok = read_versions()
    out = []
    upd_lokal = os.path.join(ROOT, 'release', 'lokal', 'esp32-updater@0x2F0000.bin')
    upd_rel = os.path.join(ROOT, 'release', 'ota', 'updater-esp32.bin')
    for did, sec, name, desc, chip, relf, env, updater in DEVICES:
        dev = {'id': did, 'sec': sec, 'name': name, 'desc': desc, 'chip': chip, 'src': []}
        # Pfade relativ zum Manifest (flasher/gen/m/) -> Projektordner = ../../../
        up = '../../../'
        if relf and os.path.exists(os.path.join(ROOT, 'release', relf)):
            v = rel.get(relf, '?')
            s = {'key': 'release', 'label': 'GitHub-Release', 'version': v}
            mk = 'm/%s-release-komplett.json' % did
            json.dump(manifest(name, v, chip, [(up + 'release/' + relf, 0)], True), open(os.path.join(GEN, mk), 'w'))
            s['komplett'] = mk
            ota = os.path.join(ROOT, 'release', 'ota', relf)
            if os.path.exists(ota) and did != 'h2':
                parts = ([(up + 'flasher/gen/boot_app0.bin', OTADATA_OFFSET)] if have_boot else []) + [(up + 'release/ota/' + relf, APP_OFFSET)]
                mk = 'm/%s-release-update.json' % did
                json.dump(manifest(name, v, chip, parts, False), open(os.path.join(GEN, mk), 'w'))
                s['update'] = mk
            dev['src'].append(s)
        if env and os.path.exists(os.path.join(ROOT, 'release', 'lokal', env + '.bin')):
            v, when = lok.get(env, ('?', ''))
            s = {'key': 'lokal', 'label': 'Lokal gebaut', 'version': v, 'when': when}
            parts = [(up + 'release/lokal/%s.bin' % env, 0)]
            if updater and os.path.exists(upd_lokal):
                parts.append((up + 'release/lokal/esp32-updater@0x2F0000.bin', UPDATER_OFFSET))
            elif updater and os.path.exists(upd_rel):
                parts.append((up + 'release/ota/updater-esp32.bin', UPDATER_OFFSET))
            mk = 'm/%s-lokal-komplett.json' % did
            json.dump(manifest(name, v, chip, parts, True), open(os.path.join(GEN, mk), 'w'))
            s['komplett'] = mk
            if os.path.exists(os.path.join(ROOT, 'release', 'lokal', env + '-update.bin')):
                parts = ([(up + 'flasher/gen/boot_app0.bin', OTADATA_OFFSET)] if have_boot else []) + \
                        [(up + 'release/lokal/%s-update.bin' % env, APP_OFFSET)]
                mk = 'm/%s-lokal-update.json' % did
                json.dump(manifest(name, v, chip, parts, False), open(os.path.join(GEN, mk), 'w'))
                s['update'] = mk
            dev['src'].append(s)
        out.append(dev)
    json.dump({'devices': out}, open(os.path.join(GEN, 'geraete.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    n = sum(1 for d in out if d['src'])
    print('Flasher: %d von %d Geräten mit Firmware (flasher/gen/)' % (n, len(out)))


if __name__ == '__main__':
    main()
