#include "cerbo.h"

#if WITH_CERBO
#include <Arduino.h>
#include <map>
#include <math.h>
#include <string.h>
#include <Preferences.h>
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "net.h"
#include "i18n/i18n.h"

// ---------------------------------------------------------------- Venus OS MQTT
// Werte:     N/<Portal-ID>/<Dienst>/<Instanz>/<Pfad>   Inhalt {"value": <Zahl|Text|null>}
// Keepalive: R/<Portal-ID>/keepalive – ohne ihn sendet der GX nach 60 s nichts mehr.
//            Leerer Inhalt = alle Werte einmal neu senden; danach mit "suppress-republish" nur Aenderungen.
// Die Portal-ID verraet der GX auch ohne Keepalive ueber N/<Portal-ID>/system/0/Serial.
static const uint32_t KEEPALIVE_MS = 30000;
static const char *KA_SUPPRESS = "{\"keepalive-options\":[\"suppress-republish\"]}";

// Abonniert wird nur, was die fuenf Kacheln brauchen (P = Portal-ID oder +)
static const char *SUBS[] = {
    "system/0/Serial",
    "system/0/Dc/#",                    // Battery/*, Pv/*, System/Power, Charger/Power
    "system/0/Ac/ActiveIn/Source",
    "system/0/Ac/Grid/L1/Power",
    "system/0/Ac/Genset/L1/Power",
    "vebus/+/Ac/ActiveIn/#",            // L1/V, L1/I, P, CurrentLimit, Connected
    "vebus/+/State",
    "vebus/+/Mode",
    "vebus/+/Dc/0/#",
    "charger/+/State",
    "charger/+/Dc/0/#",
    "charger/+/Ac/In/L1/I",
    "solarcharger/+/State",
    "solarcharger/+/History/Daily/0/Yield",
};

enum { T_SOLAR, T_GRID, T_CHARGER, T_BATTERY, T_DC, T_COUNT };
static const char *TILE_ID[T_COUNT] = {"solar", "grid", "charger", "battery", "dc"};

struct Data {
  float pv_w = NAN, pv_i = NAN;
  float soc = NAN, bat_v = NAN, bat_i = NAN, bat_w = NAN, ttg_s = NAN, consumed = NAN;
  float dcsys_w = NAN, dc_charger_w = NAN;
  int ac_source = -1;
  float grid_w = NAN, genset_w = NAN;
  bool vebus = false;
  float vb_in_p = NAN, vb_in_v = NAN, vb_in_i = NAN, vb_limit = NAN;
  int vb_connected = -1, vb_state = -1, vb_mode = -1;
  float vb_dc_w = NAN, vb_dc_i = NAN, vb_dc_v = NAN;
  bool charger = false;
  int chg_state = -1;
  float chg_i = NAN, chg_v = NAN, chg_ac_i = NAN;
  int sol_state = -1;
  std::map<int, float> yield;   // Ertrag heute je Solarregler
  bool seen[T_COUNT] = {false};
};

static SemaphoreHandle_t s_mtx = nullptr;
struct Lock {
  Lock() { xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(s_mtx); }
};

static CerboConfig s_cfg;
static Data s_d;
static std::string s_portal;          // erkannte oder eingestellte Portal-ID
static std::string s_status = "Nicht eingerichtet";
static volatile uint32_t s_ver = 1;
static volatile bool s_connected = false;
static volatile bool s_restart = true;     // Client (neu) aufbauen
static volatile bool s_full = false;       // naechster Keepalive ohne "suppress": alle Werte anfordern
static uint32_t s_disc_ms = 0, s_ka_ms = 0;
static esp_mqtt_client_handle_t s_client = nullptr;
static std::string s_c_host, s_c_id, s_c_user, s_c_pass;   // muessen leben, solange der Client laeuft

static void set_status(const std::string &s) {
  Lock l;
  if (s_status != s) {
    s_status = s;
    s_ver++;
  }
}

// ---------------------------------------------------------------- Konfiguration
static void load_config() {
  Preferences p;
  p.begin("cerbo", true);
  s_cfg.host = p.getString("host", "").c_str();
  s_cfg.port = p.getUShort("port", 1883);
  s_cfg.portal = p.getString("portal", "").c_str();
  s_cfg.user = p.getString("user", "").c_str();
  s_cfg.pass = p.getString("pass", "").c_str();
  p.end();
}

