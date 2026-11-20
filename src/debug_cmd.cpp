// Befehle ueber die serielle Schnittstelle – siehe debug_cmd.h
#include "rules.h"
#include "debug_cmd.h"
#include <Arduino.h>
#include <ctype.h>
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/usb_serial_jtag_ll.h"   // USB des ESP32-S3 direkt lesen, ohne Treiber und Puffer
#include "app_config.h"
#include "net.h"
#include "z2m.h"
#include "zb_direct.h"
#include "board.h"
#include "settings.h"
#include "hymer.h"
#include "hobby.h"
#include "truma.h"
#include "cerbo.h"
#include "web_server.h"
#include "board_crowpanel146.h"
#if defined(BOARD_P4) && BOARD_P4
// P4: WLAN und Bluetooth laufen im ESP32-C6 (ESP-Hosted). Dessen Firmware laesst sich vom P4 aus aktualisieren:
// Befehl "c6update" laedt sie von espressif.github.io (WLAN mit Internet noetig) und schreibt sie in den C6.
#include "esp32-hal-hosted.h"
#include "ESP_HostedOTA.h"
#define HAS_C6 1
#else
#define HAS_C6 0
#endif

#ifndef DISPLAY_TESTING
#define DISPLAY_TESTING 0
#endif

static char s_line[32];
static uint8_t s_len = 0;

// ---------------------------------------------------------------- Speicher nach jedem Startschritt
struct Mark {
  const char *name;
  uint32_t ms, free_int, min_int, min_ps;   // min_ps: Tiefstwert PSRAM in KB
};
static Mark s_marks[20];
static uint8_t s_nmarks = 0;

void debug_mark(const char *name) {
  if (s_nmarks >= sizeof(s_marks) / sizeof(s_marks[0])) return;
  s_marks[s_nmarks++] = {name, millis(), (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                         (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                         (uint32_t)(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM) / 1024)};
}

static void print_boot() {
  log_printf("\nInterner RAM beim Start (frei nach dem Schritt, Verbrauch des Schritts, Tiefstwert bis dahin):\n");
  uint32_t prev = 0;
  for (uint8_t i = 0; i < s_nmarks; i++) {
    const Mark &m = s_marks[i];
    long used = i ? (long)prev - (long)m.free_int : 0;
    log_printf("  %6lu ms  %-14s frei %4u KB  Verbrauch %6ld B  Tiefstwert %4u KB  PSRAM-Tiefstwert %5u KB\n",
               (unsigned long)m.ms, m.name, (unsigned)(m.free_int / 1024), used, (unsigned)(m.min_int / 1024),
               (unsigned)m.min_ps);
    prev = m.free_int;
  }
  log_printf("  jetzt              frei %4u KB, Tiefstwert %u KB\n\n",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024));
}

// ---------------------------------------------------------------- Tasks (Liste nur waehrend der Ausgabe im PSRAM)
static void print_tasks() {
  UBaseType_t n = uxTaskGetNumberOfTasks() + 4;
  TaskStatus_t *t = (TaskStatus_t *)heap_caps_malloc(n * sizeof(TaskStatus_t), MALLOC_CAP_SPIRAM);
  if (!t) return;
  n = uxTaskGetSystemState(t, n, nullptr);
  log_printf("\n%u Tasks (Reserve = nie benutzter Stack in Byte):\n", (unsigned)n);
  for (UBaseType_t i = 0; i < n; i++) {
    int core = t[i].xCoreID == tskNO_AFFINITY ? -1 : (int)t[i].xCoreID;
    log_printf("  %-16s Prio %2u  Kern %2d  Reserve %6u\n", t[i].pcTaskName, (unsigned)t[i].uxCurrentPriority, core,
               (unsigned)(t[i].usStackHighWaterMark * sizeof(StackType_t)));
  }
  log_printf("\n");
  heap_caps_free(t);
}

// ---------------------------------------------------------------- Screenshot (nur Display-Tests)
// Schickt den Bildspeicher roh ueber USB: "SOCSHOT <b> <h> RGB565\n", Pixel (little endian), "\nSOCEND\n".
// Abholen mit tools/screenshot.py. Liest direkt aus dem PSRAM – kein zusaetzlicher Speicher.
#if DISPLAY_TESTING
static bool jtag_write(const uint8_t *p, size_t n) {
  while (n) {
    uint32_t t0 = millis();
    while (!usb_serial_jtag_ll_txfifo_writable()) {
      if (millis() - t0 > 1000) return false;   // niemand liest mit
      delayMicroseconds(50);
    }
    uint32_t k = usb_serial_jtag_ll_write_txfifo(p, n);
    usb_serial_jtag_ll_txfifo_flush();
    p += k;
    n -= k;
  }
  return true;
}

