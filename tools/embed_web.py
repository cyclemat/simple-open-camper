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
