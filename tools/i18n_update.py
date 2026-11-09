#!/usr/bin/env python3
"""Uebersetzungsdateien fuer die Panel-Oberflaeche pflegen.

Sammelt alle deutschen Texte aus src/ (Zeichenketten, die nach Oberflaechentext aussehen) und traegt fehlende
in src/i18n/<sprache>.h ein. Vorhandene Uebersetzungen bleiben erhalten; neue Eintraege bekommen eine leere
Uebersetzung ("") und erscheinen auf dem Panel so lange auf Deutsch.

  python tools/i18n_update.py            # alle Sprachdateien in src/i18n/ aktualisieren
  python tools/i18n_update.py it         # neue Sprache "it" anlegen bzw. aktualisieren
  python tools/i18n_update.py --report   # nur zaehlen, nichts schreiben

Eine neue Sprache: Skript mit dem Kuerzel aufrufen, rechte Seite in src/i18n/<kuerzel>.h uebersetzen,
in platformio.ini unter [features] "lang = <kuerzel>" setzen, bauen.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src')
I18N = os.path.join(SRC, 'i18n')
SKIP_DIRS = {'fonts', 'generated', 'i18n', 'sat'}
# Texte dieser Dateien erscheinen nicht auf dem Panel (Browser-Editor, Konfiguration)
SKIP_FILES = {'webserver.cpp', 'app_config.h', 'backup.cpp', 'layout.cpp', 'sat_proto.cpp'}

STR = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
ENTRY = re.compile(r'^\s*\{\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\},?\s*(//.*)?$')
# Zeilen, deren Zeichenketten sicher keine Oberflaechentexte sind
SKIP_LINE = re.compile(r'#include|#error|log_[ievwd]\(|log_[ievwd]\b|ESP_LOG|Serial\.|prefs?\.(get|put|begin|remove)|s_prefs\.|'
                       r'\["|JsonObject|\{"[A-Za-z]+", "[A-Za-z]+"\}|"ON"|deserialize|serializeJson|subscribe|publish|strcmp|'
                       r'== "|!= "|rfind\(|compare\(|\.find\(|startsWith|NimBLEUUID|UUID_|snprintf\(k,|_KEY_PREFIX|'
                       r'PARAMS|TOPICS|write_param|queue_write|rpc_query|rpc_payload|command\(key|'
                       r'z2m_publish|esp_country|set_country|configTzTime|cw_str|xml_tag|MQTT_')


def looks_like_text(s):
    if len(s) < 2 or s.startswith(('%', '/', '#', '\\x', 'http', 'zigbee2mqtt', 'wled/', 'shellies')):
        return False
    if '\\xEE' in s:                        # Icon-Zeichen
        return False
    letters = re.findall(r'[A-Za-zÄÖÜäöüß]', s)
    if len(letters) < 2:
        return False
    if re.fullmatch(r'[a-z0-9_.:\-]+', s):  # Schluessel, Topics, Dateinamen
        return False
    if '/' in s and re.fullmatch(r'[A-Za-z0-9_.+#/]+', s):   # MQTT-Topics wie "system/0/Dc/#"
        return False
    if re.fullmatch(r'[A-Z0-9_]+', s) and len(s) > 3:   # Konstanten
        return False
    return bool(re.search(r'[A-ZÄÖÜ]', s) or ' ' in s or re.search(r'[äöüß]', s))


# Folge von Zeichenketten, die nur durch Leerraum getrennt sind (der Compiler fuegt sie zusammen)
SEQ = re.compile(r'"(?:[^"\\\n]|\\.)*"(?:\s*"(?:[^"\\\n]|\\.)*")*')


def strip_comments(text):
    # // und /* */ entfernen, Zeichenketten und Zeilenzahl erhalten
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == '\\' else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith('//', i):
            while i < n and text[i] != '\n':
                i += 1
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append('\n' * text.count('\n', i, j))
            i = j
        elif c == "'":   # Zeichenkonstanten wie '"' ueberspringen
            j = i + 1
            while j < n and text[j] != "'":
                j += 2 if text[j] == '\\' else 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def collect():
    keys, where = [], {}
    for base, dirs, files in os.walk(SRC):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for fn in sorted(files):
            if not fn.endswith(('.cpp', '.h', '.c')) or fn in SKIP_FILES:
                continue
            path = os.path.join(base, fn)
            rel = os.path.relpath(path, ROOT).replace(os.sep, '/')
            text = strip_comments(open(path, encoding='utf-8').read())
            lines = text.split('\n')
            for m in SEQ.finditer(text):
                n = text.count('\n', 0, m.start()) + 1
                # ausdruecklich mit tr("...") markiert: immer aufnehmen
                explicit = text[max(0, m.start() - 3):m.start()] == 'tr('
                if not explicit and SKIP_LINE.search(lines[n - 1]):
                    continue
                # Fortsetzung einer Tabelle/Anweisung, deren erste Zeile ausgeschlossen ist
                prev = lines[n - 2].rstrip() if n >= 2 else ''
                if not explicit and prev.endswith(',') and SKIP_LINE.search(prev):
                    continue
                s = ''.join(x.group(1) for x in STR.finditer(m.group(0)))
                # Auswahllisten ("Nie\n30 Sekunden\n...") zeilenweise
                parts = s.split('\\n') if '\\n' in s else [s]
                for p in parts:
                    if (explicit or looks_like_text(p)) and p and p not in where:
                        where[p] = '%s:%d' % (rel, n)
                        keys.append(p)
    return keys, where


def read_lang(path):
    entries = {}
    if os.path.exists(path):
        for line in open(path, encoding='utf-8'):
            m = ENTRY.match(line)
            if m:
                entries[m.group(1)] = m.group(2)
    return entries


def write_lang(code, path, keys, where, old):
    lines = [
        '// Uebersetzung der Panel-Oberflaeche: %s' % code,
        '// Links der deutsche Originaltext (nicht aendern), rechts die Uebersetzung. "" = noch nicht uebersetzt,',
        '// dann bleibt der Text deutsch. Pflege mit: python tools/i18n_update.py ' + code,
        '// Platzhalter wie %d, %s, %u und Sonderzeichen (\\xE2\\x80\\xAF) in der Uebersetzung beibehalten.',
        '// clang-format off',
    ]
    for k in keys:
        v = old.get(k, '')
        lines.append('{"%s", "%s"},  // %s' % (k, v, where.get(k, '')))
    # Eintraege, deren Text im Code nicht mehr vorkommt, bleiben erhalten (falls noch gebraucht)
    gone = [k for k in old if k not in where]
    if gone:
        lines.append('// --- nicht mehr im Quelltext gefunden ---')
        for k in gone:
            lines.append('{"%s", "%s"},' % (k, old[k]))
    lines.append('// clang-format on')
    open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    report = '--report' in sys.argv
    keys, where = collect()
    os.makedirs(I18N, exist_ok=True)
    langs = set(args) | {f[:-2] for f in os.listdir(I18N) if f.endswith('.h') and f not in ('i18n.h',)}
    print('%d Texte im Quelltext' % len(keys))
    for code in sorted(langs):
        path = os.path.join(I18N, code + '.h')
        old = read_lang(path)
        done = sum(1 for k in keys if old.get(k))
        print('  %-3s %4d von %d übersetzt, %d fehlen' % (code, done, len(keys), len(keys) - done))
        if not report:
            write_lang(code, path, keys, where, old)


if __name__ == '__main__':
    main()
