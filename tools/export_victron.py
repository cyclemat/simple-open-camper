# Erzeugt das eigenstaendige Repository "simply-open-camper-victron-beta" (Mini-Display nur Victron) aus diesem Repo.
# Quelle bleibt dieses Repo (Umgebung esp32-cyd-victron in satellite/); das Ziel ist ein schlankes PlatformIO-Projekt
# nur mit den Dateien, die diese Firmware braucht.
#
#   python tools/export_victron.py [Zielordner]      (Standard: ../simply-open-camper-victron-beta)
#
# Kopiert die Quellen, schreibt platformio.ini, README, .gitignore und den GitHub-Build. Dateien, die es im Ziel
# nicht mehr gibt, werden NICHT geloescht (bei Bedarf von Hand). Git-Befehle fuehrt das Skript nicht aus.
import os
import shutil
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
DST = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "..", "simply-open-camper-victron-beta"))

FILES = [
    "src/vicdisp/vd_main.cpp", "src/vicdisp/vd_main.h", "src/vicdisp/vd_ble.cpp", "src/vicdisp/vd_page.h",
    "src/sat/sat_display.cpp", "src/sat/sat_display.h", "src/sat/rd_client.h",
    "src/cerbo.cpp", "src/cerbo.h", "src/devsetup.cpp", "src/devsetup.h", "src/gx_find.cpp", "src/gx_find.h", "src/bms.cpp", "src/bms.h",
    "src/truma.cpp", "src/truma.h", "src/mopeka.cpp", "src/mopeka.h", "src/truma_inetx.cpp", "src/truma_inetx.h",
    "src/hymer.cpp", "src/hymer.h", "src/hobby.h", "src/hymer_tls.cpp", "src/hymer_tls.h", "src/settings.h", "src/sat_proto.h", "src/app.h", "src/web_server.h", "src/hymer_cloud.h", "src/truma_heat.h", "src/victron.cpp", "src/victron.h", "src/net.h",
    "src/i18n/i18n.cpp", "src/i18n/i18n.h", "src/i18n/en.h",
    "src/fonts/fonts.h", "src/fonts/icons.h", "src/fonts/font_ui_16.c", "src/fonts/font_ui_20.c",
    "src/fonts/font_ui_26.c", "src/fonts/font_ui_40.c",
    "src/fonts/font_icons_24.c",
    "src/ui/theme.h", "src/lv_psram_alloc.c",
    # Power Stations (bms_ps.inc) mit EcoFlow-Verschluesselung (micro-ecc), Version, Updater, Satelliten-Kopfdateien
    "src/bms_ps.inc", "src/soc_version.h", "src/updater_link.cpp", "src/updater_link.h", "src/panelsat_pair.h",
    "src/sat/sat_config.h", "src/sat/sat_io.h",
    "src/third_party/uecc_ef.c", "src/third_party/uecc_ef.h", "src/third_party/ecoflow/ef_keydata.h",
    "src/third_party/micro-ecc/LICENSE.txt", "src/third_party/README.md", "src/third_party/ecoflow/LICENSE-Apache-2.0.txt", "src/third_party/micro-ecc/curve-specific.inc",
    "src/third_party/micro-ecc/platform-specific.inc", "src/third_party/micro-ecc/types.h",
    "src/third_party/micro-ecc/uECC.c.inc", "src/third_party/micro-ecc/uECC.h", "src/third_party/micro-ecc/uECC_vli.h",
    "include/lv_conf.h", "include/lv_psram_alloc.h",
    "assets/splash_cyd.jpg", "assets/banner_small.jpg", "assets/victron_preview.png", "assets/van-left.svg", "assets/van-right.svg",
    "tools/embed_splash_cyd.py", "tools/upload_wait.py", "tools/i18n_page.py", "tools/i18n/vicdisp_en.json",
    "LICENSE", "CONTRIBUTING.md",
]

