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
    "src/sat/sat_display.cpp", "src/sat/sat_display.h",
    "src/cerbo.cpp", "src/cerbo.h", "src/devsetup.cpp", "src/devsetup.h", "src/gx_find.cpp", "src/gx_find.h", "src/bms.cpp", "src/bms.h",
    "src/truma.cpp", "src/truma.h", "src/mopeka.cpp", "src/mopeka.h", "src/truma_inetx.cpp", "src/truma_inetx.h",
    "src/hymer.cpp", "src/hymer.h", "src/hymer_tls.cpp", "src/hymer_tls.h", "src/settings.h", "src/sat_proto.h", "src/app.h", "src/web_server.h", "src/hymer_cloud.h", "src/truma_heat.h", "src/victron.cpp", "src/victron.h", "src/net.h",
    "src/i18n/i18n.cpp", "src/i18n/i18n.h",
    "src/fonts/fonts.h", "src/fonts/icons.h", "src/fonts/font_ui_16.c", "src/fonts/font_ui_20.c",
    "src/fonts/font_icons_24.c",
    "src/ui/theme.h", "src/lv_psram_alloc.c",
    "include/lv_conf.h", "include/lv_psram_alloc.h",
    "assets/splash_cyd.jpg", "assets/banner_small.jpg", "assets/van-left.svg", "assets/van-right.svg",
    "tools/embed_splash_cyd.py", "tools/upload_wait.py",
    "LICENSE",
]

PLATFORMIO_INI = """\
; Simply Open Camper – Victron-Display (Beta)
; Eigene, schlanke Firmware fuer das Mini-Display ESP32-2432S028R ("Cheap Yellow Display", 2,8" 320x240):
; Cerbo GX per WLAN (MQTT im LAN) und Victron-Geraete direkt per Bluetooth (Instant Readout), alles als Kacheln.
;
; Bauen und hochladen:   pio run -t upload            (VS Code: PlatformIO -> cyd-victron -> Upload)
; Einrichten:            WLAN "SOC-Victron-XXXXXX", Standardpasswort panelsat (muss beim ersten Mal geaendert werden)
;
; Erzeugt aus github.com/cyclemat/simply-open-camper (tools/export_victron.py) – Aenderungen bitte dort machen.

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
bluetti = 0
allpowers = 0
anker = 0
ecoflow = 0
ecoworthy = 1
ant = 1
vatrer = 1

[env:cyd-victron]
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
    post:tools/upload_wait.py
build_flags =
    -DCORE_DEBUG_LEVEL=2
    -DPANEL_SATELLITE=1
    -DPANEL_LANG=de
    -DWITH_CERBO=1
    -DWITH_HYMER=0
    -DWITH_TRUMA_INETX=0
    -DSAT_DISPLAY=1
    -DSAT_BOARD_CYD=1
    -DSAT_VICTRON_ONLY=1
    -DWITH_BMS=0
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
    -DSAT_SPLASH=1
    -DLV_CONF_INCLUDE_SIMPLE
    -DLV_LVGL_H_INCLUDE_SIMPLE
    -Iinclude
lib_deps =
    bblanchon/ArduinoJson@7.4.2
    h2zero/NimBLE-Arduino@^2.3.0
    lvgl/lvgl@8.4.0
    lovyan03/LovyanGFX@^1.1.16
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
        run: pio run -e cyd-victron
      - name: Firmware ablegen
        uses: actions/upload-artifact@v4
        with:
          name: victron-display-firmware
          path: |
            .pio/build/cyd-victron/firmware.bin
            .pio/build/cyd-victron/firmware.factory.bin
"""

# README: tools/victron_readme.md (mit Banner assets/banner_small.jpg)


def write(rel, text):
    p = os.path.join(DST, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main():
    os.makedirs(DST, exist_ok=True)
    for rel in FILES:
        src = os.path.join(ROOT, rel)
        dst = os.path.join(DST, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)
    write("platformio.ini", PLATFORMIO_INI)
    write(".gitignore", GITIGNORE)
    write(".github/workflows/build.yml", WORKFLOW)
    shutil.copyfile(os.path.join(ROOT, "tools", "victron_readme.md"), os.path.join(DST, "README.md"))
    shutil.copyfile(os.path.join(ROOT, "satellite", "partitions_upd.csv"), os.path.join(DST, "partitions_upd.csv"))
    print("Exportiert nach %s (%d Dateien)" % (DST, len(FILES) + 4))


if __name__ == "__main__":
    main()
