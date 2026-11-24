# Legt Versionen von ESP-IDF-Komponenten fest, bevor pioarduino die Arduino-Bibliotheken neu uebersetzt
# ("Hybrid Compile" bei custom_sdkconfig, z. B. Varianten mit 32 Netzwerk-Sockets).
#
# Warum: Beim Neu-Uebersetzen holt der Komponenten-Manager die NEUESTEN passenden Versionen (z. B. esp_hosted 2.13.x),
# die Kopfdateien und der Funkchip passen aber zu den fertigen Bibliotheken (2.12.13) -> am P4 kein WLAN
# ("esp_wifi_init 0x102", 2026-11-24). custom_component_add hilft nicht (ueberspringt vorhandene Eintraege).
#
# Einstellung in der platformio.ini der Umgebung (eine Zeile je Komponente):
#   custom_pin_components =
#       espressif/esp_hosted==2.12.13
#       espressif/esp_wifi_remote==1.6.4
#   extra_scripts = pre:../tools/pin_components.py
#
# Das Skript aendert idf_component.yml (und die Sicherung .orig) im Framework-Paket. Musste es etwas aendern und sind
# die Bibliotheken schon uebersetzt, wird die Markierung "sdkconfig" umbenannt -> der naechste Build uebersetzt neu.
import os
import re

Import("env")  # noqa: F821

pins = []
for line in (env.GetProjectOption("custom_pin_components", "") or "").splitlines():  # noqa: F821
    line = line.strip()
    if line and not line.startswith(";") and "==" in line:
        name, ver = line.split("==", 1)
        pins.append((name.strip(), ver.strip()))

if pins:
    platform = env.PioPlatform()  # noqa: F821
    fw = platform.get_package_dir("framework-arduinoespressif32")
    libs = platform.get_package_dir("framework-arduinoespressif32-libs")
    changed = False
    for fn in ("idf_component.yml", "idf_component.yml.orig"):
        path = os.path.join(fw, fn) if fw else ""
        if not path or not os.path.exists(path):
            continue
        with open(path, encoding="utf-8") as f:
            text = f.read()
        new = text
        for name, ver in pins:
            # Block "  <name>:" bis zum naechsten Eintrag gleicher Tiefe; darin die Zeile "    version: ..."
            m = re.search(r"(?m)^  " + re.escape(name) + r":\n((?:    .*\n|\s*-.*\n)*)", new)
            if not m:
                print("pin_components: %s nicht in %s gefunden" % (name, fn))
                continue
            block = m.group(1)
            cur = re.search(r"(?m)^    version:\s*(.*)$", block)
            if cur and cur.group(1).strip().strip("'\"") == "==" + ver:
                continue   # schon festgelegt
            if cur:
                nb = re.sub(r"(?m)^    version:.*$", "    version: '==%s'" % ver, block, count=1)
            else:
                nb = block + "    version: '==%s'\n" % ver
            new = new[:m.start(1)] + nb + new[m.end(1):]
        if new != text:
            with open(path, "w", encoding="utf-8") as f:
                f.write(new)
            changed = True
            print("pin_components: %s angepasst (%s)" % (fn, ", ".join("%s==%s" % p for p in pins)))
    marker = os.path.join(libs, "sdkconfig") if libs else ""
    if changed and marker and os.path.exists(marker):
        os.replace(marker, marker + ".vorher")
        print("pin_components: Bibliotheken werden mit den festgelegten Versionen neu uebersetzt (~1-2 h)")
