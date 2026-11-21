#!/usr/bin/env python3
"""Schiebt das erzeugte Wiki (wiki/*.md, siehe tools/make_wiki.py) ins GitHub-Wiki des Haupt-Repos.

Voraussetzungen (einmalig):
  1. Das Repo ist oeffentlich (bei privaten Repos gibt es das Wiki nur mit bezahltem GitHub-Tarif).
  2. Im Repo unter Settings -> Features "Wikis" einschalten.
  3. Im Reiter "Wiki" einmal "Create the first page" -> Speichern (erst dann legt GitHub das Wiki-Repo an).
Danach: python tools/publish_wiki.py   (erzeugt das Wiki neu und ersetzt alle Seiten)
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SLUG = 'cyclemat/simple-open-camper'
GH = r'C:\Program Files\GitHub CLI\gh.exe' if os.name == 'nt' else 'gh'


def run(cmd, cwd=None):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0:
        sys.exit('Fehler bei %s:\n%s%s' % (' '.join(cmd), r.stdout, r.stderr))
    return r.stdout


info = json.loads(run([GH, 'repo', 'view', SLUG, '--json', 'visibility,hasWikiEnabled']))
if info.get('visibility') != 'PUBLIC':
    sys.exit('Abbruch: Das Repo ist noch %s – das Wiki erst veroeffentlichen, wenn es oeffentlich ist.' % info.get('visibility'))
if not info.get('hasWikiEnabled'):
    sys.exit('Abbruch: Im Repo unter Settings -> Features "Wikis" einschalten und im Reiter Wiki einmal die erste Seite speichern.')

run([sys.executable, os.path.join(ROOT, 'tools', 'make_wiki.py')])
tmp = tempfile.mkdtemp(prefix='soc-wiki-')
try:
    run(['git', 'clone', '-q', 'https://github.com/%s.wiki.git' % SLUG, tmp])
    for f in os.listdir(tmp):
        if f.endswith('.md'):
            os.remove(os.path.join(tmp, f))
    for f in os.listdir(os.path.join(ROOT, 'wiki')):
        if f.endswith('.md'):
            shutil.copyfile(os.path.join(ROOT, 'wiki', f), os.path.join(tmp, f))
    run(['git', 'add', '-A'], cwd=tmp)
    if not run(['git', 'status', '--porcelain'], cwd=tmp).strip():
        print('Wiki ist schon aktuell.')
        sys.exit(0)
    # anonyme GitHub-Adresse statt privater E-Mail
    run(['git', '-c', 'user.name=cyclemat', '-c', 'user.email=6072436+cyclemat@users.noreply.github.com',
         'commit', '-q', '-m', 'Wiki aus der README aktualisiert'], cwd=tmp)
    run(['git', 'push', '-q'], cwd=tmp)
    print('Wiki veroeffentlicht: https://github.com/%s/wiki' % SLUG)
finally:
    shutil.rmtree(tmp, ignore_errors=True)
