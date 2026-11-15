# Neue Version fuer ein Release: letzte Zahl in src/soc_version.h um 1 erhoehen (0.0.1.beta -> 0.0.2.beta).
# Aufruf: python tools/release_version.py      (gibt die neue Version aus)
import os
import re
import sys

p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "soc_version.h")
s = open(p, encoding="utf-8").read()
m = re.search(r'#define SOC_VERSION "([^"]+)"', s)
if not m:
    sys.exit("SOC_VERSION nicht gefunden")
old = m.group(1)
# letzte Zahlengruppe erhoehen, Zusatz (z. B. ".beta") bleibt
m2 = re.match(r"^(.*?)(\d+)(\D*)$", old)
if not m2:
    sys.exit("Version %s hat keine Zahl am Ende" % old)
new = "%s%d%s" % (m2.group(1), int(m2.group(2)) + 1, m2.group(3))
open(p, "w", encoding="utf-8", newline="\n").write(s.replace('"%s"' % old, '"%s"' % new, 1))
print(new)
