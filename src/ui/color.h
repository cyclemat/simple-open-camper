// Farbumrechnungen fuer die Darstellung des tatsaechlichen Lampenlichts
#pragma once
#include <lvgl.h>
#include <math.h>
#include "../z2m.h"

static inline lv_color_t kelvin_color(float k) {
  float t = k / 100.f, r, g, b;
  if (t <= 66) {
    r = 255;
    g = 99.4708025861f * logf(t) - 161.1195681661f;
    b = t <= 19 ? 0 : 138.5177312231f * logf(t - 10) - 305.0447927307f;
  } else {
    r = 329.698727446f * powf(t - 60, -0.1332047592f);
    g = 288.1221695283f * powf(t - 60, -0.0755148492f);
    b = 255;
  }
  auto c = [](float v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); };
  return lv_color_make(c(r), c(g), c(b));
}

static inline lv_color_t mired_color(int mired) { return kelvin_color(1000000.f / (mired > 0 ? mired : 370)); }

static inline void xy_to_rgb(float x, float y, uint8_t &R, uint8_t &G, uint8_t &B) {
  if (y <= 0.0001f) y = 0.0001f;
  float z = 1.f - x - y, Y = 1.f, X = Y / y * x, Z = Y / y * z;
  float r = X * 1.656492f - Y * 0.354851f - Z * 0.255038f;
  float g = -X * 0.707196f + Y * 1.655397f + Z * 0.036152f;
  float b = X * 0.051713f - Y * 0.121364f + Z * 1.011530f;
  float m = fmaxf(r, fmaxf(g, b));
  if (m > 1) { r /= m; g /= m; b /= m; }
  auto gam = [](float v) {
    v = v < 0 ? 0 : v;
    v = v <= 0.0031308f ? 12.92f * v : 1.055f * powf(v, 1.f / 2.4f) - 0.055f;
    return (uint8_t)(v > 1 ? 255 : v * 255);
  };
  R = gam(r); G = gam(g); B = gam(b);
}

// Liefert Farbton/Saettigung des aktuellen Lichts (fuer das Farbrad), false wenn unbekannt
static inline bool light_hs(const ControlView &v, float &h, float &s) {
  if (v.hue >= 0 && v.sat >= 0) { h = v.hue; s = v.sat; return true; }
  if (v.cx >= 0 && v.cy >= 0) {
    uint8_t r, g, b;
    xy_to_rgb(v.cx, v.cy, r, g, b);
    lv_color_hsv_t hsv = lv_color_rgb_to_hsv(r, g, b);
    h = hsv.h; s = hsv.s;
    return true;
  }
  return false;
}

// Darstellungsfarbe einer eingeschalteten Kachel
static inline lv_color_t light_display_color(const ControlView &v) {
  if (v.kind == CTL_SWITCH) return lv_color_hex(0x3C8DFF);
  bool colormode = v.cmode == "xy" || v.cmode == "hs";
  float h, s;
  if (v.color && (colormode || !v.has_ct) && light_hs(v, h, s) && s > 8)
    return lv_color_hsv_to_rgb((uint16_t)h % 360, (uint8_t)fminf(100, s * 0.9f + 8), 100);
  if (v.has_ct && v.ct > 0) {
    lv_color_t k = mired_color(v.ct);
    return lv_color_mix(k, lv_color_hex(0xF3A43B), 200);  // etwas Waerme, damit Weiss nicht grell wirkt
  }
  return lv_color_hex(0xF3A43B);
}