static void save_config() {
  Preferences p;
  p.begin("cerbo", false);
  p.putString("host", s_cfg.host.c_str());
  p.putUShort("port", s_cfg.port);
  p.putString("portal", s_cfg.portal.c_str());
  p.putString("user", s_cfg.user.c_str());
  p.putString("pass", s_cfg.pass.c_str());
  p.end();
}

CerboConfig cerbo_config() {
  Lock l;
  return s_cfg;
}

void cerbo_set_config(const CerboConfig &c) {
  {
    Lock l;
    s_cfg = c;
    if (!s_cfg.port) s_cfg.port = 1883;
    s_d = Data();
    s_portal.clear();
    s_ver++;
  }
  save_config();
  s_restart = true;
}

bool cerbo_configured() {
  Lock l;
  return !s_cfg.host.empty();
}
bool cerbo_connected() { return s_connected; }
uint32_t cerbo_version() { return s_ver; }

std::string cerbo_status() {
  Lock l;
  return s_status;
}
std::string cerbo_portal_id() {
  Lock l;
  return s_portal;
}

// ---------------------------------------------------------------- Empfang
// {"value": 12.5} -> 12.5, {"value": null} -> NAN. Text landet in *str (falls angegeben).
static float parse_value(const char *p, size_t len, std::string *str = nullptr) {
  std::string s(p, len);
  size_t k = s.find("\"value\"");
  if (k == std::string::npos) return NAN;
  k = s.find(':', k);
  if (k == std::string::npos) return NAN;
  k++;
  while (k < s.size() && (s[k] == ' ' || s[k] == '\t')) k++;
  if (k >= s.size() || s[k] == 'n' || s[k] == '[' || s[k] == '{') return NAN;
  if (s[k] == '"') {
    size_t e = s.find('"', k + 1);
    if (str && e != std::string::npos) *str = s.substr(k + 1, e - k - 1);
    return NAN;
  }
  if (s[k] == 't') return 1;
  if (s[k] == 'f') return 0;
  char *end = nullptr;
  float v = strtof(s.c_str() + k, &end);
  return end == s.c_str() + k ? NAN : v;
}

static int to_int(float v) { return isnan(v) ? -1 : (int)lroundf(v); }

static void subscribe_all() {
  std::string portal;
  {
    Lock l;
    portal = s_cfg.portal.empty() ? "+" : s_cfg.portal;
  }
  for (const char *t : SUBS) esp_mqtt_client_subscribe(s_client, ("N/" + portal + "/" + t).c_str(), 0);
}

