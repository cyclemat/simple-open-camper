#!/usr/bin/env python3
"""Baut web/editor.html aus web/editor.src.html: bettet Inter (WOFF, Latin-1) und die Phosphor-Icons ein,
damit der Editor ohne Internet direkt vom Panel geladen werden kann."""
import base64, io, json, os
from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INTER = '/usr/share/fonts/opentype/inter/'
UNICODES = list(range(0x20, 0x7F)) + list(range(0xA0, 0x100)) + [0x2009, 0x2013, 0x2014, 0x2018, 0x2019, 0x201A,
                                                                  0x201C, 0x201D, 0x201E, 0x2022, 0x2026, 0x202F,
                                                                  0x20AC, 0x2212, 0x2192, 0x2190, 0x00D7]


def woff_b64(path):
    opts = subset.Options()
    opts.flavor = 'woff'
    opts.layout_features = ['kern', 'liga', 'calt', 'tnum', 'case', 'ss01']
    opts.name_IDs = []
    opts.hinting = False
    opts.desubroutinize = True
    f = TTFont(path)
    s = subset.Subsetter(opts)
    s.populate(unicodes=UNICODES)
    s.subset(f)
    buf = io.BytesIO()
    f.flavor = 'woff'
    f.save(buf)
    return base64.b64encode(buf.getvalue()).decode()


def main():
    faces = []
    for w, file in ((400, 'Inter-Regular.otf'), (500, 'Inter-Medium.otf'), (600, 'Inter-SemiBold.otf')):
        b = woff_b64(INTER + file)
        faces.append("@font-face{font-family:'Inter Panel';font-weight:%d;font-style:normal;font-display:swap;"
                     "src:url(data:font/woff;base64,%s) format('woff')}" % (w, b))
    icons = json.load(open(os.path.join(ROOT, 'assets', 'icons', 'editor_icons.json')))
    manifest = json.load(open(os.path.join(ROOT, 'assets', 'icons', 'manifest.json')))
    tile_ids = [i['id'] for i in manifest['icons'] if i['tile']]
    src = open(os.path.join(ROOT, 'web', 'editor.src.html'), encoding='utf-8').read()
    src = src.replace('/*@@FONTS@@*/', '\n'.join(faces))
    src = src.replace('/*@@ICONS@@*/', 'const ICON_PATHS=%s;const TILE_ICONS=%s;' % (
        json.dumps(icons, separators=(',', ':')), json.dumps(tile_ids)))
    out = os.path.join(ROOT, 'web', 'editor.html')
    open(out, 'w', encoding='utf-8').write(src)
    print('editor.html: %d KB' % (len(src.encode()) // 1024))


if __name__ == '__main__':
    main()
