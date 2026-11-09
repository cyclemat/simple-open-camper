# Vor dem Hochladen auf Enter warten – Zeit, das Panel anzustecken, den seriellen Monitor zu schliessen
# oder die Boot-Taste zu halten. Ohne Terminal (z. B. automatisierte Builds) oder mit
# PANEL_UPLOAD_NOWAIT=1 wird ohne Rueckfrage hochgeladen.
Import("env")  # noqa: F821 – stellt PlatformIO bereit
import os
import sys


def wait_enter(source, target, env):
    if os.environ.get("PANEL_UPLOAD_NOWAIT") or not sys.stdin or not sys.stdin.isatty():
        print("Upload ohne Rückfrage (kein Terminal oder PANEL_UPLOAD_NOWAIT gesetzt)")
        return
    port = env.subst("$UPLOAD_PORT") or "automatisch gesuchten Port"
    try:
        input("\n>>> Firmware ist gebaut. Bereit zum Hochladen auf %s – Enter drücken, Strg+C bricht ab ... " % port)
    except (EOFError, KeyboardInterrupt):
        print("\nHochladen abgebrochen.")
        env.Exit(1)


env.AddPreAction("upload", wait_enter)  # noqa: F821