static void handle(const std::string &topic, const char *payload, size_t len) {
  // N/<portal>/<dienst>/<instanz>/<pfad>
  if (topic.size() < 3 || topic.compare(0, 2, "N/") != 0) return;
  size_t a = topic.find('/', 2);
  if (a == std::string::npos) return;
  std::string portal = topic.substr(2, a - 2);
  size_t b = topic.find('/', a + 1);
  if (b == std::string::npos) return;
  std::string svc = topic.substr(a + 1, b - a - 1);
  size_t c = topic.find('/', b + 1);
  if (c == std::string::npos) return;
  int inst = atoi(topic.c_str() + b + 1);
  std::string path = topic.substr(c + 1);
  std::string str;
  float v = parse_value(payload, len, &str);

  Lock l;
  if (!s_cfg.portal.empty() && portal != s_cfg.portal) return;   // anderer GX am selben Broker
  if (s_portal.empty()) {
    s_portal = portal;
    s_full = true;        // jetzt bekannt: sofort Keepalive senden
    s_ka_ms = 0;
    log_i("Cerbo GX: Portal-ID %s", portal.c_str());
  }
  Data &d = s_d;
  if (svc == "system") {
    if (path == "Serial") return;
    else if (path == "Dc/Pv/Power") { d.pv_w = v; d.seen[T_SOLAR] = true; }
    else if (path == "Dc/Pv/Current") d.pv_i = v;
    else if (path == "Dc/Battery/Soc") { d.soc = v; d.seen[T_BATTERY] = true; }
    else if (path == "Dc/Battery/Voltage") { d.bat_v = v; d.seen[T_BATTERY] = true; }
    else if (path == "Dc/Battery/Current") d.bat_i = v;
    else if (path == "Dc/Battery/Power") d.bat_w = v;
    else if (path == "Dc/Battery/TimeToGo") d.ttg_s = v;
    else if (path == "Dc/Battery/ConsumedAmphours") d.consumed = v;
    else if (path == "Dc/System/Power") { d.dcsys_w = v; d.seen[T_DC] = true; }
    else if (path == "Dc/Charger/Power") d.dc_charger_w = v;
    else if (path == "Ac/ActiveIn/Source") d.ac_source = to_int(v);
    else if (path == "Ac/Grid/L1/Power") { d.grid_w = v; if (!isnan(v)) d.seen[T_GRID] = true; }
    else if (path == "Ac/Genset/L1/Power") { d.genset_w = v; if (!isnan(v)) d.seen[T_GRID] = true; }
    else return;
  } else if (svc == "vebus") {
    d.vebus = true;
    d.seen[T_GRID] = d.seen[T_CHARGER] = true;
    if (path == "Ac/ActiveIn/P") d.vb_in_p = v;
    else if (path == "Ac/ActiveIn/L1/P" && isnan(d.vb_in_p)) d.vb_in_p = v;
    else if (path == "Ac/ActiveIn/L1/V") d.vb_in_v = v;
    else if (path == "Ac/ActiveIn/L1/I") d.vb_in_i = v;
    else if (path == "Ac/ActiveIn/CurrentLimit") d.vb_limit = v;
    else if (path == "Ac/ActiveIn/Connected") d.vb_connected = to_int(v);
    else if (path == "State") d.vb_state = to_int(v);
    else if (path == "Mode") d.vb_mode = to_int(v);
    else if (path == "Dc/0/Power") d.vb_dc_w = v;
    else if (path == "Dc/0/Current") d.vb_dc_i = v;
    else if (path == "Dc/0/Voltage") d.vb_dc_v = v;
    else return;
  } else if (svc == "charger") {
    d.charger = true;
    d.seen[T_CHARGER] = true;
    if (path == "State") d.chg_state = to_int(v);
    else if (path == "Dc/0/Current") d.chg_i = v;
    else if (path == "Dc/0/Voltage") d.chg_v = v;
    else if (path == "Ac/In/L1/I") { d.chg_ac_i = v; d.seen[T_GRID] = true; }
    else return;
  } else if (svc == "solarcharger") {
    d.seen[T_SOLAR] = true;
    if (path == "State") d.sol_state = to_int(v);
    else if (path == "History/Daily/0/Yield") {
      if (isnan(v)) d.yield.erase(inst);
      else d.yield[inst] = v;
    } else return;
  } else return;
  s_ver++;
}

static std::string s_rx_topic, s_rx;   // nur im MQTT-Task benutzt

static void mqtt_event(void *, esp_event_base_t, int32_t id, void *data) {
  auto *ev = (esp_mqtt_event_handle_t)data;
  switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
      s_connected = true;
      s_full = true;
      s_ka_ms = 0;
      subscribe_all();
      set_status(tr("Verbunden, warte auf Daten …"));
      log_i("Cerbo GX verbunden");
      break;
    case MQTT_EVENT_DISCONNECTED:
      if (s_connected) {
        s_disc_ms = millis();
        log_w("Cerbo GX getrennt");
      }
      s_connected = false;
      set_status(tr("Getrennt, verbinde neu …"));
      break;
    case MQTT_EVENT_ERROR:
      if (ev->error_handle) {
        if (ev->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
          set_status(tr("Zugang abgelehnt – Passwort prüfen"));
        } else if (ev->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
          char b[96];
          snprintf(b, sizeof(b), tr("GX %s nicht erreichbar – MQTT am GX eingeschaltet?"), s_c_host.c_str());
          set_status(b);
        }
      }
      break;
    case MQTT_EVENT_DATA:
      // Venus-Werte sind klein; laengere Nachrichten (gestueckelt) werden ignoriert
      if (ev->current_data_offset != 0 || ev->data_len != ev->total_data_len || ev->total_data_len > 512) break;
      s_rx_topic.assign(ev->topic, ev->topic_len);
      handle(s_rx_topic, ev->data, ev->data_len);
      break;
    default:
      break;
  }
}

// ---------------------------------------------------------------- Client
static void stop_client() {
  if (!s_client) return;
  esp_mqtt_client_stop(s_client);
  esp_mqtt_client_destroy(s_client);
  s_client = nullptr;
  s_connected = false;
}

