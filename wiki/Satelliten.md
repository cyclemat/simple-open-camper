# Satelliten

## 🛰️ Satelliten

Ein Satellit ist ein günstiger **ESP32**, den du irgendwo im Fahrzeug verbaust – am
Gaskasten, unter dem Bett beim Frischwassertank, im Technikfach. Er liest Bluetooth-Geräte und eigene Sensoren und
schickt die Werte ans Panel. Das spart im Panel Arbeitsspeicher und Funkzeit (Bluetooth und WLAN teilen sich dort
eine Antenne) und überbrückt Strecken, die Bluetooth allein nicht schafft.

**Unterstützte Chips** – je Chip eine eigene Umgebung im PlatformIO-Menü:

| Umgebung | Chip / Board | Analog (ADC1) | I²C (Standard) | Hinweise |
|---|---|---|---|---|
| `satellite` | ESP32 (WROOM, esp32dev) | GPIO 32–39 | 21 / 22 | die meisten Pins, GPIO 34–39 nur Eingang |
| `satellite-c3` | ESP32-C3 (Super Mini, DevKitM-1) – [bei Amazon](https://link.amazon/B04vjKxhb) | GPIO 0–4 | 8 / 9 | klein, 13 freie Pins, Log über USB |
| `satellite-c3-oled` | ESP32-C3 mit 0,42"-OLED (ABrobot, 72 × 40) – [bei Amazon](https://link.amazon/B00lVYc7g) | GPIO 0–4 | 5 / 6 (mit dem OLED) | zeigt einen Sensorwert bzw. die Wasserwaage, siehe unten |
| `satellite-s3` | ESP32-S3 (DevKitC-1) | GPIO 1–10 | 8 / 9 | Log über die Buchse „UART“ |
| `satellite-c6` | ESP32-C6 (DevKitC-1, Super Mini) – [bei Amazon](https://link.amazon/B0gJnV7Pv) | GPIO 0–6 | 23 / 22 | Log über USB |

Alle brauchen mindestens 4 MB Flash. Das Web-IF zeigt nur die Pins, die der jeweilige Chip frei hat.

**ESP32-C3 mit OLED** (`satellite-c3-oled`): Das kleine Display zeigt beim Start abwechselnd einen Camper oder einen
Wohnwagen mit „S.O.C.“, danach **automatisch** – mit Lagesensor MPU-6050 eine **Wasserwaage in zwei Richtungen**
(Blase im Kreis, Neigung längs und quer), sonst die angeschlossenen Sensoren nacheinander (z. B. Temperatur), ohne
Sensor Name und Adresse des Satelliten. Fest einstellbar in der Weboberfläche (Karte „OLED“), ebenso Helligkeit und
Drehen. Der MPU-6050 kommt an dieselben Pins wie das OLED (SDA GPIO 5, SCL GPIO 6). Zum Schonen wird das OLED alle
20 Minuten kurz ganz schwarz und ganz weiß.

**16-fach-Relaisplatine** (Umgebung `satellite-relay16`, ESP32-WROOM-32E mit 2× 74HC595) –
[Relaisplatine bei Amazon](https://link.amazon/B08IzxSMo), [USB-TTL-Wandler bei Amazon](https://link.amazon/B02ciSu8U):

- Die Platine hat **keinen USB-Anschluss** – zum Flashen brauchst du einen **USB-TTL-Wandler** mit 3,3-V-Pegel:
  TX des Wandlers an RX der Platine, RX an TX, GND an GND. Zum Flashen **IO0 beim Einschalten auf GND** halten
  (bzw. BOOT drücken), danach loslassen.
- Sie braucht eine **eigene 5-V-Versorgung** – der Wandler reicht dafür nicht (beim Schalten der Relais bricht
  sonst die Spannung ein bzw. die USB-Verbindung ab). GND von Platine und Wandler verbinden.
- Spätere Updates gehen über die Weboberfläche des Satelliten, ohne Wandler.

**Was ein Satellit liest und schaltet**

| Art | Umfang | Hinweise |
|---|---|---|
| ⛽ Truma LevelControl, 🛢️ Mopeka, ☀️ Victron | per Bluetooth | dieselben Module wie im Panel |
| 💧 Tanksonden | bis 4 Tanks, je bis 5 Sonden | Stangen verschiedener Länge, jede mit eigener Prozentstufe |
| 🌡️ DS18B20 | bis 8 Fühler an einem GPIO | 1-Wire, Fühler per Knopf suchen, Korrektur je Fühler |
| 📈 Analog | bis 4 Eingänge (ADC1, siehe oben) | zwei Punkte zum Umrechnen von mV in den Wert |
| 📐 MPU-6050 (GY-521) | Neigung längs und quer | Nullpunkt per Knopf, Beruhigung siehe unten |
| 🔌 Schalter | bis 8 Ausgänge (Relais), 16 mit der Relaisplatine (`satellite-relay16`, 2× 74HC595) | aktiv Low wählbar, Zustand nach dem Start: aus / an / wie zuletzt |
| 🔘 Eingänge | bis 8 Kontakte oder Taster | Pull-up und Umkehren wählbar |

**Zwei Wege zum Panel** – am Satelliten einzeln einschaltbar (ab Werk ist beides aus):

- **UDP** im WLAN direkt ans Panel (Port 47811, Rundruf oder feste Panel-IP) – **ohne Server**. Werte und
  Schaltbefehle sind mit dem gemeinsamen **Satelliten-Passwort** verschlüsselt (AES-CTR). Die Einrichtung per Knopf
  (unten) schaltet UDP automatisch ein.
- **MQTT** über einen Broker, mit **Home-Assistant-Discovery** – die Werte erscheinen dann auch in Home Assistant.
  Topics: `panelsat/<id>/sensor/<punkt>`, schalten mit `panelsat/<id>/switch/<punkt>/set` (`ON` / `OFF` / `TOGGLE`).

Bluetooth dient nur noch zum **Einrichten** – als Datenweg war es zu langsam. Auch die ausgewerteten
Bluetooth-Geräte (Truma, Mopeka, Victron) sind ab Werk aus: am Satelliten nur einschalten, was er lesen soll.

Im Panel muss dafür `satellites = 1` unter `[features]` in der `platformio.ini` stehen (Standard).

### Einrichten

1. Firmware aufspielen: im PlatformIO-Menü die passende Umgebung wählen (`satellite`, `satellite-c3` …) und
   **Upload** klicken – oder `pio run -e satellite-c3 -t upload`. Der Satellit ist intern ein eigenes
   PlatformIO-Projekt im Ordner `satellite/` mit eigenem Paketordner; die Einträge im Panel-Menü reichen nur
   weiter. So installieren sich Panel und Satellit nicht gegenseitig das Framework neu.
2. Mit dem Panel verbinden – am einfachsten **per Knopf** (siehe unten), sonst über das eigene WLAN des Satelliten.
3. Im Web-IF des Satelliten die Bluetooth-Geräte und die GPIOs belegen. Das Web-IF zeigt zu jedem Pin, was er kann,
   warnt bei Startpins, Boot-Taste und LED und bei doppelter Belegung.

#### 🔘 Per Knopf mit dem Panel verbinden (empfohlen)

So kommt ein neuer Satellit ohne Tippen von WLAN-Daten ins Netz – ähnlich wie bei Shelly oder Hue:

1. Am Panel unter **Einstellungen → Geräte → Satelliten** auf **„Neuen Satelliten hinzufügen“** tippen (im Editor:
   Reiter **Satelliten → „Neuen Satelliten suchen“**). Das Panel sucht dann 3 Minuten lang per Bluetooth.
2. Am Satelliten die **BOOT-Taste 3 Sekunden halten** (ESP32/S3: GPIO 0, C3/C6: GPIO 9). Hat er keine Taste:
   **3× schnell hintereinander Strom aus/an**. Er ist dann **3 Minuten** lang per Bluetooth zum Einrichten sichtbar.
3. Er erscheint als **„Satellite-…“** – antippen bzw. **„Einrichten“**.
4. Das Panel verbindet sich per Bluetooth, handelt mit dem Satelliten einen geheimen Schlüssel aus (ECDH P-256) und
   überträgt **verschlüsselt** (AES-256):
   - den **WLAN-Zugang** – passend zur Betriebsart des Panels:

     | Panel-Betriebsart | Der Satellit bekommt |
     |---|---|
     | Camp-WLAN (Panel ist Teilnehmer) | Zugang zu **diesem** Netz |
     | Router (Panel spannt das Netz auf) | Zugang zum **Panel-WLAN** |
     | Kombi | das **Panel-WLAN** (bleibt gleich, auch auf einem anderen Campingplatz) |
     | WLAN aus | **keine Einrichtung** – Satelliten brauchen ein WLAN |

   - das **Satelliten-Passwort** des Panels, damit beide sofort zusammenpassen,
   - den **MQTT-Broker** des Panels (Adresse, Port, Benutzer, Passwort) – nur wenn das Panel einen nutzt. Der
     Satellit sendet dann zusätzlich per MQTT (auch an Home Assistant).
5. Der Satellit verbindet sich, **meldet seine IP-Adresse** zurück (das Panel zeigt sie an), startet neu und spricht
   das Panel ab dann per **UDP** an (mit Broker zusätzlich per MQTT).

Die Daten gehen nur nach dem Tastendruck am Gerät und nur verschlüsselt über die Luft – aus der Ferne lässt sich
einem Satelliten so kein fremdes WLAN unterschieben. Ändert sich am Panel die Betriebsart oder das Netz: am
Satelliten einfach erneut BOOT halten und neu einrichten. Abschaltbar beim Bauen mit `-DSAT_PAIR=0` (spart Flash).

#### Über das eigene WLAN des Satelliten

Ohne WLAN-Zugang öffnet der Satellit das WLAN **„SOC-Satellite-…“** (Passwort `panelsat`). Verbinden – die Seite
öffnet sich meist von selbst, sonst **http://4.3.2.1/**. Beim ersten Öffnen ein eigenes Zugangspasswort vergeben, dann
WLAN, Weg zum Panel (UDP) und das Satelliten-Passwort einstellen (dasselbe wie am Panel). Das eigene WLAN des
Satelliten öffnet sich auch jederzeit mit 3× schnell aus/an.

Am Panel nimmt **Antippen** eines Satelliten in der Liste ihn auch wieder heraus (**Entfernen**), im Editor heißt das
**Vergessen**.

### 💧 Tanksonden

Mehrere Edelstahl-Stangen verschiedener Länge im Tank plus eine gemeinsame Masse-Stange. Steht Wasser an einer Sonde,
leitet es zur Masse; angezeigt wird die höchste Stufe mit Kontakt (z. B. 20 / 40 / 60 / 80 / 100 %).

- **Je Sonde ein GPIO** – am einfachsten. Die Masse-Stange kommt an GND oder an einen eigenen GPIO.
  Für Module mit Transistor gibt es „Kontakt = High“.
- **Widerstandsleiter an einem Analog-Pin** – alle Sonden über Widerstände an einem Pin. Im Web-IF trägst du je Stufe
  die gemessene Spannung ein, der Satellit wählt die nächstliegende.
- **Gegen Korrosion** bekommen die Sonden nur beim Messen kurz Strom (eigenes Intervall, Standard 60 s) und sind
  sonst stromlos. Jede Sonde wird dreimal gelesen, es zählt die Mehrheit.
- Ist eine höhere Sonde nass und eine niedrigere trocken, meldet der Satellit den Wert als **unplausibel** – meist
  eine verschmutzte Stange oder ein Kabelfehler.

### 📐 Neigung beruhigen (MPU-6050)

Im Camper wackelt immer etwas: Jemand läuft herum, der Wind drückt. Damit der Wert nicht springt, glättet der
Satellit in mehreren Stufen – alles im Web-IF einstellbar:

| Stufe | Standard | Wirkung |
|---|---|---|
| Tiefpass im Sensor | 5 Hz | filtert Vibrationen schon im MPU-6050 |
| Median | letzte 5 Werte | einzelne Ausreißer fallen weg |
| Zeitfilter | 2 s | gleitender Mittelwert (0 = aus) |
| Totband | 0,2° | kleinere Änderungen werden nicht gemeldet |
| Nur bei Stillstand | ab 3 °/s | dreht sich der Sensor (Gyro), bleibt der letzte Wert stehen, bis 1 s Ruhe ist |

Den **Nullpunkt** setzt du per Knopf, wenn das Fahrzeug gerade steht. Achsen tauschen und umkehren geht ebenfalls,
falls die Platine anders herum eingebaut ist.

Auf dem Panel wird daraus die **[Wasserwaage](Sensoren)** *(Beta)*:
Kachel mit Zentimetern je Seite/Rad, Einrichten im Editor mit eigenem Nullpunkt und Assistent für die Einbaulage.

---
*Diese Seite wird aus der [README](https://github.com/cyclemat/simple-open-camper) erzeugt (`tools/make_wiki.py`) – Änderungen bitte dort.*
