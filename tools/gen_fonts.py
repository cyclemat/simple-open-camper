#!/usr/bin/env python3
"""
Erzeugt LVGL-8-Fonts (lv_font_fmt_txt, 4 bpp) als C-Dateien:
  * Textschriften aus Inter (SIL OFL) mit vollem Latin-1 (Umlaute, ß, €, „“ …)
  * Icon-Schriften aus den PNGs von tools/gen_icons.js (Phosphor, MIT), Codepoints ab U+E000
Ersetzt lv_font_conv, damit keine Node-Abhaengigkeit zur Laufzeit noetig ist.
Aufruf: python3 tools/gen_fonts.py
"""
import json, os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'src', 'fonts')
ICON_DIR = os.path.join(ROOT, 'assets', 'icons')
INTER = '/usr/share/fonts/opentype/inter/'

TEXT_CPS = list(range(0x20, 0x7F)) + list(range(0xA0, 0x100)) + [
    0x2009, 0x2013, 0x2014, 0x2018, 0x2019, 0x201A, 0x201C, 0x201D, 0x201E,
    0x2022, 0x2026, 0x202F, 0x20AC, 0x2212]

CLOCK_CPS = [ord(c) for c in ' -0123456789:']

TEXT_FONTS = [
    # name, file, size, fallback (Montserrat liefert die LV_SYMBOL-Zeichen der Tastatur), Zeichensatz
    ('font_ui_16', 'Inter-Regular.otf', 16, 'lv_font_montserrat_16', TEXT_CPS),
    ('font_ui_20', 'Inter-Medium.otf', 20, 'lv_font_montserrat_20', TEXT_CPS),
    ('font_ui_26', 'Inter-SemiBold.otf', 26, 'lv_font_montserrat_28', TEXT_CPS),
    ('font_ui_40', 'InterDisplay-SemiBold.otf', 40, None, TEXT_CPS),
    ('font_clock_140', 'InterDisplay-Regular.otf', 140, None, CLOCK_CPS),   # Bildschirmschoner-Uhr
    ('font_clock_104', 'InterDisplay-Regular.otf', 104, None, CLOCK_CPS),   # Uhr kleiner Displays (320 px breit)
]
ICON_SIZES = [24, 32, 44]


