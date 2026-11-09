# PlatformIO-Pre-Script: packt web/editor.html gzip-komprimiert als C-Array in
# src/generated/editor_html_gz.h, damit die Firmware den Editor selbst ausliefert.
import gzip, os
try:
    Import("env")  # noqa: F821  (nur innerhalb von PlatformIO vorhanden)
    root = env.subst("$PROJECT_DIR")  # noqa: F821
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
old = open(dst).read() if os.path.exists(dst) else ""
if old != out:
    with open(dst, "w") as f:
        f.write(out)
print("Editor eingebettet: %d Bytes (gzip: %d)" % (len(raw), len(data)))
