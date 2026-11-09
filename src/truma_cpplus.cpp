// Truma CP Plus ueber den LIN-Bus – das Geraet antwortet als "iNet Box".
// Protokoll nach den offenen Nachbauten (danielfett/inetbox.py, Fabian-Schmidt/esphome-truma_inetbox), eigene Umsetzung.
//
// LIN: 9600 Baud, das CP Plus ist Master. Jeder Rahmen: Break, 0x55, PID (6 Bit ID + 2 Paritaetsbits).
//   0x18  Abfrage "hast du etwas?" – Antwort 8 Byte, Byte 0 = 0xFF (ja) bzw. 0xFE (nein), erweiterte Pruefsumme
//   0x3C  Diagnoseanfrage des CP Plus (NAD, PCI, Dienst …), klassische Pruefsumme
//   0x3D  Diagnoseantwort – wir senden, wenn wir einen Rahmen in der Warteschlange haben
// Dienste: B0 NAD zuweisen, B2 Kennung lesen, B9 Lebenszeichen, BA Statuspuffer abholen, BB Statuspuffer schreiben.
// Statuspuffer: 00 1F | 00 1E 00 00 22 FF FF FF 54 01 | LEN TYP ZAEHLER PRUEFSUMME | DATEN
// Temperaturen: (°C + 273) * 10, 16 Bit little endian, 0 = aus.
#include "truma_cpplus.h"
#include "i18n/i18n.h"

#if !WITH_TRUMA_CPPLUS
void cpplus_begin() {}
uint32_t cpplus_version() { return 0; }
bool cpplus_get(TrumaHeatInfo &) { return false; }
void cpplus_set_heat(bool, float, uint8_t) {}
void cpplus_set_water(TrumaWater) {}
void cpplus_set_energy(TrumaEnergy, uint16_t) {}
void cpplus_sync_clock() {}
#else

#include <Arduino.h>
#include <atomic>
#include <time.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const uart_port_t LIN_UART = (uart_port_t)TRUMA_LIN_UART;
static const uint8_t NAD_BROADCAST = 0x7F;
static const uint8_t PREAMBLE[12] = {0x00, 0x1F, 0x00, 0x1E, 0x00, 0x00, 0x22, 0xFF, 0xFF, 0xFF, 0x54, 0x01};
static const uint8_t T_INIT_REQ = 0x0A, T_DEVICE = 0x0B, T_ACK = 0x0D, T_CLOCK_W = 0x14, T_CLOCK = 0x15,
                     T_HEATER_W = 0x32, T_HEATER = 0x33;
static const uint8_t MODE_OFF = 0x00, MODE_ECO = 0x01, MODE_HIGH = 0x0A, MODE_BOOST = 0x0B;

// ---------------------------------------------------------------- Zustand
struct HeaterStatus {
  uint16_t room_t = 0, mode = 0, el_a = 0, water_t = 0, el_b = 0;
  uint8_t mix_a = 0, mix_b = 0;
  uint16_t water_cur = 0, room_cur = 0;
  uint8_t op = 0, err_lo = 0, err_hi = 0;
};
struct HeaterCmd {
  uint16_t room_t = 0, mode = 0, el_a = 0, water_t = 0, el_b = 0;
  uint8_t mix_a = 0, mix_b = 0;
};

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static std::atomic<uint32_t> s_ver{1};
static bool s_running = false;           // UART laeuft
static uint8_t s_nad = 0x03;
// vom CP Plus
static HeaterStatus s_st;
static bool s_has_st = false;
static uint32_t s_st_ms = 0;
static uint16_t s_last_room_t = 0;      // letzte eingestellte Raumtemperatur (fuer "Heizung an")
static uint8_t s_clock_mode = 0;
static uint8_t s_dev_count = 0, s_dev_mask = 0, s_heater_type = 0;
static bool s_inited = false;
static uint32_t s_init_ms = 0;
static volatile uint32_t s_alive_ms = 0;
static bool s_was_alive = false;
// an das CP Plus
static HeaterCmd s_cmd;
static bool s_cmd_pending = false;
static uint32_t s_cmd_sent_ms = 0;      // 0 = keine Bestaetigung offen
static bool s_clock_pending = false;
static uint32_t s_clock_next_ms = 0;
static uint8_t s_counter = 1;
static char s_note[64] = "";            // letzte Meldung (z. B. Ablehnung)

