// Wasserwaage (siehe level.h). Kein eigener Task: gerechnet wird beim Abfragen (Kachel, Fenster, Weboberflaeche).
#include "level.h"
#if WITH_LEVEL
#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#include <mutex>
#include "bms.h"
#include "panelsat.h"
#include "i18n/i18n.h"

namespace {
std::mutex s_mtx;
LevelCfg s_cfg;
uint32_t s_ver = 1;
int s_wiz = 0;              // 0 aus, 1 wartet auf "Front angehoben", 2 wartet auf "rechte Seite angehoben"
float s_wiz_a = 0, s_wiz_b = 0;
const float DEG = 57.29578f;
}  // namespace

static void save(const LevelCfg &c) {
  Preferences p;
  if (!p.begin("level", false)) return;
  p.putBool("on", c.on);
  p.putString("src", c.src.c_str());
  p.putUChar("type", c.type);
  p.putFloat("yaw", c.yaw);
  p.putBool("mir", c.mirror);
  p.putFloat("z0", c.z[0]);
  p.putFloat("z1", c.z[1]);
  p.putFloat("g0", c.g0[0]);
  p.putFloat("g1", c.g0[1]);
  p.putFloat("g2", c.g0[2]);
  p.putInt("tr", c.track_cm);
  p.putInt("hi", c.hitch_cm);
  p.putInt("wb", c.wb_cm);
  p.putInt("rp", c.ramp_cm);
  p.putFloat("tol", c.tol);
  p.end();
}

void level_begin() {
  Preferences p;
  LevelCfg c;
  if (p.begin("level", true)) {
    c.on = p.getBool("on", false);
    c.src = p.getString("src", "").c_str();
    c.type = p.getUChar("type", 0) ? 1 : 0;
    c.yaw = p.getFloat("yaw", 0);
    c.mirror = p.getBool("mir", false);
    c.z[0] = p.getFloat("z0", 0);
    c.z[1] = p.getFloat("z1", 0);
    c.g0[0] = p.getFloat("g0", 0);
    c.g0[1] = p.getFloat("g1", 0);
    c.g0[2] = p.getFloat("g2", 1);
    c.track_cm = p.getInt("tr", 200);
    c.hitch_cm = p.getInt("hi", 450);
    c.wb_cm = p.getInt("wb", 380);
    c.ramp_cm = p.getInt("rp", 10);
    c.tol = p.getFloat("tol", 0.3f);
    p.end();
  }
  std::lock_guard<std::mutex> l(s_mtx);
  s_cfg = c;
}

LevelCfg level_cfg() {
  std::lock_guard<std::mutex> l(s_mtx);
  return s_cfg;
}

void level_set_cfg(const LevelCfg &in) {
  LevelCfg c = in;
  c.type = c.type ? 1 : 0;
  c.yaw = fmodf(fmodf(c.yaw, 360.0f) + 360.0f, 360.0f);
  c.track_cm = constrain(c.track_cm, 100, 300);
  c.hitch_cm = constrain(c.hitch_cm, 100, 900);
  c.wb_cm = constrain(c.wb_cm, 150, 700);
  c.ramp_cm = constrain(c.ramp_cm, 2, 40);
  c.tol = constrain(c.tol, 0.1f, 2.0f);
  {
    std::lock_guard<std::mutex> l(s_mtx);
    if (c.src != s_cfg.src) {   // anderer Sensor: alter Nullpunkt gilt nicht mehr
      c.z[0] = c.z[1] = 0;
      c.g0[0] = c.g0[1] = 0;
      c.g0[2] = 1;
      s_wiz = 0;
    }
    s_cfg = c;
    s_ver++;
  }
  save(c);
}

uint32_t level_version() {
  LevelCfg c = level_cfg();
  uint32_t v = s_ver;
#if WITH_BMS
  if (c.on && c.src.rfind(BMS_KEY_PREFIX, 0) == 0) v += bms_version() * 31;
#endif
  return v;
}