static void send_screenshot() {
  const uint8_t *fb = (const uint8_t *)board_framebuffer();
  if (!fb) {
    log_printf("Kein Bildspeicher\n");
    return;
  }
  char head[48];
  int hn = snprintf(head, sizeof(head), "SOCSHOT %d %d RGB565\n", LCD_H_RES, LCD_V_RES);
  uint32_t t0 = millis();
  bool ok = jtag_write((const uint8_t *)head, hn);
  size_t total = (size_t)LCD_H_RES * LCD_V_RES * 2;
  for (size_t pos = 0; ok && pos < total; pos += 4096) ok = jtag_write(fb + pos, total - pos < 4096 ? total - pos : 4096);
  if (ok) jtag_write((const uint8_t *)"\nSOCEND\n", 8);
  log_printf("Screenshot %s (%lu ms)\n", ok ? "gesendet" : "abgebrochen", (unsigned long)(millis() - t0));
}
#endif

#if HAS_C6
// Funkchip (ESP32-C6) aus einer Datei auf der SD-Karte aktualisieren – z. B. zurueck auf Elecrows Original
// (network_adapter.bin, V2.12.3) als /sd/c6.bin. Prueft vorher, ob es wirklich eine C6-Firmware ist.
static void c6_from_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    log_printf("Datei %s nicht gefunden (SD-Karte eingesteckt?)\n", path);
    return;
  }
  uint8_t hdr[16] = {0};
  fread(hdr, 1, sizeof(hdr), f);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  // Abbild-Kopf: 0xE9, Chip-Kennung bei Byte 12 (13 = ESP32-C6)
  if (hdr[0] != 0xE9 || (hdr[12] | hdr[13] << 8) != 13) {
    log_printf("%s ist keine Firmware fuer den ESP32-C6 – abgebrochen\n", path);
    fclose(f);
    return;
  }
  if (!hostedIsInitialized()) {
    log_printf("Funkchip nicht bereit\n");
    fclose(f);
    return;
  }
  log_printf("Funkchip-Update aus %s (%ld Byte) ...\n", path, size);
  uint8_t *buf = (uint8_t *)heap_caps_malloc(1500, MALLOC_CAP_SPIRAM);
  bool ok = buf && hostedBeginUpdate();
  long done = 0, next = 0;
  while (ok) {
    size_t n = fread(buf, 1, 1500, f);
    if (!n) break;
    ok = hostedWriteUpdate(buf, n);
    done += n;
    if (done >= next) {
      log_printf("  %ld %%\n", done * 100 / (size ? size : 1));
      next += size / 10 + 1;
    }
  }
  fclose(f);
  heap_caps_free(buf);
  ok = ok && done == size && hostedEndUpdate() && hostedActivateUpdate();
  log_printf(ok ? "Funkchip-Update fertig – Panel startet neu.\n" : "Funkchip-Update fehlgeschlagen.\n");
  if (ok) {
    delay(1500);
    ESP.restart();
  }
}
#endif

// ---------------------------------------------------------------- Status
static void ram_line(const char *name, uint32_t caps) {
  log_printf("  %-8s frei %5u KB, Tiefstwert %5u KB, groesster Block %5u KB, gesamt %5u KB\n", name,
             (unsigned)(heap_caps_get_free_size(caps) / 1024), (unsigned)(heap_caps_get_minimum_free_size(caps) / 1024),
             (unsigned)(heap_caps_get_largest_free_block(caps) / 1024), (unsigned)(heap_caps_get_total_size(caps) / 1024));
}

static const char *reset_reason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "Einschalten";
    case ESP_RST_SW: return "Neustart (Software)";
    case ESP_RST_PANIC: return "Absturz";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "Watchdog";
    case ESP_RST_BROWNOUT: return "Unterspannung";
    case ESP_RST_DEEPSLEEP: return "Tiefschlaf";
    default: return "sonstiges";
  }
}

static void print_info() {
  uint32_t up = millis() / 1000;
  log_printf("\n===== %s %s - Status =====\n", FW_NAME, FW_VERSION);
  log_printf("Laufzeit %luh %02lum %02lus, letzter Start: %s\n", (unsigned long)(up / 3600), (unsigned long)(up / 60 % 60),
             (unsigned long)(up % 60), reset_reason());
  log_printf("Speicher:\n");
  ram_line("intern", MALLOC_CAP_INTERNAL);
  ram_line("DMA", MALLOC_CAP_DMA);
  ram_line("PSRAM", MALLOC_CAP_SPIRAM);
  log_printf("Netz: %s, WLAN \"%s\" %d dBm, IP %s\n", net_status_text(), net_ssid().c_str(), net_rssi(), net_ip().c_str());
  log_printf("Panel-WLAN: %s \"%s\", Kanal %u, %d Teilnehmer, %s\n", net_ap_active() ? "an" : "aus", g_settings.ap_ssid.c_str(),
             g_settings.ap_channel, net_ap_clients(), net_ap_ip().c_str());
  log_printf("MQTT: %s (%s), %u Geraete\n", z2m_connected() ? "verbunden" : "getrennt", z2m_status_text(), (unsigned)z2m_count());
  log_printf("Bluetooth-Protokolle:%s%s%s%s%s%s\n", ble_proto_on(BLE_P_TRUMA) ? " Truma" : "", ble_proto_on(BLE_P_MOPEKA) ? " Mopeka" : "",
             ble_proto_on(BLE_P_VICTRON) ? " Victron" : "", ble_proto_on(BLE_P_INETX) && WITH_TRUMA_INETX ? " iNetX" : "",
#if SOC_VICTRON_EDITION
             "", "");
#else
             ble_proto_on(BLE_P_SAT) ? " Satelliten" : "", ble_proto_on(BLE_P_HYMER) ? " Hymer" : "");