// Antworten fuer 0x3D (nur vom LIN-Task benutzt)
static uint8_t s_q[24][8];
static uint8_t s_q_head = 0, s_q_n = 0;
// Zusammensetzen mehrteiliger Anfragen
static uint8_t s_rx[128];
static size_t s_rx_len = 0, s_rx_need = 0;
static uint8_t s_rx_seq = 0;

static uint16_t temp_code(float c) { return (uint16_t)lroundf((c + 273.0f) * 10.0f); }
static float temp_of(uint16_t code) { return code ? code / 10.0f - 273.0f : NAN; }

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

static uint16_t u16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static void put16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }

// ---------------------------------------------------------------- Senden auf dem Bus
static int64_t s_echo_until = 0;
static int s_echo_left = 0;

static void lin_answer(const uint8_t *d8, uint8_t pid, bool enhanced) {
  uint8_t f[9];
  memcpy(f, d8, 8);
  f[8] = lin_sum(enhanced ? pid : 0, d8, 8);
  uart_write_bytes(LIN_UART, (const char *)f, 9);
  // Der Transceiver liefert das Gesendete zurueck – kurz ignorieren (falls er kein Echo hat, verfaellt das)
  s_echo_left = 9;
  s_echo_until = esp_timer_get_time() + 9 * 1250 + 3000;
}

static void q_clear() { s_q_head = s_q_n = 0; }
static void q_push(const uint8_t *f) {
  if (s_q_n >= 24) return;
  memcpy(s_q[(s_q_head + s_q_n) % 24], f, 8);
  s_q_n++;
}

static void diag_single(const uint8_t *payload, size_t n) {
  uint8_t f[8];
  memset(f, 0xFF, 8);
  f[0] = s_nad;
  f[1] = n;
  memcpy(f + 2, payload, n);
  q_push(f);
}

// Antwort beliebiger Laenge (SID + Daten) als Einzel- oder Erst-/Folgerahmen
static void diag_send(const uint8_t *msg, size_t n) {
  q_clear();
  if (n <= 6) {
    diag_single(msg, n);
    return;
  }
  uint8_t f[8];
  f[0] = s_nad;
  f[1] = 0x10 | ((n >> 8) & 0x0F);
  f[2] = n & 0xFF;
  memcpy(f + 3, msg, 5);
  q_push(f);
  size_t pos = 5;
  uint8_t seq = 1;
  while (pos < n) {
    memset(f, 0xFF, 8);
    f[0] = s_nad;
    f[1] = 0x20 | (seq & 0x0F);
    size_t k = n - pos < 6 ? n - pos : 6;
    memcpy(f + 2, msg + pos, k);
    q_push(f);
    pos += k;
    seq++;
  }
}

// Statuspuffer an das CP Plus (Antwort auf BA)
static void send_buffer(uint8_t type, const uint8_t *data, uint8_t len) {
  uint8_t m[1 + 12 + 4 + 32];
  size_t n = 0;
  m[n++] = 0xBA | 0x40;
  memcpy(m + n, PREAMBLE, 12);
  n += 12;
  uint8_t cnt = s_counter++;
  if (!s_counter) s_counter = 1;
  uint8_t hdr[5] = {0x54, 0x01, len, type, cnt};
  uint8_t tmp[5 + 32];
  memcpy(tmp, hdr, 5);
  memcpy(tmp + 5, data, len);
  m[n++] = len;
  m[n++] = type;
  m[n++] = cnt;
  m[n++] = lin_sum(0, tmp, 5 + len);
  memcpy(m + n, data, len);
  n += len;
  diag_send(m, n);
}