PLATFORMIO_INI = """\
; Simple Open Camper – Victron-Display (Beta)
; Eigene, schlanke Firmware fuer das Mini-Display ESP32-2432S028R ("Cheap Yellow Display", 2,8" 320x240):
; Cerbo GX per WLAN (MQTT im LAN) und Victron-Geraete direkt per Bluetooth (Instant Readout), alles als Kacheln.
;
; Bauen und hochladen:   pio run -t upload            (VS Code: PlatformIO -> cyd-victron -> Upload)
; Englisch / English:    pio run -e cyd-victron-en -t upload
; Einrichten:            WLAN "SOC-Victron-XXXXXX", Standardpasswort panelsat (muss beim ersten Mal geaendert werden)
;
; Wird automatisch erzeugt – Aenderungen hier werden beim naechsten Abgleich ueberschrieben.

[platformio]
default_envs = cyd-victron
; eigener Paketordner: Projekte, die das Arduino-Framework per custom_sdkconfig umbauen (z. B. das S.O.C.-Panel),
; teilen sich sonst den Paketordner und loesen gegenseitig eine Neuinstallation aus
packages_dir = .pio/packages

; Akkus mit BMS: welche Typen mitkompiliert werden (1 = an, 0 = aus) – nicht gebrauchte auf 0 spart Flash
[bms]
superb = 1
power_quarry = 1
daly = 1
jbd = 1
jk = 1
litime = 1
renogy = 1
; Renogy-Geraete ausser Akkus: Laderegler, DC-DC-Lader, Wechselrichter, Smart Shunt
renogy_dev = 1
; Power Stations (ungetestet) – im Mini-Display ab Werk aus (Flash)
bluetti = 1
allpowers = 1
anker = 1
ecoflow = 1
ecoworthy = 1
ant = 1
vatrer = 1
; SuperSense (comworks) Tankmodul "SSBT…" – Beta, ungetestet
supersense = 1

[env:cyd-victron]
; Sprache (de | en) – Englisch: Umgebung cyd-victron-en
custom_lang = de
custom_lang_flags = -DVD_PAGE_EN=0
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
framework = arduino
board = esp32dev
; 1984 KB pro App (ohne SPIFFS) mit Platz fuer Updates ueber die Weboberflaeche
board_build.partitions = partitions_upd.csv
board_upload.flash_size = 4MB
monitor_speed = 115200
monitor_filters = esp32_exception_decoder
upload_speed = 921600
; Startbild: assets/splash_cyd.jpg (320x240, JPEG baseline). Eigenes Bild als splash_cyd.jpg ins Projekt legen.
extra_scripts = pre:tools/embed_splash_cyd.py
    pre:tools/i18n_page.py
    post:tools/upload_wait.py
build_flags =
    -DCORE_DEBUG_LEVEL=2
    -DPANEL_SATELLITE=1
    -DPANEL_LANG=${this.custom_lang}
    ${this.custom_lang_flags}
    -DWITH_CERBO=1
    -DWITH_HYMER=0
    -DWITH_TRUMA_INETX=0
    -DSAT_DISPLAY=1
    -DSAT_BOARD_CYD=1
    -DSAT_VICTRON_ONLY=1
    -DSAT_BIG_FONT=1
    -DWITH_BMS=1
    -DBMS_SUPERB_ON=${bms.superb}
    -DBMS_PQ_ON=${bms.power_quarry}
    -DBMS_DALY_ON=${bms.daly}
    -DBMS_JBD_ON=${bms.jbd}
    -DBMS_JK_ON=${bms.jk}
    -DBMS_LITIME_ON=${bms.litime}
    -DBMS_RENOGY_ON=${bms.renogy}
    -DBMS_RENOGY_DEV_ON=${bms.renogy_dev}
    -DBMS_BLUETTI_ON=${bms.bluetti}
    -DBMS_ALLPOWERS_ON=${bms.allpowers}
    -DBMS_ANKER_ON=${bms.anker}
    -DBMS_ECOFLOW_ON=${bms.ecoflow}
    -DBMS_ECOWORTHY_ON=${bms.ecoworthy}
    -DBMS_ANT_ON=${bms.ant}
    -DBMS_VATRER_ON=${bms.vatrer}
    -DBMS_SUPERSENSE_ON=${bms.supersense}
    -DSAT_SPLASH=1
    -DLV_CONF_INCLUDE_SIMPLE
    -DLV_LVGL_H_INCLUDE_SIMPLE
    -Iinclude
lib_deps =
    bblanchon/ArduinoJson@7.4.2
    h2zero/NimBLE-Arduino@2.5.2
    lvgl/lvgl@8.4.0
    lovyan03/LovyanGFX@^1.1.16

; English version: display texts from src/i18n/en.h, web interface generated from tools/i18n/vicdisp_en.json
[env:cyd-victron-en]
extends = env:cyd-victron
custom_lang = en
custom_lang_flags = -DVD_PAGE_EN=1 -DUI_DECIMAL_COMMA=0
"""

