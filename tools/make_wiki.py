#!/usr/bin/env python3
"""Erzeugt das GitHub-Wiki (Ordner wiki/) aus README.md – eine Quelle, kein doppelter Text.

Jede Wiki-Seite sammelt Abschnitte der README (nach Ueberschrift, Emojis werden ignoriert). Bilder und Dateien
werden auf absolute Adressen im Repo umgeschrieben (das Wiki ist ein eigenes Git-Repo), Sprungmarken innerhalb der
README auf die passende Wiki-Seite. Dazu Home.md (Startseite) und _Sidebar.md (Inhaltsverzeichnis).

  python tools/make_wiki.py            -> wiki/*.md (im Repo, zum Ansehen auf GitHub)
  python tools/publish_wiki.py         -> ins GitHub-Wiki schieben (erst wenn das Repo oeffentlich ist)
"""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = 'https://github.com/cyclemat/simple-open-camper'
OUT = os.path.join(ROOT, 'wiki')

# Seite -> (Titel, [Ueberschriften der README, die auf diese Seite kommen])
PAGES = [
    ('Funktionen', 'Funktionen', ['Funktionen']),
    ('Panels-und-Voraussetzungen', 'Panels und Voraussetzungen', ['Voraussetzungen', 'Welches Panel? P4 oder Waveshare']),
    ('Installation', 'Installation und Flashen', ['Installation', 'Fertige Firmware ohne neu zu bauen']),
    ('Konfiguration', 'Konfiguration (Module, Sprachen)', ['Konfiguration', 'Module ein- und ausschalten', 'Sprachen & Wörterbücher',
                                                            'SMHUB Nano']),
    ('Gas-und-Tanks', 'Gas und Tanks', ['Truma LevelControl', 'Mopeka', 'Senso4s', 'Simarine STRx-Tanksensoren']),
    ('Akkus-und-Victron', 'Akkus, Power Stations, Victron und Votronic', ['Akkus und Power Stations', 'Victron', 'Victron Cerbo GX', 'Votronic']),
    ('Sensoren', 'Sensoren (BTHome, Ruuvi, SwitchBot) und Wasserwaage', ['BTHome', 'Ruuvi und SwitchBot', 'Wasserwaage']),
    ('Heizung-und-Kuehlbox', 'Heizung und Kühlbox', ['Truma-Heizung', 'Alde', 'Dieselheizung', 'Kühlbox']),
    ('Fahrzeug', 'Fahrzeug: Hymer und Hobby', ['Hymer Connect', 'Hobby']),
    ('Kabelanschluesse', 'Kabelanschlüsse (LIN, CI-BUS, Zigbee-Modul)', ['Kabelanschlüsse']),
    ('App', 'App „S.O.C. Live“ (Android)', ['App S.O.C. Live']),
    ('Live-Ansicht', 'Live-Ansicht', ['Live-Ansicht']),
    ('Tailscale', 'Tailscale – Fernzugriff', ['Tailscale']),
    ('Shelly-ESPHome-WLED', 'Shelly, ESPHome, Tasmota und WLED', ['Shelly per Bluetooth', 'ESPHome, Tasmota & Co.', 'WLED']),
    ('Startbild', 'Startbild', ['Startbild']),
    ('Ton-und-Musik', 'Ton & Musik (P4)', ['Ton & Musik']),
    ('Satelliten', 'Satelliten', ['Satelliten', 'Einrichten', 'Tanksonden', 'Neigung beruhigen']),
    ('Home-Assistant', 'Home Assistant', ['Home Assistant', 'Kein Daten-Kreislauf']),
    ('Victron-Display', 'Victron Edition (eigene Firmware)', ['Victron Edition']),
    ('Fehlersuche', 'Fehlersuche und Wartung', ['Wartung und Diagnose', 'Bekannte Einschränkungen']),
    ('REST-API', 'REST-API', ['REST-API']),
    ('Quellen-und-Lizenzen', 'Quellen und Lizenzen', ['Protokoll & verwandte Projekte', 'Verwendete Bibliotheken', 'Lizenz',
                                                      'Markenhinweis']),
    ('Mitmachen', 'Mitmachen und Unterstützen', ['Feedback & Beiträge', 'Support & Spenden', 'Beta – Tester und Ideen willkommen']),
]
GROUPS = [('Start', ['Funktionen', 'Panels-und-Voraussetzungen', 'Installation', 'Konfiguration']),
          ('Geräte', ['Gas-und-Tanks', 'Akkus-und-Victron', 'Sensoren', 'Heizung-und-Kuehlbox', 'Fahrzeug', 'Shelly-ESPHome-WLED']),
          ('Satelliten und Displays', ['Satelliten', 'Victron-Display']),
          ('Bedienen und Fernzugriff', ['App', 'Live-Ansicht', 'Ton-und-Musik', 'Tailscale', 'Home-Assistant']),
          ('Technik', ['Kabelanschluesse', 'Startbild', 'Fehlersuche', 'REST-API']),
          ('Projekt', ['Quellen-und-Lizenzen', 'Mitmachen'])]


def plain(h):
    """Ueberschrift ohne Emojis, Formatierung und Klammerzusaetze wie *(Beta)*"""
    h = re.sub(r'\*\(.*?\)\*', '', h)
    h = re.sub(r'[`*]', '', h)
    h = ''.join(c for c in h if c.isalnum() or c in ' -–&?,.()äöüÄÖÜß/:')
    return h.strip(' -–')


