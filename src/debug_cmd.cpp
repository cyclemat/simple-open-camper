// Befehle ueber die serielle Schnittstelle – siehe debug_cmd.h
#include "debug_cmd.h"
#include <Arduino.h>
#include <ctype.h>
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "hal/usb_serial_jtag_ll.h"   // USB des ESP32-S3 direkt lesen, ohne Treiber und Puffer
#include "app_config.h"
#include "net.h"
#include "z2m.h"
#include "board.h"
#include "settings.h"
#include "hymer.h"

static char s_line[32];
static uint8_t s_len = 0;

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
  log_printf("MQTT: %s (%s), %u Geraete\n", z2m_connected() ? "verbunden" : "getrennt", z2m_status_text(), (unsigned)z2m_count());
  log_printf("Bluetooth-Protokolle:%s%s%s%s%s%s\n", ble_proto_on(BLE_P_TRUMA) ? " Truma" : "", ble_proto_on(BLE_P_MOPEKA) ? " Mopeka" : "",
             ble_proto_on(BLE_P_VICTRON) ? " Victron" : "", ble_proto_on(BLE_P_INETX) ? " iNetX" : "",
             ble_proto_on(BLE_P_SAT) ? " Satelliten" : "", ble_proto_on(BLE_P_HYMER) ? " Hymer" : "");
  if (hymer_enabled()) log_printf("Hymer: %s\n", hymer_status().c_str());
  float bv = board_battery_voltage();
  if (bv > 0) log_printf("Akku: %.2f V\n", bv);
  log_printf("Hintergrundbeleuchtung: %u %%%s\n", board_backlight(), board_is_dimmed() ? " (Ruhezustand)" : "");
  log_printf("==========================================\n\n");
}

static void run(const char *cmd) {
  if (!strcmp(cmd, "debug") || !strcmp(cmd, "info") || !strcmp(cmd, "ram") || !strcmp(cmd, "status")) {
    print_info();
  } else if (!strcmp(cmd, "reboot") || !strcmp(cmd, "neustart")) {
    log_printf("Neustart ...\n");
    delay(200);
    ESP.restart();
  } else if (!strcmp(cmd, "help") || !strcmp(cmd, "?") || !strcmp(cmd, "hilfe")) {
    log_printf("Befehle: debug (bzw. info/ram/status), reboot, help\n");
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
