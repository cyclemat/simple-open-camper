<p align="center">
  <img src="assets/banner.png" alt="Simple Open Camper (S.O.C.) – touch panel for your camper" width="100%">
</p>

<h1 align="center"><img src="assets/van-right.svg" height="36" alt=""> Simple Open Camper <img src="assets/van-left.svg" height="36" alt=""></h1>
<h3 align="center">S.O.C. – touch panel for your camper: Zigbee2MQTT, gas, water, heating and on-board systems – local, no cloud, no Home Assistant</h3>

<p align="center">
  <img src="https://img.shields.io/badge/Hardware-ESP32--P4%20%7C%20ESP32--S3-0089D0" alt="Hardware">
  <img src="https://img.shields.io/badge/Display-1024%C3%97600%20Touch-blue" alt="Display">
  <img src="https://img.shields.io/badge/Framework-Arduino%20%2F%20ESP--IDF%205-orange" alt="Framework">
  <img src="https://img.shields.io/badge/Connection-MQTT%20%7C%20Bluetooth%20LE%20%7C%20LIN-blueviolet" alt="Connection">
  <img src="https://img.shields.io/badge/Satellites-ESP32-0075A9" alt="Satellites">
  <img src="https://img.shields.io/badge/Cloud-none-brightgreen" alt="No cloud">
  <img src="https://img.shields.io/badge/License-PolyForm%20Noncommercial-blue" alt="License PolyForm Noncommercial">
  <img src="https://img.shields.io/badge/Status-Beta%20%2F%20Testing-yellow" alt="Status">
</p>

<p align="center">
  <a href="README.md">🇩🇪 Deutsch</a> · <strong>🇬🇧 English</strong>
</p>

> **Lights, gas, heating and battery on one screen – straight over MQTT, Bluetooth and LIN bus.**

---

## 🚧 Beta – testers and ideas welcome!

The panel runs day to day with Zigbee2MQTT (tested with **131 devices / 73 tiles**). Some modules are built from
open protocol descriptions but have **not been tested on real hardware yet**:
Mopeka with a real sensor, Truma CP Plus, Truma iNet X, Alde, WLED and Cerbo GX, most battery types and power
stations as well as the new **SuperSense**, **Simarine STRx**, **Ruuvi**, **SwitchBot** sensors and **CS-BATTERIES**
batteries.

If you own one of them and want to help: feedback, logs and ideas are very welcome!

---

## 🚀 About

A wall panel with a 7-inch touch screen (1024 × 600) that controls lights and switches from **Zigbee2MQTT directly
over MQTT** – no Home Assistant in between. It also reads gas bottles, tanks, Victron devices and the heating system
of your motorhome. You build the layout in the browser editor served by the panel itself.

