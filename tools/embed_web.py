# PlatformIO-Pre-Script: packt web/editor.html gzip-komprimiert als C-Array in
# src/generated/editor_html_gz.h, damit die Firmware den Editor selbst ausliefert.
import gzip, os
try:
    Import("env")  # noqa: F821  (nur innerhalb von PlatformIO vorhanden)
    # Wurzel = Ordner ueber src/ – stimmt auch fuer Unterprojekte mit src_dir = ../src (display-testing/)
    root = os.path.dirname(env.subst("$PROJECT_SRC_DIR"))  # noqa: F821
except NameError:
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

src = os.path.join(root, "web", "editor.html")
dst = os.path.join(root, "src", "generated", "editor_html_gz.h")
with open(src, "rb") as f:
    raw = f.read()

# Victron-Edition (-DSOC_VICTRON_EDITION=1): Editor ohne alles, was erst mit dem grossen Panel kommt.
# /*@@FULL{*/ … /*@@FULL}*/ faellt weg, /*@@ED{ … @@ED}*/ wird zu Code, das Woerterbuch (I18N) wird leer
# (die Edition ist deutsch, das Woerterbuch nennt alle Funktionen). Siehe edition_strip() unten.
import re


def edition_strip(text):
    text = re.sub(r"/\*@@FULL\{\*/.*?/\*@@FULL\}\*/", "", text, flags=re.S)
    text = re.sub(r"<!--@@FULL\{-->.*?<!--@@FULL\}-->", "", text, flags=re.S)
    text = re.sub(r"/\*@@ED\{(.*?)@@ED\}\*/", lambda m: m.group(1), text, flags=re.S)
    text = re.sub(r"/\*@@I18N@@\*/.*?/\*@@I18N-END@@\*/", "const I18N = {};", text, flags=re.S)
    text = re.sub(r"(?m)^[ \t]*//[^\n]*\n", "", text)   # reine Kommentarzeilen (verraten sonst zu viel)
    return text


try:
    _edefs = str(env.get("CPPDEFINES", "")) + str(env.GetProjectOption("build_flags", ""))  # noqa: F821
except NameError:
    _edefs = ""
if "SOC_VICTRON_EDITION=1" in _edefs:
    raw = edition_strip(raw.decode("utf-8")).encode("utf-8")
    print("Editor: Victron-Edition (bereinigt)")
data = gzip.compress(raw, compresslevel=9, mtime=0)
os.makedirs(os.path.dirname(dst), exist_ok=True)
rows = [", ".join("0x%02x" % b for b in data[i:i + 20]) for i in range(0, len(data), 20)]
body = ",\n    ".join(rows)
out = (
    "// Automatisch erzeugt von tools/embed_web.py aus web/editor.html – nicht bearbeiten.\n"
    "#pragma once\n#include <stdint.h>\n#include <stddef.h>\n\n"
    "static const uint8_t EDITOR_HTML_GZ[] = {\n    " + body + "\n};\n"
    "static const size_t EDITOR_HTML_GZ_LEN = sizeof(EDITOR_HTML_GZ);\n"
)
old = open(dst, encoding="utf-8", errors="replace").read() if os.path.exists(dst) else ""
if old != out:
    with open(dst, "w", encoding="utf-8") as f:
        f.write(out)
print("Editor eingebettet: %d Bytes (gzip: %d)" % (len(raw), len(data)))

# ------------------------------------------------------------------ Startbild
# Beim Start 3 Sekunden gezeigt (src/splash.cpp). Eigene splash.jpg im Projektordner hat Vorrang, sonst das
# Standardbild assets/splash.jpg. Ohne beide bleibt nur ein leerer Schalter (SPLASH_JPG_LEN 0) – kein Platzbedarf.
splash = os.path.join(root, "splash.jpg")
if not os.path.exists(splash):
    splash = os.path.join(root, "assets", "splash.jpg")