GITIGNORE = """\
.pio/
.vscode/
src/generated/
*.log
__pycache__/
"""

WORKFLOW = """\
name: Build

on:
  push:
    branches: [main]
    paths-ignore: ['**.md']
  workflow_dispatch:

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.12'
      - name: PlatformIO installieren
        run: pip install "platformio==6.2.0"
      - name: Cache
        uses: actions/cache@v4
        with:
          path: |
            ~/.platformio
            .pio
          key: pio-${{ runner.os }}-${{ hashFiles('platformio.ini') }}
      - name: Bauen
        run: pio run -e cyd-victron -e cyd-victron-en
      - name: Firmware ablegen
        uses: actions/upload-artifact@v4
        with:
          name: victron-display-firmware
          path: |
            .pio/build/cyd-victron/firmware.bin
            .pio/build/cyd-victron/firmware.factory.bin
            .pio/build/cyd-victron-en/firmware.bin
            .pio/build/cyd-victron-en/firmware.factory.bin
"""

# README: tools/victron_readme.md (mit Banner assets/banner_small.jpg)


# Teile des Hauptprojekts, die im Victron-Display nichts zu suchen haben (Zusatzsensoren der Satelliten).
# Jede Ersetzung MUSS passen – sonst bricht der Export ab, damit nichts unbemerkt durchrutscht.
STRIP = {
    "src/sat_proto.h": [
        ("  SC_POWER = 10, SC_CO2 = 11, SC_WEIGHT = 12, SC_FLOW = 13, SC_VOLUME = 14,   // Zusatzsensoren (sat_sens.cpp)\n"
         "  SC_COUNT = 15\n",
         "  SC_COUNT = 10\n"),
    ],
    "src/sat/sat_io.h": [
        ("// gemeinsamer I2C-Bus (MPU-6050, OLED, Zusatzsensoren)", "// gemeinsamer I2C-Bus (MPU-6050, OLED)"),
        ("bool sat_io_mpu_pins(int &sda, int &scl);  // MPU-6050 eingeschaltet? Dann seine I2C-Pins\n", ""),
        ("String sat_io_pin_owner(int pin);          // wer belegt diesen GPIO in der GPIO-Karte (\"\" = frei)\n", ""),
    ],
}


# Alpha-Funktionen (Wunsch des Nutzers: im Hauptprojekt vorhanden, aber im oeffentlichen Repo nicht erkennbar).
# Reifendruck (src/tpms.*): Bloecke "#if WITH_TPMS … #endif", Einbindung, Bedingungen, Weboberflaeche, Woerterbuecher.
import re as _re
ALPHA_WORDS = _re.compile(r"tpms|TPMS|Reifen|Rad drehen|vorne links|vorne rechts|hinten links|hinten rechts")


def strip_alpha(rel, text):
    if rel.endswith((".cpp", ".h", ".c", ".inc")):
        text = _re.sub(r"(?ms)^[ \t]*#if WITH_TPMS\b.*?^[ \t]*#endif[^\n]*\n", "", text)
        text = _re.sub(r"(?m)^[ \t]*#include \"(\.\./)?tpms\.h\"[^\n]*\n", "", text)
        text = _re.sub(r"\(ble_proto_on\(BLE_P_TPMS\) && tpms_\w+\(\)\)\s*\|\|\s*", "", text)
        text = _re.sub(r"\s*\|\|\s*\(ble_proto_on\(BLE_P_TPMS\) && tpms_\w+\(\)\)", "", text)
        text = _re.sub(r"\s*\|\|\s*\(WITH_TPMS && p == BLE_P_TPMS\)", "", text)
        text = _re.sub(r"(?m)^[^\n]*(tpms_begin\(\)|BLE_P_TPMS = |Reifen)[^\n]*\n", "", text)
        text = text.replace("; mit WITH_TPMS auch Reifendruck (tpms.cpp)", "")
        text = text.replace("// SuperSense (Beta): Tankmodul, Reifendruck", "// SuperSense (Beta): Tankmodul")
        text = _re.sub(r'(?m)^\{"(vorne links|vorne rechts|hinten links|hinten rechts|Kein Empfang – Sensor schlaeft)[^\n]*\n', "", text)
    if rel == "src/vicdisp/vd_page.h":
        text = _re.sub(r'(?s)<div class="card" id="tpCard".*?<div id="tpFound"></div></div>\n\n', "", text)
        text = _re.sub(r"(?s) const P=S\.tpms;.*?\n(?= const B=S\.bms)", "", text)
        text = text.replace(",#mpList,#tpList')", ",#mpList')")
        text = _re.sub(r"(?m)^[^\n]*/api/tpms[^\n]*\n", "", text)
    if rel == "tools/i18n/vicdisp_en.json":
        import json as _json
        pairs = [x for x in _json.loads(text) if not ALPHA_WORDS.search(x[0]) and "{id:" not in x[0] and "Rad:" not in x[0]
                 and "'Batterie '+num(x.mv" not in x[0]]
        text = "[\n" + ",\n".join(_json.dumps(x, ensure_ascii=False) for x in pairs) + "\n]\n"
    return text


