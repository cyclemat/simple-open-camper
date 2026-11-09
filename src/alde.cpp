// Alde Compact 3020/3030 ueber den LIN-Bus (siehe alde.h)
//
// Infoblock 0x1B (geschuetzt 0x5B, der Kessel antwortet mit 8 Byte):
//   [0] Zone 1 ist  = raw*0.5-42 °C     [1] Zone 2 ist (0xFE = keine Zone 2)   [2] aussen = raw*0.5-42 °C
//   [3] Bit0-5 Soll Zone 1 = raw*0.5+5, Bit6 Gas frei, Bit7 Gasventil offen
//   [4] Bit0-5 Soll Zone 2, Bit6-7 Elektro 0..3 kW
//   [5] Bit0 an, Bit1 beschaeftigt, Bit2 Stoerung, Bit3-4 Warmwasser (0 aus, 1 an, 2 Boost), Bit5 Landstrom,
//       Bit6 (3020: Klima-Automatik), Bit7 Pumpe
//   Werte 0xFB..0xFF bei Temperaturen = ungueltig/fehlt
// Steuerblock 0x1A (wir senden 8 Byte): [0..2] 0, [3] Soll1|Gas<<6, [4] Soll2|Elektro<<6, [5] an|Wasser<<3, [6..7] 0xFF
// Roter Bus (3030, nur lesen): ID 0x15 (0x55), 9600 Baud: Glykol = (b0+b1*256)/10, Warmwasser = (b2+b3*256)/10 °C
#include "alde.h"

#if !WITH_ALDE
void alde_begin() {}
uint32_t alde_version() { return 0; }
bool alde_get(TrumaHeatInfo &) { return false; }
void alde_set_heat(bool, float) {}
void alde_set_water(TrumaWater) {}
void alde_set_gas(bool) {}
void alde_set_electric(uint8_t) {}
#else

#if WITH_TRUMA_CPPLUS
#include "truma_cpplus.h"
#if TRUMA_LIN_UART == ALDE_LIN_UART
#error "CP Plus und Alde brauchen je einen eigenen LIN-Anschluss (lin_uart in [truma] und [alde] unterschiedlich setzen)"
#endif
#endif

#include <Arduino.h>
#include <atomic>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const uart_port_t U = (uart_port_t)ALDE_LIN_UART;
static const uint8_t ID_CTRL = 0x1A, ID_INFO = 0x1B, ID_DIAG = 0x3C, ID_RED = 0x15;
static const uint32_t CYCLE_MS = 500;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static std::atomic<uint32_t> s_ver{1};
static bool s_running = false;
static uint8_t s_info[8];
static bool s_has = false;
static uint32_t s_info_ms = 0;
static uint8_t s_cks = ALDE_CHECKSUM;        // erkannt bzw. eingestellt: 1 klassisch, 2 erweitert
static uint32_t s_fail = 0;                  // Abfragen ohne Antwort in Folge
// Befehl
static bool s_cmd = false;
static uint8_t s_sp1 = 0, s_gas = 0, s_el = 0, s_on = 0, s_water = 0;   // gewuenschter Zustand
static uint8_t s_cmd_left = 0;               // so oft wird der Steuerblock noch gesendet
static uint32_t s_cmd_ms = 0;
// roter Bus
static float s_fluid = NAN, s_hot = NAN;
static uint32_t s_red_ms = 0;

static uint8_t pid_of(uint8_t id) {
  id &= 0x3F;
  uint8_t p0 = ((id >> 0) ^ (id >> 1) ^ (id >> 2) ^ (id >> 4)) & 1;
  uint8_t p1 = ~((id >> 1) ^ (id >> 3) ^ (id >> 4) ^ (id >> 5)) & 1;
  return id | (p0 << 6) | (p1 << 7);
}

static uint8_t lin_sum(unsigned s, const uint8_t *d, size_t n) {
  for (size_t i = 0; i < n; i++) {
    s += d[i];
    if (s >= 256) s -= 255;
  }
  return (uint8_t)~s;
}

static float temp_of(uint8_t raw) { return raw >= 0xFB ? NAN : raw * 0.5f - 42.0f; }

