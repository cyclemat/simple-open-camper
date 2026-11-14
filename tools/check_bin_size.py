# Nach dem Bauen: passt die fertige firmware.bin in den App-Bereich der Partitionstabelle?
# Die Anzeige "Flash: xx %" von PlatformIO zaehlt nicht alles mit – eine zu grosse Datei startet dann gar nicht
# ("Image length … doesn't fit in partition length …", Bildschirm bleibt schwarz).
Import("env")  # noqa: F821
import csv
import os


def _app_size(env):
    part = env.subst("$PARTITIONS_TABLE_CSV")
    if not part or not os.path.isfile(part):
        return None
    slot = env.GetProjectOption("custom_app_slot", "ota_0")   # Updater: ota_1
    size = None
    with open(part, encoding="utf-8") as f:
        for row in csv.reader(line for line in f if line.strip() and not line.lstrip().startswith("#")):
            row = [c.strip() for c in row]
            if len(row) >= 5 and row[1] == "app" and row[2] in (slot, "factory"):
                v = int(row[4], 0) if row[4][:2].lower() == "0x" else int(row[4].rstrip("Kk")) * (1024 if row[4][-1:] in "Kk" else 1)
                size = v if size is None else min(size, v)
    return size


def _check(source, target, env):
    binf = str(target[0])
    if not os.path.isfile(binf):
        return
    n = os.path.getsize(binf)
    lim = _app_size(env)
    if lim is None:
        return
    print("Firmware-Datei %d Byte, App-Bereich %d Byte (%.1f %%)" % (n, lim, 100.0 * n / lim))
    if n > lim:
        raise SystemExit("FEHLER: firmware.bin ist %d Byte zu gross fuer den App-Bereich – sie wuerde nicht starten." % (n - lim))


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _check)  # noqa: F821
