#include "power.h"
#include "board.h"
#include <Arduino.h>

static const uint32_t SAMPLE_MS = 5000;
static const int HIST = 36;                 // 36 x 5 s = 3 Minuten Verlauf
static float s_hist[HIST];
static int s_n = 0, s_pos = 0;
static float s_ema = 0;
static uint32_t s_last = 0;
static PowerInfo s_info;

// Ruhespannung -> Ladestand (typische Li-Ionen-Kurve)
static int pct_of(float v) {
  static const float V[] = {3.30f, 3.50f, 3.60f, 3.70f, 3.80f, 3.90f, 4.00f, 4.10f, 4.20f};
  static const int P[] = {0, 8, 18, 38, 58, 72, 84, 94, 100};
  if (v <= V[0]) return 0;
  for (int i = 1; i < 9; i++)
    if (v < V[i]) return P[i - 1] + (int)((v - V[i - 1]) / (V[i] - V[i - 1]) * (P[i] - P[i - 1]));
  return 100;
}

void power_tick() {
  uint32_t now = millis();
  if (s_last && now - s_last < SAMPLE_MS) return;
  s_last = now;
  float v = board_battery_voltage();
  if (v < 2.5f || v > 5.5f) {
    s_info = PowerInfo();
    s_info.src = PWR_NO_BATTERY;
    s_n = 0;
    return;
  }
  s_ema = s_n ? s_ema + 0.3f * (v - s_ema) : v;
  s_hist[s_pos] = s_ema;
  s_pos = (s_pos + 1) % HIST;
  if (s_n < HIST) s_n++;
  s_info.has_battery = true;
  s_info.volt = s_ema;
  s_info.percent = pct_of(s_ema);
  if (s_n < HIST) {
    // Erste drei Minuten: nur grob einordnen
    if (s_ema >= 4.15f) s_info.src = PWR_USB_FULL;
    return;
  }
  float oldest = s_hist[s_pos];   // aeltester Wert im Ring
  float slope = s_ema - oldest;   // Volt in 3 Minuten
  if (slope > 0.015f) s_info.src = PWR_CHARGING;
  else if (slope < -0.010f) s_info.src = PWR_BATTERY;
  else if (s_ema >= 4.12f) s_info.src = PWR_USB_FULL;
  else if (s_info.src == PWR_UNKNOWN || s_info.src == PWR_USB_FULL) s_info.src = PWR_BATTERY;
  // sonst bleibt die letzte Einschaetzung (Spannung aendert sich kaum)
}

PowerInfo power_get() { return s_info; }

const char *power_src_text(PowerSrc s) {
  switch (s) {
    case PWR_NO_BATTERY: return "kein Akku";
    case PWR_CHARGING: return "USB, lädt";
    case PWR_USB_FULL: return "USB, Akku voll";
    case PWR_BATTERY: return "Akkubetrieb";
    default: return "wird ermittelt …";
  }
}