def strip(rel, path):
    rules = STRIP.get(rel, [])
    if not rules and not rel.endswith((".cpp", ".h", ".c", ".inc", ".json")):
        return   # Bilder usw. unveraendert
    with open(path, encoding="utf-8", newline="") as f:
        text = f.read()
    crlf = "\r\n" in text
    text = text.replace("\r\n", "\n")
    text = strip_alpha(rel, text)
    for old, new in rules:
        if old not in text:
            raise SystemExit("export_victron: Filter passt nicht mehr in %s:\n  %r" % (rel, old))
        text = text.replace(old, new)
    with open(path, "w", encoding="utf-8", newline="\r\n" if crlf else "\n") as f:
        f.write(text)


def write(rel, text):
    p = os.path.join(DST, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


# frueher exportiert, jetzt nicht mehr (Alpha) – im Ziel-Repo entfernen
REMOVED = ["src/tpms.h", "src/tpms.cpp"]


def main():
    os.makedirs(DST, exist_ok=True)
    for rel in REMOVED:
        if os.path.exists(os.path.join(DST, rel)):
            os.remove(os.path.join(DST, rel))
    for rel in FILES:
        src = os.path.join(ROOT, rel)
        dst = os.path.join(DST, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)
        strip(rel, dst)
    write("platformio.ini", PLATFORMIO_INI)
    write(".gitignore", GITIGNORE)
    write(".github/workflows/build.yml", WORKFLOW)
    shutil.copyfile(os.path.join(ROOT, "tools", "victron_readme.md"), os.path.join(DST, "README.md"))
    shutil.copyfile(os.path.join(ROOT, "tools", "victron_changelog.md"), os.path.join(DST, "CHANGELOG.md"))
    shutil.copyfile(os.path.join(ROOT, "satellite", "partitions_upd.csv"), os.path.join(DST, "partitions_upd.csv"))
    write_flasher()
    print("Exportiert nach %s (%d Dateien)" % (DST, len(FILES) + 9))


def write_flasher():
    """Web-Flasher fuer GitHub Pages (ESP Web Tools): index.html + manifest.json + Hintergrundbild."""
    import json
    import re
    with open(os.path.join(ROOT, "src", "soc_version.h"), encoding="utf-8") as f:
        m = re.search(r'SOC_VERSION\s+"([^"]+)"', f.read())
    ver = m.group(1) if m else "?"
    shutil.copyfile(os.path.join(ROOT, "tools", "victron_flasher.html"), os.path.join(DST, "index.html"))
    os.makedirs(os.path.join(DST, "assets"), exist_ok=True)
    shutil.copyfile(os.path.join(ROOT, "assets", "splash.jpg"), os.path.join(DST, "assets", "flasher_bg.jpg"))
    # je Sprache ein Manifest; die Seite schaltet zwischen beiden um (Knopf Deutsch | English)
    for fname, title, binf in (("manifest.json", "Victron-Display (Simple Open Camper)", "victron-display-cyd.bin"),
                               ("manifest-en.json", "Victron display (Simple Open Camper, English)",
                                "victron-display-cyd-en.bin"),
                               ("manifest-ws7.json", "Victron-Display 7\" (Waveshare ESP32-S3)", "victron-display-ws7.bin")):
        manifest = {
            "name": title,
            "version": ver,
            "new_install_prompt_erase": True,
            "builds": [{"chipFamily": "ESP32-S3" if "ws7" in binf else "ESP32",
                        "parts": [{"path": "release/" + binf, "offset": 0}]}],
        }
        write(fname, json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
    write(".nojekyll", "")


if __name__ == "__main__":
    main()
