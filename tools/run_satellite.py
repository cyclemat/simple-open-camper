# Umgebung "satellite" im Panel-Projekt: baut nicht selbst, sondern reicht den Auftrag an das eigene
# Satelliten-Projekt in satellite/ weiter (eigener Paketordner). So steht der Satellit im PlatformIO-Menue,
# ohne dass Panel und Satellit sich gegenseitig das Arduino-Framework neu installieren.
Import("env")  # noqa: F821 – stellt PlatformIO bereit
import os
import subprocess
import sys

from SCons.Script import COMMAND_LINE_TARGETS  # noqa: E402

targets = [str(t) for t in COMMAND_LINE_TARGETS]

# Abfragen der IDE (IntelliSense) nicht weiterreichen – der Satellit hat dafuer seinen eigenen Ordner
if any(t.startswith("__") for t in targets):
    env.Exit(0)  # noqa: F821

proj = os.path.join(env.subst("$PROJECT_DIR"), "satellite")  # noqa: F821
sat_env = env.GetProjectOption("custom_sat_env", "esp32")  # noqa: F821
cmd = [sys.executable, "-m", "platformio", "run", "-d", proj, "-e", sat_env]
for t in targets:
    if t not in ("buildprog",):
        cmd += ["-t", t]
port = env.subst("$UPLOAD_PORT")  # noqa: F821
if port:
    cmd += ["--upload-port", port]

print("Satellit: " + " ".join(cmd[2:]))
sys.stdout.flush()
env.Exit(subprocess.call(cmd))  # noqa: F821