// ---------------------------------------------------------------- Bus
// Break: 0x00 mit einem Viertel der Baudrate = 9 Bit low, also deutlich ueber die geforderten 13 Bitzeiten
static void send_header(uint8_t pid) {
  uart_flush_input(U);
  uart_set_baudrate(U, ALDE_BAUD / 4);
  uint8_t z = 0;
  uart_write_bytes(U, (const char *)&z, 1);
  uart_wait_tx_done(U, pdMS_TO_TICKS(20));
  uart_set_baudrate(U, ALDE_BAUD);
  esp_rom_delay_us(100);   // Break-Trenner
  uint8_t h[2] = {0x55, pid};
  uart_write_bytes(U, (const char *)h, 2);
}

// Antwort eines Slaves: im Empfangspuffer steht das Echo (0x00, 0x55, PID), danach 8 Daten + Pruefsumme
static bool read_response(uint8_t pid, uint8_t out[8], uint32_t timeout_ms) {
  uint8_t buf[40];
  size_t n = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < timeout_ms && n < sizeof(buf)) {
    int r = uart_read_bytes(U, buf + n, sizeof(buf) - n, pdMS_TO_TICKS(5));
    if (r > 0) n += r;
    for (size_t i = 0; i + 1 < n; i++) {
      if (buf[i] != 0x55 || buf[i + 1] != pid || n < i + 2 + 9) continue;
      const uint8_t *d = buf + i + 2;
      bool classic = lin_sum(0, d, 8) == d[8], enhanced = lin_sum(pid, d, 8) == d[8];
      if ((s_cks == 1 && !classic) || (s_cks == 2 && !enhanced) || (!classic && !enhanced)) continue;
      if (s_cks == 0) s_cks = enhanced ? 2 : 1;   // automatisch: so senden wir auch
      memcpy(out, d, 8);
      return true;
    }
  }
  return false;
}

static void send_frame(uint8_t id, const uint8_t d[8], bool force_classic) {
  uint8_t pid = pid_of(id);
  send_header(pid);
  uint8_t f[9];
  memcpy(f, d, 8);
  bool enh = !force_classic && s_cks != 1;
  f[8] = lin_sum(enh ? pid : 0, d, 8);
  uart_write_bytes(U, (const char *)f, 9);
  uart_wait_tx_done(U, pdMS_TO_TICKS(30));
}

// Anmeldung wie das Alde Smart Control (Lesen der Kennung: Lieferant 0x41DE, Funktion je Modell)
static void send_registration() {
  uint8_t fn = ALDE_MODEL == 3020 ? 0x01 : ALDE_MODEL == 3030 ? 0x02 : 0x03;
  uint8_t d[8] = {0x10, 0x06, 0xB2, 0x00, 0xDE, 0x41, fn, 0x00};
  send_frame(ID_DIAG, d, true);   // Diagnoserahmen: immer klassische Pruefsumme
}

// ---------------------------------------------------------------- roter Bus (3030, nur mitlesen)
static void red_task(void *) {
  const uart_port_t R = (uart_port_t)ALDE_RED_UART;
  uint8_t st = 0, buf[9], n = 0;
  uint32_t last = 0;
  for (;;) {
    uint8_t b;
    if (uart_read_bytes(R, &b, 1, pdMS_TO_TICKS(50)) <= 0) { st = 0; continue; }
    uint32_t now = millis();
    if (st && now - last > 10) st = 0;
    last = now;
    switch (st) {
      case 0: if (b == 0x00) st = 1; break;
      case 1: st = b == 0x55 ? 2 : (b == 0x00 ? 1 : 0); break;
      case 2: if (b == pid_of(ID_RED)) { st = 3; n = 0; } else st = 0; break;
      case 3:
        buf[n++] = b;
        if (n == 9) {
          st = 0;
          uint8_t pid = pid_of(ID_RED);
          if (lin_sum(pid, buf, 8) == buf[8] || lin_sum(0, buf, 8) == buf[8]) {
            float fl = (buf[0] | (buf[1] << 8)) / 10.0f, hw = (buf[2] | (buf[3] << 8)) / 10.0f;
            bool ch = fabsf(fl - s_fluid) >= 0.5f || fabsf(hw - s_hot) >= 0.5f || isnan(s_fluid);
            s_fluid = fl;
            s_hot = hw;
            s_red_ms = now;
            if (ch) s_ver++;
          }
        }
        break;
    }
  }
}

