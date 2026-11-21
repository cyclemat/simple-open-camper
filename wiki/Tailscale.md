# Tailscale – Fernzugriff

### 🔐 Tailscale – Fernzugriff von unterwegs (P4 und rundes Panel) *(Beta)*

- Bringt das Panel per [microlink](https://github.com/CamM2325/microlink) (Tailscale-kompatibler Client, MIT) in dein
  **Tailnet**. Editor und Weboberfläche sind dann von überall erreichbar – unter der 100.x-Adresse oder dem Namen
  (z. B. `http://simply-open-camper/`, mit MagicDNS), auf allen Geräten mit Tailscale (Handy, Laptop …). Das
  Zugangspasswort gilt weiter. Kein offener Port am Router, keine Cloud dazwischen.
- **Einrichten (ab Werk aus):**
  1. Im Browser-Editor **Einstellungen → Netzwerk** ganz nach unten zu **„Tailscale (Beta)“**.
  2. **„Panel ins Tailnet bringen“** einschalten. Ohne Auth-Key erscheint nach ein paar Sekunden der Knopf
     **„Bei Tailscale anmelden“** – Link öffnen, mit dem Tailscale-Konto anmelden, das Gerät bestätigen. Alternativ
     einen **Auth-Key** aus dem Tailscale-Admin eintragen (Settings → Keys, am besten einmalig und kurz gültig).
  3. Fertig: Stand und Adresse stehen im Editor und am Panel unter **Einstellungen → Netzwerk**.
- Der Auth-Key und die Schlüssel des Panels bleiben **nur im Panel** (nicht in der Firmware, nicht im Repo).
  **„Abmelden“** löscht sie – danach das alte Gerät im Tailscale-Admin entfernen. Gerätename im Tailnet: Feld
  „Gerätename im Tailnet“ (sonst der Netzwerkname des Panels).
- **Getestet (P4):** im Heim-WLAN direkt verbunden (Editor in 0,3 s), über Handy-Hotspot/Mobilfunk über das
  Tailscale-Relais in Frankfurt (Ping 150–200 ms, Editor in 4–5 s, auch mehrere Abrufe gleichzeitig). Rundes Panel:
  gebaut, noch ungetestet.
- Kostet etwa 105 KB Flash und gut 40 KB internen RAM – nur, wenn eingeschaltet. Nicht beim Waveshare (zu wenig
  interner RAM) und nicht bei den kleinen Displays/Satelliten (kein PSRAM). Headscale kann microlink, im Panel aber
  noch nicht einstellbar.
- Tipp: Andere Geräte im Camper (Satelliten, GX, Zusatzdisplays) erreichst du von unterwegs am einfachsten über einen
  Router mit Tailscale als „Subnet Router“ (z. B. GL.iNet).

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