// ---------------------------------------------------------------- Empfang vom CP Plus
static void handle_buffer(const uint8_t *p, size_t n) {
  // p zeigt hinter die Dienstkennung BB
  if (n < 16) return;
  uint8_t len = p[12], type = p[13], chk = p[15];
  if (n < (size_t)16 + len) return;
  const uint8_t *d = p + 16;
  uint8_t tmp[5 + 64];
  if (len > 64) return;
  tmp[0] = p[10]; tmp[1] = p[11]; tmp[2] = len; tmp[3] = type; tmp[4] = p[14];
  memcpy(tmp + 5, d, len);
  if (lin_sum(0, tmp, 5 + len) != chk) {
    log_w("CP Plus: Pruefsumme falsch (Typ 0x%02X)", type);
    return;
  }
  bool changed = false;
  portENTER_CRITICAL(&s_mux);
  if (type == T_HEATER && len >= 20) {
    HeaterStatus st;
    st.room_t = u16(d); st.mode = u16(d + 2); st.el_a = u16(d + 4); st.water_t = u16(d + 6); st.el_b = u16(d + 8);
    st.mix_a = d[10]; st.mix_b = d[11]; st.water_cur = u16(d + 12); st.room_cur = u16(d + 14);
    st.op = d[16]; st.err_lo = d[17]; st.err_hi = d[18];
    changed = !s_has_st || memcmp(&st, &s_st, sizeof(st)) != 0;
    s_st = st;
    s_has_st = true;
    s_st_ms = millis();
    if (st.room_t) s_last_room_t = st.room_t;
    if (s_cmd_sent_ms && !s_cmd_pending) { s_cmd_sent_ms = 0; s_note[0] = 0; changed = true; }
  } else if (type == T_DEVICE && len >= 12) {
    s_dev_count = d[0];
    if (d[1] < 8) s_dev_mask |= 1 << d[1];
    if (d[1] == 1) s_heater_type = d[7];
    int seen = __builtin_popcount(s_dev_mask);
    if (!s_inited && s_dev_count >= 2 && seen >= s_dev_count && (s_dev_mask & 0x02)) {
      s_inited = true;
      s_init_ms = millis();
      s_clock_next_ms = millis() + 30000;   // nach der Anmeldung einmal die Uhr stellen
      changed = true;
    }
  } else if (type == T_CLOCK && len >= 10) {
    s_clock_mode = d[6];
  } else if (type == T_ACK && len >= 1) {
    if (d[0] != 0) {
      s_cmd_pending = false;
      s_cmd_sent_ms = 0;
      snprintf(s_note, sizeof(s_note), tr("CP Plus hat den Befehl abgelehnt (Code %u)"), d[0]);
      changed = true;
    }
  }
  portEXIT_CRITICAL(&s_mux);
  if (changed) s_ver++;
}

static void handle_ba() {
  uint8_t data[16];
  portENTER_CRITICAL(&s_mux);
  bool inited = s_inited, cmd = s_cmd_pending, clk = s_clock_pending;
  HeaterCmd c = s_cmd;
  if (inited && cmd) {
    s_cmd_pending = false;
    s_cmd_sent_ms = millis();
  } else if (inited && clk) {
    s_clock_pending = false;
  }
  portEXIT_CRITICAL(&s_mux);
  if (!inited) {
    send_buffer(T_INIT_REQ, data, 0);
  } else if (cmd) {
    put16(data, c.room_t); put16(data + 2, c.mode); put16(data + 4, c.el_a);
    put16(data + 6, c.water_t); put16(data + 8, c.el_b);
    data[10] = c.mix_a; data[11] = c.mix_b;
    send_buffer(T_HEATER_W, data, 12);
    s_ver++;
  } else if (clk) {
    time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    uint8_t d[10] = {(uint8_t)tm.tm_hour, (uint8_t)tm.tm_min, (uint8_t)tm.tm_sec, 1, 1, 0, s_clock_mode, 0, 0, 0};
    send_buffer(T_CLOCK_W, d, 10);
    log_i("CP Plus: Uhr gestellt auf %02d:%02d", tm.tm_hour, tm.tm_min);
  }
}