std::vector<LevelSource> level_sources() {
  std::vector<LevelSource> r;
#if WITH_SATELLITES
  std::vector<SatNode> nodes = panelsat_nodes();
  for (auto &p : panelsat_point_list()) {
    if (p.kind != SP_TILT || p.idx != 1 || p.key.size() < 5) continue;
    std::string name = p.sat;
    for (auto &n : nodes)
      if (n.id == p.sat && !n.name.empty()) name = n.name;
    r.push_back({p.key.substr(0, p.key.size() - 1), std::string(tr("Satellit ")) + name + " (MPU-6050)"});
  }
#endif
#if WITH_BMS
  for (auto &b : bms_list())
    if (b.model == BMS_RUUVI) r.push_back({b.key, "Ruuvi " + b.name});
#endif
  return r;
}

// Messwert der Quelle: Satellit = Grad laengs/quer (sat), Ruuvi = Beschleunigung (vec)
static bool read_src(const LevelCfg &c, bool &is_vec, float *val, int32_t *age_s) {
  is_vec = false;
  if (c.src.empty()) return false;
#if WITH_SATELLITES
  if (panelsat_is_point_key(c.src)) {
    SatPoint a, b;
    if (!panelsat_point_get(c.src + "1", a) || !panelsat_point_get(c.src + "2", b)) return false;
    if (!a.ok || !b.ok || isnan(a.value) || isnan(b.value)) return false;
    val[0] = a.value;
    val[1] = b.value;
    if (age_s) *age_s = std::max(a.age_s, b.age_s);
    return true;
  }
#endif
#if WITH_BMS
  if (c.src.rfind(BMS_KEY_PREFIX, 0) == 0) {
    BmsInfo b;
    if (!bms_get(c.src, b) || isnan(b.acc[0]) || isnan(b.acc[1]) || isnan(b.acc[2])) return false;
    float n = sqrtf(b.acc[0] * b.acc[0] + b.acc[1] * b.acc[1] + b.acc[2] * b.acc[2]);
    if (n < 0.5f || n > 1.5f) return false;   // gerade bewegt oder Unsinn
    for (int k = 0; k < 3; k++) val[k] = b.acc[k] / n;
    if (age_s) *age_s = b.age_s;
    is_vec = true;
    return true;
  }
#endif
  (void)age_s;
  return false;
}

// Ruuvi: Neigung als zwei Winkel gegen den Nullpunkt g0. Waagerechte Bezugsachsen e1 (Sensor-X, bei senkrechtem
// Einbau Sensor-Y, jeweils in die Waagerechte gelegt) und e2 = g0 x e1 – so geht jede Einbaulage, auch an der Wand.
static void vec_angles(const float *g0in, const float *g, float &a, float &b) {
  float u[3] = {g0in[0], g0in[1], g0in[2]};
  float n = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
  if (n < 0.1f) {
    u[0] = u[1] = 0;
    u[2] = 1;
  } else
    for (float &x : u) x /= n;
  float e1[3] = {1, 0, 0};
  if (fabsf(u[0]) > 0.8f) {
    e1[0] = 0;
    e1[1] = 1;
  }
  float d = e1[0] * u[0] + e1[1] * u[1] + e1[2] * u[2];
  for (int k = 0; k < 3; k++) e1[k] -= d * u[k];
  n = sqrtf(e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2]);
  for (float &x : e1) x /= n;
  float e2[3] = {u[1] * e1[2] - u[2] * e1[1], u[2] * e1[0] - u[0] * e1[2], u[0] * e1[1] - u[1] * e1[0]};
  float gu = g[0] * u[0] + g[1] * u[1] + g[2] * u[2];
  float g1 = g[0] * e1[0] + g[1] * e1[1] + g[2] * e1[2];
  float g2 = g[0] * e2[0] + g[1] * e2[1] + g[2] * e2[2];
  a = atan2f(g1, gu) * DEG;
  b = atan2f(g2, gu) * DEG;
}

static bool raw_of(const LevelCfg &c, float &a, float &b, int32_t *age_s) {
  float val[3];
  bool vec;
  if (!c.on || !read_src(c, vec, val, age_s)) return false;
  if (vec) {
    vec_angles(c.g0, val, a, b);
  } else {
    a = val[0] - c.z[0];
    b = val[1] - c.z[1];
  }
  return true;
}

