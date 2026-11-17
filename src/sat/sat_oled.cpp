// Kleines OLED am Satelliten (siehe sat_oled.h). Laeuft im loop() des Satelliten, kein eigener Task.
#include "sat_oled.h"
#if SAT_OLED
#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>
#include "sat_config.h"
#include "sat_io.h"
#include "../sat_proto.h"

#ifndef SAT_OLED_ADDR
#define SAT_OLED_ADDR 0x3C
#endif

namespace {
// 0,42"-OLED: SSD1306 mit 72x40 sichtbaren Pixeln (U8g2 kennt den Versatz im Bildspeicher)
U8G2_SSD1306_72X40_ER_F_HW_I2C *s_u8 = nullptr;
bool s_ok = false;

struct OledCfg {
  bool on = true;
  String src;            // "" = automatisch, "level" = Wasserwaage, sonst Punkt-ID ("temp1", "tank2" …)
  bool rot = false;      // 180 Grad drehen
  uint8_t con = 128;     // Kontrast
};
OledCfg s_cfg;
uint32_t s_draw_ms = 0, s_page_ms = 0;
// OLED schonen: alle 20 min einmal ganz schwarz, dann ganz weiss (je 2 s) – gegen Einbrennen ungleich genutzter Pixel
const uint32_t CARE_EVERY_MS = 20UL * 60 * 1000, CARE_STEP_MS = 2000;
uint32_t s_care_ms = 0;
size_t s_page = 0;
const uint32_t PAGE_MS = 4000;   // automatisch: mehrere Sensoren nacheinander

void cfg_load() {
  Preferences p;
  if (!p.begin("satoled", true)) return;
  s_cfg.on = p.getBool("on", true);
  s_cfg.src = p.getString("src", "");
  s_cfg.rot = p.getBool("rot", false);
  s_cfg.con = p.getUChar("con", 128);
  p.end();
}
void cfg_save() {
  Preferences p;
  if (!p.begin("satoled", false)) return;
  p.putBool("on", s_cfg.on);
  p.putString("src", s_cfg.src);
  p.putBool("rot", s_cfg.rot);
  p.putUChar("con", s_cfg.con);
  p.end();
}
void apply_hw() {
  if (!s_ok) return;
  s_u8->setPowerSave(s_cfg.on ? 0 : 1);
  s_u8->setDisplayRotation(s_cfg.rot ? U8G2_R2 : U8G2_R0);
  s_u8->setContrast(s_cfg.con);
}

// Zahl mit Komma; Nachkommastellen nach Groesse (Temperatur 23,4 / Tank 75 / Spannung 12,84)
String fmt(float v, uint8_t cls) {
  int dec = cls == SC_LEVEL ? 0 : (fabsf(v) >= 100 ? 0 : 1);
  if (cls == SC_VOLTAGE && fabsf(v) < 100) dec = 2;
  char b[16];
  snprintf(b, sizeof(b), "%.*f", dec, v);
  for (char *c = b; *c; c++)
    if (*c == '.') *c = ',';
  return b;
}

// Text mittig (x) in der aktuellen Schrift
void center(int y, const char *t) {
  int w = s_u8->getUTF8Width(t);
  s_u8->drawUTF8(w >= 72 ? 0 : (72 - w) / 2, y, t);
}

// Ein Sensorwert: Name klein oben, Wert gross, Einheit klein dahinter
void draw_value(const SatPoint &p) {
  s_u8->setFont(u8g2_font_5x8_tf);
  String name = p.name.empty() ? String(p.id.c_str()) : String(p.name.c_str());
  center(7, name.c_str());
  if (p.kind == SP_INPUT || p.kind == SP_SWITCH) {
    s_u8->setFont(u8g2_font_logisoso16_tr);
    center(34, p.on ? "AN" : "AUS");
    return;
  }
  if (!p.ok || isnan(p.value)) {
    s_u8->setFont(u8g2_font_5x8_tf);
    center(24, p.status.empty() ? "kein Wert" : p.status.substr(0, 14).c_str());
    return;
  }
  String v = fmt(p.value, p.cls);
  String u = p.unit.c_str();
  s_u8->setFont(u8g2_font_5x8_tf);
  int uw = u.length() ? s_u8->getUTF8Width(u.c_str()) + 1 : 0;
  // groesste Schrift, die mit Einheit in 72 px passt
  const uint8_t *fonts[] = {u8g2_font_logisoso22_tr, u8g2_font_logisoso18_tr, u8g2_font_logisoso16_tr};
  int vw = 0;
  for (const uint8_t *f : fonts) {
    s_u8->setFont(f);
    vw = s_u8->getUTF8Width(v.c_str());
    if (vw + uw <= 72) break;
  }
  int x = (72 - vw - uw) / 2;
  if (x < 0) x = 0;
  s_u8->drawUTF8(x, 37, v.c_str());
  if (uw) {
    s_u8->setFont(u8g2_font_5x8_tf);
    s_u8->drawUTF8(x + vw + 1, 20, u.c_str());   // Einheit oben neben dem Wert
  }
}

// Wasserwaage in zwei Richtungen: Kreis mit Fadenkreuz, die Blase wandert zur hoeheren Seite (wie bei der Libelle).
// Waagrecht = quer, senkrecht = laengs. Richtung/Tausch der Achsen: MPU-6050-Einstellungen (Achsen tauschen/umkehren).
void draw_level(float lx, float qx) {
  const int cx = 19, cy = 20, r = 19;
  s_u8->drawCircle(cx, cy, r);
  s_u8->drawCircle(cx, cy, 5);
  s_u8->drawHLine(cx - r + 2, cy, 2 * r - 3);
  s_u8->drawVLine(cx, cy - r + 2, 2 * r - 3);
  const float scale = 3.0f;   // Pixel je Grad -> Rand bei ~4,5 Grad
  float dx = qx * scale, dy = -lx * scale;
  float d = sqrtf(dx * dx + dy * dy), lim = r - 5;
  if (d > lim) {
    dx *= lim / d;
    dy *= lim / d;
  }
  int bx = cx + (int)lroundf(dx), by = cy + (int)lroundf(dy);
  bool flat = fabsf(lx) < 0.3f && fabsf(qx) < 0.3f;
  if (flat) s_u8->drawDisc(bx, by, 4);   // eben: Blase ausgefuellt
  else {
    s_u8->setDrawColor(0);
    s_u8->drawDisc(bx, by, 4);
    s_u8->setDrawColor(1);
    s_u8->drawCircle(bx, by, 4);
  }
  char b[12];
  s_u8->setFont(u8g2_font_6x10_tf);
  snprintf(b, sizeof(b), "L%+.1f", lx);
  for (char *c = b; *c; c++) if (*c == '.') *c = ',';
  s_u8->drawUTF8(41, 11, b);
  snprintf(b, sizeof(b), "Q%+.1f", qx);
  for (char *c = b; *c; c++) if (*c == '.') *c = ',';
  s_u8->drawUTF8(41, 24, b);
  s_u8->setFont(u8g2_font_5x8_tf);
  s_u8->drawUTF8(41, 37, flat ? "eben" : "Grad");
}

// Ohne Sensor: Name und Adresse des Satelliten (zum Einrichten im Browser)
void draw_info() {
  s_u8->setFont(u8g2_font_5x8_tf);
  center(8, g_sat.name.length() ? g_sat.name.c_str() : "Satellit");
  String ip;
  if (WiFi.isConnected()) ip = WiFi.localIP().toString();
  else if (WiFi.getMode() & WIFI_AP) ip = WiFi.softAPIP().toString();
  center(22, WiFi.isConnected() ? "WLAN" : (WiFi.getMode() & WIFI_AP) ? "Einrichtung" : "kein WLAN");
  center(34, ip.length() ? ip.c_str() : "-");
}

bool is_value_point(const SatPoint &p) { return p.kind != SP_TILT && p.kind != SP_NONE; }

// Startbild: abwechselnd Camper und Wohnwagen (aus Linien gezeichnet, kein Bild im Flash), "S.O.C." darunter
uint32_t s_splash_until = 0;
void wheel(int x, int y, int r) {
  s_u8->setDrawColor(0);
  s_u8->drawDisc(x, y, r);
  s_u8->setDrawColor(1);
  s_u8->drawCircle(x, y, r);
  s_u8->drawDisc(x, y, 1);
}
void draw_camper() {
  s_u8->drawRFrame(9, 4, 42, 18, 3);          // Wohnaufbau
  s_u8->drawRFrame(49, 10, 13, 12, 3);        // Fahrerhaus
  s_u8->drawLine(55, 11, 60, 15);             // Frontscheibe
  s_u8->drawHLine(55, 15, 6);
  s_u8->drawFrame(13, 8, 9, 5);               // Fenster
  s_u8->drawFrame(25, 8, 9, 5);
  s_u8->drawFrame(38, 8, 7, 12);              // Tuer
  s_u8->drawPixel(43, 14);                    // Tuergriff
  s_u8->drawHLine(10, 16, 28);                // Zierstreifen
  wheel(19, 22, 4);
  wheel(54, 22, 4);
}
void draw_caravan() {
  s_u8->drawRFrame(7, 4, 46, 19, 5);          // Aufbau, runde Ecken
  s_u8->drawFrame(12, 8, 10, 5);              // Fenster hinten
  s_u8->drawFrame(40, 8, 9, 5);               // Fenster vorn
  s_u8->drawFrame(26, 8, 7, 13);              // Tuer
  s_u8->drawPixel(31, 14);                    // Tuergriff
  s_u8->drawHLine(8, 17, 18);                 // Zierstreifen
  s_u8->drawHLine(33, 17, 19);
  s_u8->drawLine(52, 19, 63, 22);             // Deichsel
  s_u8->drawLine(52, 21, 63, 22);
  s_u8->drawCircle(65, 22, 2);                // Kupplung
  s_u8->drawVLine(58, 21, 4);                 // Stuetzrad
  s_u8->drawCircle(58, 26, 1);
  wheel(36, 23, 4);                           // eine Achse
}
void draw_splash() {
  bool caravan = false;
  Preferences p;
  if (p.begin("satoled", false)) {            // bei jedem Start abwechseln
    caravan = p.getBool("spl", false);
    p.putBool("spl", !caravan);
    p.end();
  }
  s_u8->clearBuffer();
  if (caravan) draw_caravan();
  else draw_camper();
  s_u8->setFont(u8g2_font_helvB10_tr);
  center(39, "S.O.C.");
  s_u8->sendBuffer();
  s_splash_until = millis() + 2500;
}
}  // namespace

