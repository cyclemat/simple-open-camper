#!/usr/bin/env python3
"""
Symbole (Phosphor Icons, Bold, MIT) fuer Panel und Editor erzeugen – ohne Node (Ersatz fuer tools/gen_icons.js):
  1. laedt fehlende SVGs von github.com/phosphor-icons/core (assets/bold/<name>-bold.svg) nach assets/icons/svg/,
  2. rendert PNGs 24/32/44 px (weiss auf durchsichtig) mit Chrome headless (sharp gibt es hier nicht),
  3. schreibt assets/icons/manifest.json + editor_icons.json (SVG-Pfade fuer den Web-Editor),
  4. baut die LVGL-Schriften font_icons_24/32/44 und src/fonts/icons.h ueber tools/gen_fonts.py.
Aufruf: python tools/gen_icons.py [--alle]   (--alle rendert auch vorhandene PNGs neu)
Danach von Hand: Zeile "const ICON_PATHS=…;const TILE_ICONS=…;" in web/editor.html ersetzen (Ausgabe am Ende),
ICON_NAMES in web/editor.src.html und web/editor.html ergaenzen, web/i18n/en.json.
Reihenfolge = Codepoint-Reihenfolge ab U+E000: neue Symbole NUR hinten anfuegen (Layouts speichern die ID, die
Firmware das Zeichen).
"""
import json, os, re, subprocess, sys, urllib.request
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICON_DIR = os.path.join(ROOT, 'assets', 'icons')
SVG_DIR = os.path.join(ICON_DIR, 'svg')
CHROME = os.environ.get('SOC_CHROME', r'C:\Program Files\Google\Chrome\Application\chrome.exe')
PHOSPHOR = 'https://raw.githubusercontent.com/phosphor-icons/core/main/assets/bold/%s-bold.svg'
SIZES = [24, 32, 44]

# (ID, Phosphor-Name). Gleiche Liste wie PANEL_ICONS in gen_icons.js (dort React-Namen: PiLampPendantBold = lamp-pendant).
PANEL_ICONS = [
    # Kachel-Symbole (im Editor waehlbar) – Wohnen
    ('bulb', 'lightbulb'), ('filament', 'lightbulb-filament'), ('pendant', 'lamp-pendant'), ('lamp', 'lamp'),
    ('strip', 'rainbow'), ('sparkle', 'sparkle'), ('plug', 'plug'), ('toggle', 'toggle-right'), ('fan', 'fan'),
    ('tv', 'television-simple'), ('speaker', 'speaker-hifi'), ('heater', 'fire'), ('coffee', 'coffee'),
    ('washer', 'washing-machine'), ('plant', 'plant'), ('garden', 'tree'), ('garage', 'garage'), ('door', 'door-open'),
    ('bed', 'bed'), ('couch', 'couch'), ('desk', 'desk'), ('kitchen', 'cooking-pot'), ('bath', 'bathtub'),
    ('star', 'star'), ('moon', 'moon-stars'), ('lightning', 'lightning'),
    # Oberflaeche
    ('sun', 'sun'), ('sun_dim', 'sun-dim'), ('thermo', 'thermometer-simple'), ('palette', 'palette'),
    ('power', 'power'), ('gear', 'gear-six'), ('wifi', 'wifi-high'), ('wifi_off', 'wifi-slash'),
    ('link', 'link-simple'), ('link_off', 'link-simple-break'), ('pencil', 'pencil-simple'), ('close', 'x'),
    ('check', 'check'), ('left', 'caret-left'), ('right', 'caret-right'), ('info', 'info'), ('grid', 'squares-four'),
    ('eye', 'eye'), ('eye_off', 'eye-slash'), ('lock', 'lock-simple'), ('warning', 'warning'),
    ('refresh', 'arrows-clockwise'), ('monitor', 'monitor'), ('list', 'list'), ('house', 'house'),
    ('wand', 'magic-wand'), ('battery', 'battery-high'), ('plus', 'plus'),
    # Truma LevelControl
    ('gas', 'cylinder'), ('bluetooth', 'bluetooth'), ('clock', 'clock'), ('flame', 'flame'),
    ('search', 'magnifying-glass'),
    # Kachel-Symbole Boot und Technik (2026-11-25, Wunsch eines Nutzers; "propeller"/"radar" gibt es in Phosphor nicht)
    ('anchor', 'anchor'), ('boat', 'boat'), ('sailboat', 'sailboat'), ('engine', 'engine'), ('fuel', 'gas-pump'),
    ('waves', 'waves'), ('drop', 'drop'), ('shower', 'shower'), ('toilet', 'toilet'), ('pipe', 'pipe'),
    ('compass', 'compass'), ('lighthouse', 'lighthouse'), ('lifebuoy', 'lifebuoy'), ('siren', 'siren'),
    ('broadcast', 'broadcast'), ('wind', 'wind'), ('umbrella', 'umbrella'), ('ladder', 'ladder'), ('fish', 'fish'),
    ('flashlight', 'flashlight'), ('solar', 'solar-panel'), ('snowflake', 'snowflake'),
]
# Im Editor als Kachel-Symbol waehlbar: die ersten 26 (Wohnen) und alle ab "anchor"
TILE_IDS = {i for i, _ in PANEL_ICONS[:26]} | {i for i, _ in PANEL_ICONS[PANEL_ICONS.index(('anchor', 'anchor')):]}
# Nur im Web-Editor
EDITOR_EXTRA = [
    ('trash', 'trash'), ('undo', 'arrow-counter-clockwise'), ('save', 'floppy-disk'), ('image', 'image'),
    ('upload', 'upload-simple'), ('copy', 'copy'), ('caret_down', 'caret-down'), ('drag', 'dots-six-vertical'),
    ('dots', 'dots-three'), ('stack', 'stack'),
]