bool level_raw(float &a, float &b, int32_t *age_s) { return raw_of(level_cfg(), a, b, age_s); }

// Sensor-Winkel -> Neigung laengs (+ Front tiefer) und quer (+ rechte Seite hoeher)
static void orient(const LevelCfg &c, float a, float b, float &pitch, float &roll) {
  float y = c.yaw / DEG;
  pitch = a * cosf(y) + b * sinf(y);
  roll = (-a * sinf(y) + b * cosf(y)) * (c.mirror ? -1.0f : 1.0f);
}

std::string level_zero() {
  LevelCfg c = level_cfg();
  if (!c.on) return tr("Wasserwaage ist ausgeschaltet.");
  float val[3];
  bool vec;
  if (!read_src(c, vec, val, nullptr)) return tr("Der Sensor liefert gerade keinen Wert.");
  if (vec)
    for (int k = 0; k < 3; k++) c.g0[k] = val[k];
  else {
    c.z[0] = val[0];
    c.z[1] = val[1];
  }
  {
    std::lock_guard<std::mutex> l(s_mtx);
    if (s_cfg.src != c.src) return tr("Sensor wurde gerade gewechselt.");
    for (int k = 0; k < 3; k++) s_cfg.g0[k] = c.g0[k];
    s_cfg.z[0] = c.z[0];
    s_cfg.z[1] = c.z[1];
    s_ver++;
    c = s_cfg;
  }
  save(c);
  return "";
}

int level_wizard_step() { return s_wiz; }

std::string level_wizard(const std::string &step, bool &ok) {
  ok = false;
  LevelCfg c = level_cfg();
  if (step == "cancel") {
    s_wiz = 0;
    ok = true;
    return tr("Abgebrochen.");
  }
  float a, b;
  if (!raw_of(c, a, b, nullptr)) {
    s_wiz = 0;
    return tr("Der Sensor liefert gerade keinen Wert.");
  }
  if (step == "start") {
    s_wiz_a = a;
    s_wiz_b = b;
    s_wiz = 1;
    ok = true;
    return c.type ? tr("Jetzt mit beiden Vorderrädern auf die Keile fahren (oder die Front anders anheben), dann „Weiter“.")
                  : tr("Jetzt das Stützrad ein gutes Stück hochkurbeln (Deichsel hoch), dann „Weiter“.");
  }
  float da = a - s_wiz_a, db = b - s_wiz_b;
  float len = sqrtf(da * da + db * db);
  if (step == "front" && s_wiz == 1) {
    if (len < 0.4f) return tr("Kaum Neigung gemessen – die Front bitte etwas mehr anheben und noch einmal „Weiter“.");
    c.yaw = atan2f(-db, -da) * DEG;   // Front hoch = laengs negativ
    level_set_cfg(c);
    s_wiz_a = a;
    s_wiz_b = b;
    s_wiz = 2;
    ok = true;
    return c.type ? tr("Längs passt jetzt. Optional: nur mit den rechten Rädern auf Keile fahren, dann „Weiter“ – oder „Fertig“.")
                  : tr("Längs passt jetzt. Optional: die rechte Seite anheben (Hubstütze/Keil), dann „Weiter“ – oder „Fertig“.");
  }
  if (step == "right" && s_wiz == 2) {
    float y = c.yaw / DEG;
    float q = -da * sinf(y) + db * cosf(y);
    if (fabsf(q) < 0.3f) return tr("Kaum Neigung zur Seite gemessen – die rechte Seite bitte etwas mehr anheben.");
    c.mirror = q < 0;   // rechte Seite hoch = quer positiv
    level_set_cfg(c);
    s_wiz = 0;
    ok = true;
    return tr("Fertig – die Einbaulage ist eingestellt. Jetzt wieder gerade hinstellen und bei Bedarf den Nullpunkt setzen.");
  }
  if (step == "done") {
    s_wiz = 0;
    ok = true;
    return tr("Fertig – die Einbaulage ist eingestellt.");
  }
  return tr("Bitte den Assistenten neu starten.");
}