static void handle_request(const uint8_t *m, size_t n, uint8_t nad) {
  if (!n) return;
  static const uint8_t IDENT[4] = {0x17, 0x46, 0x00, 0x1F};   // Truma, iNet Box
  uint8_t sid = m[0];
  if (sid == 0xB0 && n >= 6 && !memcmp(m + 1, IDENT, 4)) {
    uint8_t r[1] = {0xF0};
    diag_send(r, 1);      // Antwort noch mit der alten Adresse
    s_nad = m[5];
  } else if (sid == 0xB2 && n >= 6 && !memcmp(m + 2, IDENT, 4)) {
    uint8_t id = m[1];
    if (id == 0x00) { uint8_t r[6] = {0xF2, 0x17, 0x46, 0x00, 0x1F, 0x01}; diag_send(r, 6); }
    else if (id == 0x20) { uint8_t r[6] = {0xF2, 0x17, 0x46, 0x00, 0x00, 0x00}; diag_send(r, 6); }
    else if (id == 0x22) { uint8_t r[6] = {0xF2, 0x00, 0x00, 0x00, 0x00, 0x00}; diag_send(r, 6); }
    else { uint8_t r[3] = {0x7F, 0xB2, 0x12}; diag_send(r, 3); }
  } else if (sid == 0xB9 && nad == s_nad) {
    uint8_t r[2] = {0xF9, 0x00};
    diag_send(r, 2);
  } else if (sid == 0xBA && nad == s_nad) {
    handle_ba();
  } else if (sid == 0xBB && nad == s_nad) {
    handle_buffer(m + 1, n - 1);
    uint8_t r[1] = {0xFB};
    diag_send(r, 1);
  }
}

static void handle_diag(const uint8_t *f) {
  uint8_t nad = f[0], pci = f[1];
  if (nad != s_nad && nad != NAD_BROADCAST) {
    s_rx_need = 0;     // Anfrage an ein anderes Geraet (z. B. die Heizung)
    return;
  }
  uint8_t kind = pci >> 4;
  if (kind == 0) {
    size_t len = pci & 0x0F;
    if (len > 6) return;
    handle_request(f + 2, len, nad);
  } else if (kind == 1) {
    s_rx_need = ((pci & 0x0F) << 8) | f[2];
    if (s_rx_need > sizeof(s_rx)) { s_rx_need = 0; return; }
    memcpy(s_rx, f + 3, 5);
    s_rx_len = 5;
    s_rx_seq = 1;
  } else if (kind == 2 && s_rx_need) {
    if ((pci & 0x0F) != (s_rx_seq & 0x0F)) { s_rx_need = 0; return; }
    s_rx_seq++;
    size_t k = s_rx_need - s_rx_len < 6 ? s_rx_need - s_rx_len : 6;
    memcpy(s_rx + s_rx_len, f + 2, k);
    s_rx_len += k;
    if (s_rx_len >= s_rx_need) {
      size_t n = s_rx_need;
      s_rx_need = 0;
      handle_request(s_rx, n, nad);
    }
  }
}

// ---------------------------------------------------------------- LIN-Task
static void periodic() {
  uint32_t now = millis();
  bool alive = s_alive_ms && now - s_alive_ms < 10000;
  if (alive != s_was_alive) {
    s_was_alive = alive;
    if (!alive) {
      portENTER_CRITICAL(&s_mux);
      s_inited = false;      // nach einer Unterbrechung meldet sich das CP Plus neu an
      s_dev_mask = 0;
      portEXIT_CRITICAL(&s_mux);
    }
    s_ver++;
  }
  if (s_inited && s_clock_next_ms && (int32_t)(now - s_clock_next_ms) >= 0) {
    s_clock_next_ms = now + 24UL * 3600 * 1000;   // einmal am Tag nachstellen
    if (time(nullptr) > 1700000000) s_clock_pending = true;
  }
  // Ohne neuen Status nach 30 s gilt der Befehl als verloren
  if (s_cmd_sent_ms && now - s_cmd_sent_ms > 30000) {
    s_cmd_sent_ms = 0;
    snprintf(s_note, sizeof(s_note), "Keine Rückmeldung vom CP Plus");
    s_ver++;
  }
}