# Rundes Display (CrowPanel 1.46"): eigenes 360x360-Bild splash_round.jpg (Projektordner bzw. assets/)
try:
    _defs = str(env.get("CPPDEFINES", "")) + str(env.GetProjectOption("build_flags", ""))  # noqa: F821
except NameError:
    _defs = ""
if "BOARD_CROWPANEL146" in _defs:
    for _c in (os.path.join(root, "splash_round.jpg"), os.path.join(root, "assets", "splash_round.jpg")):
        if os.path.exists(_c):
            splash = _c
            break
sdst = os.path.join(root, "src", "generated", "splash_jpg.h")
head = "// Automatisch erzeugt von tools/embed_web.py aus splash.jpg bzw. assets/splash.jpg - nicht bearbeiten.\n#pragma once\n#include <stdint.h>\n#include <stddef.h>\n\n"
if os.path.exists(splash):
    with open(splash, "rb") as f:
        jpg = f.read()
    if jpg[:2] != b"\xff\xd8":
        print("WARNUNG: splash.jpg ist kein JPEG – Startbild weggelassen")
        jpg = b""
    elif b"\xff\xc2" in jpg:
        print("WARNUNG: splash.jpg ist 'progressive' gespeichert – das kann der Decoder nicht. Bitte als 'baseline' speichern.")
if os.path.exists(splash) and jpg:
    rows = [", ".join("0x%02x" % b for b in jpg[i:i + 20]) for i in range(0, len(jpg), 20)]
    sout = (head + "#define SPLASH_JPG_LEN %d\n" % len(jpg) +
            "static const uint8_t SPLASH_JPG[] = {\n    " + ",\n    ".join(rows) + "\n};\n")
    print("Startbild eingebettet: %s, %d Bytes" % (os.path.relpath(splash, root), len(jpg)))
else:
    sout = head + "#define SPLASH_JPG_LEN 0\n"
sold = open(sdst, encoding="utf-8", errors="replace").read() if os.path.exists(sdst) else ""
if sold != sout:
    with open(sdst, "w", encoding="utf-8") as f:
        f.write(sout)

# ------------------------------------------------------------------ Blockly (Regel-Editor)
# web/blockly/ (Apache-2.0) -> src/generated/blockly_gz.h. Eingebunden nur mit WITH_RULES (webserver.cpp),
# sonst kostet es keinen Flash.
bdir = os.path.join(root, "web", "blockly")
bdst = os.path.join(root, "src", "generated", "blockly_gz.h")
if os.path.exists(os.path.join(bdir, "blockly_compressed.js")):
    def _gz_arr(name, data):
        z = gzip.compress(data, compresslevel=9, mtime=0)
        rows = [", ".join("0x%02x" % b for b in z[i:i + 24]) for i in range(0, len(z), 24)]
        return ("static const uint8_t %s[] = {\n    " % name + ",\n    ".join(rows) + "\n};\n"
                "static const size_t %s_LEN = sizeof(%s);\n" % (name, name)), len(z)
    def _rd(n):
        with open(os.path.join(bdir, n), "rb") as f:
            return f.read()
    parts, total = [], 0
    for name, data in (("BLOCKLY_JS_GZ", _rd("blockly_compressed.js") + b"\n" + _rd("blocks_compressed.js")),
                       ("BLOCKLY_DE_GZ", _rd("de.js")), ("BLOCKLY_EN_GZ", _rd("en.js"))):
        a, n = _gz_arr(name, data)
        parts.append(a)
        total += n
    bout = ("// Automatisch erzeugt von tools/embed_web.py aus web/blockly/ (Blockly, Apache-2.0) - nicht bearbeiten.\n"
            "#pragma once\n#include <stdint.h>\n#include <stddef.h>\n\n" + "\n".join(parts))
    bold = open(bdst, encoding="utf-8", errors="replace").read() if os.path.exists(bdst) else ""
    if bold != bout:
        with open(bdst, "w", encoding="utf-8") as f:
            f.write(bout)
    print("Blockly eingebettet: %d Bytes gzip" % total)