def quant4(a):
    return min(15, (a * 15 + 127) // 255)


def pack4(pixels):
    out = bytearray()
    acc = 0
    n = 0
    for p in pixels:
        acc = (acc << 4) | quant4(p)
        n += 1
        if n == 2:
            out.append(acc)
            acc = 0
            n = 0
    if n == 1:
        out.append(acc << 4)
    return bytes(out)


def glyph_from_image(img, origin_x, baseline_y):
    """img: L-Bild; liefert (box_w, box_h, ofs_x, ofs_y, bitmap)"""
    bbox = img.getbbox()
    if bbox is None:
        return 0, 0, 0, 0, b''
    x0, y0, x1, y1 = bbox
    crop = img.crop(bbox)
    return x1 - x0, y1 - y0, x0 - origin_x, baseline_y - y1, pack4(list(crop.get_flattened_data() if hasattr(crop, 'get_flattened_data') else crop.getdata()))


def write_font(name, glyphs, line_height, base_line, cmap_c, fallback, comment):
    """glyphs: Liste von dict(adv, w, h, ox, oy, bmp) in Glyph-ID-Reihenfolge (ab 1)"""
    bmp_all = bytearray()
    dsc = ['    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserviert */']
    for g in glyphs:
        idx = len(bmp_all)
        bmp_all += g['bmp']
        dsc.append('    {.bitmap_index = %d, .adv_w = %d, .box_w = %d, .box_h = %d, .ofs_x = %d, .ofs_y = %d}' % (
            idx, g['adv'], g['w'], g['h'], g['ox'], g['oy']))
    assert len(bmp_all) < (1 << 20)
    lines = []
    for i in range(0, len(bmp_all), 24):
        lines.append('    ' + ', '.join('0x%02x' % b for b in bmp_all[i:i + 24]) + ',')
    fb = '&%s' % fallback if fallback else 'NULL'
    fb_decl = 'LV_FONT_DECLARE(%s);\n' % fallback if fallback else ''
    src = f'''/* Automatisch erzeugt von tools/gen_fonts.py – nicht von Hand bearbeiten.
 * {comment}
 */
#include <lvgl.h>

{fb_decl}
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {{
{chr(10).join(lines) if lines else '    0x00,'}
}};

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {{
{(','+chr(10)).join(dsc)}
}};

{cmap_c}

#if LVGL_VERSION_MAJOR == 8
static lv_font_fmt_txt_glyph_cache_t cache;
#endif

static const lv_font_fmt_txt_dsc_t font_dsc = {{
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
}};

const lv_font_t {name} = {{
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = {line_height},
    .base_line = {base_line},
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = -2,
    .underline_thickness = 1,
    .dsc = &font_dsc,
    .fallback = {fb},
    .user_data = NULL,
}};
'''
    with open(os.path.join(OUT, name + '.c'), 'w', encoding='utf-8') as f:
        f.write(src)
    return len(bmp_all)


def gen_text(name, file, size, fallback, cps):
    font = ImageFont.truetype(os.path.join(INTER, file), size)
    ascent, descent = font.getmetrics()
    glyphs = []
    for cp in cps:
        ch = chr(cp)
        W, H = size * 3, size * 3
        img = Image.new('L', (W, H), 0)
        ox, oy = size // 2, size * 2
        ImageDraw.Draw(img).text((ox, oy), ch, font=font, fill=255, anchor='ls')
        w, h, gx, gy, bmp = glyph_from_image(img, ox, oy)
        adv = int(round(font.getlength(ch) * 16))
        glyphs.append(dict(adv=adv, w=w, h=h, ox=gx, oy=gy, bmp=bmp))
    first = cps[0]
    offs = ', '.join('0x%x' % (cp - first) for cp in cps)
    cmap = f'''static const uint16_t unicode_list_0[] = {{
    {offs}
}};

static const lv_font_fmt_txt_cmap_t cmaps[] = {{
    {{
        .range_start = {first}, .range_length = {cps[-1] - first + 1}, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = {len(cps)},
        .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }}
}};'''
    n = write_font(name, glyphs, ascent + descent, descent, cmap, fallback,
                   f'Inter ({file}), {size} px, 4 bpp, Latin-1 + typografische Zeichen. Lizenz: SIL Open Font License 1.1')
    print(f'{name}: {len(glyphs)} Glyphen, {n} Bytes Bitmap, line_height {ascent + descent}')


def gen_icons(manifest, size):
    name = f'font_icons_{size}'
    glyphs = []
    for ic in manifest['icons']:
        img = Image.open(os.path.join(ICON_DIR, f"{ic['id']}_{size}.png")).convert('RGBA').split()[3]
        w, h, gx, gy, bmp = glyph_from_image(img, 0, size)
        glyphs.append(dict(adv=size * 16, w=w, h=h, ox=gx, oy=gy, bmp=bmp))
    cmap = f'''static const lv_font_fmt_txt_cmap_t cmaps[] = {{
    {{
        .range_start = 0xE000, .range_length = {len(glyphs)}, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0,
        .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }}
}};'''
    n = write_font(name, glyphs, size, 0, cmap, None, f'Phosphor Icons (Bold), {size} px, 4 bpp. Lizenz: MIT')
    print(f'{name}: {len(glyphs)} Icons, {n} Bytes')


def utf8(cp):
    return ''.join('\\x%02X' % b for b in chr(cp).encode('utf-8'))


def gen_headers(manifest):
    decl = '\n'.join(f'LV_FONT_DECLARE({n});' for n, *_ in TEXT_FONTS)
    decl += '\n' + '\n'.join(f'LV_FONT_DECLARE(font_icons_{s});' for s in ICON_SIZES)
    with open(os.path.join(OUT, 'fonts.h'), 'w') as f:
        f.write(f'''// Automatisch erzeugt von tools/gen_fonts.py
#pragma once
#include <lvgl.h>
#ifdef __cplusplus
extern "C" {{
#endif
{decl}
#ifdef __cplusplus
}}
#endif
''')
    defs = '\n'.join(f'#define ICON_{ic["id"].upper()} "{utf8(ic["cp"])}"' for ic in manifest['icons'])
    tiles = [ic for ic in manifest['icons'] if ic['tile']]
    rows = ',\n'.join(f'    {{"{ic["id"]}", ICON_{ic["id"].upper()}}}' for ic in manifest['icons'])
    with open(os.path.join(OUT, 'icons.h'), 'w') as f:
        f.write(f'''// Automatisch erzeugt von tools/gen_fonts.py – Phosphor Icons (MIT)
#pragma once
#include <string.h>

{defs}

struct IconEntry {{ const char *id; const char *utf8; }};
static const IconEntry ICON_TABLE[] = {{
{rows}
}};
static const int ICON_TABLE_LEN = {len(manifest['icons'])};
static const int TILE_ICON_COUNT = {len(tiles)};

// Liefert das UTF-8-Zeichen eines Icons anhand seiner ID (z. B. "bulb"); Fallback: Gluehbirne
static inline const char *icon_utf8(const char *id) {{
    if (id) for (int i = 0; i < ICON_TABLE_LEN; i++) if (strcmp(ICON_TABLE[i].id, id) == 0) return ICON_TABLE[i].utf8;
    return ICON_BULB;
}}
''')


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    manifest = json.load(open(os.path.join(ICON_DIR, 'manifest.json')))
    for args in TEXT_FONTS:
        gen_text(*args)
    for s in ICON_SIZES:
        gen_icons(manifest, s)
    gen_headers(manifest)
