# Updater-Firmware per USB an die Stelle von app1 (ota_1) schreiben statt an app0 (dort liegt die volle Firmware).
Import("env")  # noqa: F821
import os


def _ota1_offset(csv):
    with open(csv, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            t = [x.strip() for x in line.split(",")]
            if len(t) >= 5 and t[1] == "app" and t[2] == "ota_1":
                return int(t[3], 0)
    return None


csv = env.subst("$PARTITIONS_TABLE_CSV")  # noqa: F821
off = _ota1_offset(csv) if csv and os.path.isfile(csv) else None
if off is None:
    raise SystemExit("Updater: Partitionstabelle ohne ota_1 (%s)" % csv)
# Die Plattform setzt ESP32_APP_OFFSET beim Hochladen erneut (auf ota_0) – deshalb die Adresse fest in den Befehl
env.Replace(UPLOADCMD="$UPLOADER $UPLOADERFLAGS %s $SOURCE" % hex(off))  # noqa: F821
print("Updater wird nach 0x%X geschrieben (app1)" % off)
