#!/usr/bin/env python3
"""Uebersetzungen fuer den Browser-Editor pflegen.

Die Texte im Editor (web/editor.src.html) sind deutsch. Die Wörterbuecher liegen als JSON in web/i18n/<kuerzel>.json:
    {"Einstellungen": "Settings", "vor {} Min.": "{} min ago", ...}
"{}" steht fuer einen veraenderlichen Teil (Zahl, Name …) und wird in der Uebersetzung an derselben Reihenfolge
eingesetzt. Leere Uebersetzung ("") = bleibt deutsch.

  python tools/i18n_editor.py            # Texte sammeln, fehlende in alle JSON-Dateien eintragen, Woerterbuecher
                                         # in web/editor.src.html und web/editor.html einbauen
  python tools/i18n_editor.py fr         # zusaetzlich neue Sprache "fr" anlegen
  python tools/i18n_editor.py --report   # nur zaehlen

Der Editor waehlt die Sprache des Panels ([features] lang in platformio.ini) oder per Link: http://<panel>/?lang=en
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'web', 'editor.src.html')
OUT = os.path.join(ROOT, 'web', 'editor.html')
DIR = os.path.join(ROOT, 'web', 'i18n')
MARK_A, MARK_B = '/*@@I18N@@*/', '/*@@I18N-END@@*/'
EXPR = '\x00'   # Platzhalter fuer ${...} beim Zerlegen

GERMAN_WORDS = set('''nicht und oder kein keine keinen noch ist sind mit für der die das den dem des aus an auf vor bei im
am zu zum zur wird werden wurde alle neu schon eingerichtet gefunden verbunden gespeichert läuft bitte hier dann nur
nach von wie was wenn ohne über unter ein eine einen einem gerät geräte seite seiten kachel kacheln mehr weniger sekunden
minuten min std tag tagen gerade eben jetzt leer voll aus an'''.split())


def looks_like_text(s):
    t = s.strip()
    if len(t) < 2 or t.startswith(('http', '/api', '#', '.', '--')):
        return False
    letters = re.findall(r'[A-Za-zÄÖÜäöüß]', t.replace('{}', ''))
    if len(letters) < 2:
        return False
    if re.search(r'[;=<>]|^[\w-]+\(|=>|\$\{|\}\s*$', t.replace('{}', '')):   # Code, CSS
        return False
    if re.search(r'[äöüßÄÖÜ]', t):
        return True
    words = re.findall(r'[A-Za-zÄÖÜäöüß]+', t)
    if any(w.lower() in GERMAN_WORDS for w in words):
        return True
    if re.fullmatch(r'[a-z0-9_.\-: ]+', t):                         # Klassen, Schluessel, Einheiten
        return False
    return bool(re.search(r'[A-ZÄÖÜ]', t))


# ---------------------------------------------------------------- JavaScript zerlegen
def scan_js(js, out):
    """Sammelt Zeichenketten und Template-Literale (rekursiv) als Listen von Teilen."""
    i, n = 0, len(js)
    while i < n:
        c = js[i]
        if js.startswith('//', i):
            while i < n and js[i] != '\n':
                i += 1
        elif js.startswith('/*', i):
            j = js.find('*/', i + 2)
            i = n if j < 0 else j + 2
        elif c in '\'"':
            j = i + 1
            buf = []
            while j < n and js[j] != c and js[j] != '\n':
                if js[j] == '\\' and j + 1 < n:
                    buf.append(js[j:j + 2])
                    j += 2
                else:
                    buf.append(js[j])
                    j += 1
            out.append(''.join(buf))
            i = j + 1
        elif c == '`':
            i = scan_template(js, i + 1, out)
        elif c == '/' and re.match(r'[=(,:!&|?{};\[]\s*$', js[max(0, i - 3):i] or '('):
            # Regulaerer Ausdruck ueberspringen
            j = i + 1
            while j < n and js[j] != '/' and js[j] != '\n':
                j += 2 if js[j] == '\\' else 1
            i = j + 1
        else:
            i += 1


def scan_template(js, i, out):
    parts = []
    buf = []
    n = len(js)
    while i < n:
        c = js[i]
        if c == '\\':
            buf.append(js[i:i + 2])
            i += 2
        elif c == '`':
            parts.append(''.join(buf))
            out.append(EXPR.join(parts))
            return i + 1
        elif js.startswith('${', i):
            parts.append(''.join(buf))
            buf = []
            depth, j = 1, i + 2
            start = j
            while j < n and depth:
                ch = js[j]
                if ch in '\'"':
                    q = ch
                    j += 1
                    while j < n and js[j] != q:
                        j += 2 if js[j] == '\\' else 1
                elif ch == '`':
                    j = scan_template(js, j + 1, out) - 1
                elif ch == '{':
                    depth += 1
                elif ch == '}':
                    depth -= 1
                j += 1
            scan_js(js[start:j - 1], out)   # Zeichenketten im Ausdruck
            i = j
        else:
            buf.append(c)
            i += 1
    return n


ATTR = re.compile(r'\b(?:title|placeholder|aria-label|alt)="([^"]*)"')
TAG = re.compile(r'<[^<>]*>')


def texts_from_markup(s, add):
    for m in ATTR.finditer(s):
        add(m.group(1))
    for run in TAG.split(s):
        add(run)


def collect():
    html = open(SRC, encoding='utf-8').read()
    keys = []
    seen = set()

    def add(raw):
        t = re.sub(r'\s+', ' ', raw.replace('\\n', ' ')).strip()
        if EXPR in t:
            if t.count(EXPR) > 3 or EXPR + EXPR in t or not re.sub(EXPR, '', t).strip(' ,.:–-·()'):
                return
            t = t.replace(EXPR, '{}')
        t = t.replace('\\u202F', ' ').replace("\\'", "'").replace('\\"', '"')
        if '&' in t and ';' in t:
            return
        if looks_like_text(t) and t not in seen:
            seen.add(t)
            keys.append(t)

    # Markup ausserhalb der Skripte
    body = re.sub(r'<script>.*?</script>|<style>.*?</style>', ' ', html, flags=re.S)
    texts_from_markup(body, add)
    # Skripte
    for js in re.findall(r'<script>(.*?)</script>', html, flags=re.S):
        js = js.split(MARK_A)[0] + js.split(MARK_B)[-1] if MARK_A in js else js
        out = []
        scan_js(js, out)
        for s in out:
            if '<' in s:
                texts_from_markup(s, add)
            else:
                add(s)
    return keys


def load(code):
    path = os.path.join(DIR, code + '.json')
    if os.path.exists(path):
        return json.load(open(path, encoding='utf-8'))
    return {}


def save(code, keys, old):
    data = {k: old.get(k, '') for k in keys}
    for k, v in old.items():      # nicht mehr gefundene Eintraege behalten (koennen dynamisch sein)
        if k not in data:
            data[k] = v
    os.makedirs(DIR, exist_ok=True)
    with open(os.path.join(DIR, code + '.json'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
        f.write('\n')
    return data


def inject(dicts):
    block = MARK_A + 'const I18N = ' + json.dumps(
        {c: {k: v for k, v in d.items() if v} for c, d in sorted(dicts.items())}, ensure_ascii=False,
        separators=(',', ':')) + ';' + MARK_B
    for path in (SRC, OUT):
        s = open(path, encoding='utf-8').read()
        a, b = s.find(MARK_A), s.find(MARK_B)
        if a < 0 or b < 0:
            print('  Marker fehlt in', os.path.relpath(path, ROOT))
            continue
        s = s[:a] + block + s[b + len(MARK_B):]
        open(path, 'w', encoding='utf-8', newline='').write(s)
    print('  Wörterbücher eingebaut (%d KB)' % (len(block.encode('utf-8')) // 1024))


def main():
    langs = [a for a in sys.argv[1:] if not a.startswith('--')]
    report = '--report' in sys.argv
    keys = collect()
    os.makedirs(DIR, exist_ok=True)
    codes = sorted(set(langs) | {f[:-5] for f in os.listdir(DIR) if f.endswith('.json')})
    print('%d Texte im Editor' % len(keys))
    dicts = {}
    for code in codes:
        old = load(code)
        done = sum(1 for k in keys if old.get(k))
        print('  %-3s %4d von %d übersetzt, %d fehlen' % (code, done, len(keys), len(keys) - done))
        dicts[code] = old if report else save(code, keys, old)
    if not report:
        inject(dicts)


if __name__ == '__main__':
    main()