static void lin_task(void *) {
  enum { S_BREAK, S_SYNC, S_PID, S_DATA } st = S_BREAK;
  uint8_t id = 0, pid = 0, buf[9];
  int n = 0;
  int64_t last = 0, last_periodic = 0;
  for (;;) {
    uint8_t b;
    int r = uart_read_bytes(LIN_UART, &b, 1, pdMS_TO_TICKS(20));
    int64_t now = esp_timer_get_time();
    if (now - last_periodic > 1000000) {
      last_periodic = now;
      periodic();
    }
    if (r <= 0) {
      st = S_BREAK;
      continue;
    }
    if (s_echo_left > 0) {
      if (now < s_echo_until) {
        s_echo_left--;
        last = now;
        continue;
      }
      s_echo_left = 0;
    }
    if (st != S_BREAK && now - last > 6000) st = S_BREAK;   // Luecke im Rahmen
    last = now;
    switch (st) {
      case S_BREAK:
        if (b == 0x00) st = S_SYNC;
        else if (b == 0x55) st = S_PID;
        break;
      case S_SYNC:
        st = b == 0x55 ? S_PID : (b == 0x00 ? S_SYNC : S_BREAK);
        break;
      case S_PID:
        if (pid_of(b) != b) { st = S_BREAK; break; }
        pid = b;
        id = b & 0x3F;
        if (id == 0x18) {
          s_alive_ms = millis();
          uint8_t d[8];
          memset(d, 0xFF, 8);
          bool upd = !s_inited || s_cmd_pending || s_clock_pending;
          d[0] = upd ? 0xFF : 0xFE;
          lin_answer(d, pid, true);
          st = S_BREAK;
        } else if (id == 0x3D && s_q_n) {
          lin_answer(s_q[s_q_head], pid, false);
          s_q_head = (s_q_head + 1) % 24;
          s_q_n--;
          st = S_BREAK;
        } else {
          n = 0;
          st = S_DATA;
        }
        break;
      case S_DATA:
        buf[n++] = b;
        if (n == 9) {
          st = S_BREAK;
          if (id == 0x3C && lin_sum(0, buf, 8) == buf[8]) {
            s_alive_ms = millis();
            handle_diag(buf);
          }
        }
        break;
    }
  }
}

// ---------------------------------------------------------------- oeffentliche API
void cpplus_begin() {
  if (TRUMA_LIN_RX < 0 || TRUMA_LIN_TX < 0) {
    log_w("CP Plus: LIN-Pins nicht gesetzt (TRUMA_LIN_RX/TRUMA_LIN_TX)");
    return;
  }
  if (TRUMA_LIN_EN >= 0) {
    gpio_set_direction((gpio_num_t)TRUMA_LIN_EN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)TRUMA_LIN_EN, 1);
  }
  uart_config_t cfg = {};
  cfg.baud_rate = 9600;
  cfg.data_bits = UART_DATA_8_BITS;
  cfg.parity = UART_PARITY_DISABLE;
  cfg.stop_bits = UART_STOP_BITS_1;
  cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  cfg.source_clk = UART_SCLK_DEFAULT;
  if (uart_driver_install(LIN_UART, 512, 0, 0, nullptr, 0) != ESP_OK || uart_param_config(LIN_UART, &cfg) != ESP_OK ||
      uart_set_pin(LIN_UART, TRUMA_LIN_TX, TRUMA_LIN_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
    log_e("CP Plus: UART %d liess sich nicht einrichten", (int)LIN_UART);
    return;
  }
  // Jedes Byte sofort weiterreichen – die Antwort muss kurz nach dem PID auf dem Bus liegen
  uart_set_rx_full_threshold(LIN_UART, 1);
  uart_set_rx_timeout(LIN_UART, 1);
  s_running = true;
  xTaskCreatePinnedToCore(lin_task, "lin", 4096, nullptr, 18, nullptr, 0);
  log_i("CP Plus: LIN auf UART%d (RX %d, TX %d)", (int)LIN_UART, TRUMA_LIN_RX, TRUMA_LIN_TX);
}

uint32_t cpplus_version() { return s_ver; }

