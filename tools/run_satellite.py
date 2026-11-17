# Umgebung "satellite" im Panel-Projekt: baut nicht selbst, sondern reicht den Auftrag an das eigene
# Satelliten-Projekt in satellite/ weiter (eigener Paketordner). So steht der Satellit im PlatformIO-Menue,
# ohne dass Panel und Satellit sich gegenseitig das Arduino-Framework neu installieren.
Import("env")  # noqa: F821 – stellt PlatformIO bereit
import os
import subprocess
import sys

from SCons.Script import COMMAND_LINE_TARGETS  # noqa: E402

targets = [str(t) for t in COMMAND_LINE_TARGETS]

# custom_sub_dir: Unterprojekt (Standard satellite/, z. B. auch display-testing/), custom_sat_env: Umgebung darin
sub = env.GetProjectOption("custom_sub_dir", "satellite")  # noqa: F821
proj = os.path.join(env.subst("$PROJECT_DIR"), sub)  # noqa: F821
sat_env = env.GetProjectOption("custom_sat_env", "esp32")  # noqa: F821

# Abfrage der IDE (__idedata: IntelliSense und Menuepunkte unter "Custom", z. B. "Nur flashen"): die Angaben des
# Unterprojekts holen und hierher kopieren. Nur wenn das Unterprojekt schon einmal gebaut wurde (sonst wuerde die
# Abfrage dort Pakete installieren) – andere "__"-Abfragen nicht weiterreichen.
if any(t.startswith("__") for t in targets):
    if "__idedata" in targets and os.path.isdir(os.path.join(proj, ".pio", "build", sat_env)):
        import shutil
        rc = subprocess.call([sys.executable, "-m", "platformio", "run", "-d", proj, "-e", sat_env, "-t", "__idedata"],
                             stdout=subprocess.DEVNULL)
        src = os.path.join(proj, ".pio", "build", sat_env, "idedata.json")
        dst = os.path.join(env.subst("$BUILD_DIR"), "idedata.json")  # noqa: F821
        if rc == 0 and os.path.isfile(src):
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(src, dst)
    env.Exit(0)  # noqa: F821
cmd = [sys.executable, "-m", "platformio", "run", "-d", proj, "-e", sat_env]
for t in targets:
    if t not in ("buildprog",):
        cmd += ["-t", t]
port = env.subst("$UPLOAD_PORT")  # noqa: F821
if port:
    cmd += ["--upload-port", port]

print("Unterprojekt %s: %s" % (sub, " ".join(cmd[2:])))
sys.stdout.flush()
env.Exit(subprocess.call(cmd))  # noqa: F821