> 💡 **How it started:** I run Home Assistant in my own caravan – but just for lights and a few readings, that
> system draws too much power in the long run. I was looking for something more frugal. That is how the panel
> began, as a **light controller for the SMHUB Nano**: a screen on the wall that switches the Zigbee lights directly.
>
> The idea to take it further came from my
> [Truma LevelControl integration for Home Assistant](https://github.com/cyclemat/HAOS-Truma-Levelcontrol-Integration):
> if the panel can read the gas level over Bluetooth by itself, there is no need for Home Assistant for that.
> Then came tanks, Victron, the Truma heater, Alde and satellites – step by step it grew into a control panel
> for the whole camper. Hence the new name: the "Zigbee-Panel" became **Simple Open Camper**.

- 🏠 Everything runs **locally** on the panel – no cloud, no account
- 📶 Own Wi-Fi possible (router mode) – no home network needed, ideal for motorhomes
- 🧩 Every optional module can be built in or left out in `platformio.ini`

> The panel UI is available in German, English, Spanish and French (`lang` in `platformio.ini`).

---

## ✨ Features

**Operation**
- 💡 **Tiles** show the real light colour (colour, white tone, brightness bar). Tap to toggle, long-press for
  brightness, colour temperature, saturation and colour wheel – only where the device supports it.
- 🗂️ **Multiple pages** (swipe or tabs), each with an optional background image (e.g. floor plan).
- ✏️ **Web editor** at `http://<panel>/`: drag & drop devices, sizes, icons, pages, backgrounds, names, settings,
  firmware update over Wi-Fi. Without a panel connection it starts with demo data.
- 🔒 **Access password** for the editor and web interface – you set your own on first opening.
- ⌨️ **Settings on the panel** with an on-screen keyboard (QWERTZ, ä ö ü ß).
- 🌍 **UI in German, English, Spanish or French** – more languages are easy to add.
- 🌙 **Screen saver**: dim, black (easiest on the LCD), dark clock or bouncing logo (your own PNG possible).
- 💾 **Backup to SD card**: layout, names, all settings, sensors, logo and backgrounds.
- 🕐 **Set the clock by hand** when there is no internet; otherwise via internet or browser.
- 🔋 **Battery indicator** in the top bar (level, estimated: on battery / charging / USB full).

**Devices**
- 🐝 **Zigbee2MQTT** – lights and switches, large networks too
- 🔌 **Shelly** (Gen1 and Gen2+) on the same broker
- 🧩 **ESPHome, Tasmota & co.** – devices with MQTT discovery show up by themselves, like satellites (sensors, switches, lights)
- 🌈 **WLED** on the same broker – on/off, brightness, colour
- ⛽ **Truma LevelControl** over Bluetooth – up to two gas bottles
- 🛢️ **Mopeka** over Bluetooth – up to four gas and tank sensors
- ⚖️ **Senso4s** *(beta)* – Basic/Plus gas cylinder scale over Bluetooth, level in % and kg
- ☀️ **Victron** over Bluetooth ("Instant Readout") – solar chargers, battery monitors, DC-DC chargers
- 🔋 **Victron Cerbo GX** (Venus OS) over MQTT – solar, grid/shore power, charger, battery (SOC) and DC system
- 🔥 **Truma heating** (Combi/VarioHeat) – via **CP Plus** (LIN bus) or **iNet X** (Bluetooth)
- ♨️ **Alde hydronic heating** (Compact 3020, 3030, 3030 Plus) via LIN bus
- 🔋 **Batteries with Bluetooth BMS** – Super B, Power Quarry/Shentec, Daly/Bulltron, JBD/Xiaoxiang, CS-BATTERIES *(beta)*,
  JK-BMS, LiTime/Redodo/PowerQueen, Renogy, Eco-Worthy, ANT, Vatrer: state of charge, voltage, current, temperature,
  cells – read-only; pairing PIN or password from the manufacturer's app supported
- ⚡ **Power stations** over Bluetooth – Bluetti, Allpowers, Anker SOLIX, EcoFlow: charge level, power, switch AC/DC output
- 💧 **SuperSense tank module** *(beta)* – up to four tank sensors over Bluetooth
- 💧 **Simarine STRx** *(beta)* – tank sensors STR1/STR2/STR3 via Bluetooth broadcast, open or encrypted
- 🌡️ **Climate sensors** *(beta)* – **Ruuvi** Tag/Air and **SwitchBot** Meter/Meter Plus/Outdoor Meter: temperature,
  humidity, pressure, CO₂ and particulate matter on the Ruuvi Air; the panel only listens, no pairing
- 🔥 **Diesel heater** *(beta)* – Vevor/Hcalory, HeaterCC/AirHeater: show and control (on/off, level, temperature)
- 🧊 **Fridge** *(beta)* – Alpicool, IceCube/Plug-in Festivals, Bodega and more: show and control (on/off, target per zone)
- 📡 **BTHome** *(beta)* – Shelly BLU (door, motion, button, H&T), Xiaomi with pvvx firmware and more, also encrypted
- 🐝 **Zigbee direct** *(beta, P4 and round panel)* – with the ESP32-H2 radio module, no Zigbee2MQTT needed: pair devices, lights,
  sockets (with power and energy), switches and sensors as tiles
- 📲 **App "S.O.C. Live"** *(beta, Android)* – the panel on your phone, Tailscale built in: tiles like on the panel, tap switches or shows all values
- 📱 **Live view** – operate the panel in the browser or on the phone (`/live`), password protected
- 🔐 **Tailscale** *(beta, P4 and round panel)* – editor and web interface from anywhere, no open port and no cloud:
  log the panel into your own tailnet via a link
- 🧩 **Rules** *(beta, P4 and round panel)* – automations with graphical blocks (Blockly) in the editor, e.g. motion light,
  light by button, dimming by button

**Three network modes**

| Mode | Description |
|---|---|
| Network | Panel and hub on your home Wi-Fi |
| Router | Panel opens its own Wi-Fi with DHCP; the SMHUB Nano joins it, the panel finds the broker automatically |
| Combined | Own Wi-Fi for the hub **and** home Wi-Fi, with internet forwarding (NAT) |

---

## 📋 Requirements

- A panel – see [Which panel?](#which-panel) for a comparison:
  **Elecrow CrowPanel Advanced 7" (ESP32-P4)** (EC-P4-7-V1.2), **Waveshare ESP32-S3-Touch-LCD-7B** (WS-S3-7B) or, as a
  small round panel, the **Elecrow CrowPanel 1.46" Rotary** (EC-S3-1.46)
- MQTT broker with **Zigbee2MQTT** (e.g. SMHUB Nano)
- VS Code with **PlatformIO** (pioarduino, Arduino-ESP32 3.x / ESP-IDF 5.x)
- Optional: microSD card (FAT32/exFAT) for backgrounds and backups
- For CP Plus or Alde: a **LIN transceiver** (TJA1020/TJA1021/MCP2025) and an RJ12 cable

---

<a id="which-panel"></a>

## 🖥️ Which panel? P4 or Waveshare

**Device codes** – used for short in the text and in files: manufacturer – chip – inches – board revision (the revision
only where it matters for the firmware).

| Code | Device |
|---|---|
| **EC-P4-7-V1.2** | Elecrow CrowPanel Advanced 7" HMI (ESP32-P4), board V1.2 – later revisions accordingly V1.3 etc. |
| **WS-S3-7B** | Waveshare ESP32-S3-Touch-LCD-7B |
| **EC-S3-1.46** | Elecrow CrowPanel 1.46" Rotary (round, ESP32-S3) |
| **CYD-ESP32-2.8** | "Cheap Yellow Display" ESP32-2432S028R (2.8", ESP32) – Victron display, satellite, extra display |

Both large panels are 7 inch with 1024 × 600 pixels and run the same firmware and the same editor. The difference is
the chip – and therefore how much can run at the same time.

| | **Elecrow CrowPanel Advanced 7" (ESP32-P4)** (EC-P4-7-V1.2) | **Waveshare ESP32-S3-Touch-LCD-7B** (WS-S3-7B) |
|---|---|---|
| Chip | ESP32-P4, 2 RISC-V cores, 400 MHz | ESP32-S3, 2 Xtensa LX7 cores, 240 MHz |
| Internal RAM | 768 KB – **~270 KB free** in operation | 512 KB – **~45 KB free** in operation (with Wi-Fi, Zigbee, Cerbo and Bluetooth) |
| PSRAM | **32 MB** | 8 MB (octal, 80 MHz) |
| Flash | 16 MB | 16 MB |
| Wi-Fi / Bluetooth | separate **ESP32-C6** radio chip (Wi-Fi 6, BLE 5) – takes load off the main chip | on the chip itself – Wi-Fi and Bluetooth share RAM and antenna with everything else |
| Display | IPS, MIPI-DSI, graphics acceleration (DMA2D, hardware JPEG) | parallel RGB, the image is constantly read from PSRAM |
| Battery | no measuring point on the P4 – only with your own voltage divider (2 × 100 kΩ to GPIO 49) | battery connector with voltage measurement |
| Firmware size | ~78 % of the app partition | ~75 % of the app partition |
| Build | ~1–2 min (pre-built framework libraries) | first build 10–20 min (framework is rebuilt) |
| Environment | `crowpanel-p4` (folder `p4/`) | `waveshare-7b` |
| Where to buy | [on Amazon](https://link.amazon/B04j2pDnV) | [on Amazon](https://www.amazon.de/dp/B0FDQXKFJ5) |

**P4 – pros:** plenty of headroom, all modules at the same time; built-in MQTT broker (P4 and round panel); radio on its own chip;
steady image without a PSRAM bottleneck; planned: Zigbee directly via the ESP32-H2 radio module.
**P4 – cons:** more expensive; battery not measurable out of the box; the C6 radio chip may need a firmware update
(serial command `c6update`); early chips (revision v1.x) compute Bluetooth pairing keys wrongly with the hardware
ECC – the firmware therefore computes them in software automatically.

**Waveshare – pros:** cheap and widely available; everything on one chip; battery connector with measurement, SD card,
RTC.
**Waveshare – cons:** **internal RAM is very tight**; the image is read from the same PSRAM the panel draws into – with
full pages and a lot of movement, stripes/glitches are possible (refresh rate therefore 25 Hz); Wi-Fi and Bluetooth
share memory and antenna.

> ⚠️ **Waveshare: don't build in everything!** With UI, Wi-Fi, Zigbee2MQTT, Cerbo GX and Bluetooth the ESP32-S3 is
> **almost full** (as of 0.2.0: ~45 KB internal RAM free). Only build in the modules you really need
> (`[features]`, `[truma]`, `[alde]` in `platformio.ini`) – `check-all` is for test builds only, not for uploading.
> Also switch off Bluetooth protocols you don't use on the panel (e.g. iNet X without pairing): disabled modules
> use no RAM. If you want many modules at the same time, choose the **P4**.

**Round panel – Elecrow CrowPanel 1.46" Rotary** (EC-S3-1.46; ESP32-S3R8, 8 MB PSRAM, 16 MB flash, round 360 × 360, rotary knob,
LED ring; environment `crowpanel-146`, folder `crowpanel146/`): small panel or additional display with one tile per
screen. No settings menu – set up in the browser (own Wi-Fi with captive portal). Where to buy:
[on Amazon](https://link.amazon/B02xEzSrj).

---

## 📦 Installation

1. Install VS Code and PlatformIO, open this folder (path without spaces, e.g. `C:\zp\simply-open-camper`).
2. Connect the board to the USB-C port labelled **"UART"** – that is also where the logs come out.
3. Choose the environment: `waveshare-7b`, `crowpanel-p4` or `crowpanel-146` (in the PlatformIO menu), then
   **PlatformIO: Upload**. On the Waveshare the **first build takes 10–20 minutes** because the Arduino framework is
   rebuilt with custom settings (XIP from PSRAM against flicker, NAT for combined mode).
4. Before uploading, the terminal waits for **Enter** – time to plug in.
5. The panel then guides you through the setup.
6. When you first open the editor (`http://<panel>/`), set your own **access password** (factory default
   `panelsat`). Forgotten? Reset it on the panel under **Settings → Network** (round panel: serial command `webpw`).

### 🛠 Maintenance and diagnostics

- **Web flasher:** double-click **`flasher.bat`** in the project folder (or `python tools/flasher.py`). It starts a small
  web server for this computer only and opens **http://localhost:8765/flasher/** with the sections **Panels** and
  **Satellites and accessories**. Per device: firmware from the **GitHub release** or the **locally built** one, and
  **"Nur Firmware (Update)"** (settings stay) or **"Komplett (neu einrichten)"** (everything erased). Chrome or Edge.
- **Debug log (troubleshooting):** switch on in the web interface (panel: editor → Settings → Info; satellites,
  Victron display, extra displays: card "Fehlersuche"), **off by default**. Records every firmware message with the
  time, plus memory, Wi-Fi and connections (MQTT, Cerbo …) every 60 s and the reason for the last restart (crash,
  watchdog, brownout). With an SD card (P4, Waveshare, CYD with card slot) as files `log-NNNNN.txt` (up to 1 MB each,
  the last 10; panel: `panel/logs`, CYD: `logs`), otherwise the last 8–64 KB in memory. View, download and delete
  right in the browser. When off it uses no memory.
- `tools/embed_web.py` packs the editor gzip-compressed into the firmware on every build.
- `tools/upload_wait.py` waits for Enter before uploading (`PANEL_UPLOAD_NOWAIT=1` disables it).
- `pio run -e check-all` builds a test variant with **all** modules (not meant for uploading).
- Only needed if you change fonts, icons or the editor:

```sh
node tools/gen_icons.js        # icons (needs react-icons + sharp)
python3 tools/gen_fonts.py     # LVGL fonts from Inter + icons
python3 tools/build_editor.py  # web/editor.src.html -> web/editor.html
```

---

## ⚙ Configuration

### Enabling and disabling modules (`platformio.ini`)

| Section | Switch | Meaning |
|---|---|---|
| `[features]` | `wled = 1` | WLED over MQTT |
| | `cerbo = 1` | Victron Cerbo GX over MQTT |
| | `lang = de` | Language of the panel UI: `de`, `en`, `es`, `fr` |
| | `hymer = 1` | Hymer Connect over Bluetooth (switch on/off in the settings; satellite: `[sat] hymer`) |
| `[truma]` | `cpplus = 0` | Truma CP Plus over LIN (transceiver required) |
| | `lin_uart`, `lin_rx`, `lin_tx`, `lin_en` | LIN connection for the CP Plus (default UART2) |
| | `inetx = 1` | Truma iNet X over Bluetooth (switch on/off in the settings) |
| `[alde]` | `enabled = 0` | Alde over LIN (the panel is LIN master) |
| | `model` | `3020`, `3030` or `3031` (= 3030 Plus) |
| | `baud`, `checksum` | 19200 / auto; possibly 9600 on the 3020 |
| | `lin_uart`, `lin_rx`, `lin_tx`, `lin_en` | LIN connection (CP Plus and Alde need separate UARTs) |
| | `red_uart`, `red_rx` | 3030 only: internal bus, read-only (−1 = off) |

Truma LevelControl, Mopeka and Victron are always built in and switched on or off on the panel under
**Einstellungen → Gas/Heizung** or **Victron**. Disabled protocols are not scanned and don't load radio and Wi-Fi.

### 🌍 Languages & dictionaries

| Part | Languages | Selection | Dictionary | Tool |
|---|---|---|---|---|
| **Panel** (screen) | German, English, Spanish, French | `lang` under `[features]` in `platformio.ini` | `src/i18n/<code>.h` | `tools/i18n_update.py` |
| **Browser editor** | German, English | automatically like the panel, or `http://<panel>/?lang=en` | `web/i18n/<code>.json` | `tools/i18n_editor.py` |

The texts in the source code are German – they are also the **key** in the dictionary. If a translation is
missing, the German text is shown; nothing breaks. Device names are never translated.

#### 📖 Panel dictionary (`src/i18n/<code>.h`)

One line per text: German on the left (do not change), translation on the right, followed by the source location.

```c
{"Einstellungen", "Settings"},                       // src/ui/ui_settings.cpp:1424
{"vor %ld Min.", "%ld min ago"},                     // src/ui/ui_gas.cpp:29
{"Kein Echo – Sensor richtig angebracht?", ""},      // empty = stays German
```

**New language** (e.g. Italian):

1. `python tools/i18n_update.py it` – creates `src/i18n/it.h` with all texts (right side empty).
2. Translate the right side.
3. Set `lang = it` in `platformio.ini` and build.

#### 📖 Editor dictionary (`web/i18n/<code>.json`)

A JSON object: German text → translation. `{}` stands for a variable part (number, name …) and is inserted into
the translation in the same order.

```json
{
 "Einstellungen": "Settings",
 "vor {} Min.": "{} min ago",
 "„{}“ heißt danach überall „{}“ – auch in Home Assistant.": "“{}” will then be called “{}” everywhere – including Home Assistant."
}
```

**New language** (e.g. French):

1. `python tools/i18n_editor.py fr` – creates `web/i18n/fr.json` with all texts (translations empty).
2. Translate the values.
3. Run `python tools/i18n_editor.py` again – it injects the dictionaries into `web/editor.src.html` and
   `web/editor.html`. Then build and upload.

#### ✍️ Rules for translators

- **Never change the left side / the key** – that is how the text is found in the code.
- **Keep placeholders:** on the panel `%s`, `%d`, `%u`, `%ld`, `%.1f`; in the editor `{}`. Count and order must match.
- **Keep leading/trailing spaces** (e.g. `"  ·  Boiler "`) – such pieces are combined with other texts.
- The panel fonts cover the Latin-1 range (á é í ó ú ñ ç à è ê ü ß …), but e.g. not "œ" and no non-Latin scripts.
- **New texts in the code:** `python tools/i18n_update.py` or `python tools/i18n_editor.py` add them to every
  dictionary; with `--report` they only show what is missing.

Pull requests with new or improved translations are very welcome!

### SMHUB Nano

- **Network mode:** On the Nano, enable "Allow External" under **Settings → MQTT**. Enter the Nano's IP on the
  panel (port 1883, base topic `zigbee2mqtt`).
- **Router mode:** On the panel choose **Netzwerk → Router** (the panel restarts). On the Nano select the panel
  Wi-Fi (2.4 GHz) – name and password are shown on the panel. The editor is then at `http://192.168.4.1/`.
- **Radio channels:** Wi-Fi and Zigbee share 2.4 GHz. Zigbee2MQTT uses channel 11 by default → keep the panel
  Wi-Fi on 13. In combined mode the panel Wi-Fi follows the channel of your home Wi-Fi.

---

## 📡 Setting up devices

### ⛽ Truma LevelControl

- The sensor only advertises every few minutes and allows **one connection at a time**. If the Truma app,
  Home Assistant or the Victron GX is connected, the panel retries on the next round.
- Scanning runs up to three minutes; polling interval 5 to 60 minutes (default 10).
- The last reading survives restarts.

### 🛢️ Mopeka

- Supported: Pro Check, Pro Plus, Pro Check Universal, Pro Check H2O, Lippert BottleCheck, Pro-200, TD40/TD200 and
  the older Standard sensors. The panel only listens – the Mopeka app keeps working.
- While scanning, briefly press the **sync button** on the sensor – it is then marked "Taste gedrückt".
- Choose the bottle type in the detail view: 6/11/14 kg (Europe), 20/30/40 lb (US) or custom values (mm, kg/l).

### ⚖️ Senso4s *(beta, untested)*

- The **Senso4s Basic / Plus** gas cylinder scale sits under the cylinder and broadcasts the level in % openly over
  Bluetooth (updated every 15 minutes). No pairing, no password, the Senso4s app keeps working.
- Set up the cylinder (tare weight, capacity) and the zeroing once in the **Senso4s app**.
- Set up like a Mopeka sensor: enable the **Mopeka** Bluetooth protocol, **Mopeka → Search** – the scale shows up as
  "Senso4s Basic" or "Senso4s Plus". Choose the cylinder size and the panel calculates the kg.
- Notices from the scale appear in the status: zeroing needed, setup error, batteries empty, on the Plus warnings
  about temperature, incline or motion.

### 🔋 Batteries and power stations

- On the panel **Settings → Batteries (BMS)** or **Power Stations** → "Search", in the editor the **Batteries** tab.
- **Close the manufacturer's app first** – most batteries only talk to one device at a time.
- If the battery asks for a **pairing PIN** (e.g. Super B) or has a **password** in its app (e.g. CS-BATTERIES), enter
  it in the "PIN / password" field. The panel shows when the battery asks for it.
- **CS-BATTERIES** *(beta, untested)*: JBD BMS with login – the panel handles the app login itself, only a password
  you set yourself has to be entered.
- **Cell voltages:** many batteries now show every single cell – JBD/Xiaoxiang, CS-BATTERIES, JK-BMS, Daly/Bulltron,
  ANT, Eco-Worthy, Renogy, Vatrer and Power Quarry/Shentec. **Long-press the battery tile**: window with all values the
  battery reports (voltage, current, power, remaining time, temperature, cycles, health, capacity, alarms) and all
  cells as boxes – highest outlined orange, lowest blue, plus the difference (Δ).
- PIN/password and removing are only in the settings, not in the tile window.
- Read-only – nothing is switched or changed on the battery. Power stations: AC/DC output can be switched.

### 💧 Simarine STRx tank sensors *(beta, untested)*

Simarine tank sensors **STR1** (analog), **STR2** (NMEA 2000) and **STR3** (RV-C) can broadcast their level over
Bluetooth. The panel only listens – no pairing, **no app key needed**.

1. Set up the tank in the **Simarine Connect** app (height, capacity, fluid type, calibration).
2. For each sensor enable **Device Management → Broadcast Settings → Broadcast Mode** – **it is off by default**.
3. Choose the mode:
   - **Open** – level, capacity and fluid type readable by anyone. Nothing to enter; easiest for a first test.
   - **Low / Medium / High Security** – level encrypted. Every sensor has **its own key**: tap **Show Encryption Key**
     in the app and enter the **32 characters** when adding the sensor or later in the **PIN / password** field. The key is
     not the 4-digit device PIN of the app. In Medium/High the sensor does not send capacity and fluid type – the panel
     then shows the percentage only.
4. **Close the Simarine app / disconnect it from the sensor** – while a phone is connected the sensor does not broadcast.
5. On the panel **Batteries (BMS) → Search** (editor: Batteries tab). The sensor shows up as "Simarine STRx". Add it –
   tile with the level, details show the content in litres, fluid type and whether the broadcast is open or encrypted.

If there is no value, the status says why: broadcast off, key missing, key does not match. If you regenerate the key
in the Simarine app, enter the new one. Do not use it as the only measurement for safety-critical levels.

### 🔥 Diesel heater *(beta, untested)*

- Chinese diesel heaters with Bluetooth: **Vevor/Hcalory** with the "AirHeaterBLE" app (also the encrypted variant) and
  **HeaterCC/AirHeater**. Names like BYD-…, Heater…, "Air Heater", HC-…, BAC-…, CRONUS…, Hcalory…, boygu… are detected.
- Set up like a battery: **Batteries (BMS) → Search**, shows up as "Dieselheizung". Close the app first.
- Tile: on/off, level or target temperature, cabin temperature; faults (E1…E10) in red.
- **Control:** long-press the tile → **on/off**, **level ±**, **target temperature ±**. On the Victron display in the
  web interface (battery card).
- Safety: only on/off, level and temperature – no pump or setup values. If the heater reports a fault, only switching
  off is possible. At least 0.7 s between commands.
- The app passkey is 1234 by default; if you changed it, enter it in the **PIN / password** field.
- Not yet: Hcalory MVP1/MVP2 and Sunster (other protocols).

### 🧊 Fridge *(beta, untested)*

- Compressor fridges with the **Alpicool / Car Fridge Freezer** and **IceCube / Plug-in Festivals** apps, also Bodega,
  Brass Monkey and others (names WT-…, A1-…, AK1/2/3-…), single and dual zone.
- Set up like a battery: **Batteries (BMS) → Search**, shows up as "Kühlbox". On the first connection you may have to
  press the **settings button on the fridge**. Close the app first – while it is connected the fridge talks to nobody else.
- Tile: inside temperature large, target below (second zone next to it).
- **Control:** long-press the tile → **on/off**, **target ±** per zone. Settings are always written from the last read
  state, nothing is guessed; reset is never sent.

### 📡 BTHome *(beta, untested)*

- Open standard for many cheap wireless sensors: **Shelly BLU** (door/window, motion, button, H&T), **Xiaomi
  thermometers with pvvx firmware** and many more. The panel only listens, no pairing.
- Set up like a battery: **Batteries (BMS) → Search**, the device shows up as "BTHome".
- Tile: temperature and humidity, or the state for door/motion sensors ("door open", "motion", "wet!" …). Button
  events ("button: short/double/long") show in the state.
- **Encrypted** (e.g. Shelly BLU with key): enter the key (32 characters, from the Shelly app) in the **PIN / password**
  field.

### 🌡️ Ruuvi and SwitchBot *(beta, untested)*

- **Ruuvi Tag / Ruuvi Air** and **SwitchBot Meter, Meter Plus, Outdoor Meter** broadcast their values openly over
  Bluetooth. No pairing, no password, the manufacturer's app may stay open, no SwitchBot hub needed.
- Set up like a battery: **Batteries (BMS) → Search** – the sensor shows up as "Ruuvi" or "SwitchBot Meter".
- Tile: temperature large, humidity below (Ruuvi Air: CO₂). Details show pressure, particulate matter and battery.

### ☀️ Victron

- Enable **Instant Readout** in VictronConnect (**Settings → Product info → Instant Readout**).
- Paste each device's key from **Show details** into the editor or type it on the panel.
- Read-only – Instant Readout cannot switch anything.

### 🔋 Victron Cerbo GX

- On the GX enable **Settings → Services → MQTT** (plain, port 1883).
- Enter the GX IP address on the panel under **Einstellungen → Victron** or in the editor. The panel detects the
  portal ID itself; a password is only needed if one is set on the GX for the local network.
- The panel connects directly to the GX (its own MQTT connection, independent of the Zigbee2MQTT broker),
  subscribes only to the values it needs and keeps the GX publishing with a keepalive.
- Five tiles: **Solar** (PV power, charge state, yield today), **Grid** (power, voltage, current limit),
  **Charger** (Multi/Quattro or mains charger: state, switch, power), **Battery** (SOC, voltage, power,
  time to go) and **DC system** (load power).
- Read-only – switching (e.g. inverter mode) is not built in yet.

### 🔥 Truma heating

- **CP Plus:** LIN transceiver on the UART header (switch set to "UART"), LIN to RJ12 pin 3, ground to pin 5
  (on the Combi or via a splitter). Then run **PR SET → Init** once on the CP Plus.
  Wiring in detail: [Wired connections](#-wired-connections-lin-bus-and-ci-bus).
- **iNet X:** Enable the protocol in the settings, tap **"iNet X koppeln"** and then open **"Add device"** on the
  control panel (3 minutes). The iNet X remembers at most four devices.

### ♨️ Alde

- Connect the LIN wire to the **yellow RJ12 socket** (pin 4, ground pin 2) and enter model and pins under `[alde]`.
  The panel is the LIN master here (1 kΩ resistor needed) – see [Wired connections](#-wired-connections-lin-bus-and-ci-bus).
- The panel polls the boiler every 500 ms and only sends on changes – settings on the Alde panel stay valid.
- Compact 3020 HE: as far as known it can only be read, not controlled – not supported.

### 🚐 Hymer Connect

- For Erwin Hymer Group vehicles with **HYMER Connect** (Hymer, Bürstner, Dethleffs, Eriba, LMC, Carado …): lights,
  main switch, water pump, battery, lithium BMS, fresh/grey water, solar, shore power, outside temperature – directly
  over Bluetooth from the SCU, **no cloud**.
- On the panel enable "Hymer Connect" under **Einstellungen → Fahrzeug** (Vehicle), tap **Hymer koppeln** and then press
  **CONNECTION** on the control panel in the vehicle (3 minutes). On a satellite the same works in its web UI.
- The values show up in the editor like the ports of a satellite named **"Hymer"** and can be dragged onto the panel;
  lights and switches toggle with a tap.
- Bluetooth only – a cable connection to the SCU is not provided.
- **Not yet tested on a real SCU** – feedback very welcome.

### 🏕️ Hobby (HobbyConnect box or CI-BUS) *(beta, untested)*

- For **Hobby** caravans and motorhomes: lights (switchable), leisure battery (voltage, current, state of charge,
  time left, temperature), fresh water, inside/outside temperature, shore power, heating, hot water, fridge,
  air conditioning, main switch – directly on the panel, no app and no cloud.
- **Two ways**, each switchable and **off by default** – under **Einstellungen → Fahrzeug** (Vehicle):
  - **HobbyConnect box via Bluetooth:** switch on, tap the box that was found. Disconnect the Hobby app on the phone
    first (the box only talks to one device).
  - **CI-BUS cable at the LSG** (replaces the box): RS232-to-TTL converter (e.g. MAX3232, **3.3 V**) on a free serial
    port; choose port (UART1, UART2 …) and RX/TX pin on the panel. 115200 baud; the panel fetches the value numbers
    from the LSG itself. Wiring: see [Wired connections](#-wired-connections-lin-bus-and-ci-bus).
- The values show up in the editor like the ports of a satellite **"Hobby"**; the leisure battery additionally as a
  **battery monitor tile**. On the panel **"Kacheln anlegen"** creates all tiles at once. The **main switch** only
  works over the cable – careful if the panel itself is behind the main switch.
- `LIGHT_DUSCHE`/`LIGHT_WASCH` are swapped depending on the vehicle – just rename the tile. The serial command
  `hobby` lists all received values (unknown ones too) – please include it for other models.
- Protocol: [`docs/protokolle/hobby_ble_cibus.md`](docs/protokolle/hobby_ble_cibus.md) (German).

### 🔌 Wired connections (LIN bus and CI-BUS)

Truma CP Plus, Alde and the Hobby CI-BUS are wired to the panel. **Hymer Connect needs no cable** – it only uses
Bluetooth to the SCU (see above).

**Basic rules**

- The ESP32 pins only tolerate **3.3 V**. Never connect the bus directly: LIN (12 V) only through a
  **LIN transceiver**, CI-BUS (RS232, ±12 V) only through an **RS232-to-TTL converter**.
- Connect **ground (GND)** of panel, converter and vehicle.
- The panel's **RX** goes to the converter's **output**, the panel's **TX** to its **input**. Nothing arrives?
  Swap RX and TX.
- Each device needs its **own serial port** (UART). UART0 is reserved for log/USB.

| Device | Bus and connector | Converter | Baud | Master | Setting |
|---|---|---|---|---|---|
| Truma CP Plus | LIN, RJ12: **pin 3 = LIN, pin 5 = ground** (on the Combi or via a splitter) | LIN transceiver | 9600 | CP Plus | `platformio.ini` → `[truma]` |
| Alde 3020/3030 | LIN, **yellow** RJ12 socket: **pin 4 = LIN, pin 2 = ground** | LIN transceiver + master resistor | 19200 | **panel** | `platformio.ini` → `[alde]` |
| Hobby (CI-BUS) | RS232 at the LSG, Hobby wiring kit "Bluetooth-Modul" | RS232-to-TTL converter (MAX3232) | 115200 | LSG | on the panel: **Einstellungen → Fahrzeug** (Vehicle) |
| Hymer Connect | – (Bluetooth) | – | – | – | Einstellungen → Fahrzeug |

**LIN transceiver** (TJA1020/TJA1021 ready-made module; MCP2025 likewise):

| Transceiver | Connection |
|---|---|
| VBAT / VS | +12 V vehicle supply (through a fuse) |
| GND | vehicle ground **and** panel GND |
| LIN | LIN wire (pin see table above) |
| RXD | panel RX pin – open-drain output: 4.7 kΩ pull-up to 3.3 V (usually already on ready-made modules) |
| TXD | panel TX pin |
| SLP / NSLP (MCP2025: CS) | to 3.3 V – or to a GPIO entered as `lin_en` (the panel drives it high) |

- **Alde:** the panel is the LIN master. It needs a **1 kΩ resistor with a diode** (e.g. 1N4148, anode to +12 V)
  from +12 V to the LIN wire – many modules have a "Master" jumper for this.
- **CP Plus:** the CP Plus is the master, the panel only answers – **no** 1 kΩ resistor, "Master" jumper open.
- Alde 3030 (optional): the internal red bus (glycol/hot-water temperature) can only be listened to with a second
  transceiver (`red_uart`, `red_rx`).

**RS232-to-TTL converter for the Hobby CI-BUS** (e.g. MAX3232 module):

| Converter (TTL side) | Panel |
|---|---|
| VCC | **3.3 V** (not 5 V – otherwise 5 V reach the ESP32) |
| GND | GND |
| TXD (output) | RX pin |
| RXD (input) | TX pin |

The RS232 side (D-sub 9) goes to Hobby's "Bluetooth-Modul" wiring kit (TX, RX, ground of the LSG). The kit's pinout is
documented in [BastyJuice/Hobby_Caravan_Serial_2_CI-BUS_Connectivity](https://github.com/BastyJuice/Hobby_Caravan_Serial_2_CI-BUS_Connectivity).
Nothing arrives? Swap RX and TX on the RS232 side (null modem). Port and pins are chosen on the panel (no rebuild);
the status shows whether the LSG answers.

**Pins per panel**

| Panel | Connection | Note |
|---|---|---|
| Waveshare 7B | UART header, **GPIO 15/16** (switch set to "UART", not RS485) | Free pins are scarce – the header serves **one** device (CP Plus, Alde **or** Hobby). Docs and wiki disagree on the direction: nothing arrives → swap RX/TX. |
| CrowPanel P4 | free GPIOs on the headers | Enter pins in `p4/platformio.ini` or (Hobby) on the panel. **UART3 / GPIO 27/28** is used by the Zigbee radio module (H2) – don't use it while the module is plugged in. |
| Round panel 1.46" | UART socket on the back (1.25 mm, 4-pin): **1 = RX (GPIO 44), 2 = TX (GPIO 43), 3 = 5 V, 4 = GND** | This is where the **Zigbee radio module (H2)** goes, see below. The socket uses **5 V logic** (level shifters on the board). |

**Zigbee radio module (ESP32-H2) on the round panel** *(beta, untested)*: the module is made for the P4's 3.3 V slot,
but the socket on the back of the round panel supplies 5 V and 5 V logic. In between you need:

| From the socket | Via | To the H2 module |
|---|---|---|
| Pin 3 (5 V) | 3.3 V regulator (e.g. AMS1117-3.3, HT7333) | 3V3 |
| Pin 4 (GND) | – | GND |
| Pin 2 (panel TX) | level shifter 5 V → 3.3 V (e.g. BSS138 module) | module RX |
| Pin 1 (panel RX) | level shifter 3.3 V → 5 V | module TX |

Then switch on **Settings → Zigbee → "Use Zigbee module"** in the browser editor (off by default) and pair devices;
they can also be renamed, configured and removed there. New devices get a tile automatically. The pinout comes from
Elecrow's schematic – measure before soldering. **Never connect 5 V or 5 V logic directly to the H2!**

### 📱 Live view (operate the panel in the browser)

- **`http://<panel>/live`** (or the **"Live"** button at the top of the editor) shows the panel with its pages and tiles
  in the browser – **tap to switch**, **long press** for brightness or on/off. Values refresh every 1.5 s.
- Protected by the **access password** (like the editor). On the phone "Add to Home screen" – it then starts like an app;
  held sideways the panel gets bigger. With **Tailscale** also from anywhere (`http://<name-in-tailnet>/live`).
- Costs the panel only processing time while someone is watching; switching uses the same paths as the screen.

### 📲 App "S.O.C. Live" (Android) *(beta)*

The panel on your phone – at home, at the campsite and on the road over mobile data. The app shows the tiles **exactly
as the panel computes them** (values, colours, levels), landscape across the whole screen.

<p><img src="assets/app/live.png" width="49%" alt="S.O.C. Live – tiles like on the panel"> <img src="assets/app/detail.png" width="49%" alt="S.O.C. Live – all values of a battery"></p>

- **Tap** switches lights and sockets; for readings (battery, Victron, gas, heating, sensors) a window with **all
  values** opens – for a battery e.g. state of charge, voltage, current, power, temperature and cells.
- **Long press** on a dimmable light: brightness.
- **Tailscale is built in** – no Tailscale app needed, no open port on the router.
- You enter the panel's access password once; the app remembers it (only on the phone).

**Install and set up**

1. Download [`release/soc-live.apk`](release/soc-live.apk) to the phone and open it (Android 7 or newer, 64-bit;
   allow "install unknown apps" once).
2. Read and accept the notes.
3. **"Sign in with Tailscale"** – the browser opens the Tailscale sign-in, the phone shows up as **soc-live** in your
   tailnet. No account yet? **"Create Tailscale account"** (free for personal use).
4. The app **searches the panel in the tailnet** and connects; with several panels you tap one.
   Requirement: Tailscale is on at the panel, same account (see [Tailscale](#-tailscale--remote-access-from-anywhere-p4-and-round-panel-beta)).
5. The first time, the panel asks for its access password – done.

**Without Tailscale** (only on the panel's Wi-Fi or the same network): on first start choose "Without Tailscale – only
on the panel's Wi-Fi" and enter the address (panel Wi-Fi: `192.168.4.1`, otherwise the IP from Settings → Network).

**Settings:** **long-press** the back button – address, Tailscale on/off, search panel again, auth key (instead of the
browser sign-in), keep screen on, full screen, delete saved password, sign out of Tailscale.
Tailscale only runs while the app is used (off after 2 min in the background).

### 🔐 Tailscale – remote access from anywhere (P4 and round panel) *(beta)*

- Brings the panel into your **tailnet** via [microlink](https://github.com/CamM2325/microlink) (Tailscale-compatible
  client, MIT). Editor and web interface are then reachable from anywhere under the 100.x address or the name
  (e.g. `http://simply-open-camper/`, with MagicDNS), on all devices running Tailscale (phone, laptop …). The access
  password still applies. No open port on the router, no cloud in between.
- **Setup (off by default):**
  1. In the browser editor go to **Settings → Network**, scroll down to **"Tailscale (Beta)"**.
  2. Switch on **"Panel ins Tailnet bringen"**. Without an auth key the button **"Bei Tailscale anmelden"** appears after
     a few seconds – open the link, log in with your Tailscale account, approve the device. Alternatively enter an
     **auth key** from the Tailscale admin (Settings → Keys, ideally one-off and short-lived).
  3. Done: status and address are shown in the editor and on the panel under **Settings → Network**.
- The auth key and the panel's keys stay **in the panel only** (not in the firmware, not in the repo). **"Abmelden"**
  deletes them – then remove the old device in the Tailscale admin. Device name in the tailnet: field
  "Gerätename im Tailnet" (otherwise the panel's network name).
- **Tested (P4):** directly connected on the home Wi-Fi (editor in 0.3 s), via phone hotspot/mobile data through the
  Tailscale relay in Frankfurt (ping 150–200 ms, editor in 4–5 s, also several requests at once). Round panel: built,
  not tested yet.
- Costs about 105 KB flash and a little over 40 KB internal RAM – only when switched on. Not on the Waveshare (too little
  internal RAM) and not on the small displays/satellites (no PSRAM). microlink supports Headscale, but the panel can't
  configure it yet.
- Tip: other devices in the camper (satellites, GX, extra displays) are easiest to reach remotely through a router running
  Tailscale as a "subnet router" (e.g. GL.iNet).

### 🖼️ Splash screen

At start-up the panel shows a splash image for 3 seconds and frees the memory afterwards. The default is the banner
(`assets/splash.jpg`); your own **`splash.jpg`** in the project folder takes precedence at build time. Save it as baseline JPEG (not progressive), ideally 1024×600 – larger images are scaled down
by 1/2, 1/4 or 1/8.

### 🔌 Shelly via Bluetooth *(beta, untested)*

- For **Shelly Gen2, Gen3 and Gen4** (Plus, Pro, Mini …) – no Wi-Fi connection and no MQTT broker needed. P4, round
  panel (EC-S3-1.46) and Waveshare 7B. Gen1 has no Bluetooth: keep using Shelly over MQTT for those.
- Enable Bluetooth on the Shelly (Shelly app → Settings → Bluetooth).
- Panel: enable **Settings → Shelly (BT)**, “Search Shellys”, add the one found (name, password only if set on the
  Shelly). The editor has the same page under Settings.
- The panel connects briefly: state every 30 s, switching immediately (1–2 s). Switches, lights/dimmers, power and
  energy appear as tiles like the MQTT Shellys and also work in rules and on remote displays.

### 🧩 ESPHome, Tasmota & co. (MQTT discovery)

Devices that announce themselves on the broker via **Home Assistant discovery** are shown on the panel automatically –
without Home Assistant. Each device appears in the device list **like a satellite** (its own group with its name);
drag tiles onto the panel from there. Supported: **sensors** (value + unit), **binary sensors** (door, motion, water …),
**switches** and **lights** (on/off). The values also work in rules, the app, the live view and on extra displays.

1. Panel: **Settings → MQTT → "Show devices from MQTT discovery"** (off by default; prefix `homeassistant`, can be
   changed in the editor).
2. Connect the device to the same broker as the panel – in a camper the easiest is the panel's **built-in broker**.
   Example for ESPHome:

   ```yaml
   mqtt:
     broker: 192.168.4.1        # panel IP (panel Wi-Fi) or its address on your network
     username: !secret mqtt_user   # only if a user is set on the panel's broker
     password: !secret mqtt_pass
   ```

   Tasmota: enter the broker under *Configuration → MQTT* and set `SetOption19 1` (Home Assistant discovery).
3. After a few seconds "Found: … values from … devices" shows below the switch.

Own satellites and Zigbee2MQTT devices are skipped (the panel knows them directly); the panel does not forward anything
from discovery devices – no loop. Value templates are understood in the simple form (`{{ value }}`,
`{{ value_json.ENERGY.Power }}`); controls like numbers, selects or climate will come later.

### 🌈 WLED

- In WLED enable **Config → Sync Interfaces → MQTT**, enter the same broker and keep the device topic
  `wled/<name>`. The panel discovers the devices automatically.
- Colour temperature and effects are not available over MQTT – keep using the WLED app for those.

---

## 🛰️ Satellites

A satellite is a cheap **ESP32** that you place anywhere in the vehicle – at the gas
locker, under the bed next to the fresh water tank, in the utility compartment. It reads Bluetooth devices and its own
sensors and sends the values to the panel. This saves memory and radio time on the panel (Bluetooth and Wi-Fi share
one antenna there) and bridges distances Bluetooth alone cannot cover.

**Supported chips** – one environment per chip in the PlatformIO menu:

| Environment | Chip / board | Analog (ADC1) | I²C (default) | Notes |
|---|---|---|---|---|
| `satellite` | ESP32 (WROOM, esp32dev) | GPIO 32–39 | 21 / 22 | most pins, GPIO 34–39 input only |
| `satellite-c3` | ESP32-C3 (Super Mini, DevKitM-1) – [on Amazon](https://link.amazon/B04vjKxhb) | GPIO 0–4 | 8 / 9 | small, 13 free pins, log over USB |
| `satellite-c3-oled` | ESP32-C3 with 0.42" OLED (ABrobot, 72 × 40) – [on Amazon](https://link.amazon/B00lVYc7g) | GPIO 0–4 | 5 / 6 (shared with the OLED) | shows a sensor value or the spirit level, see below |
| `satellite-s3` | ESP32-S3 (DevKitC-1) | GPIO 1–10 | 8 / 9 | log over the "UART" port |
| `satellite-c6` | ESP32-C6 (DevKitC-1, Super Mini) – [on Amazon](https://link.amazon/B0gJnV7Pv) | GPIO 0–6 | 23 / 22 | log over USB |

All need at least 4 MB flash. The web UI only lists the pins that the chip actually has free.

**ESP32-C3 with OLED** (`satellite-c3-oled`): on start the small display alternately shows a motorhome or a
caravan with "S.O.C.", then **automatically** – with an MPU-6050 tilt sensor a **two-axis spirit level** (bubble in a
circle, pitch and roll), otherwise the connected sensors one after another (e.g. temperature), without a sensor the
satellite's name and address. Can be fixed in the web interface ("OLED" card), as can brightness and rotation. The
MPU-6050 goes on the same pins as the OLED (SDA GPIO 5, SCL GPIO 6). To protect the OLED it briefly turns fully black
and fully white every 20 minutes.

**16-channel relay board** (environment `satellite-relay16`, ESP32-WROOM-32E with 2× 74HC595) –
[relay board on Amazon](https://link.amazon/B08IzxSMo), [USB-TTL adapter on Amazon](https://link.amazon/B02ciSu8U):

- The board has **no USB port** – to flash it you need a **USB-TTL adapter** with 3.3 V levels: adapter TX to board
  RX, RX to TX, GND to GND. To flash, hold **IO0 to GND while powering on** (or press BOOT), then release.
- It needs its **own 5 V supply** – the adapter is not enough (switching relays would otherwise make the voltage
  drop or the USB connection break). Connect the GND of board and adapter.
- Later updates go through the satellite's web interface, without the adapter.

**What a satellite reads and switches**

| Type | Capacity | Notes |
|---|---|---|
| ⛽ Truma LevelControl, 🛢️ Mopeka, ☀️ Victron | via Bluetooth | same modules as in the panel |
| 💧 Tank probes | up to 4 tanks, up to 5 probes each | rods of different lengths, each with its own percentage step |
| 🌡️ DS18B20 | up to 8 sensors on one GPIO | 1-Wire, search sensors with a button, offset per sensor |
| 📈 Analog | up to 4 inputs (ADC1, see above) | two points to convert mV into the value |
| 📐 MPU-6050 (GY-521) | tilt, lengthwise and crosswise | zero point via button, smoothing see below |
| 🔌 Switches | up to 8 outputs (relays) | active low selectable, state after boot: off / on / last |
| 🔘 Inputs | up to 8 contacts or buttons | pull-up and invert selectable |

**Three ways to the panel** – each can be enabled on the satellite:

- **Bluetooth** straight to the panel, no Wi-Fi needed. Encrypted (AES-CTR) with one shared password for all
  satellites. The panel adopts satellites with a matching password automatically.
- **MQTT** via the broker, with **Home Assistant discovery** – so the values also show up in Home Assistant.
  Topics: `panelsat/<id>/sensor/<point>`, switch with `panelsat/<id>/switch/<point>/set` (`ON` / `OFF` / `TOGGLE`).
- **UDP** (port 47811) as broadcast or to a fixed panel IP.

The panel needs `satellites = 1` under `[features]` in `platformio.ini` (default).

### Setup

1. Flash the firmware: pick the matching environment in the PlatformIO menu (`satellite`, `satellite-c3` …) and
   click **Upload** – or `pio run -e satellite-c3 -t upload`. Internally the satellite is a separate PlatformIO
   project in the `satellite/` folder with its own package folder; the entries in the panel menu just hand over.
   That way panel and satellite don't keep reinstalling each other's framework.
2. Without Wi-Fi access the satellite opens the Wi-Fi **"SOC-Satellite-…"** (password `panelsat`).
   Connect and open **http://192.168.4.1/**.
3. In the web UI set up Wi-Fi, the way to the panel and the Bluetooth password, then add Bluetooth devices and assign
   the GPIOs. The web UI shows what each pin can do and warns about boot pins, boot button, LED and double use.

### 💧 Tank probes

Several stainless steel rods of different lengths in the tank plus a shared ground rod. When water touches a probe it
conducts to ground; the highest step with contact is shown (e.g. 20 / 40 / 60 / 80 / 100 %).

- **One GPIO per probe** – the simplest way. The ground rod goes to GND or to its own GPIO.
  For modules with a transistor there is "contact = high".
- **Resistor ladder on one analog pin** – all probes on one pin through resistors. In the web UI you enter the
  measured voltage for each step; the satellite picks the closest one.
- **Against corrosion** the probes are only powered briefly while measuring (own interval, default 60 s) and are
  unpowered otherwise. Each probe is read three times, the majority wins.
- If a higher probe is wet and a lower one dry, the satellite reports the value as **implausible** – usually a dirty
  rod or a wiring fault.

### 📐 Calming the tilt reading (MPU-6050)

Something always moves in a camper: people walking around, wind pushing. To keep the value from jumping, the
satellite smooths in several stages – all adjustable in the web UI:

| Stage | Default | Effect |
|---|---|---|
| Low-pass in the sensor | 5 Hz | filters vibration inside the MPU-6050 already |
| Median | last 5 values | single outliers are dropped |
| Time filter | 2 s | moving average (0 = off) |
| Dead band | 0.2° | smaller changes are not reported |
| Only when still | from 3 °/s | while the sensor rotates (gyro), the last value is held until 1 s of rest |

Set the **zero point** with a button while the vehicle is level. Axes can be swapped and inverted if the board is
mounted the other way round.

---

## 🏠 Home Assistant

The panel does not need Home Assistant – but works well together with it. If you use HA, the panel can also serve
as a **control panel for Home Assistant**.

| Direction | What | Status |
|---|---|---|
| **Satellites → HA** | All satellites register themselves via MQTT discovery: tank probes, temperatures, relays (switchable), inputs, level, extra sensors and their Bluetooth sensors | ✅ working |
| **Zigbee2MQTT → HA** | as usual through Zigbee2MQTT itself | ✅ working |
| **Panel → HA** | Via the integration [simple-open-camper-HA-Integration](https://github.com/cyclemat/simple-open-camper-HA-Integration): selected panel values (lights, switches, batteries, Victron, gas, satellite tanks and temperatures …) as sensors, switches and lights in HA | 🧪 beta |
| **HA → Panel** | Via the same integration: selected HA entities (lights, switches, sensors …) appear on the panel like a satellite "Home Assistant" – as tiles, in rules, the app and the live view | 🧪 beta |

### 🔌 "Simple Open Camper" integration for Home Assistant *(beta)*

Home Assistant connects to the panel itself – **one** WebSocket connection, local or over Tailscale, no MQTT broker.
In the integration you choose **which panel values HA gets** and **which HA entities the panel shows**; each side only
gets what is selected.

1. **Panel:** browser editor → **Settings → Network → Home Assistant** → switch on "Allow Home Assistant to connect" and
   copy the **access code**. (Firmware: for the P4 until the next version use the test build from
   [`release/test/`](release/test/), afterwards any regular version.)
2. **HA – install:** HACS → ⋮ → **Custom repositories** → `https://github.com/cyclemat/simple-open-camper-HA-Integration`,
   type **Integration** → download → restart HA. (Without HACS: copy `custom_components/simple_open_camper` to
   `config/custom_components/`.)
3. **HA – set up:** Settings → Devices & services → **Add integration → Simple Open Camper** → enter the panel's address
   (LAN IP, Tailscale IP or tailnet name) and the access code.
4. **HA – choose:** on the integration **Configure** →
   step 1 "Take over from the panel": tick panel values (show up in HA, grouped like on the panel) →
   step 2 "Provide to the panel": pick HA entities (show up in the panel's device list, one group per area).

If the panel is not on the same network as HA (e.g. in the camper on the road), put both into your tailnet: on the panel
[Tailscale](#-tailscale--remote-access-from-anywhere-p4-and-round-panel-beta), in HA the Tailscale add-on – then use the
panel's 100.x address.

### 🔁 No data loop

To make sure no value runs in circles (panel → HA → panel → HA …), fixed rules apply:

- The panel reports to HA **only what is attached directly to the panel** (Bluetooth, H2 radio module, heating bus,
  rules).
- What the panel **already receives via MQTT** is **not** passed on: Zigbee2MQTT devices, Cerbo GX, satellites and
  extra displays (they register themselves or have their own HA integrations).
- Values the panel fetches **from Home Assistant** never go back to HA.
- The panel never reads topics below `homeassistant/` as input.
- The HA selection in the editor hides devices that come from the panel itself or its satellites – the panel already
  has them directly.

## 📟 Victron display (small stand-alone firmware)

For everyone who only wants to see **Victron and the gas level**, there is a lean firmware for the 2.8" display
ESP32-2432S028R (“Cheap Yellow Display”, CYD-ESP32-2.8, about €15): Cerbo GX via WiFi, Victron devices, Truma LevelControl, Mopeka,
batteries with Bluetooth BMS and power stations, plus SuperSense, Simarine STRx, Ruuvi and SwitchBot as beta – as tiles, without a big
panel. **English or German.** The same firmware also exists for the Waveshare 7" with settings on the screen.

- **⚡ [Web flasher](https://cyclemat.github.io/simply-open-camper-victron-beta/)** – install from the browser
  (Chrome/Edge), choose the language there
- Code, manual and ready-made files: [simply-open-camper-victron-beta](https://github.com/cyclemat/simply-open-camper-victron-beta)
- In this project: environments `satellite-cyd-victron` (German) and `satellite-cyd-victron-en` (English)

## ⚠ Known limitations

| Problem | What to adjust |
|---|---|
| Picture flickers/drifts | Lower `LCD_PCLK_HZ` in `src/app_config.h` to 21 or 18 MHz |
| Touch mirrored/rotated | `TOUCH_SWAP_XY`, `TOUCH_INVERT_X/Y` in `src/app_config.h` |
| Bounce buffer artefacts | Set `LCD_BOUNCE_BUFFER_PX` to 0 |
| No MQTT in router mode | "Allow External" enabled on the Nano? Broker running? Status is shown under **Info** |
| SD card not detected | Format FAT32/exFAT, restart the panel; if needed lower `SD_FREQ_KHZ` |
| Panel restarts with large Zigbee networks | Check the log on "UART"; "Interner RAM knapp" shows where memory gets tight |
| No log on the PC | Use the **"UART"** port, not "USB" |
| Missing umlauts | Run `python3 tools/gen_fonts.py` again |
| `IncompatiblePlatform … 6.2.0-post.1` | Install `pioarduino==6.2.0` in PlatformIO's Python (pioarduino issue #563) |
| `whitespace character in project paths` | Move the project to a path without spaces |

- The board doesn't report whether USB power is present – "USB / battery" is estimated from the voltage trend.
  The 7B has no RTC chip; without internet the time is lost on power failure.
- The 7B's UART header shares its interface with RS485 (switch on the board).

---

## 🔌 REST API

Used by the editor, also handy for your own scripts:

| Endpoint | Purpose |
|---|---|
| `GET /api/info`, `GET /api/devices` | Panel state, device list |
| `GET\|POST /api/layout` | Layout (`?force=1` on conflict) |
| `POST /api/rename` | `{ieee, name}` rename in Zigbee2MQTT |
| `GET\|POST /api/bg?page=<id>&fmt=jpg` | Background (JPEG 1024 × 600, `&delete=1`) |
| `GET\|POST /api/backup` | `{"create":true}`, `{"restore":"<name>"}`, `{"delete":"<name>"}` |
| `GET\|POST /api/settings` | Settings, incl. `{"ble":{"truma","mopeka","victron","inetx"}}` |
| `POST /api/wifi`, `/api/time`, `/api/refresh`, `/api/reboot`, `/api/saver` | Wi-Fi, time, reload, restart, screen saver |
| `POST /api/ota` | Firmware update (firmware.bin) |
| `GET\|POST /api/saver_logo?fmt=bin\|png` | Custom logo (`&delete=1`) |
| `GET\|POST /api/truma` | `{"scan":true}`, `{"measure":"<mac>"}`, `{"add":{mac,type,name}}`, `{"remove"}`, `{"rename"}`, `{"interval"}` |
| `GET\|POST /api/mopeka` | `{"scan":true}`, `{"add":{mac,name,tank}}`, `{"remove"}`, `{"rename"}`, `{"tank":{mac,tank,empty,full,capacity}}` |
| `GET\|POST /api/victron` | `{"scan":true}`, `{"add":{mac,key,name}}`, `{"key":{mac,key}}`, `{"remove"}`, `{"rename"}` |
| `GET\|POST /api/heat` | `{"src":"cpplus"\|"inetx"\|"alde", "heat":{on,target,mode}, "water", "energy":{e,power}, "clock", "pair", "forget"}` |

---

## 🧩 Protocols & related projects

The device protocols are based on these open projects. What was taken is the **protocol description** (byte
layout, constants, sequences) – the code here is an independent C++ implementation. Exceptions live in
`src/third_party/` (third-party code or data, unchanged with licence notice) and are marked below. Many thanks to the
authors!

| Module (file) | Reference | Licence of the reference | Used for |
|---|---|---|---|
| Truma LevelControl (`src/truma.cpp`) | [cyclemat/HAOS-Truma-Levelcontrol-Integration](https://github.com/cyclemat/HAOS-Truma-Levelcontrol-Integration) | GPL-3.0 | Password/RC4 key, measurement sequence |
| Mopeka (`src/mopeka.cpp`) | [esphome/esphome](https://github.com/esphome/esphome) – `mopeka_pro_check`, `mopeka_std_check` | GPL-3.0 (C++) / MIT (Python) | Pro/Standard packet layout, temperature compensation, echo evaluation, tank dimensions |
| | [Bluetooth-Devices/mopeka-iot-ble](https://github.com/Bluetooth-Devices/mopeka-iot-ble) | MIT | Model list, medium coefficients (air, water) |
| Truma CP Plus (`src/truma_cpplus.cpp`) | [danielfett/inetbox.py](https://github.com/danielfett/inetbox.py) | GPL-3.0 | LIN frames, diagnostic services, status buffers, encoding |
| | [Fabian-Schmidt/esphome-truma_inetbox](https://github.com/Fabian-Schmidt/esphome-truma_inetbox) | GPL-3.0 | Registration, buffer layouts, ESP32 timing |
| | [muccc/WomoLIN](https://github.com/muccc/WomoLIN) | see project | Early decoding of the Truma LIN protocol (via the two above) |
| Truma iNet X (`src/truma_inetx.cpp`) | [rpodgorny/hass-truma-inetx](https://github.com/rpodgorny/hass-truma-inetx) | GPL-3.0; its bundled protocol files come from [daaaaan/truma-inetx-ble](https://github.com/daaaaan/truma-inetx-ble) **without a licence** | GATT characteristics, transfer sequence, frames, topics/parameters |
| Alde (`src/alde.cpp`) | [daviddootson/alde_3030_plus](https://github.com/daviddootson/alde_3030_plus) | MIT | Bus layout, control and info frames, red bus (glycol/hot water) |
| | [holdestmade/esphome-alde-3020](https://github.com/holdestmade/esphome-alde-3020) | MIT | 3020 specifics, checksum detection, timing |
| | [darenshaw/truma-inet-x-to-alde-3020](https://github.com/darenshaw/truma-inet-x-to-alde-3020), [inetbox.py issue #28](https://github.com/danielfett/inetbox.py/issues/28) | GPL-3.0 / – | Notes on models and limits (3020 HE) |
| Hymer Connect (`src/hymer.cpp`, `src/hymer_tls.cpp`) | [Facetti/hymer-connect-ha-ble](https://github.com/Facetti/hymer-connect-ha-ble), [BetaHydri/hymer-connect-ha](https://github.com/BetaHydri/hymer-connect-ha) | MIT | GATT services, TLS profile, PIA frames, sensor maps; the seven subscription messages are taken verbatim |
| Tailscale (`src/tailscale.cpp`, `src/third_party/microlink/`) | [CamM2325/microlink](https://github.com/CamM2325/microlink) (includes wireguard-lwip, BSD-3-Clause) | MIT | Tailscale-compatible client, taken over unchanged (only settings instead of Kconfig) |
| Hobby (`src/hobby.cpp`) | [AndreasKirsten79/hobby-caravan-pi](https://github.com/AndreasKirsten79/hobby-caravan-pi) (with [paveltresnak/hobby-caravan-esphome-ble](https://github.com/paveltresnak/hobby-caravan-esphome-ble)) | not stated | protocol knowledge only: BLE service, login, fragments, commands |
| | [BastyJuice/Hobby_Caravan_Serial_2_CI-BUS_Connectivity](https://github.com/BastyJuice/Hobby_Caravan_Serial_2_CI-BUS_Connectivity) | MIT | CI-BUS serial: login, commands, variable table |
| WLED (`src/wled.cpp`) | [WLED – MQTT interface](https://kno.wled.ge/interfaces/mqtt/) | EUPL-1.2 (WLED) | Topics and commands |
| Cerbo GX (`src/cerbo.cpp`) | [victronenergy/dbus-flashmq](https://github.com/victronenergy/dbus-flashmq), [dbus API in the Victron wiki](https://github.com/victronenergy/venus/wiki/dbus) | see project | Topics, keepalive, value paths |
| Victron Smart Lithium, Smart BatteryProtect (`src/victron.cpp`) | [keshavdv/victron-ble](https://github.com/keshavdv/victron-ble) | Unlicense | Layout of records 0x05 and 0x09 |
| Batteries with BMS (`src/bms.cpp`) | [patman15/aiobmsble](https://github.com/patman15/aiobmsble) | Apache-2.0 | Detection, frames and values for Daly/Bulltron, JBD, JK, LiTime/Redodo, Eco-Worthy, ANT, Vatrer, Super B Epsilon V2 |
| | [mr-manuel/venus-os_dbus-serialbattery](https://github.com/mr-manuel/venus-os_dbus-serialbattery) | AGPL-3.0 | Protocols JBD, JK-BMS (JK02), LiTime |
| CS-BATTERIES, SuperSense (`src/bms.cpp`) | own analysis of the apps | – | CS-BATTERIES login (app key, password), SuperSense tank module |
| Ruuvi (`src/bms.cpp`) | [ruuvi/ruuvi-sensor-protocols](https://github.com/ruuvi/ruuvi-sensor-protocols) | see project | Data formats 3, 5 and 6 (advertisements) |
| Simarine STRx (`src/bms.cpp`) | [Simarine/strx-ble-broadcast-protocol](https://github.com/Simarine/strx-ble-broadcast-protocol) | see project | Advertisements, metadata, AES-CCM encryption |
| Diesel heater (`src/bms.cpp`) | [Spettacolo83/diesel-heater-ble](https://github.com/Spettacolo83/diesel-heater-ble), [homeassistant-diesel-heater](https://github.com/Spettacolo83/homeassistant-diesel-heater), [mSoftMS/AirHeater-BLE](https://github.com/mSoftMS/AirHeater-BLE) | MIT | Frames AA55/AA66/ABBA, XOR key, commands, detection |
| Fridge (`src/bms.cpp`) | [klightspeed/BrassMonkeyFridgeMonitor](https://github.com/klightspeed/BrassMonkeyFridgeMonitor), [deeteeppg/bodega-bluetooth-le-hass](https://github.com/deeteeppg/bodega-bluetooth-le-hass) | MIT | Frames, status, commands |
| | Analysis of the IceCube / Plug-in Festivals app (user's document) | – | Set layout per model, bind |
| BTHome (`src/bms.cpp`) | [bthome.io](https://bthome.io), [Bluetooth-Devices/bthome-ble](https://github.com/Bluetooth-Devices/bthome-ble) | MIT | Object table, encryption |
| Senso4s (`src/mopeka.cpp`) | [ksanislo/senso4s_ble](https://github.com/ksanislo/senso4s_ble) | Apache-2.0 | Advertisements, error and warning codes |
| SwitchBot (`src/bms.cpp`) | [OpenWonderLabs/SwitchBotAPI-BLE](https://github.com/OpenWonderLabs/SwitchBotAPI-BLE) | see project | Advertisements of the thermo-hygrometers |
| | Manufacturer documents or own analysis of the apps | – | Super B (older batteries), Power Quarry/Shentec |
| Renogy (`src/bms.cpp`) | [cyrils/renogy-bt](https://github.com/cyrils/renogy-bt) | GPL-3.0 | Modbus registers of charge controllers, DC-DC chargers, batteries, inverters |
| | [IAmTheMitchell/renogy-ble](https://github.com/IAmTheMitchell/renogy-ble) | Apache-2.0 | Device detection, Smart Shunt 300, Pro battery |
| Power stations (`src/bms_ps.inc`) | [Patrick762/bluetti-bt-lib](https://github.com/Patrick762/bluetti-bt-lib), [warhammerkid/bluetti_mqtt](https://github.com/warhammerkid/bluetti_mqtt) | MIT | Bluetti: Modbus registers, encryption of newer models |
| | [madninjaskillz/allpowers-ble](https://github.com/madninjaskillz/allpowers-ble) | MIT | Allpowers |
| | [flip-dots/SolixBLE](https://github.com/flip-dots/SolixBLE) | MIT | Anker SOLIX: login, packets, field mapping per model |
| | [rabits/ha-ef-ble](https://github.com/rabits/ha-ef-ble) | Apache-2.0 | EcoFlow: login, encryption, data structures; **key table taken over** (`src/third_party/ecoflow/ef_keydata.h`) |
| Round panel (`src/board_crowpanel146.cpp`) | [Elecrow-RD/CrowPanel-1.46inch-HMI-ESP32-Rotary-Display](https://github.com/Elecrow-RD/CrowPanel-1.46inch-HMI-ESP32-Rotary-Display) | no licence stated | Pin assignment, display init commands |

For the project licence see [Licence](#licence). The licences of the references apply to their code, not to the
own implementation here. For the iNet X, the original
protocol analysis has no licence. This is not legal advice.

### 📚 Libraries, fonts and icons used

| Name | Source | Licence | Used for |
|---|---|---|---|
| LVGL 8.4 | [lvgl/lvgl](https://github.com/lvgl/lvgl) | MIT | UI of all panels and displays |
| ArduinoJson 7 | [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson) | MIT | JSON (MQTT, web API, settings) |
| NimBLE-Arduino | [h2zero/NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | Apache-2.0 | Bluetooth (Waveshare, round panel, satellites, Victron display) |
| esp-nimble-cpp | [h2zero/esp-nimble-cpp](https://github.com/h2zero/esp-nimble-cpp) | Apache-2.0 | Bluetooth on the P4 (via the C6 radio chip) |
| LovyanGFX | [lovyan03/LovyanGFX](https://github.com/lovyan03/LovyanGFX) | FreeBSD (BSD-2-Clause) | Display driver for round panel and CYD |
| micro-ecc | [kmackay/micro-ecc](https://github.com/kmackay/micro-ecc) | BSD-2-Clause | **included** in `src/third_party/`: EcoFlow key exchange, Bluetooth pairing on the P4 |
| Arduino-ESP32 | [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32) | LGPL-2.1 | Framework |
| ESP-IDF | [espressif/esp-idf](https://github.com/espressif/esp-idf) | Apache-2.0 | Framework, incl. MQTT, HTTP server, NimBLE stack |
| ESP-Hosted | [espressif/esp-hosted-mcu](https://github.com/espressif/esp-hosted-mcu) | see project | Wi-Fi/Bluetooth via the C6 (P4) |
| pioarduino | [pioarduino/platform-espressif32](https://github.com/pioarduino/platform-espressif32) | Apache-2.0 | PlatformIO platform |
| Inter | [rsms/inter](https://github.com/rsms/inter) | SIL OFL 1.1 | Font (converted in `src/fonts/`) |
| Phosphor Icons | [phosphor-icons/core](https://github.com/phosphor-icons/core) | MIT | Icons (via react-icons, `tools/gen_icons.js`) |

---

## 📝 Changes

What changed from version to version (German): **[CHANGELOG.md](CHANGELOG.md)**

## 🛠 Feedback & contributions

- Please report bugs and ideas as issues – ideally with a log from the "UART" port
- Pull requests are welcome
- Tested Mopeka, CP Plus, iNet X, Alde, WLED, Cerbo GX, batteries, power stations, SuperSense, Simarine, Ruuvi or SwitchBot?
  Let me know – even if it doesn't work!

---

## ❤ Support & donations

This project is built in my spare time. If it helps you, I'd be happy about a coffee:

**[☕ PayPal – support CycleMat](https://paypal.me/cyclemat)**

No pressure, no ads, no paywalls – just open source and passion ❤

---

<a id="licence"></a>

## 📜 Licence

**[PolyForm Noncommercial 1.0.0](LICENSE)** – Copyright © 2026 cyclemat.

- ✅ **Allowed:** private use, modification and sharing – also for clubs, research and education, in each case
  without a commercial purpose.
- ❌ **Not allowed without a separate licence:** selling the firmware or devices running it, commercial installation
  or offering it as a service. **Commercial licences are granted by the copyright holder only** – please open an issue
  if you are interested.
- Parts in `src/third_party/` are under their own permissive licences (see [there](src/third_party/README.md)).
- Contributions: see [CONTRIBUTING.md](CONTRIBUTING.md).
- Versions previously distributed under GPL-3.0 remain GPL-3.0 for their recipients.

---

## 📄 Trademark notice

**Private, community-driven hobby project.**

- Not affiliated with Truma, Alde, Mopeka, Victron Energy, WLED, Shelly, Waveshare, Zigbee2MQTT, Ruuvi, SwitchBot,
  CS-BATTERIES, SuperSense (comworks), Simarine or the battery and power station makers mentioned.
- "Truma", "LevelControl", "iNet", "Alde", "Mopeka", "Victron", "WLED", "Shelly", "Ruuvi", "SwitchBot", "CS-BATTERIES",
  "SuperSense" and "Simarine" are trademarks or names of their
  respective owners and are only mentioned to describe compatibility.
- The project only connects to your own devices.
- Use at your own risk – especially when working on heating and gas systems.

---

<p align="center"><strong>CycleMat · Your lights. Your heating. Your panel.</strong></p>
