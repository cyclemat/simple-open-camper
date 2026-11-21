#!/usr/bin/env python3
"""Fertige Firmware LOKAL nach release/ legen – wie der Schritt "Firmware nach release/ legen" in
.github/workflows/build.yml (fuer Zeiten ohne GitHub-Actions-Minuten).

  python tools/release_local.py            # aus den Build-Ordnern (.pio/build/<env>) aller Teilprojekte
  python tools/release_local.py --dry      # nur anzeigen, nichts kopieren

Vorher alle Umgebungen bauen und den Code committen: die Dateien bekommen "Version (Commit)" des aktuellen HEAD.
Pro Board: release/<name>.bin = Komplettdatei (USB, Adresse 0x0, loescht Einstellungen), release/ota/<name>.bin = App
(Update ueber die Weboberflaeche bzw. USB an 0x10000). Geaenderte Dateien wandern vorher nach release/vorher/.
Victron-Display und Zusatzdisplay bekommen den Updater (0x2F0000) mit in die Komplettdatei, das Zigbee-Modul die
Treiberliste (0x1F0000, release/zb_drivers.bin – erzeugt nur GitHub, bleibt sonst wie sie ist).
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime, timezone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REL = os.path.join(ROOT, 'release')
DRY = '--dry' in sys.argv

# Umgebung -> (Teilprojekt, Dateiname in release/) – Namen wie in build.yml
ENVS = [
    ('waveshare-7b', '.', 'panel-waveshare-7b'),
    ('crowpanel-p4', 'p4', 'panel-crowpanel-p4'),
    ('crowpanel-146', 'crowpanel146', 'panel-crowpanel-146'),
    ('crowpanel-146-remote', 'crowpanel146', 'zusatzdisplay-146'),
    ('esp32-cyd-victron', 'satellite', 'victron-display-cyd'),
    ('esp32-cyd-victron-en', 'satellite', 'victron-display-cyd-en'),
    ('esp32', 'satellite', 'satellit-esp32'),
    ('esp32-c3', 'satellite', 'satellit-esp32-c3'),
    ('esp32-c3-oled', 'satellite', 'satellit-esp32-c3-oled'),
    ('esp32-s3', 'satellite', 'satellit-esp32-s3'),
    ('esp32-c6', 'satellite', 'satellit-esp32-c6'),
    ('esp32-relay16', 'satellite', 'satellit-relay16'),
    ('esp32-cyd', 'satellite', 'satellit-cyd'),
    ('esp32-cyd-remote', 'satellite', 'zusatzdisplay-cyd'),
    ('shunt-c3', 'shunt', 'satellit-shunt-c3'),
    ('zigbee-h2', 'h2', 'zigbee-modul-h2'),
]
WITH_UPDATER = {'esp32-cyd-victron', 'esp32-cyd-victron-en', 'esp32-cyd-remote'}
ESPTOOL = os.path.join(ROOT, 'satellite', '.pio', 'packages', 'tool-esptoolpy', 'esptool.py')


def run(*a):
    return subprocess.run(a, cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()


def merge(chip, out, parts):
    args = [sys.executable, ESPTOOL, '--chip', chip, 'merge_bin', '-o', out]
    for off, f in parts:
        args += [off, f]
    subprocess.run(args, check=True, capture_output=True)


def same(a, b):
    if not os.path.exists(a) or not os.path.exists(b) or os.path.getsize(a) != os.path.getsize(b):
        return False
    with open(a, 'rb') as x, open(b, 'rb') as y:
        return x.read() == y.read()


def read_versions(p):
    d = {}
    if os.path.exists(p):
        for ln in open(p, encoding='utf-8'):
            k, _, v = ln.strip().partition(' ')
            if k:
                d[k] = v
    return d


def main():
    ver = re.search(r'SOC_VERSION\s+"([^"]+)"', open(os.path.join(ROOT, 'src', 'soc_version.h'), encoding='utf-8').read()).group(1)
    sha = run('git', 'rev-parse', '--short=7', 'HEAD')
    if run('git', 'status', '--porcelain', '--', 'src', 'web', 'platformio.ini', 'p4', 'crowpanel146', 'satellite', 'shunt', 'h2'):
        print('WARNUNG: Quelltext hat nicht committete Aenderungen – Version/Commit passt dann nicht genau.')
    print(f'Version {ver} ({sha})')
    tmp = tempfile.mkdtemp()
    new = os.path.join(tmp, 'new')
    os.makedirs(os.path.join(new, 'ota'))
    upd = os.path.join(ROOT, 'satellite', '.pio', 'build', 'esp32-updater', 'firmware.bin')
    if not os.path.exists(upd):
        upd = os.path.join(ROOT, 'release', 'lokal', 'esp32-updater@0x2F0000.bin')
    upd = upd if os.path.exists(upd) else None
    zbd = os.path.join(REL, 'zb_drivers.bin')
    zbd = zbd if os.path.exists(zbd) else None
    missing = []
    for env, proj, name in ENVS:
        d = os.path.join(ROOT, proj, '.pio', 'build', env)
        app, fac = os.path.join(d, 'firmware.bin'), os.path.join(d, 'firmware.factory.bin')
        if not os.path.exists(app) or not os.path.exists(fac):
            missing.append(env)
            continue
        shutil.copyfile(app, os.path.join(new, 'ota', name + '.bin'))
        out = os.path.join(new, name + '.bin')
        if env in WITH_UPDATER and upd:
            merge('esp32', out, [('0x0', fac), ('0x2F0000', upd)])
        elif env == 'zigbee-h2' and zbd:
            merge('esp32h2', out, [('0x0', fac), ('0x1F0000', zbd)])
        else:
            shutil.copyfile(fac, out)
    if upd:
        shutil.copyfile(upd, os.path.join(new, 'ota', 'updater-esp32.bin'))
    if missing:
        print('Nicht gebaut (bleiben wie sie sind):', ', '.join(missing))

    cur = read_versions(os.path.join(REL, 'versionen.txt'))
    prev = read_versions(os.path.join(REL, 'vorher', 'versionen.txt'))
    changed = []
    for sub in ('', 'ota'):
        for f in sorted(os.listdir(os.path.join(new, sub))):
            if not f.endswith('.bin'):
                continue
            k = (sub + '/' if sub else '') + f
            src, dst = os.path.join(new, sub, f), os.path.join(REL, sub, f)
            if same(src, dst):
                continue
            changed.append(k)
            if DRY:
                continue
            if os.path.exists(dst):
                os.makedirs(os.path.join(REL, 'vorher', sub), exist_ok=True)
                shutil.move(dst, os.path.join(REL, 'vorher', sub, f))
                prev[k] = cur.get(k, 'unbekannt')
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(src, dst)
            cur[k] = f'{ver} ({sha})'
    print(f'{len(changed)} Datei(en) neu:', ', '.join(changed) or '–')
    if DRY:
        return
    with open(os.path.join(REL, 'versionen.txt'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.writelines(f'{k} {cur[k]}\n' for k in sorted(cur))
    with open(os.path.join(REL, 'vorher', 'versionen.txt'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.writelines(f'{k} {prev[k]}\n' for k in sorted(prev))

    def kb(p):
        return f'{os.path.getsize(p) // 1024} KB' if os.path.exists(p) else '–'
    L = [f'# Fertige Firmware – Version {ver}', '',
         f'Gebaut aus Commit {sha} ({datetime.now(timezone.utc):%Y-%m-%d %H:%M} UTC).',
         'Die jeweils vorige Fassung liegt in [`vorher/`](vorher/).', '',
         '**Per USB (komplett, Adresse 0x0):** die Datei des Boards in diesem Ordner, z. B.',
         '`esptool.py write_flash 0x0 panel-waveshare-7b.bin` – fuer neue Geraete. **Achtung: loescht die Einstellungen**',
         '(WLAN, Geraete, Kacheln), weil die Datei auch den Einstellungsbereich enthaelt.', '',
         '**Per USB, Einstellungen behalten:** die gleichnamige Datei aus `ota/` an Adresse 0x10000, z. B.',
         '`esptool.py write_flash 0x10000 ota/panel-waveshare-7b.bin`.', '',
         '**Ueber die Weboberflaeche (Update):** die gleichnamige Datei in `ota/`.', '',
         '| Board | Version | Komplett (USB) | Update (Weboberflaeche) | vorige Fassung |', '|---|---|---|---|---|']
    for f in sorted(os.listdir(os.path.join(REL, 'ota'))):
        if not f.endswith('.bin'):
            continue
        b = f[:-4]
        full = f'[{b}.bin]({b}.bin) ({kb(os.path.join(REL, b + ".bin"))})' if os.path.exists(os.path.join(REL, b + '.bin')) else '–'
        pv = (f'{prev.get("ota/" + f, "?")}: [komplett](vorher/{b}.bin), [Update](vorher/ota/{b}.bin)'
              if os.path.exists(os.path.join(REL, 'vorher', 'ota', f)) else '–')
        L.append(f'| {b} | {cur.get("ota/" + f, "?")} | {full} | [ota/{f}](ota/{f}) ({kb(os.path.join(REL, "ota", f))}) | {pv} |')
    if zbd:
        L += ['', f'**Zigbee-Modul (ESP32-H2):** [zb_drivers.bin](zb_drivers.bin) ({kb(zbd)}) = Geraetetreiber allein,',
              'per USB nach `0x1F0000` – das Programm (`ota/zigbee-modul-h2.bin` nach `0x10000`) bleibt dabei unberuehrt.']
    apk = os.path.join(REL, 'soc-live.apk')
    if os.path.exists(apk):
        L += ['', f'**Android-App „S.O.C. Live“:** [soc-live.apk](soc-live.apk) ({kb(apk)}) – Live-Ansicht des Panels als App,',
              'Android 7 oder neuer, 64 Bit; Installation aus unbekannten Quellen erlauben.']
    with open(os.path.join(REL, 'README.md'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(L) + '\n')
    shutil.rmtree(tmp, ignore_errors=True)
    print('release/ aktualisiert.')


if __name__ == '__main__':
    main()