void sat_oled_begin() {
  cfg_load();
  if (!sat_i2c_begin(SAT_OLED_SDA, SAT_OLED_SCL)) {
    log_w("OLED: I2C startet nicht");
    return;
  }
  Wire.beginTransmission(SAT_OLED_ADDR);
  if (Wire.endTransmission() != 0) {
    log_w("OLED: keine Antwort an 0x%02X (SDA %d, SCL %d)", SAT_OLED_ADDR, SAT_OLED_SDA, SAT_OLED_SCL);
    return;
  }
  s_u8 = new U8G2_SSD1306_72X40_ER_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE, SAT_OLED_SCL, SAT_OLED_SDA);
  s_u8->setI2CAddress(SAT_OLED_ADDR * 2);
  s_u8->setBusClock(400000);
  s_ok = s_u8->begin();
  apply_hw();
  if (s_ok && s_cfg.on) draw_splash();
  log_i("OLED %s", s_ok ? "bereit" : "Start fehlgeschlagen");
}

void sat_oled_loop() {
  if (!s_ok || !s_cfg.on) return;
  uint32_t now = millis();
  if (s_splash_until && (int32_t)(now - s_splash_until) < 0) return;   // Startbild stehen lassen
  s_splash_until = 0;
  if (!s_care_ms) s_care_ms = now;
  uint32_t care = now - s_care_ms;
  if (care >= CARE_EVERY_MS) {
    uint32_t t = care - CARE_EVERY_MS;
    if (t < 2 * CARE_STEP_MS) {
      static int8_t shown = -1;
      int8_t phase = t < CARE_STEP_MS ? 0 : 1;   // 0 schwarz, 1 weiss
      if (phase != shown) {
        shown = phase;
        s_u8->clearBuffer();
        if (phase) s_u8->drawBox(0, 0, 72, 40);
        s_u8->sendBuffer();
      }
      return;
    }
    s_care_ms = now;     // fertig, naechste Runde in 20 min
    s_draw_ms = 0;       // sofort wieder normal zeichnen
  }
  float lx, qx;
  bool tilt = sat_io_tilt(lx, qx);
  bool want_level = s_cfg.src == "level" || (s_cfg.src.isEmpty() && tilt);
  if (now - s_draw_ms < (want_level ? 150u : 1000u)) return;
  s_draw_ms = now;
  s_u8->clearBuffer();
  if (want_level) {
    if (tilt) draw_level(lx, qx);
    else {
      s_u8->setFont(u8g2_font_5x8_tf);
      center(16, "Wasserwaage:");
      center(28, "MPU-6050 fehlt");
    }
  } else {
    std::vector<SatPoint> pts = sat_io_points();
    std::vector<const SatPoint *> show;
    for (auto &p : pts)
      if (s_cfg.src.isEmpty() ? is_value_point(p) : p.id == s_cfg.src.c_str()) show.push_back(&p);
    if (show.empty()) {
      if (s_cfg.src.isEmpty()) draw_info();
      else {
        s_u8->setFont(u8g2_font_5x8_tf);
        center(16, s_cfg.src.c_str());
        center(28, "nicht da");
      }
    } else {
      if (now - s_page_ms >= PAGE_MS) {
        s_page_ms = now;
        s_page++;
      }
      draw_value(*show[s_page % show.size()]);
    }
  }
  s_u8->sendBuffer();
}

bool sat_oled_present() { return s_ok; }

void sat_oled_json(JsonObject o) {
  o["present"] = s_ok;
  o["on"] = s_cfg.on;
  o["src"] = s_cfg.src;
  o["rot"] = s_cfg.rot;
  o["con"] = s_cfg.con;
  o["sda"] = SAT_OLED_SDA;
  o["scl"] = SAT_OLED_SCL;
  JsonArray a = o["pts"].to<JsonArray>();
  for (auto &p : sat_io_points()) {
    if (p.kind == SP_NONE || p.kind == SP_TILT) continue;
    JsonObject x = a.add<JsonObject>();
    x["id"] = p.id;
    x["name"] = p.name.empty() ? p.id : p.name;
  }
}

bool sat_oled_apply(JsonObjectConst o) {
  if (o["on"].is<bool>()) s_cfg.on = o["on"];
  if (o["src"].is<const char *>()) s_cfg.src = (const char *)o["src"];
  if (o["rot"].is<bool>()) s_cfg.rot = o["rot"];
  if (!o["con"].isNull()) s_cfg.con = constrain((int)(o["con"] | 128), 1, 255);
  cfg_save();
  apply_hw();
  s_draw_ms = 0;
  return true;
}
#endif