#endif
  if (hymer_enabled()) log_printf("Hymer: %s\n", hymer_status().c_str());
  if (cerbo_configured()) log_printf("Cerbo GX: %s – %s\n", cerbo_status().c_str(), cerbo_debug_text().c_str());
#if HAS_C6
  {
    uint32_t a = 0, b = 0, c = 0, x = 0, y = 0, z = 0;
    hostedGetHostVersion(&a, &b, &c);
    hostedGetSlaveVersion(&x, &y, &z);
    bool upd = hostedIsInitialized() && hostedHasUpdate();
    log_printf("Funkchip %s: Firmware %u.%u.%u, passend zum P4 waere %u.%u.%u%s\n", hostedGetSlaveTargetName(), (unsigned)x,
               (unsigned)y, (unsigned)z, (unsigned)a, (unsigned)b, (unsigned)c, upd ? " – Update verfuegbar (Befehl c6update)" : "");
  }
#endif
  float bv = board_battery_voltage();
  if (bv > 0) log_printf("Akku: %.2f V\n", bv);
  log_printf("Hintergrundbeleuchtung: %u %%%s\n", board_backlight(), board_is_dimmed() ? " (Ruhezustand)" : "");
  log_printf("==========================================\n\n");
}

#if HAS_C6
// Selbsttest der Bluetooth-Schluesselrechnung (P-256, Secure Connections) im NimBLE-Stack des P4.
// Grund: Kopplung mit PIN scheitert am P4 mit "DHKey Check Failed"; der Stack rechnet mit dem ECC-Rechenwerk des Chips.
extern "C" int ble_sm_alg_gen_key_pair(uint8_t *pub, uint8_t *priv);
extern "C" int ble_sm_alg_gen_dhkey(const uint8_t *peer_pub_key_x, const uint8_t *peer_pub_key_y, const uint8_t *our_priv_key,
                                    uint8_t *out_dhkey);
static void hexle(const char *t, const uint8_t *d) {   // Schluessel liegen little-endian vor
  char b[65];
  for (int i = 0; i < 32; i++) snprintf(b + 2 * i, 3, "%02x", d[31 - i]);
  log_printf("  %s %s\n", t, b);
}
static void sm_selftest() {
  // Basispunkt G der Kurve P-256 (big-endian), hier little-endian abgelegt
  static const char *GX = "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296";
  static const char *GY = "4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5";
  uint8_t gx[32], gy[32];
  for (int i = 0; i < 32; i++) {
    unsigned v;
    sscanf(GX + 2 * i, "%2x", &v);
    gx[31 - i] = v;
    sscanf(GY + 2 * i, "%2x", &v);
    gy[31 - i] = v;
  }
  int fails = 0;
  for (int run = 0; run < 3; run++) {
    uint8_t pa[64], ka[32], pb[64], kb[32], d1[32], d2[32], g1[32];
    int r1 = ble_sm_alg_gen_key_pair(pa, ka), r2 = ble_sm_alg_gen_key_pair(pb, kb);
    int r3 = ble_sm_alg_gen_dhkey(pb, pb + 32, ka, d1), r4 = ble_sm_alg_gen_dhkey(pa, pa + 32, kb, d2);
    int r5 = ble_sm_alg_gen_dhkey(gx, gy, ka, g1);   // a*G = oeffentlicher Schluessel A (x)
    bool sym = !memcmp(d1, d2, 32), pub = !memcmp(g1, pa, 32);
    log_printf("Lauf %d: Rueckgaben %d %d %d %d %d, a*B == b*A: %s, a*G == A: %s\n", run + 1, r1, r2, r3, r4, r5,
               sym ? "ja" : "NEIN", pub ? "ja" : "NEIN");
    if (!sym || !pub || r1 || r2 || r3 || r4 || r5) {
      fails++;
      hexle("a*B", d1);
      hexle("b*A", d2);
      hexle("A.x", pa);
      hexle("a*G", g1);
    }
  }
  log_printf(fails ? "ERGEBNIS: Schluesselrechnung FEHLERHAFT (%d von 3)\n" : "ERGEBNIS: Schluesselrechnung in Ordnung\n", fails);
}
#endif