static std::string cm_text(float v) {
  char b[24];
  snprintf(b, sizeof(b), "%.1f\xE2\x80\xAF" "cm", v);
#if UI_DECIMAL_COMMA
  for (char *p = b; *p; p++)
    if (*p == '.') *p = ',';
#endif
  return b;
}

bool level_get(const std::string &key, VictronInfo &v) {
  if (key != LEVEL_KEY) return false;
  LevelCfg c = level_cfg();
  if (!c.on) return false;
  v = VictronInfo();
  v.key = LEVEL_KEY;
  v.name = tr("Wasserwaage");
  v.kind = VIC_LEVEL;
  v.lv_type = c.type;
  v.lv_ramp = c.ramp_cm;
  v.lv_tol = c.tol;
  float a, b;
  int32_t age = -1;
  if (c.src.empty()) {
    v.status = tr("Kein Sensor gewählt – in der Weboberfläche einstellen.");
    return true;
  }
  if (!raw_of(c, a, b, &age)) {
    v.status = tr("Sensor liefert keine Neigung.");
    return true;
  }
  v.has = true;
  v.age_s = age < 0 ? 0 : age;
  float pitch, roll;
  orient(c, a, b, pitch, roll);
  v.lv_pitch = pitch;
  v.lv_roll = roll;
  auto rise = [](float deg, int cm) { return tanf(fabsf(deg) / DEG) * cm; };
  float worst = std::max(fabsf(pitch), fabsf(roll));
  bool ok = worst <= c.tol;
  if (c.type == 0) {   // Wohnwagen
    v.lv_side = fabsf(roll) <= c.tol ? 0 : (roll < 0 ? 1 : -1) * rise(roll, c.track_cm);   // + rechts anheben
    v.lv_jack = fabsf(pitch) <= c.tol ? 0 : (pitch > 0 ? 1 : -1) * rise(pitch, c.hitch_cm);   // + Stuetzrad hoch
    v.lv_state = ok ? 0 : worst <= 1.5f ? 1 : 2;
    if (ok) v.note = tr("Ausgerichtet");
    else {
      std::string s;
      if (v.lv_side != 0) s = (v.lv_side > 0 ? tr("Rechts ") : tr("Links ")) + cm_text(fabsf(v.lv_side)) + tr(" anheben");
      if (v.lv_jack != 0)
        s += (s.empty() ? std::string() : std::string(", ")) + tr("Stützrad ") + cm_text(fabsf(v.lv_jack)) +
             (v.lv_jack > 0 ? tr(" hoch") : tr(" runter"));
      v.note = s;
    }
  } else {             // Reisemobil: Keilhoehe je Rad (vorne rechts, vorne links, hinten rechts, hinten links)
    float front = pitch > c.tol ? rise(pitch, c.wb_cm) : 0, rear = pitch < -c.tol ? rise(pitch, c.wb_cm) : 0;
    float right = roll < -c.tol ? rise(roll, c.track_cm) : 0, left = roll > c.tol ? rise(roll, c.track_cm) : 0;
    v.lv_wheel[0] = front + right;
    v.lv_wheel[1] = front + left;
    v.lv_wheel[2] = rear + right;
    v.lv_wheel[3] = rear + left;
    int best = 0;
    for (int k = 1; k < 4; k++)
      if (v.lv_wheel[k] > v.lv_wheel[best]) best = k;
    bool too_high = v.lv_wheel[best] > c.ramp_cm;
    v.lv_state = ok ? 0 : (too_high || worst > 1.5f) ? 2 : 1;
    static const char *NAMES[] = {"Vorne rechts", "Vorne links", "Hinten rechts", "Hinten links"};
    if (ok) v.note = tr("Ausgerichtet");
    else
      v.note = tr(NAMES[best]) + std::string(" ") + cm_text(v.lv_wheel[best]) +
               (too_high ? tr(" – Keile zu klein") : tr(" auffahren"));
  }
  v.status = "OK";
  return true;
}

std::vector<VictronInfo> level_list() {
  std::vector<VictronInfo> r;
  VictronInfo v;
  if (level_get(LEVEL_KEY, v)) r.push_back(v);
  return r;
}
#endif