// ---------------------------------------------------------------- Ablauf
static void task(void *) {
  vTaskDelay(pdMS_TO_TICKS(1000));
  send_registration();
  uint32_t next = millis();
  for (;;) {
    int32_t wait = (int32_t)(next - millis());
    if (wait > 0) vTaskDelay(pdMS_TO_TICKS(wait));
    next = millis() + CYCLE_MS;
    // Befehl: aus dem letzten Infoblock die nicht veraenderten Werte uebernehmen
    bool send = false;
    uint8_t ctl[8] = {0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
    portENTER_CRITICAL(&s_mux);
    if (s_cmd && s_has) {
      s_cmd = false;
      s_cmd_left = 3;
      s_cmd_ms = millis();
    }
    if (s_cmd_left && s_has) {
      s_cmd_left--;
      send = true;
      ctl[3] = (s_sp1 & 0x3F) | (s_gas ? 0x40 : 0);
      ctl[4] = (s_info[4] & 0x3F) | ((s_el & 3) << 6);   // Zone 2 bleibt wie sie ist
      ctl[5] = (s_on ? 0x01 : 0) | ((s_water & 3) << 3) | (s_info[5] & 0x40);
    }
    portEXIT_CRITICAL(&s_mux);
    if (send) {
      send_frame(ID_CTRL, ctl, false);
      vTaskDelay(pdMS_TO_TICKS(5));
    }
    // Infoblock abfragen
    uint8_t pid = pid_of(ID_INFO), d[8];
    send_header(pid);
    if (read_response(pid, d, 150)) {
      bool changed;
      portENTER_CRITICAL(&s_mux);
      changed = !s_has || memcmp(d, s_info, 8) != 0;
      memcpy(s_info, d, 8);
      s_has = true;
      s_info_ms = millis();
      portEXIT_CRITICAL(&s_mux);
      if (s_fail >= 6) changed = true;   // Verbindung wieder da
      s_fail = 0;
      if (changed) s_ver++;
    } else if (++s_fail == 6) {
      s_ver++;   // drei Sekunden keine Antwort: als getrennt anzeigen
      send_registration();
    }
  }
}

static bool start_uart(uart_port_t port, int rx, int tx, int baud) {
  uart_config_t cfg = {};
  cfg.baud_rate = baud;
  cfg.data_bits = UART_DATA_8_BITS;
  cfg.parity = UART_PARITY_DISABLE;
  cfg.stop_bits = UART_STOP_BITS_1;
  cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  cfg.source_clk = UART_SCLK_DEFAULT;
  if (uart_driver_install(port, 512, 0, 0, nullptr, 0) != ESP_OK) return false;
  if (uart_param_config(port, &cfg) != ESP_OK) return false;
  if (uart_set_pin(port, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) return false;
  uart_set_rx_full_threshold(port, 1);
  uart_set_rx_timeout(port, 1);
  return true;
}

// ---------------------------------------------------------------- oeffentliche API
void alde_begin() {
  if (ALDE_LIN_RX < 0 || ALDE_LIN_TX < 0) {
    log_w("Alde: LIN-Pins nicht gesetzt ([alde] lin_rx/lin_tx in platformio.ini)");
    return;
  }
  if (ALDE_LIN_EN >= 0) {
    gpio_set_direction((gpio_num_t)ALDE_LIN_EN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)ALDE_LIN_EN, 1);
  }
  if (!start_uart(U, ALDE_LIN_RX, ALDE_LIN_TX, ALDE_BAUD)) {
    log_e("Alde: UART%d liess sich nicht einrichten", (int)U);
    return;
  }
  s_running = true;
  xTaskCreatePinnedToCore(task, "alde", 4096, nullptr, 10, nullptr, 0);
  if (ALDE_RED_RX >= 0 && ALDE_MODEL != 3020) {
    if (start_uart((uart_port_t)ALDE_RED_UART, ALDE_RED_RX, UART_PIN_NO_CHANGE, 9600))
      xTaskCreatePinnedToCore(red_task, "alde_red", 3072, nullptr, 9, nullptr, 0);
  }
  log_i("Alde %d: LIN auf UART%d (RX %d, TX %d), %d Baud", ALDE_MODEL, (int)U, ALDE_LIN_RX, ALDE_LIN_TX, ALDE_BAUD);
}

uint32_t alde_version() { return s_ver; }

bool alde_get(TrumaHeatInfo &o) {
  o = TrumaHeatInfo();
  o.key = TRUMA_HEAT_KEY_ALDE;
  o.source = ALDE_MODEL == 3020 ? "Alde Compact 3020" : ALDE_MODEL == 3030 ? "Alde Compact 3030" : "Alde Compact 3030 Plus";
  o.mode_count = 0;
  o.water_boost = true;
  o.water_hot = false;
  o.energy_writable = true;
  o.energy_style = HES_ALDE;
  uint8_t d[8];
  uint32_t now = millis();
  portENTER_CRITICAL(&s_mux);
  memcpy(d, s_info, 8);
  bool has = s_has, busy = s_cmd || s_cmd_left;
  uint32_t ms = s_info_ms;
  portEXIT_CRITICAL(&s_mux);
  o.connected = has && now - ms < 3000;
  o.has = has;
  if (has) {
    o.room_actual = temp_of(d[0]);
    o.outdoor = temp_of(d[2]);
    o.room_target = (d[3] & 0x3F) * 0.5f + 5.0f;
    o.heat_on = d[5] & 0x01;
    bool gas = d[3] & 0x40;
    uint8_t el = d[4] >> 6;
    o.el_power = el * 1000;
    o.energy = gas && el ? TE_MIX : el ? TE_ELECTRIC : gas ? TE_GAS : TE_NONE;
    uint8_t w = (d[5] >> 3) & 3;
    o.water = w == 0 ? TW_OFF : w == 2 ? TW_BOOST : TW_ECO;
    o.running = (d[3] & 0x80) || (d[5] & 0x80) || el;   // Gasventil offen, Pumpe laeuft oder Heizstab an
    o.error = (d[5] & 0x04) ? 1 : 0;
    o.age_s = (now - ms) / 1000;
  }
  if (!isnan(s_hot) && now - s_red_ms < 10000) {
    o.water_actual = s_hot;
    o.fluid = s_fluid;
  }
  o.busy = busy;
  if (!s_running) o.status = "LIN-Pins nicht eingestellt";
  else if (!o.connected) o.status = "Keine Antwort vom Alde-Kessel (LIN)";
  else if (o.error) o.status = "Störung am Alde-Kessel";
  else if (busy) o.status = "Befehl wird übertragen …";
  else o.status = "OK";
  return true;
}

// Befehl vorbereiten: vom letzten Infoblock ausgehen (oder vom noch nicht gesendeten Befehl)
static void prepare() {
  if (!s_cmd && !s_cmd_left) {
    s_sp1 = s_info[3] & 0x3F;
    s_gas = (s_info[3] & 0x40) ? 1 : 0;
    s_el = s_info[4] >> 6;
    s_on = s_info[5] & 0x01;
    s_water = (s_info[5] >> 3) & 3;
  }
}

void alde_set_heat(bool on, float t) {
  portENTER_CRITICAL(&s_mux);
  prepare();
  s_on = on;
  if (on) {
    t = t < 5 ? 5 : t > 30 ? 30 : t;
    s_sp1 = (uint8_t)lroundf((t - 5.0f) / 0.5f);
  }
  s_cmd = true;
  portEXIT_CRITICAL(&s_mux);
  s_ver++;
}

void alde_set_water(TrumaWater w) {
  portENTER_CRITICAL(&s_mux);
  prepare();
  s_water = w == TW_OFF ? 0 : w == TW_BOOST ? 2 : 1;
  s_cmd = true;
  portEXIT_CRITICAL(&s_mux);
  s_ver++;
}

void alde_set_gas(bool on) {
  portENTER_CRITICAL(&s_mux);
  prepare();
  s_gas = on;
  s_cmd = true;
  portEXIT_CRITICAL(&s_mux);
  s_ver++;
}

void alde_set_electric(uint8_t kw) {
  portENTER_CRITICAL(&s_mux);
  prepare();
  s_el = kw > 3 ? 3 : kw;
  s_cmd = true;
  portEXIT_CRITICAL(&s_mux);
  s_ver++;
}

#endif