static void run(const char *cmd) {
  if (!strcmp(cmd, "debug") || !strcmp(cmd, "info") || !strcmp(cmd, "ram") || !strcmp(cmd, "status")) {
    print_info();
  } else if (!strcmp(cmd, "tasks")) {
    print_tasks();
  } else if (!strcmp(cmd, "boot")) {
    print_boot();
  } else if (!strcmp(cmd, "ble")) {
    log_printf("Zaehle Bluetooth-Geraete 20 s lang ...\n");
    truma_ble_census(20);
#if WITH_HYMER
  } else if (!strcmp(cmd, "hobby")) {
    hobby_print_debug();
  } else if (!strcmp(cmd, "hymer")) {
    hymer_print_debug();
#endif
#if BOARD_CROWPANEL146
  } else if (!strcmp(cmd, "touch")) {
    log_printf("Touch: 20 s lang Rohwerte und Gesten ausgeben - jetzt wischen/tippen\n");
    board_touch_debug(20000);
#endif
#if DISPLAY_TESTING
  } else if (!strcmp(cmd, "screenshot")) {
    send_screenshot();
#endif
#if HAS_C6
  } else if (!strcmp(cmd, "c6update")) {
    log_printf("Funkchip-Update: lade %s ...\n", hostedGetUpdateURL());
    bool ok = updateEspHostedSlave();   // braucht WLAN mit Internet; danach startet der C6 neu
    log_printf(ok ? "Funkchip-Update fertig – Panel startet neu.\n" : "Funkchip-Update nicht moeglich (siehe oben).\n");
    if (ok) {
      delay(1500);
      ESP.restart();
    }
  } else if (!strcmp(cmd, "smtest")) {
    sm_selftest();
  } else if (!strcmp(cmd, "c6datei")) {
    c6_from_file("/sd/c6.bin");
#endif
#if WITH_ZB_DIRECT
  } else if (!strncmp(cmd, "zb", 2) && (cmd[2] == 0 || cmd[2] == ' ')) {
    zbd_debug(cmd + 2);   // zb | zb list | zb permit [s] | zb refresh | zb trace | zb diag | zb raw {"c":…}
#endif
  } else if (!strcmp(cmd, "rules") || !strcmp(cmd, "regeln")) {
    rules_debug();
  } else if (!strcmp(cmd, "webpw")) {
    web_password_reset();
    log_printf("Zugangspasswort der Weboberflaeche ist wieder \"panelsat\" - beim naechsten Oeffnen ein neues festlegen.\n");
  } else if (!strcmp(cmd, "reboot") || !strcmp(cmd, "neustart")) {
    log_printf("Neustart ...\n");
    delay(200);
    ESP.restart();
  } else if (!strcmp(cmd, "help") || !strcmp(cmd, "?") || !strcmp(cmd, "hilfe")) {
    log_printf("Befehle: debug (bzw. info/ram/status), tasks, boot, ble, webpw (Web-Passwort zuruecksetzen), reboot, help%s%s%s%s\n",
               WITH_HYMER ? ", hymer" : "", WITH_HOBBY ? ", hobby" : "", DISPLAY_TESTING ? ", screenshot (tools/screenshot.py)" : "",
               HAS_C6 ? ", c6update (Funkchip aus dem Internet), c6datei (Funkchip aus /sd/c6.bin)" : "");
#if WITH_ZB_DIRECT
    log_printf("Zigbee-Modul: zb (Stand), zb list, zb permit [Sekunden], zb refresh, zb trace, zb diag, zb raw {\"c\":\"info\"}\n");
#endif
  } else {
    log_printf("Unbekannter Befehl \"%s\" - help zeigt die Befehle\n", cmd);
  }
}

static void feed(char c) {
  if (c == '\r' || c == '\n') {
    if (s_len) {
      s_line[s_len] = 0;
      s_len = 0;
      run(s_line);
    }
  } else if (s_len < sizeof(s_line) - 1 && (uint8_t)c >= ' ') {
    s_line[s_len++] = tolower((unsigned char)c);
  }
}

void debug_poll() {
  uint8_t b[16];
  while (usb_serial_jtag_ll_rxfifo_data_available()) {
    uint32_t n = usb_serial_jtag_ll_read_rxfifo(b, sizeof(b));
    if (!n) break;
    for (uint32_t i = 0; i < n; i++) feed((char)b[i]);
  }
  while (Serial.available()) feed((char)Serial.read());   // UART0 (Stiftleiste / USB-Seriell-Wandler)
}