static void start_client() {
  CerboConfig c = cerbo_config();
  if (c.host.empty()) {
    set_status(tr("Nicht eingerichtet"));
    return;
  }
  s_c_host = c.host;
  s_c_user = c.user;
  s_c_pass = c.pass;
  char cid[40];
  snprintf(cid, sizeof(cid), "simply-open-camper-gx-%06llx", (unsigned long long)(ESP.getEfuseMac() & 0xFFFFFF));
  s_c_id = cid;

  esp_mqtt_client_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.broker.address.hostname = s_c_host.c_str();
  cfg.broker.address.port = c.port;
  cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
  cfg.credentials.client_id = s_c_id.c_str();
  if (!s_c_user.empty()) cfg.credentials.username = s_c_user.c_str();
  if (!s_c_pass.empty()) cfg.credentials.authentication.password = s_c_pass.c_str();
  cfg.session.keepalive = 30;
  cfg.network.reconnect_timeout_ms = 10000;
  cfg.network.timeout_ms = 8000;
  cfg.buffer.size = 1024;
  cfg.buffer.out_size = 512;
  cfg.task.stack_size = 4096;
  cfg.task.priority = 4;
  s_client = esp_mqtt_client_init(&cfg);
  if (!s_client) {
    set_status(tr("MQTT-Client konnte nicht starten"));
    return;
  }
  esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY, mqtt_event, nullptr);
  esp_mqtt_client_start(s_client);
  char b[96];
  snprintf(b, sizeof(b), tr("Verbinde mit %s …"), s_c_host.c_str());
  set_status(b);
}

static void send_keepalive() {
  std::string portal;
  {
    Lock l;
    portal = s_portal;
  }
  if (portal.empty()) return;
  bool full = s_full;
  s_full = false;
  std::string t = "R/" + portal + "/keepalive";
  const char *pl = full ? "" : KA_SUPPRESS;
  esp_mqtt_client_enqueue(s_client, t.c_str(), pl, strlen(pl), 0, 0, true);
}

static void cerbo_task(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    bool net = net_connected() || net_ap_active();
    if (s_restart && net) {
      s_restart = false;
      stop_client();
      start_client();
    }
    if (!s_client || !s_connected) continue;
    uint32_t now = millis();
    bool have_portal;
    {
      Lock l;
      have_portal = !s_portal.empty();
      if (have_portal && s_status != "OK" && (s_d.seen[T_BATTERY] || s_d.seen[T_SOLAR] || s_d.seen[T_DC])) {
        s_status = "OK";
        s_ver++;
      } else if (!have_portal && s_status == tr("Verbunden, warte auf Daten …")) {
        s_status = tr("Verbunden, warte auf die Portal-ID des GX …");
        s_ver++;
      }
    }
    if (have_portal && (s_full || !s_ka_ms || now - s_ka_ms >= KEEPALIVE_MS)) {
      s_ka_ms = now ? now : 1;
      send_keepalive();
    }
  }
}

void cerbo_begin() {
  if (!s_mtx) s_mtx = xSemaphoreCreateRecursiveMutex();
  load_config();
  {
    Lock l;
    s_portal = s_cfg.portal;
  }
  s_restart = true;
  xTaskCreatePinnedToCore(cerbo_task, "cerbo", 4096, nullptr, 3, nullptr, 0);
}

// ---------------------------------------------------------------- Kacheln
static void blank(VictronInfo &v) {
  v.bat_v = v.bat_i = v.pv_w = v.yield_kwh = v.load_i = NAN;
  v.soc = v.consumed_ah = v.aux = NAN;
  v.in_v = v.in_i = NAN;
  v.power_w = v.ac_w = v.ac_v = v.ac_i = v.ac_limit = NAN;
  v.aux_mode = 3;
}