bool cpplus_get(TrumaHeatInfo &o) {
  o = TrumaHeatInfo();
  o.key = TRUMA_HEAT_KEY_CPPLUS;
  o.source = "Truma CP Plus";
  o.mode_names[0] = "Eco";
  o.mode_names[1] = "Hoch";
  o.mode_names[2] = "Boost";
  o.mode_count = 3;
  o.water_boost = true;
  o.energy_writable = true;
  uint32_t now = millis();
  portENTER_CRITICAL(&s_mux);
  HeaterStatus st = s_st;
  bool has = s_has_st, inited = s_inited, pending = s_cmd_pending || s_cmd_sent_ms;
  uint32_t st_ms = s_st_ms;
  uint16_t last_room = s_last_room_t;
  std::string note = s_note;
  portEXIT_CRITICAL(&s_mux);
  o.connected = s_alive_ms && now - s_alive_ms < 10000;
  o.has = has;
  if (has) {
    o.heat_on = st.room_t != 0 && st.mode != MODE_OFF;
    o.room_target = temp_of(st.room_t ? st.room_t : last_room);
    o.room_actual = temp_of(st.room_cur);
    o.heat_mode = st.mode == MODE_HIGH ? 1 : st.mode == MODE_BOOST ? 2 : 0;
    o.running = st.op >= 4;
    float wt = temp_of(st.water_t);
    o.water = isnan(wt) ? TW_OFF : wt < 50 ? TW_ECO : wt < 70 ? TW_HOT : TW_BOOST;
    o.water_actual = temp_of(st.water_cur);
    o.energy = st.mix_a <= 3 ? (TrumaEnergy)st.mix_a : TE_GAS;
    o.el_power = st.el_a;
    o.error = st.err_hi * 100 + st.err_lo;
    o.age_s = (now - st_ms) / 1000;
  }
  o.busy = pending;
  char b[64];
  if (!s_running) o.status = "LIN-Pins nicht eingestellt";
  else if (!o.connected) o.status = "Keine Verbindung zum CP Plus (LIN)";
  else if (!inited) o.status = "Warte auf Anmeldung – am CP Plus „PR SET“ → Init ausführen";
  else if (!has) o.status = "Warte auf Status …";
  else if (!note.empty()) o.status = note;
  else if (pending) o.status = "Befehl wird übertragen …";
  else if (o.error) { snprintf(b, sizeof(b), tr("Störung, Code %d"), o.error); o.status = b; }
  else o.status = "OK";
  return true;
}

// Befehl auf Grundlage des letzten Status (oder eines noch nicht abgeholten Befehls) vorbereiten
static bool prepare(HeaterCmd &c) {
  if (!s_has_st) return false;
  if (!s_cmd_pending) {
    c.room_t = s_st.room_t; c.mode = s_st.mode; c.el_a = s_st.el_a; c.water_t = s_st.water_t; c.el_b = s_st.el_b;
    c.mix_a = s_st.mix_a; c.mix_b = s_st.mix_b;
  } else {
    c = s_cmd;
  }
  return true;
}

static void commit(const HeaterCmd &c) {
  s_cmd = c;
  s_cmd_pending = true;
  s_note[0] = 0;
}

void cpplus_set_heat(bool on, float target_c, uint8_t mode) {
  portENTER_CRITICAL(&s_mux);
  HeaterCmd c;
  if (prepare(c)) {
    if (!on) {
      c.room_t = 0;
      c.mode = MODE_OFF;
    } else {
      float t = target_c < 5 ? 5 : target_c > 30 ? 30 : target_c;
      c.room_t = temp_code(t);
      c.mode = mode == 1 ? MODE_HIGH : mode == 2 ? MODE_BOOST : MODE_ECO;
      if (c.mix_a == TE_NONE) c.mix_a = c.mix_b = TE_GAS;
    }
    commit(c);
  }
  portEXIT_CRITICAL(&s_mux);
  s_ver++;
}

void cpplus_set_water(TrumaWater w) {
  portENTER_CRITICAL(&s_mux);
  HeaterCmd c;
  if (prepare(c)) {
    c.water_t = w == TW_ECO ? temp_code(40) : w == TW_HOT ? temp_code(60) : w == TW_BOOST ? temp_code(200) : 0;
    if (w != TW_OFF && c.mix_a == TE_NONE) c.mix_a = c.mix_b = TE_GAS;
    commit(c);
  }
  portEXIT_CRITICAL(&s_mux);
  s_ver++;
}

void cpplus_set_energy(TrumaEnergy e, uint16_t p) {
  portENTER_CRITICAL(&s_mux);
  HeaterCmd c;
  if (prepare(c)) {
    if (e == TE_GAS || e == TE_NONE) {
      c.mix_a = c.mix_b = TE_GAS;
      c.el_a = c.el_b = 0;
    } else {
      c.mix_a = c.mix_b = e;
      c.el_a = c.el_b = p >= 1800 ? 1800 : 900;
    }
    commit(c);
  }
  portEXIT_CRITICAL(&s_mux);
  s_ver++;
}

void cpplus_sync_clock() {
  if (time(nullptr) > 1700000000) s_clock_pending = true;
  s_ver++;
}

#endif