def anchor(h):
    """GitHub-Sprungmarke einer README-Ueberschrift"""
    a = h.strip().lower()
    a = re.sub(r'[^\w\- ]', '', a, flags=re.UNICODE)
    return a.replace(' ', '-')


FLASHER_URL = 'https://cyclemat.github.io/simple-open-camper/flasher/'
FLASHER_BOX = ('<h2 align="center"><a href="%s">⚡ Zum Online-Flasher ⚡</a></h2>\n'
               '<p align="center">Firmware für Panels, Satelliten und Zubehör per USB direkt aus Chrome oder Edge aufspielen – '
               'ohne Download, ohne Installation.</p>\n\n' % FLASHER_URL)


def main():
    text = open(os.path.join(ROOT, 'README.md'), encoding='utf-8').read()
    # Abschnitte: (Ebene, roher Titel, Text inkl. Ueberschrift)
    secs, cur = [], None
    for line in text.split('\n'):
        m = re.match(r'^(##|###) (.+)$', line)
        if m:
            cur = [len(m.group(1)), m.group(2), [line]]
            secs.append(cur)
        elif cur:
            cur[2].append(line)
    # Zuordnung Abschnitt -> Seite (erste passende Seite gewinnt; Unterabschnitte gehoeren zur Seite des Oberabschnitts,
    # wenn sie nicht selbst zugeordnet sind)
    page_of, used = {}, set()
    last_page = None
    for i, (lvl, title, _) in enumerate(secs):
        p = plain(title)
        hit, best = None, 0   # genaueste (laengste) passende Ueberschrift gewinnt ("Victron-Display" vor "Victron")
        for pid, _, heads in PAGES:
            for h in heads:
                if p.startswith(h) and len(h) > best:
                    hit, best = pid, len(h)
        if hit:
            page_of[i] = hit
            last_page = hit if lvl == 2 else last_page
        elif lvl == 3 and last_page:
            page_of[i] = last_page
        if lvl == 2 and not hit:
            last_page = None
    # Sprungmarken der README -> Wiki-Seite
    links = {anchor(secs[i][1]): pid for i, pid in page_of.items()}

    def fix(md):
        md = re.sub(r'\]\(#([^)]+)\)', lambda m: '](%s)' % links[m.group(1)] if m.group(1) in links else m.group(0), md)
        md = re.sub(r'\]\(((?:assets|docs|src|tools|web|release|satellite|shunt|h2|p4|crowpanel146|flasher)/[^)\s]+)\)',
                    lambda m: '](%s/%s/main/%s)' % (REPO, 'raw' if re.search(r'\.(png|jpe?g|gif|svg|bin)$', m.group(1)) else 'blob',
                                                     m.group(1)), md)
        md = re.sub(r'\]\((README_EN\.md|CHANGELOG\.md|LICENSE|CONTRIBUTING\.md|flasher\.bat)\)', r'](%s/blob/main/\1)' % REPO, md)
        md = re.sub(r'src="(assets/[^"]+)"', r'src="%s/raw/main/\1"' % REPO, md)
        return md

    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith('.md'):
            os.remove(os.path.join(OUT, f))
    titles = {pid: t for pid, t, _ in PAGES}
    for pid, title, _ in PAGES:
        body = []
        for i, (lvl, t, lines) in enumerate(secs):
            if page_of.get(i) == pid:
                body.append('\n'.join(lines).rstrip().rstrip('-').rstrip())
        md = '# %s\n\n' % title + '\n\n'.join(body).replace('\n---\n', '\n') + '\n'
        md += '\n---\n*Diese Seite wird aus der [README](%s) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*\n' % REPO
        open(os.path.join(OUT, pid + '.md'), 'w', encoding='utf-8', newline='\n').write(fix(md))
    # Startseite: Einleitung der README (bis "Funktionen") + Inhaltsverzeichnis
    intro = []
    for i, (lvl, t, lines) in enumerate(secs):
        if plain(t).startswith('Über das Projekt'):
            intro = lines[1:]
    toc = '\n'.join('\n### %s\n' % g + '\n'.join('- [%s](%s)' % (titles[p], p) for p in pids) for g, pids in GROUPS)
    home = ('# Simple Open Camper – Handbuch\n\n'
            'Bedienpanel für Wohnmobil und Wohnwagen: Licht, Gas, Heizung und Batterie auf einem Bildschirm – '
            'per MQTT, Bluetooth, Zigbee und Kabel.\n\n' + FLASHER_BOX + '\n'.join(intro).strip() + '\n\n## Inhalt\n' + toc +
            '\n\n---\n[Code und fertige Firmware](%s) · [Änderungen](%s/blob/main/CHANGELOG.md) · '
            '[English README](%s/blob/main/README_EN.md)\n' % (REPO, REPO, REPO))
    open(os.path.join(OUT, 'Home.md'), 'w', encoding='utf-8', newline='\n').write(fix(home))
    side = '**[Startseite](Home)**\n\n**[⚡ Online-Flasher](%s)**\n' % FLASHER_URL + '\n'.join('\n**%s**\n' % g + '\n'.join('- [%s](%s)' % (titles[p], p) for p in pids)
                                               for g, pids in GROUPS) + '\n'
    open(os.path.join(OUT, '_Sidebar.md'), 'w', encoding='utf-8', newline='\n').write(side)
    missing = [secs[i][1] for i in range(len(secs)) if i not in page_of and secs[i][0] == 2]
    print('Wiki: %d Seiten in wiki/' % (len(PAGES) + 2))
    if missing:
        print('Nicht im Wiki (Absicht?):', ', '.join(plain(m) for m in missing))


if __name__ == '__main__':
    main()