static bool tile(int t, VictronInfo &v) {
  Lock l;
  if (s_cfg.host.empty()) return false;
  const Data &d = s_d;
  v = VictronInfo();
  blank(v);
  v.key = std::string(CERBO_KEY_PREFIX) + TILE_ID[t];
  v.mac = "Cerbo GX " + s_cfg.host;
  v.gx = true;
  v.has = d.seen[t];
  v.age_s = s_connected ? 0 : (s_disc_ms ? (int32_t)((millis() - s_disc_ms) / 1000) : -1);
  v.status = s_status;
  switch (t) {
    case T_SOLAR: {
      v.kind = VIC_SOLAR;
      v.name = tr("Solar");
      v.pv_w = d.pv_w;
      v.bat_v = d.bat_v;
      v.bat_i = d.pv_i;
      v.state = d.sol_state;
      if (!d.yield.empty()) {
        float y = 0;
        for (auto &e : d.yield) y += e.second;
        v.yield_kwh = y;
      }
      break;
    }
    case T_GRID:
      v.kind = VIC_GRID;
      v.name = tr("Netz");
      v.ac_source = d.ac_source;
      if (d.vebus) {
        v.ac_w = d.vb_in_p;
        v.ac_v = d.vb_in_v;
        v.ac_i = d.vb_in_i;
        v.ac_limit = d.vb_limit;
        v.ac_connected = d.vb_connected;
      } else {
        v.ac_w = d.ac_source == 2 ? d.genset_w : d.grid_w;
        v.ac_i = d.chg_ac_i;
      }
      if (isnan(v.ac_w)) v.ac_w = d.ac_source == 2 ? d.genset_w : d.grid_w;
      if (v.ac_connected < 0 && d.ac_source >= 0) v.ac_connected = (d.ac_source == 240 || d.ac_source == 0) ? 0 : 1;
      break;
    case T_CHARGER:
      v.kind = VIC_CHARGER;
      v.name = tr("Ladegerät");
      if (d.vebus) {
        v.state = d.vb_state;
        v.mode = d.vb_mode;
        v.power_w = d.vb_dc_w;
        v.bat_v = d.vb_dc_v;
        v.bat_i = d.vb_dc_i;
      } else {
        v.state = d.chg_state;
        v.bat_v = d.chg_v;
        v.bat_i = d.chg_i;
        v.power_w = d.dc_charger_w;
        if (isnan(v.power_w) && !isnan(d.chg_v) && !isnan(d.chg_i)) v.power_w = d.chg_v * d.chg_i;
      }
      break;
    case T_BATTERY:
      v.kind = VIC_SHUNT;
      v.name = tr("Batterie");
      v.soc = d.soc;
      v.bat_v = d.bat_v;
      v.bat_i = d.bat_i;
      v.power_w = !isnan(d.bat_w) ? d.bat_w : (!isnan(d.bat_v) && !isnan(d.bat_i) ? d.bat_v * d.bat_i : NAN);
      v.consumed_ah = isnan(d.consumed) ? NAN : fabsf(d.consumed);
      v.remaining_min = (isnan(d.ttg_s) || d.ttg_s < 0) ? -1 : (int)(d.ttg_s / 60);
      break;
    case T_DC:
      v.kind = VIC_DCSYS;
      v.name = tr("DC-System");
      v.power_w = d.dcsys_w;
      v.bat_v = d.bat_v;
      v.bat_i = (!isnan(d.dcsys_w) && !isnan(d.bat_v) && d.bat_v > 1) ? d.dcsys_w / d.bat_v : NAN;
      break;
  }
  return true;
}

bool cerbo_get(const std::string &key, VictronInfo &out) {
  if (!cerbo_is_key(key)) return false;
  std::string id = key.substr(strlen(CERBO_KEY_PREFIX));
  for (int t = 0; t < T_COUNT; t++)
    if (id == TILE_ID[t]) return tile(t, out);
  return false;
}

std::vector<VictronInfo> cerbo_list() {
  std::vector<VictronInfo> r;
  for (int t = 0; t < T_COUNT; t++) {
    VictronInfo v;
    if (tile(t, v)) r.push_back(v);
  }
  return r;
}

#else   // ---------------------------------------------------------------- ausgebaut

void cerbo_begin() {}
uint32_t cerbo_version() { return 0; }
bool cerbo_configured() { return false; }
bool cerbo_connected() { return false; }
std::string cerbo_status() { return ""; }
std::string cerbo_portal_id() { return ""; }
CerboConfig cerbo_config() { return CerboConfig(); }
void cerbo_set_config(const CerboConfig &) {}
bool cerbo_get(const std::string &, VictronInfo &) { return false; }
std::vector<VictronInfo> cerbo_list() { return {}; }

#endif

const char *cerbo_mode_text(int m) {
  switch (m) {
    case 1: return "Nur Laden";
    case 2: return "Nur Wechselrichter";
    case 3: return "Ein";
    case 4: return "Aus";
    default: return "–";
  }
}

const char *cerbo_source_text(int s) {
  switch (s) {
    case 0: return "Unbekannt";
    case 1: return "Netz";
    case 2: return "Generator";
    case 3: return "Landstrom";
    case 240: return "nicht verbunden";
    default: return "–";
  }
}