def fetch_svg(name):
    path = os.path.join(SVG_DIR, name + '-bold.svg')
    if not os.path.exists(path):
        print('lade', name)
        urllib.request.urlretrieve(PHOSPHOR % name, path)
    svg = open(path, encoding='utf-8').read()
    if 'viewBox="0 0 256 256"' not in svg:
        raise SystemExit('unerwartete viewBox: ' + name)
    return svg


def inner_paths(svg):
    m = re.search(r'<svg[^>]*>([\s\S]*)</svg>', svg)
    inner = m.group(1).strip()
    # wie react-icons: <path d="…"></path>
    return re.sub(r'<path d="([^"]*)"\s*/>', r'<path d="\1"></path>', inner)


def render(todo, size):
    """Alle Symbole in todo (id -> svg) als eine Seite rendern und zerschneiden."""
    gap = size + 8
    tag = '<svg width="%d" height="%d" ' % (size, size)
    cells = ''
    for i, svg in enumerate(todo.values()):
        cells += ('<div style="position:absolute;left:%dpx;top:0;width:%dpx;height:%dpx;color:#fff">%s</div>'
                  % (i * gap, size, size, svg.replace('<svg ', tag, 1)))
    html = os.path.join(ICON_DIR, '_render.html')
    png = os.path.join(ICON_DIR, '_render.png')
    open(html, 'w', encoding='utf-8').write(
        '<!doctype html><html><body style="margin:0;background:transparent">' + cells + '</body></html>')
    w = gap * len(todo) + 8
    subprocess.run([CHROME, '--headless=new', '--disable-gpu', '--hide-scrollbars', '--force-device-scale-factor=1',
                    '--default-background-color=00000000', f'--window-size={w},{size + 8}',
                    f'--screenshot={png}', 'file:///' + html.replace('\\', '/')],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    im = Image.open(png).convert('RGBA')
    for i, ident in enumerate(todo):
        tile = im.crop((i * gap, 0, i * gap + size, size))
        if tile.getbbox() is None:
            raise SystemExit(f'leeres Bild fuer {ident} ({size} px)')
        tile.save(os.path.join(ICON_DIR, f'{ident}_{size}.png'))
    os.remove(html)
    os.remove(png)


def main():
    alle = '--alle' in sys.argv
    os.makedirs(SVG_DIR, exist_ok=True)
    svgs = {i: fetch_svg(n) for i, n in PANEL_ICONS + EDITOR_EXTRA}
    for size in SIZES:
        todo = {i: svgs[i] for i, _ in PANEL_ICONS
                if alle or not os.path.exists(os.path.join(ICON_DIR, f'{i}_{size}.png'))}
        if todo:
            render(todo, size)
            print(f'{size} px: {len(todo)} Symbole gerendert')
    manifest = {'tile_icon_count': len(TILE_IDS),
                'icons': [{'id': i, 'cp': 0xE000 + k, 'tile': i in TILE_IDS} for k, (i, _) in enumerate(PANEL_ICONS)]}
    json.dump(manifest, open(os.path.join(ICON_DIR, 'manifest.json'), 'w'), indent=1)
    editor = {i: inner_paths(svgs[i]) for i, _ in PANEL_ICONS + EDITOR_EXTRA}
    json.dump(editor, open(os.path.join(ICON_DIR, 'editor_icons.json'), 'w'), separators=(',', ':'))
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import gen_fonts
    for size in SIZES:
        gen_fonts.gen_icons(manifest, size)
    gen_fonts.gen_headers(manifest)
    tiles = [i for i, _ in PANEL_ICONS if i in TILE_IDS]
    open(os.path.join(ICON_DIR, 'editor_line.txt'), 'w', encoding='utf-8').write(
        'const ICON_PATHS=%s;const TILE_ICONS=%s;' % (json.dumps(editor, separators=(',', ':')), json.dumps(tiles)))
    print(f'Symbole: {len(PANEL_ICONS)}, davon Kachel: {len(tiles)}, Editor: {len(editor)}.')
    print('Editor-Zeile fuer web/editor.html liegt in assets/icons/editor_line.txt')


if __name__ == '__main__':
    main()
